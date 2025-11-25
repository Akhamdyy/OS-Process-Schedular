#include "headers.h"

//Scheduling Algorithms----------------------------------------------------
void HIGHEST_PRIORITY_FIRST() {

    Message msg;
    bool generator_done = false;
    ready_q = PCB_HEAP_INIT(false); 
    PCB current_process;
    int now;

    while (!generator_done || !PCB_HEAP_IS_EMPTY(ready_q)) {
        
        while (msgrcv(msgq_id, &msg, sizeof(Message), MSG_NEW_PROCESS, IPC_NOWAIT) != -1) {
            if (msg.param.ID != -1) {
                PCB_HEAP_INSERT(ready_q, TO_PCB(msg.param));
                printf("Recieved process %d\n", msg.param.ID);
            } else {
                generator_done = true;
            }
        }

        if (!PCB_HEAP_IS_EMPTY(ready_q)) {
            
            current_process = PCB_HEAP_EXTRACT(ready_q);
            now = getClk();

            
            if (!ran[current_process.UID - 1]) {
                current_process.START_TIME = now;
                current_process.RESUME_TIME = now;
                current_process.WAIT_TIME = current_process.START_TIME - current_process.ARRIVAL_TIME;
                current_process.state = RUNNING;
                fprintf(log_file, "At time %d process %d started arr %d total %d remain %d wait %d\n",
                        now,
                        current_process.UID,
                        current_process.ARRIVAL_TIME,
                        current_process.RUN_TIME,
                        current_process.RUN_TIME,
                        current_process.WAIT_TIME);

                current_pcb_index = current_process.UID - 1;
                fflush(log_file);

                current_process.PID = CREATE_PROCESS(current_process.RUN_TIME, current_process.UID);
                pcbs[current_pcb_index] = current_process;
                ran[current_pcb_index] = true;
            } else {
                
                printf("Resuming process %d\n", current_process.UID);
                current_process = pcbs[current_process.UID - 1];
                current_process.RESUME_TIME = now;
                current_process.state = RUNNING;
                fprintf(log_file, "At time %d process %d resumed arr %d total %d remain %d wait %d\n",
                        now,
                        current_process.UID,
                        current_process.ARRIVAL_TIME,
                        current_process.RUN_TIME,
                        current_process.REMAINING_TIME,
                        current_process.WAIT_TIME);
                current_pcb_index = current_process.UID - 1;
                pcbs[current_pcb_index] = current_process;
                
                if (kill(current_process.PID, SIGCONT) == -1) {
                    perror("Error resuming process");
                }
            }

            
            while (1) {
                if (msgrcv(msgq_id, &msg, sizeof(Message), MSG_NEW_PROCESS, IPC_NOWAIT) != -1) {
                    now = getClk();
                    if (msg.param.ID != -1) {
                        PCB new_process = TO_PCB(msg.param);

                        int elapsed = now - current_process.RESUME_TIME;
                        if (elapsed < 0) elapsed = 0;
                        current_process.REMAINING_TIME -= elapsed;
                        if (current_process.REMAINING_TIME < 0) current_process.REMAINING_TIME = 0;
                        pcbs[current_process.UID - 1] = current_process;

                        if (new_process.PRIORITY < current_process.PRIORITY) {
                            if (kill(current_process.PID, SIGSTOP) == -1) {
                                printf("Process %d could not be stopped with signal %d\n", current_process.UID, SIGSTOP);
                            } else {
                                printf("Stopping process %d (preempted by %d)\n", current_process.UID, new_process.UID);
                                fprintf(log_file, "At time %d process %d stopped arr %d total %d remain %d wait %d\n",
                                        now,
                                        current_process.UID,
                                        current_process.ARRIVAL_TIME,
                                        current_process.RUN_TIME,
                                        current_process.REMAINING_TIME,
                                        current_process.WAIT_TIME);
                                fflush(log_file);
                                PCB_HEAP_INSERT(ready_q, current_process);
                                PCB_HEAP_INSERT(ready_q, new_process);
                                break; 
                            }
                        } else {
                            PCB_HEAP_INSERT(ready_q, new_process);
                        }
                    } else {
                        generator_done = true;
                    }
                }

                usleep(1000 * 500);

                
                if (usr1_received) {
                    usr1_received = 0;
                    break;
                }
            }
        } else {
            if (!generator_done) {
                if (msgrcv(msgq_id, &msg, sizeof(Message), MSG_NEW_PROCESS, !IPC_NOWAIT) != -1) {
                    if (msg.param.ID != -1) {
                        PCB_HEAP_INSERT(ready_q, TO_PCB(msg.param));
                    } else {
                        generator_done = true;
                    }
                }
            }
        }
    }

    PCB_HEAP_DESTROY(ready_q);
}


int main(int argc, char * argv[])
{
    initClk();
    
    //TODO implement the scheduler :)
    //upon termination release the clock resources.
    
    destroyClk(true);
}
