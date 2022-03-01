#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <cstring>
#include <iostream>
#include <queue>
#include <string>
#include <thread>
#include <vector>
using namespace std;
using namespace chrono;

#define MAX_CHILD_JOBS 10
#define MAX_NODES 500
#define MAX_TREE_SIZE 450

#include <stdio.h>

// https://github.com/kuroidoruido/ColorLog/blob/master/colorlog.h
#define _COLOR_RED "1;31"
#define _COLOR_BLUE "1;34"
#define _COLOR_GREEN "0;32"

extern FILE* _logFp;
void initLogger(const char*);
void log_print(FILE*, const char*, ...);

#define __LOG_COLOR(FD, CLR, CTX, TXT, args...) log_print(FD, "\033[%sm[%s] \033[0m" TXT, CLR, CTX, ##args)
#define INFO(TXT, args...) __LOG_COLOR(stdout, _COLOR_GREEN, "info", TXT, ##args)
#define DEBUG(TXT, args...) __LOG_COLOR(stdout, _COLOR_BLUE, "debug", TXT, ##args)
#define ERROR(TXT, args...) __LOG_COLOR(stderr, _COLOR_RED, "error", TXT, ##args)

FILE* _logFp = NULL;
void initLogger(const char* logFile) {
    _logFp = logFile ? fopen(logFile, "w") : stdout;
}

#define PTHREAD_MUTEX_LOCK(mutex_p)                                              \
    do {                                                                         \
        int ret = pthread_mutex_lock(mutex_p);                                   \
        if (ret != 0) {                                                          \
            ERROR("%d: pthread_mutex_lock failed: %s", __LINE__, strerror(ret)); \
            exit(1);                                                             \
        }                                                                        \
    } while (0)

#define PTHREAD_MUTEX_UNLOCK(mutex_p)                                              \
    do {                                                                           \
        int ret = pthread_mutex_unlock(mutex_p);                                   \
        if (ret != 0) {                                                            \
            ERROR("%d: pthread_mutex_unlock failed: %d", __LINE__, strerror(ret)); \
            exit(1);                                                               \
        }                                                                          \
    } while (0)

void log_print(FILE* fp, const char* fmt, ...) {
    if (_logFp != NULL)
        fp = _logFp;
    va_list args;
    va_start(args, fmt);
    vfprintf(fp, fmt, args);
    fflush(fp);
    va_end(args);
}

enum JobStatus {
    WAITING,
    ONGOING,
    DONE
};

struct Node {
    int jobId;
    std::chrono::milliseconds time2comp;

    int parentIdx;
    int childJobs[MAX_CHILD_JOBS];
    int numChildActive;
    JobStatus status;

    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;

    Node() {
        jobId = rand() % ((int)1e8) + 1;
        time2comp = std::chrono::milliseconds(rand() % 250 + 1);
        for (int i = 0; i < MAX_CHILD_JOBS; i++) {
            childJobs[i] = -1;
        }
        status = WAITING;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mutex, &attr);
        numChildActive = 0;
        parentIdx = -1;
    }

    int addChild(int childJobIdx) {
        if (numChildActive < MAX_CHILD_JOBS) {
            for (int i = 0; i < MAX_CHILD_JOBS; i++) {
                if (childJobs[i] == -1) {
                    childJobs[i] = childJobIdx;
                    numChildActive++;
                    return 0;
                }
            }
        }
        return -1;
    }

    int removeChild(int childJobIdx) {
        for (int i = 0; i < MAX_CHILD_JOBS; i++) {
            if (childJobs[i] == childJobIdx) {
                childJobs[i] = -1;
                numChildActive--;
                return 0;
            }
        }
        return -1;
    }

    int isLeaf() {
        return numChildActive == 0;
    }

    int getNumChild() {
        return numChildActive;
    }
};

struct SharedMem {
    Node nodes[MAX_NODES];
    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;
    int _count;
    int rootIdx;

