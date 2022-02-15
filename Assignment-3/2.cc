#include <pthread.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#include <cassert>
#include <chrono>
#include <iostream>
#include <string>
#include <vector>

using namespace std;
using namespace std::chrono;

int max_jobs;

#define QUEUE_SIZE 9
#define MAT_SIZE 1000
#define MAT_SIZE_HALF (MAT_SIZE / 2)
#define SLEEP_TIME 3000001
#define HEADER ("\033[1;31m" + to_string(getpid()) + "\033[0m")

class Job {
   public:
    int producer_num;
    int status;
    int mat_size;
    int mat_id;
    int mat[MAT_SIZE][MAT_SIZE];

    Job() {
        producer_num = 0;
        status = 0;
        mat_size = MAT_SIZE * MAT_SIZE;
        mat_id = 0;
    }

    Job(int _producer_num) {
        producer_num = _producer_num;
        status = 0;
        mat_size = MAT_SIZE * MAT_SIZE;
        mat_id = rand() % 100000 + 1;
        for (int i = 0; i < MAT_SIZE; i++) {
            for (int j = 0; j < MAT_SIZE; j++) {
                mat[i][j] = rand() % 19 - 9;
            }
        }
    }

    Job(const Job &job) {
        producer_num = job.producer_num;
        status = job.status;
        mat_size = job.mat_size;
        mat_id = job.mat_id;
        for (int i = 0; i < MAT_SIZE; i++) {
            for (int j = 0; j < MAT_SIZE; j++) {
                mat[i][j] = job.mat[i][j];
            }
        }
    }
};

ostream &operator<<(ostream &os, const Job &job) {
    os << "Producer/Worker Number: " << job.producer_num << endl;
    os << "Pid: " << getpid() << endl;
    os << "Matrix Size: " << job.mat_size << endl;
    os << "Matrix ID: " << job.mat_id << endl;
    return os;
}

struct SharedQueue {
    int num_jobs;               // queue-size
    int front;                  // front of queue
    int rear;                   // rear of queue
    Job job_queue[QUEUE_SIZE];  // queue
    int workidx;                // index of worker

    void Init() {
        num_jobs = 0;
        front = 0;
        rear = 0;
        workidx = -1;
    }
};

struct SharedMem {
    SharedQueue queue;
    pthread_mutex_t mutex;
    pthread_mutexattr_t attr;

    int job_created;

    void Init() {
        queue.Init();
        job_created = 0;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
        pthread_mutexattr_setpshared(&attr, PTHREAD_PROCESS_SHARED);
        pthread_mutex_init(&mutex, &attr);
    }
};

pair<int, int> get_mat_seg(int &status) {
    for (int i = 0; i < 8; i++) {
        if (!(status & (1 << i))) {
            status |= (1 << i);
            return make_pair(i / 2, i % 4);
        }
    }
    return {-1, -1};
}

bool is_full(SharedQueue *queue) {
    return queue->num_jobs >= (QUEUE_SIZE - 1);
}

bool insert_job(SharedQueue *queue, Job &job) {
    if (is_full(queue)) {
        return false;
    }
    queue->num_jobs++;
    queue->job_queue[queue->rear] = job;
    queue->rear = (queue->rear + 1) % QUEUE_SIZE;
    return true;
}

void remove_job(SharedQueue *queue) {
    queue->front = (queue->front + 1) % QUEUE_SIZE;
    queue->num_jobs--;
}

void producer(SharedMem *mem, int producer_num) {
    srand(time(NULL) + getpid());
    SharedQueue *queue = &mem->queue;
    while (1) {
        if (pthread_mutex_lock(&mem->mutex) != 0) {
            cout << "pthread_mutex_lock error" << endl;
            exit(1);
        }
        if (mem->job_created == max_jobs) {
            pthread_mutex_unlock(&mem->mutex);
            break;
        }
        pthread_mutex_unlock(&mem->mutex);

        Job job(producer_num);
        int sleep_time = rand() % SLEEP_TIME;
        usleep(sleep_time);

        if (pthread_mutex_lock(&mem->mutex) != 0) {
            cout << "pthread_mutex_lock error" << endl;
            exit(1);
        }

        while (is_full(queue)) {
            pthread_mutex_unlock(&mem->mutex);
            usleep(10);
            if (pthread_mutex_lock(&mem->mutex) != 0) {
                cout << "pthread_mutex_lock error" << endl;
                exit(1);
            }
        }
        if (mem->job_created != max_jobs) {
            mem->job_created++;
            assert(insert_job(queue, job) == true);
            cout << HEADER << "\nNEW JOB GENERATED" << endl;
            cout << "Job Inserted by Producer" << endl;
            cout << job << endl;
            pthread_mutex_unlock(&mem->mutex);
        } else {
            pthread_mutex_unlock(&mem->mutex);
            break;
        }
    }
}

