#include <bits/stdc++.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

using namespace std;

#define QUEUE_SIZE 9
#define MAT_SIZE 1000
#define MAT_SIZE_HALF 500
#define SLEEP_TIME 3000001
#define MAX_JOBS 2
struct Job {
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

    Job(const Job& job) {
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

struct SharedQueue {
    int num_jobs;
    int front;
    int rear;
    int job_created;
    Job job_queue[QUEUE_SIZE];
    int workidx;
    SharedQueue() {
        num_jobs = 0;
        front = 0;
        rear = 0;
        job_created = 0;
        workidx = -1;
    }
};

pair<int, int> get_mat_seg(int& status) {
    for (int i = 0; i < 8; i++) {
        if (!(status & (1 << i))) {
            status |= (1 << i);
            return make_pair(i / 2, i % 4);
        }
    }
    return {-1, -1};
}

SharedQueue* create_queue(int shm_key) {
    cout << "create_queue" << endl;
    SharedQueue* queue = (SharedQueue*)shmat(shm_key, NULL, 0);
    queue->num_jobs = 0;
    queue->front = 0;
    queue->rear = 0;
    queue->workidx = -1;
    return queue;
}

void print_queue(SharedQueue* queue) {
    cout << "Queue: ";
    cout << "Size of queue: " << queue->num_jobs << endl;
    cout << "Jobs Created: " << queue->job_created << endl;
    for (int j = 0; j < queue->num_jobs; j++) {
        int i = (queue->front + j) % QUEUE_SIZE;
        cout << "(" << queue->front << ", " << i << ", " << queue->job_queue[i].producer_num << ", " << queue->job_queue[i].status << ", " << queue->job_queue[i].mat_size << ", " << queue->job_queue[i].mat_id << ") ";
    }
    cout << "\n\n";
}

bool is_full(SharedQueue* queue) {
    return queue->num_jobs == QUEUE_SIZE - 1;
}

bool insert_job(SharedQueue* queue, Job job) {
    if (is_full(queue)) {
        return false;
    }
    queue->num_jobs++;
    queue->job_created++;
    // cout << "Job inserted at: " << queue->rear << endl;
    queue->job_queue[queue->rear] = job;
    queue->rear = (queue->rear + 1) % QUEUE_SIZE;
    return true;
}

void remove_job(SharedQueue* queue) {
    queue->front = (queue->front + 1) % QUEUE_SIZE;
    queue->num_jobs--;
}

void producer(SharedQueue* queue, int producer_num) {
    while (true && queue->job_created < MAX_JOBS) {
        int sleep_time = rand() % SLEEP_TIME;
        usleep(sleep_time);
        if (is_full(queue)) {
            cout << "Queue is full" << endl;
            print_queue(queue);
            continue;
        }
        if (!is_full(queue)) {
            Job* new_job = new Job(producer_num);
            cout << "Create new job" << endl;
            cout << new_job->producer_num << " " << new_job->status << " " << new_job->mat_size << " " << new_job->mat_id << endl;
            insert_job(queue, *new_job);
            print_queue(queue);
        }
    }
}

void worker(SharedQueue* queue) {
    cout << "Inside worker" << endl;
    while (!(queue->job_created == MAX_JOBS && queue->num_jobs == 1)) {
        if (queue->num_jobs < 2)
            continue;
        int sleep_time = rand() % SLEEP_TIME;
        usleep(sleep_time);

        cout << "-----------WORKER----------" << endl;

        if (queue->workidx == -1) {
            // add lock here
            queue->workidx = queue->rear;
            memset(queue->job_queue[queue->workidx].mat, 0, sizeof(queue->job_queue[queue->workidx].mat));
            queue->rear = (queue->rear + 1) % QUEUE_SIZE;
            queue->num_jobs++;
            queue->job_queue[queue->workidx].mat_id = -1;
            queue->job_queue[queue->workidx].producer_num = -1;
            queue->job_queue[queue->workidx].status = 0;
            cout << "Worker: " << queue->workidx << ": " << queue->num_jobs << endl;
            // debmat(queue->job_queue[queue->front].mat,MAT_SIZE,MAT_SIZE);
            // debmat(queue->job_queue[(queue->front+1)%QUEUE_SIZE].mat,MAT_SIZE,MAT_SIZE);
            // remove lock here
        }

        pair<int, int> segs = get_mat_seg(queue->job_queue[queue->front].status);
        if (segs.first == -1) {
            continue;
        }

        int mat[MAT_SIZE_HALF][MAT_SIZE_HALF];
        for (int i = 0; i < MAT_SIZE_HALF; i++) {
            for (int j = 0; j < MAT_SIZE_HALF; j++) {
                mat[i][j] = 0;
                for (int k = 0; k < MAT_SIZE_HALF; k++) {
                    mat[i][j] += queue->job_queue[queue->front].mat[i + (segs.first / 2) * MAT_SIZE_HALF][k + (segs.first % 2) * MAT_SIZE_HALF] * queue->job_queue[(queue->front + 1) % QUEUE_SIZE].mat[k + (segs.second / 2) * MAT_SIZE_HALF][j + (segs.second % 2) * MAT_SIZE_HALF];
                }
            }
        }
        // debmat(mat, MAT_SIZE_HALF, MAT_SIZE_HALF);

        // acquire lock here again
        for (int i = 0; i < MAT_SIZE_HALF; i++) {
            for (int j = 0; j < MAT_SIZE_HALF; j++) {
                queue->job_queue[queue->workidx].mat[i + (segs.first / 2) * MAT_SIZE_HALF][j + (segs.second % 2) * MAT_SIZE_HALF] += mat[i][j];
            }
        }

        queue->job_queue[(queue->front + 1) % QUEUE_SIZE].status++;

        // remove lock here again
        if (queue->job_queue[(queue->front + 1) % QUEUE_SIZE].status == 8) {
            cout << "removing front 2 jobs" << endl;
            int mat[MAT_SIZE][MAT_SIZE];
            for (int i = 0; i < MAT_SIZE; i++) {
                for (int j = 0; j < MAT_SIZE; j++) {
                    mat[i][j] = 0;
                    for (int k = 0; k < MAT_SIZE; k++) {
                        mat[i][j] += queue->job_queue[queue->front].mat[i][k] * queue->job_queue[(queue->front + 1) % QUEUE_SIZE].mat[k][j];
                    }
                    assert(mat[i][j] == queue->job_queue[queue->workidx].mat[i][j]);
                }
            }
            // debmat(mat, MAT_SIZE, MAT_SIZE);
            // debmat(queue->job_queue[queue->workidx].mat, MAT_SIZE, MAT_SIZE);
            remove_job(queue);
            remove_job(queue);
            queue->workidx = -1;
        }
    }
}

int main() {
    int shm_id = shmget(IPC_PRIVATE, sizeof(SharedQueue), IPC_CREAT | 0666);
    SharedQueue* queue = create_queue(shm_id);
    cout << "Queue created" << endl;
    int num_producers = 2;

    for (int i = 0; i < num_producers; i++) {
        pid_t pid = fork();  // Create a child process for the producer i
        if (pid == 0 && (i % 2 == 0)) {
            producer(queue, i);
            exit(0);
        }
        if (pid == 0 && (i % 2)) {
            worker(queue);
            exit(0);
        }
    }

    // while(true) {
    //     int sleep_time = rand() % 5;
    //     sleep(sleep_time);
    //     remove_job(queue);
    // }
    while ((wait(NULL)) > 0)
        ;
    shmdt(queue);
    shmctl(shm_id, IPC_RMID, NULL);
}
