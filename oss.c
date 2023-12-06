#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <sys/wait.h>
#include <sys/shm.h>
#include <sys/ipc.h>
#include <sys/time.h>
#include <time.h>
#include <signal.h>
#include <sys/msg.h>
#include <sys/types.h>
#include <string.h>
#include <errno.h>

//Creator: Jarod Stagner
//Turn-in date:

#define SHMKEY 55555
#define PERMS 0644
#define MSGKEY 66666

typedef struct msgbuffer {
        long mtype;
        int intData[3];//may need to add another in for pid, may not be needed though
} msgbuffer;

static void myhandler(int s){
        printf("Killing all... exiting...\n");
        kill(0,SIGTERM);

}

static int setupinterrupt(void) {
                struct sigaction act;
                act.sa_handler = myhandler;
                act.sa_flags = 0;
                return (sigemptyset(&act.sa_mask) || sigaction(SIGPROF, &act, NULL));
}
static int setupitimer(void) {
                struct itimerval value;
                value.it_interval.tv_sec = 60;
                value.it_interval.tv_usec = 0;
                value.it_value = value.it_interval;
                return (setitimer(ITIMER_PROF, &value, NULL));
}

struct PCB {
        int occupied;
        pid_t pid;
        int startSeconds;
        int startNano;
        int isWaiting;
        int eventSec;
        int eventNano;
        int eventPage;
        int eventRorW;
        int pageTable[32];
};

struct fTable {
        int processNum;
        int page;
        int dirtyBit;
        int assigned;
};

void displayPCB(int i, struct PCB *processTable, FILE *file){
        fprintf(file,"Process Table:\nEntry Occupied PID      StartS StartN Waiting    EventSec EventNano Frames 0-31\n");
        printf("Process Table:\nEntry Occupied PID      StartS StartN Frames 0-31\n");
        for (int x = 0; x < i; x++){
                fprintf(file,"%d        %d      %d      %d      %d      %d        %d        %d ",x,processTable[x].occupied,processTable[x].pid,processTable[x].startSeconds,processTable[x].startNano,processTable[x].isWaiting,processTable[x].eventSec,processTable[x].eventNano);
                printf("%d      %d      %d      %d      %d      %d        %d        %d ", x,processTable[x].occupied,processTable[x].pid,processTable[x].startSeconds,processTable[x].startNano,processTable[x].isWaiting,processTable[x].eventSec,processTable[x].eventNano);
                for(int y = 0; y < 32; y++){
                        if(y == 15){ printf("\n"); fprintf(file,"\n");}
                        printf("%3d ", processTable[x].pageTable[y]);
                        fprintf(file,"%3d ", processTable[x].pageTable[y]);
                }
                printf("\n");
                fprintf(file,"\n");
        }
}

void displayFrameTable(struct fTable frameTable*, FILE *file){
        fprintf(file,"Frame Table: ([ProcessNum, Page][DirtyBit][Assigned])\n");
        printf("Frame table: ([ProcessNum, Page][DirtyBit][Assigned])\n");
        for(int x = 0; x < 256; x++){
                if(x == 31 || x == 63 || x == 95 || x == 127 || x == 159 || x == 191 || x == 223){ printf("\n"); fprintf(file,"\n");}
                printf("[%d, %d][%d][%d] ", frameTable[x].processNum, frameTable[x].page, frameTable[x].dirtyBit, frameTable[x].assigned);
                fprintf(file,"[%d, %d][%d][%d] ", frameTable[x].processNum, frameTable[x].page, frameTable[x].dirtyBit, frameTable[x].assigned);
        }
        printf("\n");
        fprintf(file,"\n");
}

void updateTime(int *sharedTime, amount){
        sharedTime[1] = sharedTime[1] + amount;
        if (sharedTime[1] >= 1000000000 ){
                sharedTime[0] = sharedTime[0] + 1;
                sharedTime[1] = sharedTime[1] - 1000000000;
        }
}

