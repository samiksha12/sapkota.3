#include <iostream>
#include <unistd.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <cstdlib>

using namespace std;

#define PERMS 0644

const int BILLION = 1000000000;
const int BUFF_SZ = sizeof(int) * 2;

struct msgbuffer
{
    long mtype;
    int intData;
};

int main(int argc, char *argv[])
{
    if (argc < 3)
    {
        cout << "Usage: ./worker sec nano\n";
        return 1;
    }
    int seconds = atoi(argv[1]);
    int nanoseconds = atoi(argv[2]);

    int shm_key = ftok("oss.cpp", 0);
    if (shm_key <= 0)
    {
        fprintf(stderr, "Child:... Error in ftok\n");
        exit(1);
    }

    int shm_id = shmget(shm_key, BUFF_SZ, 0700);
    if (shm_id <= 0)
    {
        fprintf(stderr, "child:... Error in shmget\n");
        exit(1);
    }

    int *clockptr = (int *)shmat(shm_id, 0, 0);
    if (clockptr == (int *)-1)
    {
        fprintf(stderr, "Child:... Error in shmat\n");
        exit(1);
    }

    int *sec = &(clockptr[0]);
    int *nano = &(clockptr[1]);

    int termTimeSec = *sec + seconds;
    int termTimeNano = *nano + nanoseconds;

    if (termTimeNano >= BILLION)
    {
        termTimeSec++;
        termTimeNano -= BILLION;
    }

    int msgqid = 0;
    key_t key;

    // get a key for our message queue
    if ((key = ftok("msgq.txt", 1)) == -1)
    {
        perror("ftok");
        exit(1);
    }

    // create our message queue
    if ((msgqid = msgget(key, PERMS)) == -1)
    {
        perror("msgget in child");
        exit(1);
    }
    printf("WORKER PID:%d PPID:%d\n", getpid(), getppid());
    printf("SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d\n", *sec, *nano, termTimeSec, termTimeNano);

    printf("--Just Starting\n");

    int count = 0;
    while (true)
    {
        msgbuffer msg;

        // receive a message, but only one for us
        if (msgrcv(msgqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0) == -1)
        {
            perror("failed to receive message from parent\n");
            exit(1);
        }

        count++;
        cout << "WORKER PID:" << getpid()
             << " SysClockS:" << *sec
             << " SysClockNano:" << *nano
             << " TermTimeS:" << termTimeSec
             << " TermTimeNano:" << termTimeNano << endl;

        cout << "--" << count << " messages received from oss\n";
        // check terminating, and send message to oss of 0
        if (*sec > termTimeSec || (*sec == termTimeSec && *nano >= termTimeNano))
        {
            msg.mtype = getppid();
            msg.intData = 0;
            if (msgsnd(msgqid, &msg, sizeof(msgbuffer) - sizeof(long), 0) == -1)
            {
                perror("msgsnd to parent failed\n");
                exit(1);
            }
            printf("WORKER PID:%d PPID:%d\n", getpid(), getppid());
            printf("SysClockS:%d SysClockNano:%d TermTimeS:%d TermTimeNano:%d\n", *sec, *nano, termTimeSec, termTimeNano);
            printf("--Terminating after sending message back to oss after %d received messages\n", count);
            break;
        }
        // if it is still running send a message of 1
        msg.mtype = getppid();
        msg.intData = 1;
        if (msgsnd(msgqid, &msg, sizeof(msgbuffer) - sizeof(long), 0) == -1)
        {
            perror("msgsnd to parent failed\n");
            exit(1);
        }
    }
    shmdt(clockptr);
    return 0;
}