void worker(SharedMem *mem, int worker_num) {
    srand(time(NULL) + getpid());
    SharedQueue &queue = mem->queue;

    while (1) {
        int sleep_time = rand() % SLEEP_TIME;
        usleep(sleep_time);
        if (pthread_mutex_lock(&mem->mutex) != 0) {
            cout << "pthread_mutex_lock error" << endl;
            perror("pthread_mutex_unlock");

            exit(1);
        }

        if (mem->job_created == max_jobs && mem->queue.num_jobs == 1) {
            if (pthread_mutex_unlock(&mem->mutex) != 0) {
                cout << __LINE__ << " pthread_mutex_unlock error" << endl;

                perror("pthread_mutex_unlock");

                exit(1);
            }
            break;
        }

        while (mem->queue.num_jobs <= 1 && !(mem->job_created == max_jobs && mem->queue.num_jobs == 1)) {
            if (pthread_mutex_unlock(&mem->mutex) != 0) {
                cout << __LINE__ << " pthread_mutex_unlock error" << endl;
                exit(1);
            }
            usleep(10);
            if (pthread_mutex_lock(&mem->mutex) != 0) {
                cout << "pthread_mutex_lock error" << endl;
                perror("pthread_mutex_unlock");

                exit(1);
            }
        }

        if (mem->job_created == max_jobs && mem->queue.num_jobs == 1) {
            if (pthread_mutex_unlock(&mem->mutex) != 0) {
                cout << __LINE__ << " pthread_mutex_unlock error" << endl;
                perror("pthread_mutex_unlock");

                exit(1);
            }
            break;
        }

        pair<int, int> segs = get_mat_seg(mem->queue.job_queue[mem->queue.front].status);
        if (segs.first != -1) {
            if (segs.first == 0 && segs.second == 0) {  // first time

                if (mem->queue.job_queue[mem->queue.front].status != 1) {
                    exit(1);
                }
                mem->queue.workidx = mem->queue.rear;
                mem->queue.rear = (mem->queue.rear + 1) % QUEUE_SIZE;
                mem->queue.num_jobs++;
                mem->queue.job_queue[mem->queue.workidx].status = 0;
                mem->queue.job_queue[mem->queue.workidx].producer_num = worker_num;
                if (mem->queue.job_queue[mem->queue.workidx].mat_id == 0) {
                    mem->queue.job_queue[mem->queue.workidx].mat_id = -(rand() % 1000000 + 1);
                }
                mem->queue.job_queue[mem->queue.workidx].mat_size = MAT_SIZE * MAT_SIZE;

                for (int i = 0; i < MAT_SIZE; i++) {
                    for (int j = 0; j < MAT_SIZE; j++) {
                        mem->queue.job_queue[mem->queue.workidx].mat[i][j] = 0;
                    }
                }
                cout << HEADER << "\nNEW JOB GENERATED" << endl;
                cout << "Job Inserted by Worker" << endl;
                cout << mem->queue.job_queue[mem->queue.workidx] << endl;
            }

            int front = mem->queue.front;
            int front1 = (mem->queue.front + 1) % QUEUE_SIZE;

            cout << HEADER << "\nBlocks Fetched by Worker ID: " << worker_num << endl;
            cout << "First Matrix Producer Number: " << mem->queue.job_queue[front].producer_num << endl;
            cout << "First Matrix ID: " << mem->queue.job_queue[front].mat_id << endl;
            cout << "First Matrix Retrieved Block Number: " << segs.first + 1 << endl;
            cout << "Second Matrix Producer Number: " << mem->queue.job_queue[front].producer_num << endl;
            cout << "Second Matrix ID: " << mem->queue.job_queue[front1].mat_id << endl;
            cout << "Second Matrix Retrieved Block Number: " << segs.second + 1 << endl;
            cout << "Blocks Read from Shared Memory" << endl;
            cout << endl;

            if (pthread_mutex_unlock(&mem->mutex) != 0) {
                cout << __LINE__ << " pthread_mutex_unlock error" << endl;
                perror("pthread_mutex_unlock");
                exit(1);
            }

            int mat[MAT_SIZE_HALF][MAT_SIZE_HALF];
            for (int i = 0; i < MAT_SIZE_HALF; i++) {
                for (int j = 0; j < MAT_SIZE_HALF; j++) {
                    mat[i][j] = 0;
                    for (int k = 0; k < MAT_SIZE_HALF; k++) {
                        int a = i + (segs.first / 2) * MAT_SIZE_HALF;
                        int b = k + (segs.first % 2) * MAT_SIZE_HALF;

                        int c = k + (segs.second / 2) * MAT_SIZE_HALF;
                        int d = j + (segs.second % 2) * MAT_SIZE_HALF;

                        mat[i][j] += mem->queue.job_queue[front].mat[a][b] *
                                     mem->queue.job_queue[front1].mat[c][d];
                    }
                }
            }

            if (pthread_mutex_lock(&mem->mutex) != 0) {
                cout << "pthread_mutex_lock error" << endl;
                perror("pthread_mutex_unlock");

                exit(1);
            }
            for (int i = 0; i < MAT_SIZE_HALF; i++) {
                for (int j = 0; j < MAT_SIZE_HALF; j++) {
                    int a = i + (segs.first / 2) * MAT_SIZE_HALF;
                    int d = j + (segs.second % 2) * MAT_SIZE_HALF;
                    assert(a < MAT_SIZE && d < MAT_SIZE);
                    mem->queue.job_queue[mem->queue
                                             .workidx]
                        .mat[a][d] += mat[i][j];
                }
            }

            int p = (segs.first & 2) + (segs.second % 2);

            cout << HEADER << "\nBlocks Fetched by Worker ID: " << worker_num << endl;
            cout << "First Matrix Producer Number: " << mem->queue.job_queue[front].producer_num << endl;
            cout << "First Matrix ID: " << mem->queue.job_queue[front].mat_id << endl;
            cout << "First Matrix Retrieved Block Number: " << segs.first + 1 << endl;
            cout << "Second Matrix Producer Number: " << mem->queue.job_queue[front].producer_num << endl;
            cout << "Second Matrix ID: " << mem->queue.job_queue[front1].mat_id << endl;
            cout << "Second Matrix Retrieved Block Number: " << segs.second + 1 << endl;
            cout << "Block " << (mem->queue.job_queue[front1].status & (1 << p) ? "Added" : "Copied") << " into Block Number " << p + 1 << endl;
            cout << endl;

            mem->queue.job_queue[front1].status |= (1 << p);

            if ((++mem->queue.job_queue[mem->queue.workidx].status) == 8) {
                mem->queue.job_queue[front].status = 0;
                mem->queue.job_queue[front1].status = 0;

                remove_job(&mem->queue);
                remove_job(&mem->queue);

                mem->queue.job_queue[mem->queue.workidx].status = 0;
            }

            if (pthread_mutex_unlock(&mem->mutex) != 0) {
                cout << __LINE__ << " pthread_mutex_unlock error" << endl;
                perror("pthread_mutex_unlock");
                exit(1);
            }
        } else if (pthread_mutex_unlock(&mem->mutex) != 0) {
            cout << __LINE__ << " pthread_mutex_unlock error" << endl;
            perror("pthread_mutex_unlock");
            exit(1);
        }
    }
}

