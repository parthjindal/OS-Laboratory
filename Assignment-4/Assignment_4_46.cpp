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
#include <cmath>
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
#define MAX_TREE_SIZE 200
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
// TODO: should we have a random no. generator as a global ?

class Node {
   public:
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

    ~Node() {
        pthread_mutex_destroy(&mutex);
        pthread_mutexattr_destroy(&attr);
    }

    void init() {
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

    Node& operator=(const Node& node) {
        jobId = node.jobId;
        time2comp = node.time2comp;
        for (int i = 0; i < MAX_CHILD_JOBS; i++) {
            childJobs[i] = node.childJobs[i];
        }
        status = node.status;
        numChildActive = node.numChildActive;
        parentIdx = node.parentIdx;
        return *this;
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
            nodes[i].init();
            nodes[i].status = DONE;
        }
        rootIdx = -1;
    }

    int addNode(Node& node) {
        PTHREAD_MUTEX_LOCK(&mutex);
        if (_count < MAX_NODES) {
            for (int i = 0; i < MAX_NODES; i++) {
                // PTHREAD_MUTEX_LOCK(&nodes[i].mutex);  // FIXME: // producer locking a lock already locked by itself
                if (nodes[i].status == DONE) {
                    PTHREAD_MUTEX_LOCK(&nodes[i].mutex);
                    nodes[i] = node;
                    PTHREAD_MUTEX_UNLOCK(&nodes[i].mutex);
                    _count++;
                    if (rootIdx == -1)
                        rootIdx = i;
                    PTHREAD_MUTEX_UNLOCK(&mutex);
                    // PTHREAD_MUTEX_UNLOCK(&nodes[i].mutex);
                    return i;
                }
                // PTHREAD_MUTEX_UNLOCK(&nodes[i].mutex);
            }
        }
        PTHREAD_MUTEX_UNLOCK(&mutex);
        return -1;
    }

    int removeNode(int idx) {
        PTHREAD_MUTEX_LOCK(&mutex);  // TODO: CHECK if more locking is needed here
        nodes[idx].status = DONE;
        _count--;
        if (idx == rootIdx) {
            INFO("Removing ROOT node\n");
            PTHREAD_MUTEX_UNLOCK(&mutex);
            return 0;
        }
        PTHREAD_MUTEX_UNLOCK(&mutex);

        int parentIdx = nodes[idx].parentIdx;
        PTHREAD_MUTEX_LOCK(&nodes[parentIdx].mutex);
        // DEBUG("Removing node edge of: %d from parent: %d\n", idx, parentIdx);
        nodes[parentIdx].removeChild(idx);
        PTHREAD_MUTEX_UNLOCK(&nodes[parentIdx].mutex);

        return 0;
    }
};

SharedMem* shm;

int getRandomJob(int idx) {
    PTHREAD_MUTEX_LOCK(&shm->nodes[idx].mutex);
    if (shm->nodes[idx].status != WAITING) {
        // DEBUG("Node %d's status: %d, childjobs: %d\n", idx, shm->nodes[idx].status, shm->nodes[idx].getNumChild());
        PTHREAD_MUTEX_UNLOCK(&shm->nodes[idx].mutex);
        return -1;
    }
    double prob = (double)(1 - exp((double)(shm->nodes[idx].getNumChild() - 10)));
    prob = 0.3;
    double sample = (double)rand() / RAND_MAX;
    // DEBUG("prob: %f, sample: %f, id: %d\n", prob, sample, idx);

    if (sample < prob && shm->nodes[idx].getNumChild() < MAX_CHILD_JOBS) {
        // DEBUG("prob: %f, sample: %f, num childs: %d\n", prob, sample, shm->nodes[idx].getNumChild());
        return idx;
    }
    PTHREAD_MUTEX_UNLOCK(&shm->nodes[idx].mutex);
    for (int i = 0; i < MAX_CHILD_JOBS; i++) {
        int childIdx = shm->nodes[idx].childJobs[i];
        if (childIdx != -1) {
            // DEBUG("Calling getRandomJob for child %d\n", childIdx);
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
    INFO("Producer %d of lifetime %d, time: %ld\n", idx, lifetime.count(), time(NULL));
    auto start = high_resolution_clock::now();
    srand(time(NULL) * idx);

    while (1) {
        auto now = high_resolution_clock::now();
        if (duration_cast<seconds>(now - start).count() >= lifetime.count())
            break;
        int jobIdx = getRandomJob(shm->rootIdx);
        if (jobIdx == -1) {
            // DEBUG("Producer %d waiting for job\n", idx);
            std::this_thread::sleep_for(std::chrono::microseconds(10));
            continue;
        }
        Node node;
        node.parentIdx = jobIdx;
        int newJobIdx = shm->addNode(node); // FIXME: new node should be locked until a connection is established with parent node ???????
        if (newJobIdx == -1) {
            // ERROR("PRODUCER %d failed to add new job, tree full\n", idx);
            PTHREAD_MUTEX_UNLOCK(&shm->nodes[jobIdx].mutex);  // UNLOCK PARENT NODE
            continue;
        }
        shm->nodes[jobIdx].addChild(newJobIdx);
        DEBUG("PRODUCER %d -> Job: %d, Parent Job: %d\n", idx, newJobIdx, jobIdx);
        PTHREAD_MUTEX_UNLOCK(&shm->nodes[jobIdx].mutex);  // UNLOCK PARENT NODE

        std::this_thread::sleep_for(std::chrono::milliseconds(rand() % (250) + 1));
    }

    INFO("PRODUCER %d done at %ld\n", idx, time(NULL));
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
    srand(time(NULL) + idx);
    while (1) {
        PTHREAD_MUTEX_LOCK(&shm->nodes[shm->rootIdx].mutex);
        if (shm->nodes[shm->rootIdx].status == DONE) {
            PTHREAD_MUTEX_UNLOCK(&shm->nodes[shm->rootIdx].mutex);
            break;
        }
        PTHREAD_MUTEX_UNLOCK(&shm->nodes[shm->rootIdx].mutex);
        int jobIdx = getLeafNode(shm->rootIdx);
        if (jobIdx == -1) {
            std::this_thread::sleep_for(std::chrono::microseconds(rand() % (10) + 1));
            continue;
            // break;  // TODO: CHECK IF THIS IS OK !!!
        }
        shm->nodes[jobIdx].status = ONGOING;
        int parentIdx = shm->nodes[jobIdx].parentIdx;
        PTHREAD_MUTEX_UNLOCK(&shm->nodes[jobIdx].mutex);
        std::this_thread::sleep_for(shm->nodes[jobIdx].time2comp);
        shm->removeNode(jobIdx);
        DEBUG("CONSUMER %d -> Job: %d, Parent Job: %d\n", idx, jobIdx, parentIdx);
    }
    DEBUG("CONSUMER %d done\n", idx);
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

    int numNodes = rand() % (MAX_NODES - MAX_TREE_SIZE) + MAX_TREE_SIZE;
    DEBUG("Creating tree of %d nodes\n", numNodes);

    while (!q.empty() && shm->_count < numNodes) {
        int idx = q.front();
        q.pop();

        int numChild = rand() % (MAX_CHILD_JOBS) + 1;
        if (shm->_count + numChild > numNodes) {
            numChild = numNodes - shm->_count;
        }
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