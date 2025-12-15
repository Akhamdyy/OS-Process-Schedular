#include "headers.h"

void clearResources(int);

int msgq_id;

int main(int argc, char * argv[])
{
    signal(SIGINT, clearResources); 
    // TODO Initialization
    // 1. Read the input files.
    // 2. Ask the user for the chosen scheduling algorithm and its parameters, if there are any.
    // 3. Initiate and create the scheduler and clock processes.
    // 4. Use this function after creating the clock process to initialize clock
    FILE *inputFile = fopen("processes.txt", "r");
    if (inputFile == NULL) {
        perror("Error opening input file");
        exit(1);
    }
    struct processData *processesList = NULL;
    int processCount = 0;
    int capacity = 10;
    processesList = (struct processData *) malloc(capacity * sizeof(struct processData));
    char line[256];
    while (fgets(line, sizeof(line), inputFile)) {
        if (line[0] == '#') {
            continue; 
        }
        if(processCount >= capacity) {
            capacity *= 2;
            processesList = (struct processData *) realloc(processesList, capacity * sizeof(struct processData));
        }
        struct processData *p = &processesList[processCount];
        int count = sscanf(line, "%d %d %d %d %d %d %d", &p->id, &p->arrivaltime, &p->runningtime, &p->priority, &p->dependencyId, &p->diskLoc, &p->memSize);           
        if(count >= 6){ 
            processCount++;
        }
    }
    fclose(inputFile);
    
    int algo=0;
    int quantum=0;
    printf("Choose Scheduling Algorithm:\n");
    printf("1. HPF (Highest Priority First)\n");
    printf("2. SRTN (Shortest Remaining Time Next)\n");
    printf("3. RR (Round Robin)\n");
    printf("Enter choice (1-3): ");
    scanf("%d", &algo);
    if(algo == 3){
        printf("Enter Quantum Time: ");
        scanf("%d", &quantum);
    }
    msgq_id = msgget(MQKEY, IPC_CREAT | 0666);
    if (msgq_id == -1) {
        perror("Error: Creating message queue failed");
        exit(1);
    }
    pid_t clk_pid = fork();
    if (clk_pid == 0) {
        execl("./clk.out", "clk.out", NULL);
        perror("Error: Starting clock process failed");
        exit(1);
    }
    pid_t sch_pid = fork();
    if (sch_pid == 0) {
        char algoStr[10];
        char quantumStr[10];
        char countStr[10];
        sprintf(algoStr, "%d", algo);
        sprintf(quantumStr, "%d", quantum);
        sprintf(countStr, "%d", processCount);
        execl("./scheduler.out", "scheduler.out", algoStr, quantumStr, countStr, NULL);
        perror("Error: Starting scheduler process failed");
        exit(1);
    }
    initClk();
    
    // Generation Main Loop
    // 5. Create a data structure for processes and provide it with its parameters.
    // 6. Send the information to the scheduler at the appropriate time.
    // 7. Clear clock resources
    int current_p_idx = 0;
    while (current_p_idx < processCount) {
        int current_time = getClk();
        struct processData *p = &processesList[current_p_idx];
        if(p->arrivaltime <= current_time) {
            struct msgbuff message;
            message.mtype = 1; 
            message.pData = *p; 
            if (msgsnd(msgq_id, &message, sizeof(struct processData), !IPC_NOWAIT) == -1) {
                perror("Error: Sending message to scheduler failed");
            } else {
                printf("Process Generator: Sent process %d at time %d\n", p->id, current_time);
                current_p_idx++;
            }
        } else {
            usleep(100000); 
        }
    }
    int status;
    waitpid(sch_pid, &status, 0);
    msgctl(msgq_id, IPC_RMID, NULL);
    free(processesList);
    destroyClk(true);
    return 0;
}

void clearResources(int signum)
{
    msgctl(msgq_id, IPC_RMID, NULL);
    destroyClk(true);
    exit(0);
}