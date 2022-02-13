#include <sys/shm.h>
#include <sys/wait.h>
#include <unistd.h>

#include <iomanip>
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

void print_mat(double** mat, int r, int c) {
    cout << "[";
    for (int i = 0; i < r; i++) {
        cout << (i == 0 ? "" : " ") << "[";
        for (int j = 0; j < c; j++) {
            cout << setw(7) << mat[i][j] << ((j != c - 1) ? " " : "");
        }
        cout << "]" << ((i != r - 1) ? "\n" : "");
    }
    cout << "]";
}

int main() {
    int r1, c1, r2, c2;
    cout << "Enter r1, c1, r2, c2" << endl;
    cin >> r1 >> c1 >> r2 >> c2;
    if (c1 != r2) {
        cout << "c1 != r1" << endl;
        return 0;
    }
    cout << fixed << setprecision(4);

    srand(time(NULL));
    size_t size = sizeof(double) * (r1 * c1 + r2 * c2 + r1 * c2) + sizeof(double*) * (r1 + r2 + r1);
    int shmid = shmget(IPC_PRIVATE, size, IPC_CREAT | 0644);
    double* mem = (double*)(shmat(shmid, (void*)NULL, 0));
    double** mem2 = (double**)(mem + r1 * c1 + r2 * c2 + r1 * c2);

    for (int i = 0; i < r1; i++)
        mem2[i] = mem + i * c1;
    for (int i = 0; i < r2; i++)
        mem2[i + r1] = mem + r1 * c1 + i * c2;
    for (int i = 0; i < r1; i++)
        mem2[i + r1 + r2] = mem + r1 * c1 + r2 * c2 + i * c2;

    double** A = mem2;
    double** B = mem2 + r1;
    double** C = mem2 + r1 + r2;

    for (int i = 0; i < r1; i++) {
        for (int j = 0; j < c1; j++) {
            A[i][j] = (rand() % 2001 - 1000) * 1.0 / 1000.0;
        }
    }
    cout << "A:\n";
    print_mat(A, r1, c1);
    cout << "\n";

    for (int i = 0; i < r2; i++) {
        for (int j = 0; j < c2; j++) {
            B[i][j] = (rand() % 2001 - 1000) * 1.0 / 1000.0;
        }
    }
    cout << "\nB:\n";
    print_mat(B, r2, c2);
    cout << "\n";

    for (int i = 0; i < r1; i++) {
        for (int j = 0; j < c2; j++) {
            pid_t pid = fork();
            if (pid == 0) {
                ProcessData pd = {A, B, C, c1, i, j};
                mult(&pd);
                exit(0);
            }
            if (pid == -1) {
                perror("fork");
                exit(1);
            }
        }
    }

    while (wait(NULL) != -1)
        ;
    cout << "\nC:\n";
    print_mat(C, r1, c2);

    shmdt(mem);
    shmctl(shmid, IPC_RMID, NULL);
}