#include <bits/stdc++.h>

#include "memlab.h"
using namespace std;

void randArrInt(Ptr x, Ptr y) {
    initScope();
    cout << "creating random array" << endl;
    ArrPtr arr = createArr(x.type, 50000);
    cout << "array created" << endl;
    for (int i = 0; i < 50000; i++) {
        int val = rand() % (201) - 100;
        assignArr(arr, i, val);
    }
    endScope();
    gcActivate();
}

void randArrMediumInt(Ptr x, Ptr y) {
    initScope();
    cout << "creating random array" << endl;
    ArrPtr arr = createArr(x.type, 50000);
    cout << "array created" << endl;
    for (int i = 0; i < 50000; i++) {
        int val = rand() % (201) - 100;
        assignArr(arr, i, medium_int(val));
    }
    endScope();
    gcActivate();
}

void randArrChar(Ptr x, Ptr y) {
    initScope();
    cout << "creating random array" << endl;
    ArrPtr arr = createArr(x.type, 50000);
    cout << "array created" << endl;
    for (int i = 0; i < 50000; i++) {
        char val = 'a' + rand() % 26;
        assignArr(arr, i, val);
    }
    endScope();
    gcActivate();
}

void randArrBool(Ptr x, Ptr y) {
    initScope();
    cout << "creating random array" << endl;
    ArrPtr arr = createArr(x.type, 50000);
    cout << "array created" << endl;
    for (int i = 0; i < 50000; i++) {
        bool val = rand() % 2;
        assignArr(arr, i, val);
    }
    endScope();
    gcActivate();
}

void test() {
    initScope();
    Ptr a = createVar(Type::INT);
    assignVar(a, 10);
    Ptr b = createVar(Type::INT);
    assignVar(b, 20);
    Ptr c = createVar(Type::MEDIUM_INT);
    assignVar(c, medium_int(30));
    Ptr d = createVar(Type::MEDIUM_INT);
    assignVar(d, medium_int(40));
    Ptr e = createVar(Type::CHAR);
    assignVar(e, 'a');
    Ptr f = createVar(Type::CHAR);
    assignVar(f, 'b');
    Ptr g = createVar(Type::BOOL);
    assignVar(g, true);
    Ptr h = createVar(Type::BOOL);
    assignVar(h, false);
    randArrInt(a, b);
    randArrMediumInt(c, d);
    randArrChar(e, f);
    randArrBool(g, h);
    randArrInt(a, b);
    randArrMediumInt(c, d);
    randArrChar(e, f);
    randArrBool(g, h);
    randArrInt(a, b);
    randArrMediumInt(c, d);
    usleep(10);
    endScope();
    gcActivate();
    usleep(10);
}

int main() {
    createMem(250 * 1024 * 1024, true);  // 250MB
    test();
    freeMem();
    // createMem(250 * 1024 * 1024, false);  // 250MB
    // test();
    // freeMem();
}