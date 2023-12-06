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
        int lastRequest;
};

void displayTable(int i, struct PCB *processTable, FILE *file){
        fprintf(file,"Process Table:\nEntry Occupied PID        StartS StartN Waiting\n");
        printf("Process Table:\nEntry Occupied PID      StartS StartN Waiting\n");
        for (int x = 0; x < i; x++){
                fprintf(file,"%d        %d      %d      %d      %d      %d\n",x,processTable[x].occupied,processTable[x].pid,processTable[x].startSeconds,processTable[x].startNano,processTable[x].isWaiting);
                printf("%d      %d      %d      %d      %d      %d\n", x,processTable[x].occupied,processTable[x].pid,processTable[x].startSeconds,processTable[x].startNano,processTable[x].isWaiting);

        }
}

void displayMatrix(int total, int matrix[20][10], FILE *file){
        for (int i = 0; i < total; i++){
                for (int j = 0; j < 10; j++){
                        printf("%2d ", matrix[i][j]); // Adjust the width as needed
                        fprintf(file,"%2d ", matrix[i][j]);
                }
                fprintf(file,"\n");
                printf("\n");
        }
}

void updateTime(int *sharedTime){
        sharedTime[1] = sharedTime[1] + 100000;
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

int deadlockDetection(int totalLaunched, int aloMatrix[20][10], int reqMatrix[20][10], int aloArray[10]){
        int i, j;
        int work[10];
        int finish[totalLaunched];
        int safe_sequence[totalLaunched];
        int num_finished = 0;

        for(i = 0; i < 10; i++){
                work[i] = aloArray[i];
        }

        for(i = 0; i < totalLaunched; i++){
                finish[i] = 0;
        }

        while(num_finished < totalLaunched){
                int found = 0;
                for (i = 0; i < totalLaunched; i++){
                        if(!finish[i]){
                                int can_finish = 1;
                                for(j = 0; j < 10; j++){
                                        if(reqMatrix[i][j] > work[j]){
                                                can_finish = 0;
                                                break;
                                        }
                                }
                                if(can_finish){
                                        for(j = 0; j < 10; j++){
                                                work[j] += aloMatrix[i][j];
                                        }
                                        finish[i] = 1;
                                        safe_sequence[num_finished] = i;
                                        num_finished++;
                                        found = 1;
                                }
                        }
                }
                if(!found){
                        return 1;
                }
        }
        return 0;
}

//may need custom data structures for resource management

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


        struct PCB processTable[20];
        for (int y = 0; y < 20; y++){
                processTable[y].occupied = 0;
                processTable[y].isWaiting = 0;
        }

        int totalInSystem = 0;
        int status;
        int totalLaunched = 0;
        int next = 0;
        int nextLaunchTime[2];
        nextLaunchTime[0] = 0;
        nextLaunchTime[1] = 0;
        int canLaunch;
        int requestTrack = 0;

        int requestMatrix[20][10];
        int allocationMatrix[20][10];
        for(int x = 0; x < 20; x++){
                for(int y =0; y < 10; y++){
                        requestMatrix[x][y] = 0;
                        allocationMatrix[x][y] = 0;
                }
        }
        int allocatedResourceArray[10];
        int availableResourceArray[10];
        for(int x = 0; x < 10; x++){
                allocatedResourceArray[x] = 0;
                availableResourceArray[x] = 0;
        }
        int canGrant;
        int selected = 0;
        int grantedNow = 0;
        int grantedLater = 0;
        int oneSecCounter = 0;
        int halfSecCounter = 0;
        int deadlock = 0;
        int kills = 0;
        int totalDetections = 0;

        while(1){
                seed++;
                srand(seed);
                //printf("Looping...\n");
                //increment time grab function from project4, function may need editing in terms of time increment
                updateTime(sharedTime);
                oneSecCounter += 100000;
                halfSecCounter += 100000;

                //check if process has terminated
                for (int x = 0; x < totalLaunched; x++){
                        if (processTable[x].occupied == 1){
                                if (waitpid(processTable[x].pid, &status, WNOHANG) > 0){
                                        processTable[x].occupied = 0;
                                        //free resources
                                        //Edit charts as needed
                                        for(int y = 0; y < 10; y++){
                                                allocatedResourceArray[y] -= allocationMatrix[x][y];
                                                allocationMatrix[x][y] = 0;
                                        }
                                        if(verbose != 1){
                                        printf("Master has detected Process P%d has terminated at time %d:%d\n", x,sharedTime[0],sharedTime[1]);
                                        fprintf(file, "Master has detected Process P%d has terminated at time %d:%d\n", x,sharedTime[0],sharedTime[1]);
                                        }
                                }
                        }
                }
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

                //Check if any request from the request matrix can be fulfilled
                //increment through the PCB and check if any waiting requests can be fulfilled
                //printf("CheckPoint 3\n");
                for(int x = 0; x < totalLaunched; x++){
                        if(processTable[x].isWaiting == 1){
                                canGrant = 1;
                                for(int y = 0; y < 10; y++){
                                        if(requestMatrix[x][y] + allocatedResourceArray[y] <= 20){
                                                canGrant = 1;
                                        }else{
                                                canGrant = 0;
                                                break;
                                        }
                                }
                                if(canGrant == 1){//If request can be fulfilled, update request matrix-allocation matrix-PCB-resource array-allocation array, then message back
                                        for(int y = 0; y < 10; y++){
                                                allocatedResourceArray[y] += requestMatrix[x][y];
                                                allocationMatrix[x][y] += requestMatrix[x][y];
                                                requestMatrix[x][y] = 0;
                                        }
                                        processTable[x].isWaiting = 0;
                                        grantedLater++;
                                        //Send message back to granted process
                                        messenger.mtype = processTable[x].pid;
                                        messenger.intData[0] = 1;
                                        if (msgsnd(msqid, &messenger, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                                perror("msgsnd to child 1 failed\n");
                                                exit(1);
                                        }
                                        if(verbose != 1){
                                        printf("Master has detected that Process P%d's request can now be granted at time %d:%d\n", x,sharedTime[0],sharedTime[1]);
                                        fprintf(file, "Master has detected that Process P%d's request can now be granted at time %d:%d\n", x,sharedTime[0],sharedTime[1]);
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

                        //if a request
                        if(receiver.intData[0] == 1){
                                //check if request can be fufilled
                                if(allocatedResourceArray[receiver.intData[1]] + 1 <= 20){
                                        //if yes update resources accordingly and stat keeping variable and send message back
                                        allocatedResourceArray[receiver.intData[1]]++;
                                        allocationMatrix[selected][receiver.intData[1]]++;
                                        grantedNow++;
                                        messenger.mtype = processTable[selected].pid;
                                        messenger.intData[0] = 1;
                                        if (msgsnd(msqid, &messenger, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                                perror("msgsnd to child 1 failed\n");
                                                exit(1);
                                        }
                                        if(verbose != 1){
                                        printf("Master has detected Process P%d has requested R%d at time %d:%d and is granting the request\n", selected,receiver.intData[1],sharedTime[0],sharedTime[1]);
                                        fprintf(file,"Master has detected Process P%d has requested R%d at time %d:%d and is granting the request\n", selected,receiver.intData[1],sharedTime[0],sharedTime[1]);
                                        }
                                }else{
                                        //if no update request matrix
                                        requestMatrix[selected][receiver.intData[1]]++;
                                        if(requestMatrix[selected][receiver.intData[1]]==2){
                                                printf("Error...\n");
                                                return EXIT_FAILURE;
                                        }
                                        processTable[selected].isWaiting = 1;
                                        if(verbose != 1){
                                        printf("Master has detected Process P%d has requested R%d at time %d:%d and is not granting the request\n", selected,receiver.intData[1],sharedTime[0],sharedTime[1]);
                                        fprintf(file,"Master has detected Process P%d has requested R%d at time %d:%d and is not granting the request\n", selected,receiver.intData[1],sharedTime[0],sharedTime[1]);
                                        }
                                }
                        }
                        //if a release
                        if(receiver.intData[0] == -1){
                                //release resources accordingly and update stat keeping variable and message back
                                allocatedResourceArray[receiver.intData[1]]--;
                                allocationMatrix[selected][receiver.intData[1]]--;
                                messenger.mtype = processTable[selected].pid;
                                messenger.intData[0] = 1;
                                if (msgsnd(msqid, &messenger, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                        perror("msgsnd to child 1 failed\n");
                                        exit(1);
                                }
                                if(verbose != 1){
                                printf("Master has detected Process P%d has released 1 of R%d at time %d:%d\n", selected,receiver.intData[1],sharedTime[0],sharedTime[1]);
                                fprintf(file,"Master has detected Process P%d has released 1 of R%d at time %d:%d\n", selected,receiver.intData[1],sharedTime[0],sharedTime[1]);
                                }
                        }
                }

                //every half second, output resource table and PCB, maybe the other matrix's too
                if (halfSecCounter >= 500000000 && verbose != 1){
                        displayTable(totalLaunched, processTable, file);
                        //display matrices
                        printf("Total Allocation Array:\n");
                        fprintf(file,"Total Allocation Array:\n");
                        for(int x = 0; x < 10; x++){
                                printf("%2d ", allocatedResourceArray[x]);
                                fprintf(file,"%2d ", allocatedResourceArray[x]);
                        }
                        printf("\nAllocation Matrix:\n");
                        fprintf(file,"\nAllocation Matrix:\n");
                        displayMatrix(totalLaunched, allocationMatrix, file);
                        printf("Request Matrix:\n");
                        fprintf(file,"Request Matrix:\n");
                        displayMatrix(totalLaunched, requestMatrix, file);
                        halfSecCounter = 0;
                }
                //every second, check for deadlock
                if (oneSecCounter >= 1000000000){
                        //check for deadlock
                        printf("Master checking for deadlock at time %d:%d\n", sharedTime[0],sharedTime[1]);
                        fprintf(file,"Master checking for deadlock at time %d:%d\n", sharedTime[0],sharedTime[1]);
                        deadlock = -1;
                        totalDetections++;
                        for(int x = 0; x < 10; x++){
                                availableResourceArray[x] = 20 - allocatedResourceArray[x];
                        }
                        deadlock = deadlockDetection(totalLaunched, allocationMatrix, requestMatrix, availableResourceArray);
                        if(deadlock == 1){
                                printf("Deadlock Detected...\n");
                                fprintf(file,"Deadlock Detected...\n");
                                if(verbose == 1){
                                        displayTable(totalLaunched, processTable, file);
                                        printf("Total Allocation Array:\n");
                                        fprintf(file,"Total Allocation Array:\n");
                                        for(int x = 0; x < 10; x++){
                                                printf("%2d ", allocatedResourceArray[x]);
                                                fprintf(file,"%2d ", allocatedResourceArray[x]);
                                        }
                                        printf("\nAllocation Matrix:\n");
                                        fprintf(file,"\nAllocation Matrix:\n");
                                        displayMatrix(totalLaunched, allocationMatrix, file);
                                        printf("Request Matrix:\n");
                                        fprintf(file,"Request Matrix:\n");
                                        displayMatrix(totalLaunched, requestMatrix, file);
                                }

                                //sleep(1);
                                for(int x = 0; x < totalLaunched; x++){
                                        if(processTable[x].occupied == 1){
                                                printf("Master terminating Process P%d and freeing its resources at time %d:%d\n", x,sharedTime[0],sharedTime[1]);
                                                fprintf(file,"Master terminating Process P%d and freeing its resources at time %d:%d\n", x,sharedTime[0],sharedTime[1]);
                                                processTable[x].occupied = 0;
                                                kills++;
                                                for(int y = 0; y < 10; y++){
                                                        allocatedResourceArray[y] -= allocationMatrix[x][y];
                                                        availableResourceArray[y] = 20 - allocatedResourceArray[y];
                                                        allocationMatrix[x][y] = 0;
                                                        requestMatrix[x][y]=0;
                                                }
                                                messenger.mtype = processTable[x].pid;
                                                messenger.intData[0] = 10;
                                                if (msgsnd(msqid, &messenger, sizeof(msgbuffer)-sizeof(long), 0) == -1) {
                                                        perror("msgsnd to child 1 failed\n");
                                                         exit(1);
                                                }
                                                if(deadlockDetection(totalLaunched, allocationMatrix, requestMatrix, availableResourceArray) == 0){
                                                        printf("Deadlock Solved!\n");
                                                        fprintf(file,"Deadlock Solved!\n");
                                                        if(verbose == 1){
                                                                displayTable(totalLaunched, processTable, file);
                                                                printf("Total Allocation Array:\n");
                                                                fprintf(file,"Total Allocation Array:\n");
                                                                for(int x = 0; x < 10; x++){
                                                                        printf("%2d ", allocatedResourceArray[x]);
                                                                        fprintf(file,"%2d ", allocatedResourceArray[x]);
                                                                }
                                                                printf("\nAllocation Matrix:\n");
                                                                fprintf(file,"\nAllocation Matrix:\n");
                                                                displayMatrix(totalLaunched, allocationMatrix, file);
                                                                printf("Request Matrix:\n");
                                                                fprintf(file,"Request Matrix:\n");
                                                                displayMatrix(totalLaunched, requestMatrix, file);
                                                        }
                                                        break;
                                                }
                                        }
                                }
                                //return EXIT_FAILURE;
                        }else if(deadlock == 0){
                                printf("No deadlock detected...\n");
                                fprintf(file,"No deadlock detected...\n");
                        }else{
                                printf("Something weird...\n");
                                return EXIT_FAILURE;
                        }
                        oneSecCounter = 0;
                }
        }

        displayTable(proc, processTable, file);

        //display stats
        printf("Total Requests: %d\n", grantedNow+grantedLater);
        printf("Granted right away: %d\n", grantedNow);
        printf("Granted after some wait time: %d\n", grantedLater);
        printf("Number of times deadlock detection was run: %d\n", totalDetections);
        printf("Processes killed by deadlock removal: %d\n", kills);
        printf("Processes that died naturally: %d\n", proc-kills);
        fprintf(file,"Total Requests: %d\n", grantedNow+grantedLater);
        fprintf(file,"Granted right away: %d\n", grantedNow);
        fprintf(file,"Granted after some wait time: %d\n", grantedLater);
        fprintf(file,"Number of times deadlock detection was run: %d\n", totalDetections);
        fprintf(file,"Processes killed by deadlock removal: %d\n", kills);
        fprintf(file,"Processes that died naturally: %d\n", proc-kills);


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
