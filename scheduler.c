#include "headers.h"
#include <math.h>

#define MAX_PROCESSES 100
#define DEFAULT_HEAP_SIZE 100
#define LOG_FILE "scheduler.log"
#define MEM_LOG_FILE "memory.log"
#define PERF_FILE "scheduler.perf"
#define MEM_MEMORY_SIZE 512
#define PAGE_SIZE 16
#define NUM_FRAMES (MEM_MEMORY_SIZE / PAGE_SIZE)
#define DISK_ACCESS_TIME 10
#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define FINISHED 3
#define WAITING_IO 4 

typedef struct {
    int frame_number;
    bool valid;
    bool reference;
    bool dirty;
} PageTableEntry;

typedef struct {
    int pid;        
    int page_num;   
} Frame;

typedef struct PCB {
    int PID;            
    int UID;            
    int PRIORITY;
    int ARRIVAL_TIME;
    int RUN_TIME;
    int REMAINING_TIME;
    int START_TIME;
    int END_TIME;
    int WAIT_TIME;
    int TURNAROUND_TIME;
    int RESUME_TIME; 
    int DISK_BASE;      
    int MEM_LIMIT;      
    PageTableEntry *page_table;
    int page_table_size;
    FILE *request_file;
    int next_req_time;
    int next_req_addr;
    char next_req_type; 
    bool has_pending_req;
    bool requests_finished;
    int state;
    int dependencyId;
    int io_completion_time;
    int faulty_addr; 
} PCB;

int msgq_id;                  
PCB pcbs[MAX_PROCESSES];      
bool ran[MAX_PROCESSES];      
FILE *log_file = NULL;        
FILE *mem_log_file = NULL;
volatile sig_atomic_t usr1_received = 0; 
int current_pcb_index = -1;   
Frame frames[NUM_FRAMES]; 
int clock_ptr = 0;
double total_wta = 0;
double total_wait = 0;
double total_run_time_accum = 0;
double total_wta_sq = 0; 
int finished_process_count = 0;
int process_count_expected = 0;

void init_mmu() {
    for (int i = 0; i < NUM_FRAMES; i++) {
        frames[i].pid = -1; 
        frames[i].page_num = -1;
    }
    mem_log_file = fopen(MEM_LOG_FILE, "w");
    fprintf(mem_log_file, "#Memory Log Started\n");
}

int get_victim_frame() {
    while (1) {
        int pid = frames[clock_ptr].pid;
        int page = frames[clock_ptr].page_num;
        if (pid == -1) { 
            int victim = clock_ptr;
            clock_ptr = (clock_ptr + 1) % NUM_FRAMES;
            return victim;
        }
        PCB *owner = &pcbs[pid - 1]; 
        if (owner->page_table[page].reference) {
            owner->page_table[page].reference = false;
            clock_ptr = (clock_ptr + 1) % NUM_FRAMES;
        } else {
            int victim = clock_ptr;
            clock_ptr = (clock_ptr + 1) % NUM_FRAMES;
            return victim;
        }
    }
}

int allocate_frame(int pid, int page_num, bool is_page_table) {
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
    int victim_idx = get_victim_frame();
    int victim_pid = frames[victim_idx].pid;
    int victim_page = frames[victim_idx].page_num;
    PCB *victim_pcb = &pcbs[victim_pid - 1];
    if (victim_pcb->page_table[victim_page].dirty) {
        fprintf(mem_log_file, "#Swapping out page %d to disk\n", victim_idx);
        victim_pcb->page_table[victim_page].dirty = false;
    }
    victim_pcb->page_table[victim_page].valid = false;
    victim_pcb->page_table[victim_page].frame_number = -1;
    frames[victim_idx].pid = pid;
    frames[victim_idx].page_num = page_num;
    return victim_idx;
}

int mmu_request(PCB *pcb, int logical_addr, char type, int current_time) {
    int page_num = logical_addr / PAGE_SIZE;
    if (page_num >= pcb->page_table_size) {
        return 0; 
    }
    if (pcb->page_table[page_num].valid) {
        pcb->page_table[page_num].reference = true;
        if (type == 'w' || type == 'W') {
            pcb->page_table[page_num].dirty = true;
        }
        return 0; 
    } else {
        fprintf(mem_log_file, "PageFault upon VA %d from process %d\n", logical_addr, pcb->UID);
        pcb->faulty_addr = logical_addr;
        return 1; 
    }
}

