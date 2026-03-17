#include <iostream>
#include <fstream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/msg.h>
#include <signal.h>

using namespace std;

#define PERMS 0644

const int BUFF_SZ = sizeof(int) * 2;
int shm_key;
int shm_id;
int *customClock = nullptr;
const int BILLION = 1000000000;
const int MAXIMUM_PROCESS = 20;
int msgqid;

// PCB Structure

struct PCB
{
    int occupied;          // either true or false
    pid_t pid;             // process id of this child
    int startSeconds;      // time when it was forked
    int startNano;         // time when it was forked
    int endingTimeSeconds; // estimated time it should end
    int endingTimeNano;    // estimated time it should end
    int messageSent;       // count of message sent to the child process
};
struct PCB processTable[MAXIMUM_PROCESS];

struct msgbuffer
{
    long mtype;
    int intData;
};

ofstream logfile;

/////////incrementing clock////////
void incrementClock(int running)
{
    if (running == 0)
    {
        return;
    }
    int inc = 250000000 / running;
    customClock[1] += inc;

    while (customClock[1] >= BILLION)
    {
        customClock[0]++;
        customClock[1] -= BILLION;
    }
}

void normalizeTime(int &sec, int &nano)
{
    if (nano >= BILLION)
    {
        sec++;
        nano -= BILLION;
    }
}

bool timeReached(int targetSec, int targetNano)
{
    if (customClock[0] > targetSec)
        return true;
    if (customClock[0] == targetSec && customClock[1] >= targetNano)
        return true;
    return false;
}
// getting random time between 1 and t
void getRandomTime(double t, int &workersec, int &workernano)
{
    int maxSec = (int)t;
    int maxNano = (t - maxSec) * BILLION;

    if (maxSec == 0)
    {
        workersec = 0;
        workernano = (rand() % maxNano) + 1; // avoid 0
        return;
    }

    // Random seconds between 1 and maxSec
    workersec = (rand() % maxSec) + 1;

    if (workersec == maxSec)
    {
        if (maxNano > 0)
        {
            workernano = rand() % (maxNano + 1);
        }
        else
        {
            workernano = 0;
        }
    }
    else
    {
        workernano = rand() % BILLION;
    }
}
/////Process Table////////
void logmsg(string msg)
{
    cout << msg;
    logfile << msg;
}
void initProcessTable()
{
    for (int i = 0; i < MAXIMUM_PROCESS; i++)
        processTable[i].occupied = 0;
}
void printProcessTable()
{

    string output;

    output += "\nOSS PID:" + to_string(getpid()) +
              " SysClockS:" + to_string(customClock[0]) +
              " SysClockNano:" + to_string(customClock[1]) + "\n";

    output += "Entry Occupied PID StartS StartN EndS EndN MessageSent\n";

    for (int i = 0; i < MAXIMUM_PROCESS; i++)
    {
        if (processTable[i].occupied)
        {
            output += to_string(i) + " " +
                      to_string(processTable[i].occupied) + " " +
                      to_string(processTable[i].pid) + " " +
                      to_string(processTable[i].startSeconds) + " " +
                      to_string(processTable[i].startNano) + " " +
                      to_string(processTable[i].endingTimeSeconds) + " " +
                      to_string(processTable[i].endingTimeNano) + " " +
                      to_string(processTable[i].messageSent) + "\n";
        }
        else
        {
            output += to_string(i) + " 0\n";
        }
    }

    logmsg(output);
}

void clearPCB(pid_t pid)
{
    for (int i = 0; i < MAXIMUM_PROCESS; i++)
    {
        if (processTable[i].occupied &&
            processTable[i].pid == pid)
        {
            processTable[i].occupied = 0;
            break;
        }
    }
}

////////CleanUp
void cleanup()
{
    for (int i = 0; i < MAXIMUM_PROCESS; i++)
    {
        if (processTable[i].occupied)
            kill(processTable[i].pid, SIGTERM);
    }

    if (customClock)
        shmdt(customClock);

    shmctl(shm_id, IPC_RMID, NULL);
    msgctl(msgqid, IPC_RMID, NULL);
    logfile.close();
}

// output of oss printing in both logfile and screen

void signalHandler(int sig)
{
    logmsg("\nOSS: Caught signal. Cleaning up...\n");
    cleanup();
    exit(1);
}
void printHelp()
{
    cout << "Usage: oss [-h] [-n proc] [-s simul] [-t iter]\n"
         << "-n proc  total number of user processes (required, 1-100)\n"
         << "-s simul max simultaneous processes (default 2, 1-15)\n"
         << "-t iter  iterations per user(default 3, 0-100)\n";
}

