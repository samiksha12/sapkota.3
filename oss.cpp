#include <iostream>
#include <unistd.h>
#include <sys/wait.h>
#include <cstdlib>
#include <cstring>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <signal.h>

using namespace std;

const int BUFF_SZ = sizeof(int) * 2;
int shm_key;
int shm_id;
int *customClock = nullptr;
const int BILLION = 1000000000;
const int MAXIMUM_PROCESS = 20;

// PCB Structure

struct PCB
{
    int occupied;          // either true or false
    pid_t pid;             // process id of this child
    int startSeconds;      // time when it was forked
    int startNano;         // time when it was forked
    int endingTimeSeconds; // estimated time it should end
    int endingTimeNano;    // estimated time it should end
};
struct PCB processTable[MAXIMUM_PROCESS];