void resolve_page_fault(PCB *pcb, int current_time) {
    int page_num = pcb->faulty_addr / PAGE_SIZE;
    int frame = allocate_frame(pcb->UID, page_num, false);
    pcb->page_table[page_num].frame_number = frame;
    pcb->page_table[page_num].valid = true;
    pcb->page_table[page_num].reference = true; 
    pcb->page_table[page_num].dirty = false;    
    fprintf(mem_log_file, "At time %d page %d for process %d is loaded into memory page %d.\n", 
            current_time, page_num, pcb->UID, frame);
}

void parse_next_request(PCB *pcb) {
    if (pcb->requests_finished || !pcb->request_file) return;
    char line[100];
    while(1) {
        long pos = ftell(pcb->request_file); 
        if (fgets(line, sizeof(line), pcb->request_file) == NULL) {
            pcb->requests_finished = true;
            return;
        }
        if (line[0] == '#' || strlen(line) < 2) continue;
        int time, addr;
        char type[5];
        if (sscanf(line, "%d %d %s", &time, &addr, type) == 3) {
            pcb->next_req_time = time;
            pcb->next_req_addr = addr;
            pcb->next_req_type = type[0];
            pcb->has_pending_req = true;
            return;
        }
    }
}

PCB TO_PCB(struct processData p) {
    PCB pcb;
    memset(&pcb, 0, sizeof(PCB));
    pcb.state = READY;
    pcb.PID = -1;
    pcb.UID = p.id;                 
    pcb.PRIORITY = p.priority;      
    pcb.ARRIVAL_TIME = p.arrivaltime;
    pcb.RUN_TIME = p.runningtime;
    pcb.REMAINING_TIME = p.runningtime;
    pcb.START_TIME = -1;
    pcb.END_TIME = 0;
    pcb.WAIT_TIME = 0;
    pcb.TURNAROUND_TIME = 0;
    pcb.RESUME_TIME = 0;
    pcb.dependencyId = p.dependencyId;
    pcb.DISK_BASE = p.diskLoc;
    pcb.MEM_LIMIT = p.memSize;
    pcb.page_table_size = (p.memSize + PAGE_SIZE - 1) / PAGE_SIZE; 
    pcb.page_table = (PageTableEntry*) calloc(pcb.page_table_size, sizeof(PageTableEntry));
    char filename[50];
    sprintf(filename, "request_%d.txt", p.id); 
    pcb.request_file = fopen(filename, "r");
    pcb.has_pending_req = false;
    pcb.requests_finished = false;
    parse_next_request(&pcb); 
    return pcb;
}

int CREATE_PROCESS(int runtime, int uid) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        char runtime_s[32];
        snprintf(runtime_s, sizeof(runtime_s), "%d", runtime);
        execl("./process.out", "process.out", runtime_s, NULL);
        perror("execl(process.out)");
        _exit(1);
    }
    return (int)pid;
}

void sigusr1_handler(int signo) {
    (void)signo;
    usr1_received = 1;
}

void sigchld_handler(int signo) {
    (void)signo;
    while (waitpid(-1, NULL, WNOHANG) > 0) { }
}

void init_scheduler_globals(void) {
    for (int i = 0; i < MAX_PROCESSES; ++i) {
        ran[i] = false;
        pcbs[i].PID = -1;
    }
    log_file = fopen(LOG_FILE, "w");
    if (!log_file) {
        perror("fopen scheduler.log");
        exit(1);
    }
    fprintf(log_file, "#At time x process y state arr w total z remain y wait k\n");
    struct sigaction sa;
    sa.sa_handler = sigusr1_handler;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    sigaction(SIGUSR1, &sa, NULL);
    struct sigaction sc;
    sc.sa_handler = sigchld_handler;
    sigemptyset(&sc.sa_mask);
    sc.sa_flags = SA_RESTART | SA_NOCLDSTOP;
    sigaction(SIGCHLD, &sc, NULL);
}

typedef struct PCB_HEAP {
    PCB* arr;
    int size;
    int capacity;
    bool remaining_time; 
} PCB_HEAP;

