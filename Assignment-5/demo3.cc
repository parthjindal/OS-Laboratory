#include <bits/stdc++.h>

#include "memlab.h"
using namespace std;

int main() {
    createMem(392);
    initScope();
    Ptr x = createVar(Type::INT);
    ArrPtr y = createArr(Type::INT, 10);
    ArrPtr z = createArr(Type::CHAR, 40);
    Ptr a = createVar(Type::MEDIUM_INT);
    Ptr b = createVar(Type::BOOL);
    ArrPtr c = createArr(Type::MEDIUM_INT, 8);
    ArrPtr c1 = createArr(Type::BOOL, 320);
    Ptr d = createVar(Type::INT);
    Ptr e = createVar(Type::CHAR);
    ArrPtr f = createArr(Type::INT, 12);
    ArrPtr f1 = createArr(Type::CHAR, 12);
    Ptr g = createVar(Type::INT);
    Ptr h = createVar(Type::CHAR);
    ArrPtr i = createArr(Type::INT, 12);

    freeElem(x);
    freeElem(z);
    freeElem(b);
    freeElem(c1);
    freeElem(e);
    freeElem(f1);
    freeElem(h);
    // ArrPtr i1 = createArr(Type::CHAR, 4);
    debugPrint(stdout);
    // gcActivate();
    // Ptr l = createArr(Type::INT, 20);
    // sleep(1);
    usleep(1);
    Ptr m = createVar(Type::INT);
    debugPrint(stdout);
    endScope();
    freeMem();
}