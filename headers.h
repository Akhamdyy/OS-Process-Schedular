#include <stdio.h>      //if you don't use scanf/printf change this include
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <sys/msg.h>
#include <sys/wait.h>
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
#include <string.h>
#include <errno.h>

typedef short bool;
#define true 1
#define false 0

#define SHKEY 300
#define MQKEY 400

///==============================
//don't mess with this variable//
int * shmaddr;                 //
//===============================

struct processData {
    int id;
    int arrivaltime;
    int runningtime;
    int priority;
    int dependencyId;
};

struct msgbuff {
    long mtype;
    struct processData pData;
};

int getClk()
{
    return *shmaddr;
}

//---------------------------------------ammar's additions---------------------------------------//


/* Globals required by scheduler.c */
int msgq_id;                              /* message queue id (shared ma3 generator) */
PCB pcbs[MAX_PROCESSES];                  /* bt store active PCBs indexed bel UID-1 */
bool ran[MAX_PROCESSES];                  /* process run wla la */
FILE *log_file = NULL;                    /* scheduler log file */
volatile sig_atomic_t usr1_received = 0;  /* set bel SIGUSR1 handler lma el chile ykhallas*/
int current_pcb_index = -1;               /* index of currently running PCB (UID-1) */


PCB TO_PCB(Process_Param p) {
    PCB pcb;
    memset(&pcb, 0, sizeof(PCB));
    pcb.state = READY;
    pcb.PID = -1;
    pcb.UID = p.ID;
    pcb.PRIORITY = p.Priority;
    pcb.ARRIVAL_TIME = p.Arrival_Time;
    pcb.RUN_TIME = p.runtime;
    pcb.REMAINING_TIME = p.runtime;
    pcb.START_TIME = pcb.END_TIME = pcb.WAIT_TIME = pcb.TURNAROUND_TIME = 0;
    pcb.RESUME_TIME = 0;

    return pcb;
}


int CREATE_PROCESS(int runtime, int uid) {
    pid_t pid = fork();
    if (pid < 0) {
        perror("fork");
        return -1;
    }
    if (pid == 0) {
        
        char runtime_s[32], uid_s[32];
        snprintf(runtime_s, sizeof(runtime_s), "%d", runtime);
        snprintf(uid_s, sizeof(uid_s), "%d", uid);

        execl("./process.out", "process.out", runtime_s, uid_s, NULL);
        
        perror("execl(process.out)");
        _exit(1);
    }
    
    return (int)pid;
}


void sigusr1_handler(int signo) {
    (void)signo;
    usr1_received = 1;
    if (current_pcb_index >= 0 && current_pcb_index < MAX_PROCESSES) {
        int now = getClk();
        pcbs[current_pcb_index].END_TIME = now;
        pcbs[current_pcb_index].TURNAROUND_TIME =
            pcbs[current_pcb_index].END_TIME - pcbs[current_pcb_index].ARRIVAL_TIME;
    }
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
        perror("fopen(" LOG_FILE ")");
        
    }

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
}PCB_HEAP;


PCB_HEAP* PCB_HEAP_INIT(bool runtime) {
    PCB_HEAP* heap = (PCB_HEAP*) malloc(sizeof(PCB_HEAP));
    heap->arr = (PCB*) malloc(sizeof(PCB) * DEFAULT_HEAP_SIZE);
    heap->capacity = DEFAULT_HEAP_SIZE;
    heap->size = 0;
    heap->remaining_time = runtime;
    return heap;
}

void PCB_HEAP_INSERT(PCB_HEAP* heap, PCB obj) {
    if (heap->size == heap->capacity) {
        heap->capacity *= 2;
        heap->arr = (PCB*) realloc(heap->arr, sizeof(PCB) * heap->capacity);
    }
    heap->arr[heap->size++] = obj;
    push_up(heap, heap->size-1);

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

PCB PCB_HEAP_PEEK_MIN(PCB_HEAP* heap) {
    if (heap->size != 0) {
        PCB temp = heap->arr[0];
        return temp;
    }
    PCB temp;
    return temp;
}

bool PCB_HEAP_IS_EMPTY(PCB_HEAP* heap) {
    return heap->size == 0;
}

void PCB_HEAP_DESTROY(PCB_HEAP* heap) {
    free(heap->arr);
    free(heap);
}

int PCB_HEAP_SIZE(PCB_HEAP* heap) {
    return heap->size;
}

//------------------------------ammar's additions end------------------------------//

/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
void initClk()
{
    int shmid = shmget(SHKEY, 4, 0444);
    while ((int)shmid == -1)
    {
        //Make sure that the clock exists
        printf("Wait! The clock not initialized yet!\n");
        sleep(1);
        shmid = shmget(SHKEY, 4, 0444);
    }
    shmaddr = (int *) shmat(shmid, (void *)0, 0);
}


/*
 * All process call this function at the end to release the communication
 * resources between them and the clock module.
 * Again, Remember that the clock is only emulation!
 * Input: terminateAll: a flag to indicate whether that this is the end of simulation.
 *                      It terminates the whole system and releases resources.
*/

void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}