void push_up(PCB_HEAP* heap, int index);
void push_down(PCB_HEAP* heap, int index);

PCB_HEAP* PCB_HEAP_INIT(bool use_remaining_time) {
    PCB_HEAP* heap = (PCB_HEAP*) malloc(sizeof(PCB_HEAP));
    heap->arr = (PCB*) malloc(sizeof(PCB) * DEFAULT_HEAP_SIZE);
    heap->capacity = DEFAULT_HEAP_SIZE;
    heap->size = 0;
    heap->remaining_time = use_remaining_time;
    return heap;
}

void swap_pcb(PCB *a, PCB *b) {
    PCB temp = *a;
    *a = *b;
    *b = temp;
}

bool is_better(PCB a, PCB b, bool use_remaining) {
    if (use_remaining) {
        if (a.REMAINING_TIME == b.REMAINING_TIME)
            return a.ARRIVAL_TIME < b.ARRIVAL_TIME;
        return a.REMAINING_TIME < b.REMAINING_TIME;
    } else {
        if (a.PRIORITY == b.PRIORITY)
            return a.ARRIVAL_TIME < b.ARRIVAL_TIME;
        return a.PRIORITY > b.PRIORITY; 
    }
}

void push_up(PCB_HEAP* heap, int index) {
    while (index > 0) {
        int parent = (index - 1) / 2;
        if (is_better(heap->arr[index], heap->arr[parent], heap->remaining_time)) {
            swap_pcb(&heap->arr[index], &heap->arr[parent]);
            index = parent;
        } else {
            break;
        }
    }
}

void push_down(PCB_HEAP* heap, int index) {
    int smallest = index;
    int left = 2 * index + 1;
    int right = 2 * index + 2;
    if (left < heap->size && is_better(heap->arr[left], heap->arr[smallest], heap->remaining_time))
        smallest = left;
    if (right < heap->size && is_better(heap->arr[right], heap->arr[smallest], heap->remaining_time))
        smallest = right;
    if (smallest != index) {
        swap_pcb(&heap->arr[index], &heap->arr[smallest]);
        push_down(heap, smallest);
    }
}

void PCB_HEAP_INSERT(PCB_HEAP* heap, PCB obj) {
    if (heap->size == heap->capacity) {
        heap->capacity *= 2;
        heap->arr = (PCB*) realloc(heap->arr, sizeof(PCB) * heap->capacity);
    }
    heap->arr[heap->size++] = obj;
    push_up(heap, heap->size - 1);
}

PCB PCB_HEAP_EXTRACT(PCB_HEAP* heap) {
    if (heap->size == 0) {
        PCB temp = {0};
        return temp;
    }
    PCB temp = heap->arr[0];
    heap->arr[0] = heap->arr[--heap->size];
    push_down(heap, 0);
    return temp;
}

bool PCB_HEAP_IS_EMPTY(PCB_HEAP* heap) {
    return heap->size == 0;
}

void PCB_HEAP_DESTROY(PCB_HEAP* heap) {
    free(heap->arr);
    free(heap);
}

typedef struct PCB_NODE {
    PCB* data;
    struct PCB_NODE* next;
} PCB_NODE;

typedef struct PCB_QUEUE {
    PCB_NODE* front;
    PCB_NODE* rear;
    int size;
} PCB_QUEUE;

PCB_QUEUE* PCB_QUEUE_INIT() {
    PCB_QUEUE* q = (PCB_QUEUE*)malloc(sizeof(PCB_QUEUE));
    q->front = q->rear = NULL;
    q->size = 0;
    return q;
}

void PCB_QUEUE_ENQUEUE(PCB_QUEUE* q, PCB* pcb) {
    PCB_NODE* newNode = (PCB_NODE*)malloc(sizeof(PCB_NODE));
    newNode->data = pcb;
    newNode->next = NULL;
    if (q->rear == NULL) {
        q->front = q->rear = newNode;
    } else {
        q->rear->next = newNode;
        q->rear = newNode;
    }
    q->size++;
}

PCB* PCB_QUEUE_DEQUEUE(PCB_QUEUE* q) {
    if (q->front == NULL) {
        return NULL;
    }
    PCB_NODE* temp = q->front;
    PCB* pcb = temp->data;
    q->front = q->front->next;
    if (q->front == NULL) {
        q->rear = NULL;
    }
    free(temp);
    q->size--;
    return pcb;
}

