#include <bits/stdc++.h>

#include "memlab.h"
using namespace std;

void randArr(Ptr x, Ptr y) {
    cout << "creating random array" << endl;
    ArrPtr arr = createArr(x.type, 50000);
    cout << "array created" << endl;
    for (int i = 0; i < 50000; i++) {
        initScope();
        int r = rand() % 26;
        Ptr p1 = createVar(Type::INT);
        assignVar(p1, r);
        getVar(p1, &r);
        freeElem(p1);
        char c = 'a' + r;
        // Ptr p2 = createVar(Type::CHAR);
        // assignVar(p2, c);
        // getVar(p2, &c);
        assignArr(arr, i, c);
        endScope();
        // gcActivate();
    }
    // debugPrint();
    // sleep(1);
    freeElem(arr);
    gcActivate();
}

int main() {
    createMem(250 * 1024 * 1024);  // 250MB
    for (int i = 0; i < 10; i++) {
        cout << "called for " << i << endl;
        initScope();
        Ptr x = createVar(Type::CHAR);
        Ptr y = createVar(Type::CHAR);
        randArr(x, y);
        cout << "done" << i << endl;
        endScope();
        usleep(50 * 1000);
    }
    freeMem();
}