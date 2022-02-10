#include <sys/shm.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include <cassert>
#include <iostream>
using namespace std;

struct ProcessData {
    double** A;
    double** B;
    double** C;
    int veclen, i, j;
};

void mult(ProcessData* pd) {
    int i = pd->i;
    int j = pd->j;
    pd->C[i][j] = 0;
    for (int k = 0; k < pd->veclen; k++) {
        pd->C[i][j] += pd->A[i][k] * pd->B[k][j];
    }
}

int main() {
    int r1, c1, r2, c2;
    cin >> r1 >> c1 >> r2 >> c2;
    assert(c1 == r2);
    key_t key = ftok("shmfile", 11);
    size_t size = sizeof(double) * (r1 * c1 + r2 * c2 + r1 * c2);
    int shmid = shmget(key, size, IPC_CREAT | 0644);
    double* mem = (double*)(shmat(shmid, (void*)NULL, 0));
    double *_A[r1], *_B[r2], *_C[r1];
    for (int i = 0; i < r1; i++) {
        _A[i] = mem + i * c1;
    }
    for (int i = 0; i < r2; i++) {
        _B[i] = mem + r1 * c1 + i * c2;
    }
    for (int i = 0; i < r1; i++) {
        _C[i] = mem + r1 * c1 + r2 * c2 + i * c2;
    }
    double** A = (double**)_A;
    double** B = (double**)_B;
    double** C = (double**)_C;

    for (int i = 0; i < r1; i++) {
        for (int j = 0; j < c1; j++) {
            cin >> A[i][j];
        }
    }
    for (int i = 0; i < r2; i++) {
        for (int j = 0; j < c2; j++) {
            cin >> B[i][j];
        }
    }
    for (int i = 0; i < r1; i++) {
        for (int j = 0; j < c2; j++) {
            pid_t pid = fork();
            if (pid == 0) {
                ProcessData pd = {A, B, C, c1, i, j};
                mult(&pd);
                shmdt(mem);
                exit(0);
            } else
                continue;
        }
    }
    while (wait(NULL) != -1)
        ;
    for (int i = 0; i < r1; i++) {
        for (int j = 0; j < c2; j++) {
            cout << C[i][j] << " ";
        }
        cout << endl;
    }
    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);
}