int main(int argc, char *argv[])
{
    int n = -10;    // Since it is required I gave a dummy number, for requirement condition I am going to use ahead
    int s = 2;      // Default value is 2
    double t = 3.0; // Default value is 3seconds 0nanosecond
    double interval = 0.2;
    string logname = "logfile.txt";

    ///////////parsing options and getting command line arguments
    int option;
    while ((option = getopt(argc, argv, "hn:s:t:i:f:")) != -1)
    {
        switch (option)
        {
        case 'h':
            printHelp();
            return 0;
        case 'n':
            n = atoi(optarg); // optarg is the global variable set by getopt when a parameter with a value has been provided
            break;
        case 's':
            s = atoi(optarg);
            break;
        case 't':
            t = atof(optarg);
            break;
        case 'i':
            interval = atof(optarg);
            break;
        case 'f':
            logname = optarg;
            break;
        default:
            printHelp();
            return 1;
        }
    }

    if (n == -10)
    {
        cerr << "Error: -n (number of processes) is required. \n";
        return 1;
    }

    if (n < 1 || n > 100)
    {
        cerr << "Error: -n must be between 1 and 100.\n";
        return 1;
    }

    if (s < 1 || s > 15)
    {
        cerr << "Error: -s must be between 1 and 15.\n";
        return 1;
    }

    if (t < 0.0 || t > 60.0)
    {
        cerr << "Error: -t must be between 0.0 and 60.0.\n";
        return 1;
    }

    if (s > n)
    {
        s = n; // just in case simultaneous is greater than number of process
    }
    logfile.open(logname);
    // cout << "OSS starting, PID: " << getpid() << "\n";
    // printf("-n %d -s %d -t %f -i %f \n", n, s, t, interval);

    ////////////// Shared Memory/////////////////////
    shm_key = ftok("oss.cpp", 0);
    if (shm_key <= 0)
    {
        fprintf(stderr, "Parent:... Error in ftok\n");
        exit(1);
    }

    shm_id = shmget(shm_key, BUFF_SZ, 0700 | IPC_CREAT);
    if (shm_id <= 0)
    {
        fprintf(stderr, "Parent:... Error in shmget\n");
        exit(1);
    }

    customClock = (int *)shmat(shm_id, 0, 0);
    if (customClock == (int *)-1)
    {
        fprintf(stderr, "Parent:... Error in shmat\n");
        exit(1);
    }
    int *sec = &(customClock[0]);
    int *nano = &(customClock[1]);
    *sec = *nano = 0;

    system("touch msgq.txt");
    key_t msgkey = ftok("msgq.txt", 1);

    msgqid = msgget(msgkey, PERMS | IPC_CREAT);

    signal(SIGINT, signalHandler);
    signal(SIGALRM, signalHandler);
    alarm(60);

    initProcessTable();
    srand(time(0));
    int launched = 0;
    int active = 0;
    int currentIndex = 0;

    int intervalSec = (int)interval;
    int intervalNano = (interval - intervalSec) * BILLION;

    int lastLaunchSec = 0;
    int lastLaunchNano = 0;

    int lastPrintSec = 0;
    int lastPrintNano = 0;
    int workerSec = 0;
    int workerNano = 0;
    ////////main while logic
    while (launched < n || active > 0)
    {

        // printing every 0.5 sec
        int printSec = lastPrintSec;
        int printNano = lastPrintNano + 500000000;
        normalizeTime(printSec, printNano);

        if (timeReached(printSec, printNano))
        {
            printProcessTable();
            lastPrintSec = *sec;
            lastPrintNano = *nano;
        }

        // checking if we can launch
        int nextLaunchSec = lastLaunchSec + intervalSec;
        int nextLaunchNano = lastLaunchNano + intervalNano;
        normalizeTime(nextLaunchSec, nextLaunchNano);

        if (launched < n &&
            active < s &&
            (launched == 0 || timeReached(nextLaunchSec, nextLaunchNano)))
        {
            for (int i = 0; i < MAXIMUM_PROCESS; i++)
            {
                if (!processTable[i].occupied)
                {

                    pid_t worker = fork();
                    getRandomTime(t, workerSec, workerNano);
                    if (worker == 0)
                    {
                        char secStr[20];
                        char nanoStr[20];

                        snprintf(secStr, sizeof(secStr), "%d", workerSec);
                        snprintf(nanoStr, sizeof(nanoStr), "%d", workerNano);
                        execl("./worker", "./worker",
                              secStr, nanoStr, (char *)NULL);
                        exit(1);
                    }
                    processTable[i].occupied = 1;
                    processTable[i].pid = worker;
                    processTable[i].startSeconds = *sec;
                    processTable[i].startNano = *nano;
                    processTable[i].endingTimeSeconds = *sec + workerSec;
                    processTable[i].endingTimeNano = *nano + workerNano;
                    processTable[i].messageSent = 0;
                    normalizeTime(processTable[i].endingTimeSeconds, processTable[i].endingTimeNano);

                    active++;
                    launched++;
                    lastLaunchSec = *sec;
                    lastLaunchNano = *nano;
                    logmsg("OSS: Launched worker PID " + to_string(worker) + "\n");
                    break;
                }
            }
        }

        if (active > 0)
        {
            int checked = 0;
            while (!processTable[currentIndex].occupied && checked < MAXIMUM_PROCESS)
            {
                currentIndex = (currentIndex + 1) % MAXIMUM_PROCESS;
                checked++;
            }
            if (processTable[currentIndex].occupied)
            {

                msgbuffer msg;

                msg.mtype = processTable[currentIndex].pid;
                msg.intData = 1;

                if (msgsnd(msgqid, &msg, sizeof(msgbuffer) - sizeof(long), 0) == -1)
                {
                    perror("msgsnd to parent failed\n");
                    exit(1);
                }

                logmsg("OSS: Sending message to PID " +
                       to_string(processTable[currentIndex].pid) + " at time " +
                       to_string(*sec) + ":" + to_string(*nano) + "\n");

                msgrcv(msgqid, &msg, sizeof(msgbuffer) - sizeof(long), getpid(), 0);

                logmsg("OSS: Receiving message from PID " +
                       to_string(processTable[currentIndex].pid) + " at time " +
                       to_string(*sec) + ":" + to_string(*nano) + "\n");

                processTable[currentIndex].messageSent++;

                if (msg.intData == 0)
                {
                    logmsg("OSS: Worker " + to_string(processTable[currentIndex].pid) +
                           " is planning to terminate\n");

                    waitpid(processTable[currentIndex].pid, NULL, 0);

                    processTable[currentIndex].occupied = 0;

                    active--;
                }

                currentIndex = (currentIndex + 1) % MAXIMUM_PROCESS;
            }
        }

        incrementClock(active);
    }
    logmsg("\nOSS finished\n");

    cleanup();

    return 0;
}