    void init() {
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mutex, &attr);
        _count = 0;
        for (int i = 0; i < MAX_NODES; i++) {
            nodes[i].status = DONE;
        }
        rootIdx = -1;
    }

    int addNode(Node& node) {
        PTHREAD_MUTEX_LOCK(&mutex);
        if (_count < MAX_NODES) {
            for (int i = 0; i < MAX_NODES; i++) {
                if (nodes[i].status == DONE) {
                    nodes[i] = node;
                    _count++;
                    PTHREAD_MUTEX_UNLOCK(&mutex);
                    if (rootIdx == -1)
                        rootIdx = i;
                    return i;
                }
            }
        }
        PTHREAD_MUTEX_UNLOCK(&mutex);
        return -1;
    }

    int removeNode(int idx) {
        nodes[idx].status = DONE;
        _count--;
        if (idx == rootIdx) {
            DEBUG("remove root node\n");
            return 0;
        }
        int parentIdx = nodes[idx].parentIdx;
        PTHREAD_MUTEX_LOCK(&nodes[parentIdx].mutex);
        DEBUG("remove node %d from parent %d\n", idx, parentIdx);
        nodes[parentIdx].removeChild(idx);
        PTHREAD_MUTEX_UNLOCK(&nodes[parentIdx].mutex);
        return 0;
    }
};

SharedMem* shm;

int getRandomJob(int idx) {
    PTHREAD_MUTEX_LOCK(&shm->nodes[idx].mutex);
    if (shm->nodes[idx].status != WAITING || shm->nodes[idx].getNumChild() == MAX_CHILD_JOBS) {
        PTHREAD_MUTEX_UNLOCK(&shm->nodes[idx].mutex);
        return -1;
    }
    // 1-e^(n-10)
    double prob = (double)(MAX_CHILD_JOBS - shm->nodes[idx].getNumChild()) / (MAX_CHILD_JOBS + 1);
    double sample = (double)rand() / RAND_MAX;
    DEBUG("prob: %f, sample: %f\n", prob, sample);
    if (sample < prob) {
        DEBUG("prob: %f, sample: %f\n", prob, sample);
        return idx;
    }

    PTHREAD_MUTEX_UNLOCK(&shm->nodes[idx].mutex);
    for (int i = 0; i < MAX_CHILD_JOBS; i++) {
        int childIdx = shm->nodes[idx].childJobs[i];
        if (childIdx != -1) {
            int ret = getRandomJob(childIdx);
            if (ret != -1) {
                return ret;
            }
        }
    }
    return -1;
}

void* handleProducer(void* id) {
    int idx = *((int*)id);
    std::chrono::seconds lifetime = std::chrono::seconds(rand() % (11) + 10);
    DEBUG("Producer %d of lifetime %d\n", idx, lifetime.count());
    auto start = high_resolution_clock::now();
    srand(time(NULL) * idx);
    while (1) {
        auto now = high_resolution_clock::now();
        if (duration_cast<seconds>(now - start).count() > lifetime.count())
            break;
        int jobIdx = getRandomJob(shm->rootIdx);
        if (jobIdx == -1)
            continue;
        DEBUG("Producer %d adding newjob to job %d\n", idx, jobIdx);
        Node node;
        node.parentIdx = jobIdx;
        int newJobIdx = shm->addNode(node);
        if (newJobIdx == -1) {
            INFO("Producer %d failed to add new job, tree full\n", idx);
            continue;
        }
        shm->nodes[jobIdx].addChild(newJobIdx);
        DEBUG("Producer %d added job %d\n", idx, newJobIdx);
        PTHREAD_MUTEX_UNLOCK(&shm->nodes[jobIdx].mutex);
        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % (250) + 1));
    }

    DEBUG("Producer %d done\n", idx);
    pthread_exit(NULL);
}

