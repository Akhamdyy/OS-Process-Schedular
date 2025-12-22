#pragma once
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
#define SHM_FRAME_KEY 500

// --- MEMORY CONSTANTS ---
#define MEM_MEMORY_SIZE 512
#define PAGE_SIZE 16
#define NUM_FRAMES (MEM_MEMORY_SIZE / PAGE_SIZE)
#define DISK_ACCESS_TIME 10

// --- PROCESS STATES ---
#define READY 0
#define RUNNING 1
#define BLOCKED 2
#define FINISHED 3
#define WAITING_IO 4 

///==============================
//don't mess with this variable//
static int * shmaddr;          // CHANGED TO STATIC
//===============================

struct processData {
    int id;
    int arrivaltime;
    int runningtime;
    int priority;
    int dependencyId;
    int diskLoc;      
    int memSize;      
};

struct msgbuff {
    long mtype;
    struct processData pData;
};

// --- MEMORY STRUCTURES (Shared between Scheduler & MMU) ---

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

static int getClk()
{
    return *shmaddr;
}

/*
 * All process call this function at the beginning to establish communication between them and the clock module.
 * Again, remember that the clock is only emulation!
*/
static void initClk()
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
 * It terminates the whole system and releases resources.
*/

static void destroyClk(bool terminateAll)
{
    shmdt(shmaddr);
    if (terminateAll)
    {
        killpg(getpgrp(), SIGINT);
    }
}