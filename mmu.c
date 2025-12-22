#include "mmu.h"

static Frame *frames = NULL;   
static int shm_frame_id = -1;  
static int clock_ptr = 0;      
static PCB *pcb_list = NULL;    
static FILE *mem_log_file = NULL;

/*
 * Initialize MMU: Connect to SHM and setup logging.
 */
void init_mmu(PCB *process_list_ptr) {
    // 1. Store PCB list pointer
    pcb_list = process_list_ptr;

    // 2. Create Frame SHM
    shm_frame_id = shmget(SHM_FRAME_KEY, sizeof(Frame) * NUM_FRAMES, IPC_CREAT | 0666);
    if (shm_frame_id == -1) {
        perror("Error creating Shared Memory for Frames");
        exit(1);
    }

    // 3. Attach SHM
    frames = (Frame *)shmat(shm_frame_id, (void *)0, 0);
    if (frames == (void *)-1) {
        perror("Error attaching to Frame Shared Memory");
        exit(1);
    }

    // 4. Initialize Frames
    for (int i = 0; i < NUM_FRAMES; i++) {
        frames[i].pid = -1; 
        frames[i].page_num = -1;
    }

    // 5. Open Log
    mem_log_file = fopen("memory.log", "w");
    if (mem_log_file == NULL) {
        perror("Error opening memory.log");
        exit(1);
    }
    fprintf(mem_log_file, "#Memory Log Started\n");
    fflush(mem_log_file);
}

/*
 * Clean up SHM & Log.
 */
void destroy_mmu() {
    if (mem_log_file) fclose(mem_log_file);
    
    // Remove SHM
    shmdt(frames);
    shmctl(shm_frame_id, IPC_RMID, NULL);
}

/*
 * Find victim frame (Second Chance Algorithm).
 */
int get_victim_frame() {
    while (1) {
        int pid = frames[clock_ptr].pid;
        int page = frames[clock_ptr].page_num;

        // Check for empty frame
        if (pid == -1) { 
            int victim = clock_ptr;
            clock_ptr = (clock_ptr + 1) % NUM_FRAMES;
            return victim;
        }

        // Get owner PCB (PID is 1-based)
        PCB *owner = &pcb_list[pid - 1]; 
        
        if (owner->page_table[page].reference) {
            // Second Chance: Reset Ref Bit
            owner->page_table[page].reference = false; 
            clock_ptr = (clock_ptr + 1) % NUM_FRAMES;
        } else {
            // Victim Found
            int victim = clock_ptr;
            clock_ptr = (clock_ptr + 1) % NUM_FRAMES;
            return victim;
        }
    }
}

/*
 * Allocate frame.
 */
int allocate_frame(int pid, int page_num, bool is_page_table) {
    // 1. Find Free Frame
    for (int i = 0; i < NUM_FRAMES; i++) {
        if (frames[i].pid == -1) {
            frames[i].pid = pid;
            frames[i].page_num = page_num;
            if (!is_page_table) {
                 fprintf(mem_log_file, "#Free Physical page %d allocated\n", i);
            }
            return i;
        }
    }

    // 2. Find Victim if full
    int victim_idx = get_victim_frame();
    int victim_pid = frames[victim_idx].pid;
    int victim_page = frames[victim_idx].page_num;
    
    PCB *victim_pcb = &pcb_list[victim_pid - 1];

    // Check Dirty Bit (Swap out if needed)
    if (victim_pcb->page_table[victim_page].dirty) {
        fprintf(mem_log_file, "#Swapping out page %d to disk\n", victim_idx);
        victim_pcb->page_table[victim_page].dirty = false;
    }

    // Update Victim PT
    victim_pcb->page_table[victim_page].valid = false;
    victim_pcb->page_table[victim_page].frame_number = -1;

    // Assign to New Owner
    frames[victim_idx].pid = pid;
    frames[victim_idx].page_num = page_num;
    
    return victim_idx;
}

/*
 * Handle Memory Access.
 */
int mmu_request(PCB *pcb, int logical_addr, char type, int current_time) {
    int page_num = logical_addr / PAGE_SIZE;

    // Bounds check
    if (page_num >= pcb->page_table_size) return 0; 

    // Check Validity
    if (pcb->page_table[page_num].valid) {
        // Hit: Set Ref Bit
        pcb->page_table[page_num].reference = true;
        
        // Write: Set Dirty Bit
        if (type == 'w' || type == 'W') {
            pcb->page_table[page_num].dirty = true;
        }
        return 0; // Success
    } else {
        // Miss: Page Fault
        fprintf(mem_log_file, "PageFault upon VA %d from process %d\n", logical_addr, pcb->UID);
        pcb->faulty_addr = logical_addr; 
        return 1; // Fault
    }
}

/*
 * Load Page after I/O.
 */
void resolve_page_fault(PCB *pcb, int current_time) {
    int page_num = pcb->faulty_addr / PAGE_SIZE;
    
    // Allocate Frame
    int frame = allocate_frame(pcb->UID, page_num, false);
    
    // Update PT
    pcb->page_table[page_num].frame_number = frame;
    pcb->page_table[page_num].valid = true;
    pcb->page_table[page_num].reference = true; 
    pcb->page_table[page_num].dirty = false;    

    // Log Load
    fprintf(mem_log_file, "At time %d page %d for process %d is loaded into memory page %d.\n", 
            current_time, page_num, pcb->UID, frame);
    fflush(mem_log_file);
}