int shmid;
void sigint_handler(int signum) {
    shmctl(shmid, IPC_RMID, NULL);
    exit(1);
}

int main() {
    shmid = shmget(IPC_PRIVATE, sizeof(SharedMem), IPC_CREAT | 0666);
    SharedMem *mem = (SharedMem *)(shmat(shmid, (void *)0, 0));
    mem->Init();
    signal(SIGINT, sigint_handler);
    cout << "-----SET PARAMETERS-----" << endl;
    cout << "MATRIX DIMENTION: " << MAT_SIZE << " * " << MAT_SIZE << endl;
    cout << "MAXIMUM QUEUE SIZE: " << QUEUE_SIZE << endl;
    cout << "MAXIMUM WAITING TIME: " << (SLEEP_TIME - 1) << " microseconds" << endl;
    cout << endl;

    int num_workers, num_producers;
    cout << "Enter number of workers: ";
    cin >> num_workers;
    cout << "Enter number of producers: ";
    cin >> num_producers;
    cout << "Enter number of matrices: ";
    cin >> max_jobs;

    auto start = chrono::high_resolution_clock::now();
    vector<pid_t> prcs;
    for (int i = 1; i <= num_producers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            producer(mem, i);
            exit(0);
        } else
            prcs.push_back(pid);
    }

    cout << endl;

    for (int i = 1; i <= num_workers; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            perror("fork");
            exit(1);
        }
        if (pid == 0) {
            worker(mem, -i);
            exit(0);
        } else
            prcs.push_back(pid);
    }

    cout << "\n";

    int pid;
    int status;
    std::chrono::microseconds duration;

    while (1) {
        usleep(100);
        pthread_mutex_lock(&mem->mutex);
        if (mem->job_created == max_jobs && mem->queue.num_jobs == 1) {
            auto end = chrono::high_resolution_clock::now();
            duration = chrono::duration_cast<chrono::microseconds>(end - start);
            for (int i = 0; i < prcs.size(); i++) {
                pid = prcs[i];
                kill(pid, SIGINT);
            }
            pthread_mutex_unlock(&mem->mutex);
            break;
        }
        pthread_mutex_unlock(&mem->mutex);
    }

    cout << "\033[0;32m"
         << "MULTIPLICATION COMPLETED SUCCESSFULLY\033[0m\n"
         << endl;

    cout << "TIME TAKEN: " << duration.count() << " microseconds\n";

    long long trace = 0;
    for (int i = 0; i < MAT_SIZE; i++) {
        for (int j = 0; j < MAT_SIZE; j++) {
            trace += mem->queue.job_queue[mem->queue.front].mat[i][j];
        }
    }
    cout << "TRACE: " << trace << "\n\n";

    pthread_mutex_destroy(&mem->mutex);
    pthread_mutexattr_destroy(&mem->attr);
    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);
}