bool PCB_QUEUE_IS_EMPTY(PCB_QUEUE* q) {
    return q->front == NULL;
}

void PCB_QUEUE_DESTROY(PCB_QUEUE* q) {
    while (!PCB_QUEUE_IS_EMPTY(q)) {
        PCB_QUEUE_DEQUEUE(q);
    }
    free(q);
}

void HIGHEST_PRIORITY_FIRST(int total_processes) {
    struct msgbuff msg;
    PCB_HEAP* ready_q = PCB_HEAP_INIT(false); 
    PCB current_process;
    int now;
    PCB waiting_list[100]; 
    int wait_count = 0;
    while (finished_process_count < total_processes) {
        while (msgrcv(msgq_id, &msg, sizeof(msg.pData), 0, IPC_NOWAIT) != -1) {
            PCB new_pcb = TO_PCB(msg.pData);
            bool is_ready = true;
            if (new_pcb.dependencyId != -1) {
                if (pcbs[new_pcb.dependencyId - 1].state != FINISHED) {
                    is_ready = false;
                }
            }
            if (is_ready) {
                PCB_HEAP_INSERT(ready_q, new_pcb);
            } else {
                waiting_list[wait_count++] = new_pcb;
            }
        }
        now = getClk();
        if (usr1_received) {
            usr1_received = 0;
            PCB *fin = &pcbs[current_pcb_index];
            fin->END_TIME = now;
            fin->TURNAROUND_TIME = fin->END_TIME - fin->ARRIVAL_TIME;
            fin->WAIT_TIME = fin->TURNAROUND_TIME - fin->RUN_TIME;
            fin->state = FINISHED;
            total_wta += (double)fin->TURNAROUND_TIME / fin->RUN_TIME;
            total_wait += fin->WAIT_TIME;
            total_run_time_accum += fin->RUN_TIME;
            total_wta_sq += pow(((double)fin->TURNAROUND_TIME / fin->RUN_TIME), 2);
            finished_process_count++;
            fprintf(log_file, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n",
                    now, fin->UID, fin->ARRIVAL_TIME, fin->RUN_TIME, 0, fin->WAIT_TIME, 
                    fin->TURNAROUND_TIME, (double)fin->TURNAROUND_TIME / fin->RUN_TIME);
            fflush(log_file);
            for (int i = 0; i < wait_count; i++) {
                if (waiting_list[i].dependencyId == fin->UID) {
                    PCB_HEAP_INSERT(ready_q, waiting_list[i]);
                    waiting_list[i] = waiting_list[wait_count - 1];
                    wait_count--;
                    i--; 
                }
            }
            current_pcb_index = -1; 
        }

        if (current_pcb_index != -1 && !PCB_HEAP_IS_EMPTY(ready_q)) {
            PCB top = ready_q->arr[0]; 
            PCB *curr = &pcbs[current_pcb_index];
            if (top.PRIORITY > curr->PRIORITY) {
                kill(curr->PID, SIGSTOP);
                curr->state = BLOCKED;
                int ran_for = now - curr->RESUME_TIME;
                curr->REMAINING_TIME -= ran_for; 
                fprintf(log_file, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                        now, curr->UID, curr->ARRIVAL_TIME, curr->RUN_TIME, 
                        curr->REMAINING_TIME, curr->WAIT_TIME); 
                fflush(log_file);
                PCB_HEAP_INSERT(ready_q, *curr); 
                current_pcb_index = -1; 
            }
        }

        if (current_pcb_index == -1 && !PCB_HEAP_IS_EMPTY(ready_q)) {
            current_process = PCB_HEAP_EXTRACT(ready_q);
            current_pcb_index = current_process.UID - 1;
            if (!ran[current_pcb_index]) {
                current_process.START_TIME = now;
                current_process.RESUME_TIME = now;
                current_process.WAIT_TIME = now - current_process.ARRIVAL_TIME;
                current_process.state = RUNNING;
                fprintf(log_file, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        now, current_process.UID, current_process.ARRIVAL_TIME, 
                        current_process.RUN_TIME, current_process.RUN_TIME, current_process.WAIT_TIME);
                fflush(log_file);
                current_process.PID = CREATE_PROCESS(current_process.RUN_TIME, current_process.UID);
                pcbs[current_pcb_index] = current_process;
                ran[current_pcb_index] = true;
                
            } else {
                current_process = pcbs[current_pcb_index]; 
                current_process.RESUME_TIME = now;
                int time_spent_running = current_process.RUN_TIME - current_process.REMAINING_TIME;
                current_process.WAIT_TIME = (now - current_process.ARRIVAL_TIME) - time_spent_running;
                current_process.state = RUNNING;
                fprintf(log_file, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        now, current_process.UID, current_process.ARRIVAL_TIME, 
                        current_process.RUN_TIME, current_process.REMAINING_TIME, current_process.WAIT_TIME);
                fflush(log_file);
                pcbs[current_pcb_index] = current_process;
                kill(current_process.PID, SIGCONT);
            }
        }
        usleep(1000); 
    }
    PCB_HEAP_DESTROY(ready_q);
}