int getLeafNode(int idx) {
    PTHREAD_MUTEX_LOCK(&shm->nodes[idx].mutex);
    if (shm->nodes[idx].isLeaf() && shm->nodes[idx].status == WAITING) {
        return idx;
    }
    PTHREAD_MUTEX_UNLOCK(&shm->nodes[idx].mutex);
    for (int i = 0; i < MAX_CHILD_JOBS; i++) {
        int childIdx = shm->nodes[idx].childJobs[i];
        if (childIdx != -1) {
            int ret = getLeafNode(childIdx);
            if (ret != -1) {
                return ret;
            }
        }
    }
    return -1;
}

void* handleConsumerThread(void* id) {
    int idx = *((int*)id);
    DEBUG("Consumer %d\n", idx);
    while (1) {
        PTHREAD_MUTEX_LOCK(&shm->nodes[shm->rootIdx].mutex);
        if (shm->nodes[shm->rootIdx].status == DONE) {
            PTHREAD_MUTEX_UNLOCK(&shm->nodes[shm->rootIdx].mutex);
            break;
        }
        PTHREAD_MUTEX_UNLOCK(&shm->nodes[shm->rootIdx].mutex);
        int jobIdx = getLeafNode(shm->rootIdx);
        if (jobIdx == -1) {
            std::this_thread::sleep_for(std::chrono::milliseconds(rand() % (10) + 1));
            continue;
            // break;  // TODO: CHECK IF THIS IS OK !!!
        }
        DEBUG("Consumer %d got job %d\n", idx, jobIdx);
        shm->nodes[jobIdx].status = ONGOING;
        PTHREAD_MUTEX_UNLOCK(&shm->nodes[jobIdx].mutex);
        std::this_thread::sleep_for(shm->nodes[jobIdx].time2comp);
        shm->removeNode(jobIdx);
        DEBUG("Consumer %d removed job %d\n", idx, jobIdx);
    }
    DEBUG("Consumer %d done\n", idx);
    pthread_exit(NULL);
}

int shmid;
void sigint_handler(int signum) {
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
    exit(1);
}

void generateRandomTree() {
    Node rootnode;
    shm->addNode(rootnode);

    queue<int> q;
    q.push(shm->rootIdx);

    while (!q.empty() && shm->_count < MAX_TREE_SIZE) {
        int idx = q.front();
        q.pop();

        int numChild = rand() % (MAX_CHILD_JOBS) + 1;
        for (int i = 0; i < numChild; i++) {
            Node node;
            node.parentIdx = idx;
            int newJobIdx = shm->addNode(node);
            shm->nodes[idx].addChild(newJobIdx);
            q.push(newJobIdx);
        }
    }
}

int main() {
    shmid = shmget(IPC_PRIVATE, sizeof(SharedMem), IPC_CREAT | 0666);
    shm = (SharedMem*)shmat(shmid, NULL, 0);
    shm->init();
    srand(time(NULL));
    int producers, consumers;
    cin >> producers >> consumers;
    signal(SIGINT, sigint_handler);

    int pid = fork();
    if (pid == 0) {
        generateRandomTree();

        vector<pthread_t> producerThreads;
        for (int i = 0; i < producers; i++) {
            pthread_t p;
            DEBUG("Creating producer thread %d\n", i);
            int* id = new int(i);
            pthread_create(&p, NULL, &handleProducer, (void*)id);
            producerThreads.push_back(p);
        }

        int bpid = fork();
        if (bpid == 0) {
            vector<pthread_t> threads;
            for (int i = 0; i < consumers; i++) {
                pthread_t c;
                DEBUG("Creating consumer thread %d\n", i);
                int* id = new int(i);
                pthread_create(&c, NULL, &handleConsumerThread, (void*)id);
                threads.push_back(c);
            }
            for (int i = 0; i < consumers; i++) {
                pthread_join(threads[i], NULL);
            }
            pthread_exit(NULL);
        }
        for (int i = 0; i < producers; i++) {
            pthread_join(producerThreads[i], NULL);
        }
    }
    while (wait(NULL) > 0)
        ;
    shmdt(shm);
    shmctl(shmid, IPC_RMID, NULL);
}

/**
 * TODO LIST:
 * - Create random tree initially
 * - Add print statements
 */