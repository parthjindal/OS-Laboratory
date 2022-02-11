#include <bits/stdc++.h>
#include <pthread.h>
#include <signal.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

using namespace std;

#define QUEUE_SIZE 8
#define MAT_SIZE 1000

struct Job {
    int producer_num;
    int status;
    int mat_size;
    int mat_id;
    int mat[MAT_SIZE][MAT_SIZE];

    Job() {
        producer_num = 0;
        status = 0;
        mat_size = 0;
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
    Job job_queue[QUEUE_SIZE];

    SharedQueue() {
        num_jobs = 0;
        front = 0;
        rear = 0;
    }
};

struct SHM {
    SharedQueue queue;
    int job_created;
    int 
};

SharedQueue* create_queue(int shm_key) {
    cout << "create_queue" << endl;
    SharedQueue* queue = (SharedQueue*)shmat(shm_key, NULL, 0);
    queue->num_jobs = 0;
    cout << __LINE__ << endl;
    queue->front = 0;
    cout << __LINE__ << endl;
    queue->rear = 0;
    cout << __LINE__ << endl;
    return queue;
}

// void print_queue(SharedQueue* queue) {
//     cout << "Queue: ";
//     cout << "Size of queue: " << queue->num_jobs << endl;
//     cout << "Jobs Created: " << queue->job_created << endl;
//     for (int i = 0; i < queue->num_jobs; i++) {
//         cout << "(" << queue->job_queue[i].producer_num << ", " << queue->job_queue[i].status << ", " << queue->job_queue[i].mat_size << ", " << queue->job_queue[i].mat_id << ") ";
//     }
//     cout << "\n\n";
// }

// void insert_job(SharedQueue* queue, Job Job) {
//     queue->job_created++;
//     queue->job_queue[queue->rear] = Job;
//     queue->rear = (queue->rear + 1) % QUEUE_SIZE;
//     queue->num_jobs++;
// }

// void remove_job(SharedQueue* queue) {
//     queue->front = (queue->front + 1) % QUEUE_SIZE;
//     queue->num_jobs--;
// }

// bool is_full(SharedQueue* queue) {
//     return queue->num_jobs == QUEUE_SIZE;
// }

// void producer(SharedQueue* queue, int producer_num) {
//     while(true && queue->job_created < 10) {
//         int sleep_time = rand() % 4;
//         sleep(sleep_time);
//         if(is_full(queue)) {
//             cout << "Queue is full" << endl;
//             print_queue(queue);
//             continue;
//         }
//         if(!is_full(queue)) {
//             Job* new_job = new Job(producer_num);
//             insert_job(queue, *new_job);
//             print_queue(queue);
//         }
//     }
// }

int main() {
    int shm_id = shmget(IPC_PRIVATE, sizeof(SharedQueue), IPC_CREAT | 0666);
    SharedQueue* queue = create_queue(shm_id);
    for (int i = 0; i < QUEUE_SIZE; i++) {
        Job* new_job = new Job(i);
        queue->job_queue[i] = *new_job;
    }
    cout << "Queue created" << endl;
    int num_producers = 1;

    // for(int i = 0; i < num_producers; i++){
    // 	pid_t pid = fork(); // Create a child process for the producer i
    // 	if(pid == 0 ){
    //         producer(queue, i);
    //         exit(0);
    // 	}
    // }

    // while(true) {
    //     int sleep_time = rand() % 5;
    //     sleep(sleep_time);
    //     remove_job(queue);
    // }
    shmdt(queue);
    shmctl(shm_id, IPC_RMID, NULL);
}