void SHORTEST_REMAINING_TIME_NEXT(int total_processes) {
    struct msgbuff msg;
    PCB_HEAP* ready_q = PCB_HEAP_INIT(true); 
    PCB current_process;
    int now;
    while (finished_process_count < total_processes) {
        while (msgrcv(msgq_id, &msg, sizeof(msg.pData), 0, IPC_NOWAIT) != -1) {
            PCB_HEAP_INSERT(ready_q, TO_PCB(msg.pData));
        }
        now = getClk();
        if (usr1_received) {
            usr1_received = 0;
            PCB *fin = &pcbs[current_pcb_index];
            fin->END_TIME = now;
            fin->TURNAROUND_TIME = fin->END_TIME - fin->ARRIVAL_TIME;
            fin->WAIT_TIME = fin->TURNAROUND_TIME - fin->RUN_TIME;
            fin->state = FINISHED;
            total_wta += (double)fin->TURNAROUND_TIME / fin->RUN_TIME;
            total_wait += fin->WAIT_TIME;
            total_run_time_accum += fin->RUN_TIME;
            total_wta_sq += pow(((double)fin->TURNAROUND_TIME / fin->RUN_TIME), 2);
            finished_process_count++;
            fprintf(log_file, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n",
                    now, fin->UID, fin->ARRIVAL_TIME, fin->RUN_TIME, 0, fin->WAIT_TIME, 
                    fin->TURNAROUND_TIME, (double)fin->TURNAROUND_TIME / fin->RUN_TIME);
            fflush(log_file);
            current_pcb_index = -1; 
        }
        if (current_pcb_index != -1 && !PCB_HEAP_IS_EMPTY(ready_q)) {
            PCB top = ready_q->arr[0]; 
            PCB *curr = &pcbs[current_pcb_index];
            int ran_for = now - curr->RESUME_TIME;
            int actual_remaining = curr->REMAINING_TIME - ran_for;            
            if (top.REMAINING_TIME < actual_remaining) {
                kill(curr->PID, SIGSTOP);
                curr->state = BLOCKED;
                curr->REMAINING_TIME = actual_remaining; 
                fprintf(log_file, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                        now, curr->UID, curr->ARRIVAL_TIME, curr->RUN_TIME, 
                        curr->REMAINING_TIME, curr->WAIT_TIME); 
                fflush(log_file);
                PCB_HEAP_INSERT(ready_q, *curr); 
                current_pcb_index = -1; 
            }
        }

        if (current_pcb_index == -1 && !PCB_HEAP_IS_EMPTY(ready_q)) {
            current_process = PCB_HEAP_EXTRACT(ready_q);
            current_pcb_index = current_process.UID - 1;
            if (!ran[current_pcb_index]) {
                current_process.START_TIME = now;
                current_process.RESUME_TIME = now;
                current_process.WAIT_TIME = now - current_process.ARRIVAL_TIME;
                current_process.state = RUNNING;
                fprintf(log_file, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        now, current_process.UID, current_process.ARRIVAL_TIME, 
                        current_process.RUN_TIME, current_process.RUN_TIME, current_process.WAIT_TIME);
                fflush(log_file);
                current_process.PID = CREATE_PROCESS(current_process.RUN_TIME, current_process.UID);
                pcbs[current_pcb_index] = current_process;
                ran[current_pcb_index] = true;
            } else {
                current_process = pcbs[current_pcb_index]; 
                current_process.RESUME_TIME = now;
                int time_spent_running = current_process.RUN_TIME - current_process.REMAINING_TIME;
                current_process.WAIT_TIME = (now - current_process.ARRIVAL_TIME) - time_spent_running;
                current_process.state = RUNNING;
                fprintf(log_file, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        now, current_process.UID, current_process.ARRIVAL_TIME, 
                        current_process.RUN_TIME, current_process.REMAINING_TIME, current_process.WAIT_TIME);
                fflush(log_file);
                pcbs[current_pcb_index] = current_process;
                kill(current_process.PID, SIGCONT);
            }
        }
        usleep(1000); 
    }
    PCB_HEAP_DESTROY(ready_q);
}