void help(){
        printf("This program is designed to take in 4 parameters: \n-n [num processes to launch]\n-s [num that can run at once]\n-t [time in nanoseconds between launches]\n-f [file to write log]\n");
        printf("This program will not run without these paramters being provided, do not set n over 20\nThis program simulates children asking for resources, deadlock detection, and deadlock resolution\n");
        printf("This program also takes in a paramter -v 1 if you want to just see output from deadlocks\n");
}

int main(int argc, char** argv) {

        msgbuffer messenger;
        msgbuffer receiver;
        int msqid;

        if ((msqid = msgget(MSGKEY, PERMS | IPC_CREAT)) == -1) {
                perror("msgget in parent");
                exit(1);
        }

        if (setupinterrupt() == -1) {
                perror("Failed to set up handler for SIGPROF");
                return 1;
        }
        if (setupitimer() == -1) {
                perror("Failed to set up the ITIMER_PROF interval timer");
                return 1;
        }

        srand(time(NULL));
        int seed = rand();
        int proc = 5;
        int simul = 3;
        int maxTime = 100000;//default parameters
        int verbose = 0;
        FILE *file;

        int shmid = shmget(SHMKEY, sizeof(int)*2, 0777 | IPC_CREAT);
        if(shmid == -1){
                printf("Error in shmget\n");
                return EXIT_FAILURE;
        }
        int * sharedTime = (int *) (shmat (shmid, 0, 0));
        sharedTime[0] = 0;
        sharedTime[1] = 0;

        int option;
        while((option = getopt(argc, argv, "hn:s:t:f:v:")) != -1) {//Read command line arguments
                switch(option){
                        case 'h':
                                help();
                                return EXIT_FAILURE;
                                break;
                        case 'n':
                                proc = atoi(optarg);
                                break;
                        case 's':
                                simul = atoi(optarg);
                                break;
                        case 't':
                                maxTime = atoi(optarg);
                                break;
                        case 'f':
                                file = fopen(optarg,"w");
                                break;
                        case 'v':
                                verbose = atoi(optarg);
                                break;
                        case '?':
                                help();
                                return EXIT_FAILURE;
                                break;
                }
        }

        fprintf(file,"Ran with arguments -n %d -s %d -t %d \n", proc,simul,maxTime);


        struct PCB processTable[30];
        for (int y = 0; y < 30; y++){
                processTable[y].occupied = 0;
                processTable[y].isWaiting = 0;
                processTable[y].eventPage = -1;
                for(int x = 0; x < 32; x++){
                        processTable[y].pageTable[x] = -1;
                }
        }
        struct fTable frameTable[256];
        for(int c = 0; c < 256; c++){
                frameTable[c].processNum = -1;
                frameTable[c].page = -1;
                frameTable[c].dirtyBit = 0;
                frameTable[c].assigned = -1;
        }

        int totalInSystem = 0;
        int status;
        int totalLaunched = 0;
        int next = 0;
        int nextLaunchTime[2];
        nextLaunchTime[0] = 0;
        nextLaunchTime[1] = 0;
        int canLaunch;
       
        int halfSecCounter = 0;
        int framesFilled = 0;
        int firstOutFrame = -1;
        int fifoCounter = 0;
        int selected;
        int pageIndex = 0;
        int isPagePresent = 0;

        while(1){
                seed++;
                srand(seed);
                //printf("Looping...\n");
                updateTime(sharedTime, 100000);//add these around
                halfSecCounter += 100000;

                //check if process has terminated
                for (int x = 0; x < totalLaunched; x++){
                        if (processTable[x].occupied == 1){
                                if (waitpid(processTable[x].pid, &status, WNOHANG) > 0){
                                        processTable[x].occupied = 0;
                                        //clear pageTable
                                        for(int y = 0; y < 32; y++){
                                                processTable[x].pageTable[y] = -1;
                                        }
                                        //clear entries out of frametable
                                        for(int z = 0; z < 256; z++){
                                                if(frameTable[z].processNum == x){
                                                        frameTable[z].processNum = -1;
                                                        frameTable[z].page = -1;
                                                        frameTable[z].dirtyBit = 0;//May need editing
                                                        frameTable[z].assigned = -1;
                                                }
                                        }
                                        //if(verbose != 1){
                                        printf("OSS has detected Process P%d has terminated at time %d:%d, freeing frames\n", x,sharedTime[0],sharedTime[1]);
                                        fprintf(file, "OSS has detected Process P%d has terminated at time %d:%d, freeing frames\n", x,sharedTime[0],sharedTime[1]);
                                       // }
                                }
                        }
                }
                updateTime(sharedTime, 100000);
                halfSecCounter += 100000;
                //printf("CheckPoint 1\n");
                //recalculate total
                totalInSystem = 0;
                for (int x = 0; x < proc; x++){
                        totalInSystem += processTable[x].occupied;
                }

                //determine if simulation is over
                if(totalInSystem == 0 && totalLaunched == proc){
                        break;
                }

                if (totalLaunched != 0){//see if worker can be launched
                        if (nextLaunchTime[0] < sharedTime[0]){
                                canLaunch = 1;
                        }else if (nextLaunchTime[0] == sharedTime[0] && nextLaunchTime[1] <= sharedTime[1]){
                                canLaunch = 1;
                        }else{
                                canLaunch = 0;
                        }
                }

                //printf("CheckPoint 2\n");
                //launch worker if
                if((totalInSystem < simul && canLaunch == 1 && totalLaunched < proc) || totalLaunched == 0){

                        nextLaunchTime[1] = sharedTime[1] + maxTime;
                        nextLaunchTime[0] = sharedTime[0];
                        while (nextLaunchTime[1] >= 1000000000){
                                nextLaunchTime[0] = nextLaunchTime[0] + 1;
                                nextLaunchTime[1] = nextLaunchTime[1] - 1000000000;
                        }

                        pid_t child_pid = fork();
                        if(child_pid == 0){
                                char *args[] = {"./worker", NULL};
                                execvp("./worker", args);

                                printf("Something horrible happened...\n");
                                exit(1);
                        }else{
                                processTable[totalLaunched].occupied = 1;
                                processTable[totalLaunched].pid = child_pid;
                                processTable[totalLaunched].startSeconds = sharedTime[0];
                                processTable[totalLaunched].startNano = sharedTime[1];
                                //printf("Generating process with PID %d at time %d:%d\n",child_pid,sharedTime[0],sharedTime[1]);
                                //fprintf(file,"Generating process with PID %d at time %d:%d\n",child_pid,sharedTime[0],sharedTime[1]);
                        }
                        totalLaunched++;
                        //sleep(1);
                }
                updateTime(sharedTime, 15000);
                halfSecCounter += 15000;
                //check if request can be granted
                for(int x = 0; x < totalLaunched; x++){
                        if(processTable[x].isWaiting == 1){
                                if((processTable[x].eventSec < sharedTime[0]) || (processTable[x].eventSec == sharedTime[0] && processTable[x].eventNano <= sharedTime[1])){
                                        framesFilled = 0;
                                        for(int y = 0; y < 256; y++){
                                                if(frameTable[y].processNum != -1){
                                                        framesFilled++;
                                                }
                                                if(frameTable[y].assigned > -1 && frameTable[y].assigned < firstOutFrame){
                                                        firstOutFrame = frameTable[y].assigned;
                                                }
                                        }
                                        if(framesFiled == 256){
                                                //give the firstoutframe to the process
                                                for(int z = 0; z < 256; z++){
                                                        if(frameTable[z].assigned == firstOutFrame){
                                                                printf("Replacing frame %d with P%d page %d\n", z, x, processTable[x].eventPage);
                                                                processTable[frameTable[z].processNum].pageTable[frameTable[z].page] = -1;
                                                                if(frameTable[z].dirtyBit == 1){
                                                                        //add extra time
                                                                        printf("Dirty bit checked in frame %d, adding extra time...\n",z);
                                                                        updateTime(sharedTime, 200000);
                                                                        halfSecCounter += 200000;
                                                                }
                                                                frameTable[z].processNum = x;
                                                                frameTable[z].page = processTable[x].eventPage;
                                                                if(process[x].eventRorW == 1){
                                                                        frameTable[z].dirtyBit = 1;
                                                                }else{
                                                                        frameTable[z].dirtyBit = 0;
                                                                }
                                                                frameTable[z].assigned = fifoCounter;
                                                                fifoCounter++;
                                                                processTable[x].eventSec = 0;
                                                                processTable[x].eventNano = 0;
                                                                processTable[x].isWaiting = 0;
                                                                processTable[x].eventRorW = -1;
                                                                processTable[x].eventPage = -1;
                                                                break;
                                                        }
                                                }
                                                //add appropriate value to assigned(fifoCounter)
                                                //message back the waiting process
                                                printf("Indicating to P%d that request has been fufilled\n",x);
                                                messenger.mtype = processTable[x].pid;
                                                messenger.intData[0] = 1;
                                                if (msgsnd(msqid, &messenger, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                                        perror("msgsnd to child 1 failed\n");
                                                        exit(1);
                                                }
                                        }else{
                                                //Traverse through and give the first empty frame to process
                                                for(int z = 0; z < 256; z++){
                                                        if(frameTable[z].processNum == -1){
                                                                printf("Granting blocked request, filling frame %d with P%d page %d\n", z, x, processTable[x].eventPage);
                                                                frameTable[z].processNum = x;
                                                                frameTable[z].page = processTable[x].eventPage;
                                                                if(process[x].eventRorW == 1){
                                                                        frameTable[z].dirtyBit = 1;
                                                                }else{
                                                                        frameTable[z].dirtyBit = 0;
                                                                }
                                                                frameTable[z].assigned = fifoCounter;
                                                                fifoCounter++;
                                                                processTable[x].eventSec = 0;
                                                                processTable[x].eventNano = 0;
                                                                processTable[x].isWaiting = 0;
                                                                processTable[x].eventPage = -1;
                                                                updateTime(sharedTime, 100000);
                                                                halfSecCounter += 100000;
                                                                break;
                                                        }
                                                }
                                                //add appropriate value to assigned(fifoCounter)
                                                //message back the waiting process
                                                printf("Indicating to P%d that request has been fufilled\n",x);
                                                messenger.mtype = processTable[x].pid;
                                                messenger.intData[0] = 1;
                                                if (msgsnd(msqid, &messenger, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                                        perror("msgsnd to child 1 failed\n");
                                                        exit(1);
                                                }
                                        }
                                }
                        }
                }

                //printf("CheckPoint 4\n");
                //Dont wait for message, but check
                if(msgrcv(msqid, &receiver, sizeof(msgbuffer),getpid(),IPC_NOWAIT) == -1){
                        if(errno == ENOMSG){
                                //printf("Got no message so maybe do nothing?\n");
                        }else{
                                printf("Got an error from msgrcv\n");
                                perror("msgrcv");
                                exit(1);
                        }
                }else{//if you get a message
                        //printf("Recived message from worker\n");
                        //printf("Message details: ID:%d Payload:%d %d \n", receiver.intData[2], receiver.intData[0], receiver.intData[1]);
                        //Find out which index is the process that sent the message
                        selected = -1;
                        for(int x = 0; x < totalLaunched; x++){
                                if(processTable[x].pid == receiver.intData[2]){
                                        selected = x;
                                }
                        }
                        if(selected == -1){
                                printf("Selected Index neg...\n");
                                return EXIT_FAILURE;
                        }

                        //get page index from given value
                        pageIndex = receiver.intData[1] / 1024;
                        if(processTable[selected].pageTable[pageIndex] != -1){
                                isPagePresent = 1;
                        }else{
                                isPagePresent = 0;
                        }
                        
                        //if a write
                        if(receiver.intData[0] == 1){
                                printf("P%d requesting write of address %d at time %d:%d\n",selected,receiver.intData[1],sharedTime[0],sharedTime[1]);
                                //make dirtybit 1
                                if(isPagePresent == 1){
                                        printf("Address %d in frame %d, writing at time %d:%d\n",receiver.intData[1],processTable[selected].pageTable[pageIndex],sharedTime[0],sharedTime[1]);
                                        frameTable[processTable[selected].pageTable[pageIndex]].dirtyBit = 1;
                                        //add some time
                                        updateTime(sharedTime, 10000);
                                        halfSecCounter += 10000;
                                        //send message back
                                        printf("Indicating to P%d that request has been fufilled\n",selected);
                                        messenger.mtype = processTable[selected].pid;
                                        messenger.intData[0] = 1;
                                        if (msgsnd(msqid, &messenger, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                                perror("msgsnd to child 1 failed\n");
                                                exit(1);
                                        }
                                }else if(isPagePresent == 0){
                                        //blocked wait event
                                        printf("Pagefault, adding wait event to P%d\n", selected);
                                        processTable[selected].isWaiting = 1;
                                        processTable[selected].eventPage = pageIndex;
                                        processTable[selected].eventRorW = 1;
                                        processTable[selected].eventNano = sharedTime[1] + 14000000;
                                        processTable[selected].eventSec = sharedTime[0];
                                        if(processTable[selected].eventNano >= 1000000000){
                                                processTable[selected].eventNano = processTable[selected].eventNano - 1000000000;
                                                processTable[selected].eventSec += 1;
                                        }
                                        updateTime(sharedTime, 10000);
                                        halfSecCounter += 10000;
                                        
                                }
                        //if a read
                        }else if(receiver.intData[0] == 0){
                                printf("P%d requesting read of address %d at time %d:%d\n",selected,receiver.intData[1],sharedTime[0],sharedTime[1]);
                                if(isPagePresent == 1){
                                        printf("Address %d in frame %d, reading at time %d:%d\n",receiver.intData[1],processTable[selected].pageTable[pageIndex],sharedTime[0],sharedTime[1]);
                                        //add some time
                                        updateTime(sharedTime, 10000);
                                        halfSecCounter += 10000;
                                        //send message back
                                        printf("Indicating to P%d that request has been fufilled\n",selected);
                                        messenger.mtype = processTable[selected].pid;
                                        messenger.intData[0] = 1;
                                        if (msgsnd(msqid, &messenger, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                                perror("msgsnd to child 1 failed\n");
                                                exit(1);
                                        }
                                }else if(isPagePresent == 0){
                                        //blocked wait event
                                        printf("Pagefault, adding wait event to P%d\n", selected);
                                        processTable[selected].isWaiting = 1;
                                        processTable[selected].eventPage = pageIndex;
                                        processTable[selected].eventRorW = 0;
                                        processTable[selected].eventNano = sharedTime[1] + 14000000;
                                        processTable[selected].eventSec = sharedTime[0];
                                        if(processTable[selected].eventNano >= 1000000000){
                                                processTable[selected].eventNano = processTable[selected].eventNano - 1000000000;
                                                processTable[selected].eventSec += 1;
                                        }
                                        updateTime(sharedTime, 10000);
                                        halfSecCounter += 10000;
                                }
                        }else{
                                printf("Something weird was given by process %d\n", selected);
                                return EXIT_FAILURE;
                        }
                }

                //every half second, output resource table and PCB, maybe the other matrix's too
                if (halfSecCounter >= 500000000 && verbose != 1){
                        displayPCB(totalLaunched,processTable,file);
                        displayFrameTable(frameTable, file);
                        halfSecCounter = 0;
                }   
        }

        displayTable(proc, processTable, file);

        //display stats
      


        shmdt(sharedTime);
        shmctl(shmid,IPC_RMID,NULL);
        if (msgctl(msqid, IPC_RMID, NULL) == -1) {
                perror("msgctl to get rid of queue in parent failed");
                exit(1);
        }

        printf("Done\n");
        fprintf(file, "Done\n");
        fclose(file);

        return 0;
}
