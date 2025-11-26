#include "headers.h"

/* Modify this file as needed*/
int remainingtime;
int last_time;

void cont_handler(int signum) {
    last_time = getClk(); //bt3mel rest lel last time 3ashan may3desh el wa2t el process sleeping
}

int main(int agrc, char * argv[])
{
    initClk();
    signal(SIGCONT, cont_handler);
    //TODO it needs to get the remaining time from somewhere
    if (agrc < 2) {
        perror("Process started with no arguments");
        destroyClk(false);
        exit(1);
    }
    remainingtime = atoi(argv[1]);
    last_time = getClk();
    while (remainingtime > 0)
    {   
        if (getClk() > last_time) {
            remainingtime--;
            last_time = getClk();
        }
    }
    kill(getppid(), SIGUSR1);
    destroyClk(false);
    return 0;
}
