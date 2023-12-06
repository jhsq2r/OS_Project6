#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/msg.h>
#include <errno.h>
#include <time.h>

#define SHMKEY 55555
#define PERMS 0644
#define MSGKEY 66666

typedef struct msgbuffer {
        long mtype;
        int intData[3];
} msgbuffer;

int main(int argc, char** argv){

        msgbuffer receiver;
        receiver.mtype = 1;
        int msqid = 0;

        if ((msqid = msgget(MSGKEY, PERMS)) == -1) {
                perror("msgget in child");
                exit(1);
        }

        int shmid = shmget(SHMKEY, sizeof(int)*2, 0777);
        if(shmid == -1){
                printf("Error in shmget\n");
                return EXIT_FAILURE;
        }
        int * sharedTime = (int*) (shmat (shmid, 0, 0));

        //printf("This is Child: %d, From Parent: %d, Seconds: %d, NanoSeconds: %d\n", getpid(), getppid(), sharedTime[0], sharedTime[1]);

        //printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d JUST STARTING\n",getpid(),getppid(),sharedTime[0],sharedTime[1],exitTime[0],exitTime[1]);

        srand(time(NULL));
        int seed = rand();
        int percentChance;
        int givenTime;
        int myResource[10];
        for (int x = 0; x < 10; x++){
                myResource[x] = 0;
        }
        int randomResource = 0;
        //initial time
        int initialSeconds = sharedTime[0];
        int initialNano = sharedTime[1];
        int searchAttempts = 0;
        int messageFlag = 0;
        int startNow = 0;

        while (1){
                seed++;

                //check if 250000000 nanoseconds have passed
                //printf("Shared Time from worker is %d %d \n", sharedTime[0],sharedTime[1]);
                //printf("Determining Varaibles value: %d %d \n", initialSeconds, initialNano);
                if (sharedTime[0] - initialSeconds >= 1 || sharedTime[1] - initialNano >= 250000000 || startNow == 0){
                        startNow = 1;
                        //printf("Worker action...\n");

                        initialSeconds = sharedTime[0];
                        initialNano = sharedTime[1];
                        searchAttempts = 0;
                        srand(seed*getpid());
                        percentChance = (rand() % 100) + 1;
                        //printf("PercentChance: %d\n", percentChance);
                        if (percentChance <= 1){//terminate
                                break;
                        }else if (percentChance > 1 && percentChance < 90){//request resource
                                //Random number between 0 and 9
                                randomResource = (rand() % 10);
                                //Only request if we don't have 20 of that resource
                                if (myResource[randomResource] == 20){
                                        searchAttempts = 1;
                                        randomResource++;
                                        while (searchAttempts < 10){
                                                if (randomResource == 10){
                                                        randomResource = 0;
                                                }
                                                if (myResource[randomResource] < 20){
                                                        break;
                                                }else{
                                                        searchAttempts++;
                                                        randomResource++;
                                                }
                                        }
                                }
                                if(searchAttempts == 10){
                                        messageFlag = 0;
                                }else{
                                        messageFlag = 1;
                                        receiver.intData[0] = 1;
                                        receiver.intData[1] = randomResource;
                                }
                        }else if (percentChance >= 90 && percentChance <= 100){//release resource
                                //Random number between 0 and 9, if myResource[rand] == 0 then increment until a resource is found
                                randomResource = (rand() % 10);
                                //if increment goes all the way back to the random number, make it known that nothing was released
                                if (myResource[randomResource] == 0){
                                        searchAttempts = 1;
                                        randomResource++;
                                        while (searchAttempts < 10){
                                                if (randomResource == 10){
                                                        randomResource = 0;
                                                }
                                                if (myResource[randomResource] > 0){
                                                        break;
                                                }else{
                                                        searchAttempts++;
                                                        randomResource++;
                                                }
                                        }
                                }
                                if(searchAttempts == 10){
                                        messageFlag = 0;
                                }else{
                                        messageFlag = -1;
                                        receiver.intData[0] = -1;
                                        receiver.intData[1] = randomResource;
                                }
                        }else{
                                printf("Something went wrong: Random number %d\n", percentChance);
                                messageFlag = 0;
                                break;
                        }

                        //send message
                        if (messageFlag != 0){
                                receiver.mtype = getppid();
                                receiver.intData[2] = getpid();
                                if (msgsnd(msqid, &receiver, sizeof(msgbuffer)-sizeof(long),0) == -1){
                                        perror("msgsnd to parent failed\n");
                                        exit(1);
                                }

                                //wait for message back
                                if ( msgrcv(msqid, &receiver, sizeof(msgbuffer), getpid(), 0) == -1) {
                                        perror("failed to receive message from parent\n");
                                        exit(1);
                                }
                                //printf("WORKER PID:%d Message received\n",getpid());

                                if(receiver.intData[0] == 10){
                                        break;
                                }
                                //update myResource for the released resources
                                if (messageFlag == -1){
                                        myResource[randomResource]--;
                                }

                                //update myResource for the gained resources
                                if (messageFlag == 1){
                                        myResource[randomResource]++;
                                }
                                messageFlag = 0;
                        }
                }

        }

        //printf("Killing child %d\n", getpid());
        //printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d TERMINATING\n",getpid(),getppid(),sharedTime[0],sharedTime[1],exitTime[0],exitTime[1]);
        shmdt(sharedTime);

        return 0;

}
