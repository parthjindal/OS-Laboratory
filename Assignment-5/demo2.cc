#include <bits/stdc++.h>

#include "memlab.h"
using namespace std;

Ptr fibonacciProduct(Ptr k) {
    int val;
    getVar(k, &val);
    ArrPtr arr = createArr(Type::INT, val);
    for (int i = 0; i < val; i++) {
        if (i == 0) {
            assignArr(arr, i, 1);
        } else if (i == 1) {
            assignArr(arr, i, 1);
        } else {
            int a;
            getVar(arr, i - 1, &a);
            int b;
            getVar(arr, i - 2, &b);
            assignArr(arr, i, a + b);
        }
    }
    Ptr ans = createVar(Type::INT);
    int prod = 1;
    for (int i = 0; i < val; i++) {
        int a;
        getVar(arr, i, &a);
        prod *= a;
    }
    assignVar(ans, prod);
    gc_run();
    return ans;
}

int main() {
    int arrt[10];
    arrt[0] = 1;
    arrt[1] = 1;
    for (int i = 2; i < 10; i++) {
        arrt[i] = arrt[i - 1] + arrt[i - 2];
    }
    int prod = 1;
    for (int i = 0; i < 10; i++) {
        prod *= arrt[i];
    }
    cout << "Actual Product: " << prod << endl;
    createMem(250 * 1024 * 1024);  // 250MB
    initScope();
    Ptr x = createVar(Type::INT);
    assignVar(x, 10);
    Ptr y = fibonacciProduct(x);
    int val;
    getVar(y, &val);
    cout << "Final Product: " << val << endl;
    endScope();
    // sleep(10);
    // usleep(200 * 1000);
    gc_run();
    freeMem();
}