void ROUND_ROBIN(int total_processes, int quantum) {
    struct msgbuff msg;
    PCB_QUEUE* ready_q = PCB_QUEUE_INIT(); 
    PCB *current_process = NULL;
    int now;
    int current_q_time = 0;
    while (finished_process_count < total_processes) {
        now = getClk();
        while (msgrcv(msgq_id, &msg, sizeof(msg.pData), 0, IPC_NOWAIT) != -1) {
            PCB new_pcb = TO_PCB(msg.pData);
            int frame = allocate_frame(new_pcb.UID, 0, false);
            new_pcb.page_table[0].frame_number = frame;
            new_pcb.page_table[0].valid = true;
            new_pcb.page_table[0].reference = true;
            fprintf(mem_log_file, "At time %d page 0 for process %d is loaded into memory page %d.\n", 
            now, new_pcb.UID, frame);
            pcbs[new_pcb.UID - 1] = new_pcb;
            PCB_QUEUE_ENQUEUE(ready_q, &pcbs[new_pcb.UID - 1]);
        }
        for (int i=0; i<MAX_PROCESSES; i++) {
            if (pcbs[i].UID != 0 && pcbs[i].state == WAITING_IO) {
                if (now >= pcbs[i].io_completion_time) {
                    resolve_page_fault(&pcbs[i], now);
                    pcbs[i].state = READY;
                    PCB_QUEUE_ENQUEUE(ready_q, &pcbs[i]);
                }
            }
        }
        if (current_process == NULL && !PCB_QUEUE_IS_EMPTY(ready_q)) {
            current_process = PCB_QUEUE_DEQUEUE(ready_q);
            current_q_time = 0;   
            if (current_process->PID == -1) {
                current_process->START_TIME = now;
                current_process->PID = CREATE_PROCESS(current_process->RUN_TIME, current_process->UID);
                current_process->state = RUNNING;
                fprintf(log_file, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        now, current_process->UID, current_process->ARRIVAL_TIME, 
                        current_process->RUN_TIME, current_process->REMAINING_TIME, current_process->WAIT_TIME);
            } else {
                if (current_process->state != RUNNING) {
                     kill(current_process->PID, SIGCONT);
                     current_process->state = RUNNING;
                     int wait = now - current_process->RESUME_TIME;
                     fprintf(log_file, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        now, current_process->UID, current_process->ARRIVAL_TIME, 
                        current_process->RUN_TIME, current_process->REMAINING_TIME, current_process->WAIT_TIME);
                }
            }
        }
        if (current_process != NULL) {
            int time_elapsed = current_process->RUN_TIME - current_process->REMAINING_TIME;   
            bool blocked_by_fault = false;
            if (current_process->has_pending_req && current_process->next_req_time == time_elapsed) {
                int res = mmu_request(current_process, current_process->next_req_addr, 
                                      current_process->next_req_type, now);
                if (res == 1) { 
                    kill(current_process->PID, SIGSTOP);
                    current_process->state = WAITING_IO;
                    current_process->RESUME_TIME = now; 
                    current_process->io_completion_time = now + DISK_ACCESS_TIME;
                    fprintf(log_file, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                        now, current_process->UID, current_process->ARRIVAL_TIME, current_process->RUN_TIME, 
                        current_process->REMAINING_TIME, current_process->WAIT_TIME);
                    current_process = NULL;
                    blocked_by_fault = true;
                } else {
                    parse_next_request(current_process);
                }
            }
            if (!blocked_by_fault) {
                current_process->REMAINING_TIME--;
                current_q_time++;
                if (current_process->REMAINING_TIME <= 0) {
                    current_process->END_TIME = now;
                    current_process->TURNAROUND_TIME = now - current_process->ARRIVAL_TIME;
                    current_process->WAIT_TIME = current_process->TURNAROUND_TIME - current_process->RUN_TIME;
                    current_process->state = FINISHED;
                    fprintf(log_file, "At time %d process %d finished arr %d total %d remain %d wait %d TA %d WTA %.2f\n",
                        now, current_process->UID, current_process->ARRIVAL_TIME, current_process->RUN_TIME, 0, 
                        current_process->WAIT_TIME, current_process->TURNAROUND_TIME, 
                        (double)current_process->TURNAROUND_TIME / current_process->RUN_TIME);
                    finished_process_count++;
                    total_wta += (double)current_process->TURNAROUND_TIME / current_process->RUN_TIME;
                    total_wait += current_process->WAIT_TIME;
                    total_run_time_accum += current_process->RUN_TIME;
                    total_wta_sq += pow(((double)current_process->TURNAROUND_TIME / current_process->RUN_TIME), 2);
                    for(int i=0; i<NUM_FRAMES; i++) {
                        if (frames[i].pid == current_process->UID) {
                            frames[i].pid = -1; 
                            frames[i].page_num = -1;
                        }
                    }
                    if(current_process->request_file) fclose(current_process->request_file);
                    current_process = NULL;
                } 
                else if (current_q_time >= quantum) {
                    kill(current_process->PID, SIGSTOP);
                    current_process->state = READY;
                    current_process->RESUME_TIME = now;
                    fprintf(log_file, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                        now, current_process->UID, current_process->ARRIVAL_TIME, current_process->RUN_TIME, 
                        current_process->REMAINING_TIME, current_process->WAIT_TIME);
                    PCB_QUEUE_ENQUEUE(ready_q, current_process);
                    current_process = NULL;
                }
            }
        }
        sleep(1); 
        (*shmaddr)++; 
    }
    PCB_QUEUE_DESTROY(ready_q);
}

