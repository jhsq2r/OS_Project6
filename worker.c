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
        int randomNum;
        int percentChance;
        int numMemReferences = 0;

        while (1){
                if(numMemReferences % 1000 == 0){
                        if((rand() % 100) < 30){
                                break;
                        }
                }
                seed++;
                numMemReferences++;
                srand(seed*getpid());
                randomNum = rand() % 32000;
                percentChance = rand() % 100;
                //printf("PercentChance: %d\n", percentChance);
                if(percentChance < 40){
                        //write
                        receiver.intData[0] = 1;
                }else{
                        //read
                        receiver.intData[0] = 0;
                }
                receiver.mtype = getppid();
                receiver.intData[1] = randomNum;
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
        }

        //printf("Killing child %d\n", getpid());
        //printf("WORKER PID:%d PPID:%d SysClockS: %d SysclockNano: %d TermTimeS: %d TermTimeNano: %d TERMINATING\n",getpid(),getppid(),sharedTime[0],sharedTime[1],exitTime[0],exitTime[1]);
        shmdt(sharedTime);

        return 0;

}