int main(int argc, char * argv[])
{
    initClk();
    init_scheduler_globals();
    init_mmu();
    int algorithm = 0;
    int quantum = 0;
    process_count_expected = 0; 
    if (argc > 1) algorithm = atoi(argv[1]);
    if (argc > 2) quantum = atoi(argv[2]);
    if (argc > 3) process_count_expected = atoi(argv[3]); 
    printf("Scheduler Started: Algo %d, Expected Processes %d\n", algorithm, process_count_expected);
    msgq_id = msgget(MQKEY, 0666 | IPC_CREAT);
    if (msgq_id == -1) {
        perror("Error accessing MSGQ");
        exit(1);
    }
    switch (algorithm) {
        case 1:
            HIGHEST_PRIORITY_FIRST(process_count_expected);
            break;
        case 2:
            SHORTEST_REMAINING_TIME_NEXT(process_count_expected);
            break;
        case 3:
            ROUND_ROBIN(process_count_expected, quantum);
            break;
    }
    FILE *perf_file = fopen(PERF_FILE, "w");
    if (perf_file) {
        double cpu_util = (total_run_time_accum / (getClk())) * 100.0;
        double avg_wta = total_wta / process_count_expected;
        double avg_wait = total_wait / process_count_expected;
        double std_wta = sqrt((total_wta_sq / process_count_expected) - (avg_wta * avg_wta));
        fprintf(perf_file, "CPU utilization = %.2f%%\n", cpu_util);
        fprintf(perf_file, "Avg WTA = %.2f\n", avg_wta);
        fprintf(perf_file, "Avg Waiting = %.2f\n", avg_wait);
        fprintf(perf_file, "Std WTA = %.2f\n", std_wta);
        fclose(perf_file);
    }
    fclose(log_file);
    fclose(mem_log_file);
    msgctl(msgq_id, IPC_RMID, NULL); 
    destroyClk(true);
    return 0;
}