#include "memlab.h"

#include <pthread.h>
#include <stdlib.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "debug.h"
using namespace std;

pthread_t gcThread;

MemBlock* mem = nullptr;
Stack* stack;
SymbolTable* symTable;

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
            ERROR("%d: pthread_mutex_unlock failed: %s", __LINE__, strerror(ret)); \
            exit(1);                                                               \
        }                                                                          \
    } while (0)

int getSize(const Type& type) {
    switch (type) {
        case Type::INT:
            return 4;
        case Type::CHAR:
            return 1;
        case Type::MEDIUM_INT:
            return 3;
        case Type::BOOL:
            return 1;
        case Type::ARRAY: {
            return 0;
        }
        default:
            return 0;
    }
}

void SymbolTable::Init() {
    size = 0;
    head = 0;
    tail = MAX_SYMBOLS - 1;
    // Explicit free list
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        symbols[i].word1 = 0;
        symbols[i].word2 = ((i + 1)) << 1;
    }
    symbols[tail].word2 = -2;  // mark end of free list
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&mutex, &attr);
    // cout << "SymbolTable initialized" << endl;
}

SymbolTable::~SymbolTable() {
    pthread_mutex_destroy(&mutex);
}

int SymbolTable::alloc(unsigned int wordidx, unsigned int offset) {
    if (size == MAX_SYMBOLS) {
        return -1;
    }
    // DEBUG()
    // cout << "Allocating symbol " << wordidx << ":" << offset << endl;
    unsigned int idx = head;
    head = (symbols[head].word2 & -2) >> 1;
    symbols[idx].word1 = (wordidx << 1) | 1;  // mark as allocated
    symbols[idx].word2 = (offset << 1) | 1;   // mark as in use
    size++;
    return idx;  // local address
}

void SymbolTable::free(unsigned int idx) {
    if (size == MAX_SYMBOLS) {
        head = tail = idx;
        symbols[idx].word1 = 0;
        symbols[idx].word2 = -2;  // sentinel
        size--;
        return;
    }
    symbols[tail].word2 = idx << 1;
    symbols[idx].word1 = 0;
    symbols[idx].word2 = -2;  // sentinel
    tail = idx;
    size--;
}

int* SymbolTable::getPtr(unsigned int idx) {
    int wordidx = getWordIdx(idx);
    int offset = getOffset(idx);
    int* ptr = mem->start + wordidx + 1;
    ptr = (int*)((char*)ptr + offset);
    return ptr;
}

void Stack::Init() {
    _top = -1;
}
void Stack::push(int elem) {
    _elems[++_top] = elem;
}
int Stack::pop() {
    return _elems[_top--];
}
int Stack::top() {
    return _elems[_top];
}

void MemBlock::Init(int _size) {
    int size = (((_size + 3) >> 2) << 2) + 8;  // align to 4 bytes
                                               // and add 8 bytes for header, footer
    mem = (int*)malloc(size);
    start = mem;
    end = mem + (size >> 2);
    *start = (size >> 2) << 1;                      // 31 bits store size last bit for if free or not
    *(start + (size >> 2) - 1) = (size >> 2) << 1;  // footer
    totalFreeMem = size >> 2;
    totalFreeBlocks = 1;
    biggestFreeBlockSize = size >> 2;
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&mutex, &attr);
}

MemBlock::~MemBlock() {
    free(mem);
    pthread_mutex_destroy(&mutex);
}

int MemBlock::getMem(int size) {  // size in bytes (4 bytes aligned)
    int* p = start;
    int newsize = (((size + 3) >> 2) << 2) + 8;
    cout << "Allocating " << newsize << " bytes" << endl;
    while ((p < end) &&
           ((*p & 1) ||
            ((*p << 1) < newsize))) {
        // cout << (*p >> 1) << endl;
        p = p + (*p >> 1);
    }
    cout << "Allocating " << newsize << " bytes at " << p << endl;
    if (p == end) {
        return -1;
    }
    addBlock((int*)p, newsize);
    return (p - start);
}

void MemBlock::addBlock(int* ptr, int size) {
    int oldsize = *ptr << 1;  // old size in bytes
    int words = size >> 2;
    *ptr = (words << 1) | 1;
    *(ptr + words - 1) = (words << 1) | 1;  // footer
    if (size < oldsize) {
        *(ptr + words) = (oldsize - size) >> 1;
        *(ptr + (oldsize >> 2) - 1) = (oldsize - size) >> 1;
    }
    totalFreeMem -= words;
    if (size == oldsize) {
        totalFreeBlocks--;
    }
    if ((oldsize >> 2) == biggestFreeBlockSize) {
        biggestFreeBlockSize -= words;
    }
    biggestFreeBlockSize = max(biggestFreeBlockSize, totalFreeMem / (totalFreeBlocks + 1));
    cout << "Total free memory: " << totalFreeMem << endl;
    cout << "Total free blocks: " << totalFreeBlocks << endl;
    cout << "Biggest free block: " << biggestFreeBlockSize << endl;
    cout << endl;
}

void MemBlock::freeBlock(int wordid) {
    int* ptr = start + wordid;
    int words = *ptr >> 1;
    *ptr = *ptr & -2;                // mark as free
    *(ptr + words - 1) = *ptr & -2;  // mark as free
    totalFreeBlocks++;
    totalFreeMem += words;

    int* next = ptr + words;
    if (next != end && (*next & 1) == 0) {  // next is also free so coelesce
        words = words + (*next >> 1);
        *ptr = words << 1;                        // new size in words
        *(next + (*next >> 1) - 1) = words << 1;  // footer
        totalFreeBlocks--;
    }
    if (ptr != start && (*(ptr - 1) & 1) == 0) {  // previous is also free so coelesce
        int prevwords = (*(ptr - 1) >> 1);
        *(ptr - prevwords) = (prevwords + words) << 1;  // new size in words
        *(ptr + words - 1) = (prevwords + words) << 1;  // footer
        words = words + prevwords;
        totalFreeBlocks--;
    }
    biggestFreeBlockSize = max(biggestFreeBlockSize, words);
    biggestFreeBlockSize = max(biggestFreeBlockSize, totalFreeMem / (totalFreeBlocks + 1));
    cout << "Total free memory: " << totalFreeMem << endl;
    cout << "Total free blocks: " << totalFreeBlocks << endl;
    cout << "Biggest free block: " << biggestFreeBlockSize << endl;
    cout << endl;
}

void createMem(int size) {
    if (mem != nullptr)
        throw std::runtime_error("Memory already created");
    size_t t = sizeof(MemBlock) + sizeof(SymbolTable) + sizeof(Stack);
    void* tmem = malloc(t);
    mem = (MemBlock*)tmem;
    mem->Init(size);
    symTable = (SymbolTable*)((char*)tmem + sizeof(MemBlock));
    symTable->Init();
    stack = (Stack*)((char*)tmem + sizeof(MemBlock) + sizeof(SymbolTable));
    stack->Init();
    int ret = pthread_create(&gcThread, nullptr, garbageCollector, nullptr);
    if (ret != 0) {
        throw std::runtime_error("Error creating garbage collector thread");
    }
}

Ptr createVar(const Type& t) {
    int _size = getSize(t);
    _size = (((_size + 3) >> 2) << 2);
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int wordid = mem->getMem(_size);
    if (wordid == -1)
        throw std::runtime_error("Out of memory");
    PTHREAD_MUTEX_LOCK(&symTable->mutex);
    int local_addr = symTable->alloc(wordid, 0);
    if (local_addr == -1)
        throw std::runtime_error("Out of memory in symbol table");
    PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
    stack->push(local_addr);
    return Ptr(t, translate(local_addr));
}

void getVar(const Ptr& p, void* val) {
    int local_addr = p.addr >> 2;  // TODO: write a function here
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    memcpy(val, ptr, getSize(p.type));
    int temp = *(int*)ptr;
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
    if (p.type == Type::MEDIUM_INT) {
        if (temp & (1 << 23)) {
            temp = temp | 0xFF000000;
        }
        memcpy(val, &temp, 4);
    }
}

void getVar(const ArrPtr& p, int idx, void* val) {
    int local_addr = p.addr >> 2;  
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if(idx < 0 || idx >= p.width)
        throw std::runtime_error("Index out of bounds");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    int word = getWordForIdx(p.type, idx);
    int offset = getOffsetForIdx(p.type, idx);
    int* arr = (int*)((char*)ptr + word * 4 + offset);
    memcpy(val, (void*)arr, getSize(p.type));
    int temp = *(int*)arr;
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
    if (p.type == Type::MEDIUM_INT) {
        if (temp & (1 << 23)) {
            temp = temp | 0xFF000000;
        }
        memcpy(val, &temp, 4);
    }
}


void assignVar(const Ptr& p, int val) {
    int local_addr = p.addr >> 2;  // TODO: write a function here
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (getType(p) != Type::INT && getType(p) != Type::MEDIUM_INT)
        throw std::runtime_error("Assignment to non-int variable");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    memcpy((void*)ptr, &val, getSize(p.type));
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void assignVar(const Ptr& p, bool f) {
    int local_addr = p.addr >> 2;  // TODO: write a function here
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (getType(p) != Type::BOOL)
        throw std::runtime_error("Assignment to non-bool variable");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    memcpy((void*)ptr, &f, sizeof(bool));
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void assignVar(const Ptr& p, char c) {
    int local_addr = p.addr >> 2;  // TODO: write a function here
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (getType(p) != Type::CHAR)
        throw std::runtime_error("Assignment to non-char variable");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    memcpy((void*)ptr, &c, sizeof(char));
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

ArrPtr createArr(const Type& t, int width) {
    int _count = (1 << 2) / getSize(t);
    int _width = (width + _count - 1) / _count;  // round up
    int _size = _width << 2;
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    cout << "Creating array of size " << _size << endl;
    int wordid = mem->getMem(_size);
    cout << "wordid: " << wordid << endl;
    if (wordid == -1)
        throw std::runtime_error("Out of memory");
    PTHREAD_MUTEX_LOCK(&symTable->mutex);
    int local_addr = symTable->alloc(wordid, 0);
    if (local_addr == -1)
        throw std::runtime_error("Out of memory in symbol table");
    PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
    stack->push(local_addr);
    return ArrPtr(t, translate(local_addr), _width);
}

void initScope() {
    stack->push(-1);
}

void endScope() {
    while (stack->top() != -1) {
        int local_addr = stack->pop();
        PTHREAD_MUTEX_LOCK(&symTable->mutex);  //  todo: chekc if this is required
        if (symTable->isAllocated(local_addr))
            symTable->setUnmarked(local_addr);
        PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    }
    stack->pop();  // pop -1
}

void _freeElem(int local_addr) {
    cout << gettid() << " freeing " << local_addr << endl;
    int wordId = symTable->getWordIdx(local_addr);
    mem->freeBlock(wordId);
    symTable->free(local_addr);
}

void freeElem(const Ptr& p) {
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    PTHREAD_MUTEX_LOCK(&symTable->mutex);
    int local_addr = p.addr >> 2;  // TODO: write a function here
    if (symTable->isAllocated(local_addr)) {
        _freeElem(local_addr);
    } else {
        throw std::runtime_error("double free");
    }
    PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void gc_run() {
    cout << "GC running" << endl;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        PTHREAD_MUTEX_LOCK(&mem->mutex);
        PTHREAD_MUTEX_LOCK(&symTable->mutex);
        if (symTable->isAllocated(i) && !symTable->isMarked(i)) {
            _freeElem(i);
        }
        PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
        PTHREAD_MUTEX_UNLOCK(&mem->mutex);
    }
    cout << "GC finished" << endl;
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    PTHREAD_MUTEX_LOCK(&symTable->mutex);
    mem->totalFreeMem = max(mem->totalFreeMem, 1);
    double free_ratio = (double)mem->totalFreeMem / (double)(mem->biggestFreeBlockSize);
    cout << "Free ratio: " << free_ratio << endl;
    if (free_ratio >= 2) {
        compactMem();
    }
    PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void calcOffset() {
    cout << "Calculating offset" << endl;
    int* p = mem->start;
    int offset = 0;
    while (p < mem->end) {
        if ((*p & 1) == 0) {
            offset += (*p >> 1);
        } else {
            *(p + (*p >> 1) - 1) = (((p - offset) - mem->start) << 1) | 1;
            cout << "offset for: " << p - mem->start << " is " << (*(p + (*p >> 1) - 1) >> 1) << endl;
            cout << "offsetval: " << offset << endl;
        }
        p = p + (*p >> 1);
    }
}

void updateSymbolTable() {
    cout << "Updating symbol table" << endl;
    for (int i = 0; i < MAX_SYMBOLS; i++) {
        if (symTable->isAllocated(i)) {
            int* p = symTable->getPtr(i) - 1;
            int newWordId = *(p + (*p >> 1) - 1) >> 1;
            cout << "i, newWordId: " << i << " " << newWordId << endl;
            symTable->symbols[i].word1 = (newWordId << 1) | 1;
        }
    }
}

void compactMem() {
    cout << "Compacting memory" << endl;
    // PTHREAD_MUTEX_LOCK(&mem->mutex);
    // PTHREAD_MUTEX_LOCK(&symTable->mutex);
    calcOffset();
    updateSymbolTable();
    int* p = mem->start;
    int* next = p + (*p >> 1);
    while (next != mem->end) {
        if ((*p & 1) == 0 && (*next & 1) == 1) {
            int word1 = *p >> 1;
            int word2 = *next >> 1;
            memcpy(p, next, word2 << 2);
            p = p + word2;
            *p = word1 << 1;
            *(p + word1 - 1) = word1 << 1;
            next = p + word1;
            if (next != mem->end && (*next & 1) == 0) {
                word1 = word1 + (*next >> 1);
                *p = word1 << 1;
                *(p + word1 - 1) = word1 << 1;
                next = p + word1;
            }
        } else {
            p = next;
            next = p + (*p >> 1);
        }
    }
    p = mem->start;
    while (p < mem->end) {
        *(p + (*p >> 1) - 1) = *p;
        p = p + (*p >> 1);
    }
    mem->biggestFreeBlockSize = mem->totalFreeMem;
    mem->totalFreeBlocks = 1;
    // PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    // PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void* garbageCollector(void*) {
    while (true) {
        gc_run();
        usleep(GC_PERIOD_MS * 1000);
    }
}

int getWordForIdx(Type t, int idx) {
    int _count = (1 << 2) / getSize(t);
    int _idx = idx / _count;
    return _idx;
}

int getOffsetForIdx(Type t, int idx) {
    int _count = (1 << 2) / getSize(t);
    int _idx = idx / _count;
    return (idx - _idx * _count) * getSize(t);
}

void assignArr(const ArrPtr& p, int idx, int val) {
    int local_addr = p.addr >> 2;  // TODO: write a function here
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");

    if (getType(p) != Type::INT && getType(p) != Type::MEDIUM_INT)
        throw std::runtime_error("Assignment to non-int array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    int word = getWordForIdx(p.type, idx);
    int offset2 = getOffsetForIdx(p.type, idx);
    ptr = (int*)((char*)ptr + word * 4 + offset2);
    memcpy((void*)ptr, &val, getSize(p.type));
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void assignArr(const ArrPtr& p, int idx, char c) {
    int local_addr = p.addr >> 2;  // TODO: write a function here
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");

    if (getType(p) != Type::CHAR)
        throw std::runtime_error("Assignment to non-char array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    int word = getWordForIdx(p.type, idx);
    int offset2 = getOffsetForIdx(p.type, idx);
    ptr = (int*)((char*)ptr + word * 4 + offset2);
    memcpy((void*)ptr, &c, sizeof(char));
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void assignArr(const ArrPtr& p, int idx, bool f) {
    int local_addr = p.addr >> 2;  // TODO: write a function here
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");

    if (getType(p) != Type::BOOL)
        throw std::runtime_error("Assignment to non-bool array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    int word = getWordForIdx(p.type, idx);
    int offset2 = getOffsetForIdx(p.type, idx);
    ptr = (int*)((char*)ptr + word * 4 + offset2);
    memcpy((void*)ptr, &f, sizeof(bool));
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void assignArr(const ArrPtr& p, int arr[], int n) {
    cout << "Assigning array" << endl;
    int local_addr = p.addr >> 2;  // TODO: write a function here
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (getType(p) != Type::INT && getType(p) != Type::MEDIUM_INT)
        throw std::runtime_error("Assignment to non-int array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    cout << ptr - mem->start << endl;
    for (int i = 0; i < n; i++) {
        int word = getWordForIdx(p.type, i);
        int offset = getOffsetForIdx(p.type, i);
        cout << "Word: " << word << " Offset: " << offset << endl;
        int* temp_ptr = (int*)((char*)ptr + word * 4 + offset);
        memcpy((void*)temp_ptr, &arr[i], getSize(p.type));
    }
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
    cout << "Done assigning array" << endl;
}

void assignArr(const ArrPtr& p, char arr[], int n) {
    int local_addr = p.addr >> 2;
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");

    if (getType(p) != Type::CHAR)
        throw std::runtime_error("Assignment to non-char array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    for (int i = 0; i < n; i++) {
        int word = getWordForIdx(p.type, i);
        int offset2 = getOffsetForIdx(p.type, i);
        ptr = (int*)((char*)ptr + word * 4 + offset2);
        memcpy((void*)ptr, &arr[i], sizeof(char));
    }
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void assignArr(const ArrPtr& p, bool arr[], int n) {
    int local_addr = p.addr >> 2;
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");

    if (getType(p) != Type::BOOL)
        throw std::runtime_error("Assignment to non-bool array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    for (int i = 0; i < n; i++) {
        int word = getWordForIdx(p.type, i);
        int offset2 = getOffsetForIdx(p.type, i);
        ptr = (int*)((char*)ptr + word * 4 + offset2);
        memcpy((void*)ptr, &arr[i], sizeof(bool));
    }
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void testMemBlock() {
    MemBlock* mem = new MemBlock();
    mem->Init(1024 * 1024);  // 1 MB
    int word1 = mem->getMem(512);
    cout << "word1: " << word1 << endl;
    int word2 = mem->getMem(256);
    cout << "word2: " << word2 << endl;
    int word3 = mem->getMem(128);
    cout << "word3: " << word3 << endl;
    mem->freeBlock(word1);
    cout << "freeing word1" << endl;
    int word4 = mem->getMem(128);
    cout << "word4: " << word4 << endl;
    mem->freeBlock(word2);
    cout << "freeing word2" << endl;
    int word5 = mem->getMem(640);
    cout << "word5: " << word5 << endl;
}

void testSymbolTable() {
    SymbolTable* symtable = new SymbolTable();
    symtable->Init();
    // cout << "alloc: " << symtable->alloc(0, 0) << endl;
    int v1 = symtable->alloc(0, 0);
    cout << "v1: " << v1 << endl;
    int v2 = symtable->alloc(4, 0);
    cout << "v2: " << v2 << endl;
    int v3 = symtable->alloc(8, 0);
    cout << "v3: " << v3 << endl;
    symtable->free(v2);
    symtable->free(v1);
    int v4 = symtable->alloc(4, 0);
    cout << "v4: " << v4 << endl;
    int v5 = symtable->alloc(4, 0);
    cout << "v5: " << v5 << endl;
}

void testCreateVar() {
    createMem(1024 * 1024);
    Ptr p1 = createVar(Type::INT);
    Ptr p2 = createVar(Type::BOOL);
    Ptr p3 = createVar(Type::CHAR);
    cout << p1.addr << endl;
    cout << p2.addr << endl;
    cout << p3.addr << endl;
}

void testAssignVar() {
    createMem(1024 * 1024);
    Ptr p1 = createVar(Type::INT);
    Ptr p2 = createVar(Type::BOOL);
    Ptr p3 = createVar(Type::MEDIUM_INT);
    Ptr p4 = createVar(Type::CHAR);
    cout << p1.addr << " " << p2.addr << " " << p3.addr << " " << p4.addr
         << endl;
    assignVar(p1, 123);
    assignVar(p2, false);
    assignVar(p3, -123);
    assignVar(p4, 'z');
    int val;
    getVar(p1, &val);
    cout << val << endl;
    bool f;
    getVar(p2, &f);
    cout << f << endl;
    int val2;
    getVar(p3, &val2);
    cout << val2 << endl;
    char c;
    getVar(p4, &c);
    cout << c << endl;
}

void freeMem() {
    pthread_mutex_destroy(&mem->mutex);
    pthread_mutex_destroy(&symTable->mutex);
    free(mem->start);
    free(mem);
    free(symTable);
    free(stack);
    exit(0);
}

void testCode() {
    createMem(1024 * 1024 * 512);  // 512 MB
    initScope();
    Ptr p1 = createVar(Type::INT);
    cout << "p1.addr: " << p1.addr << endl;
    Ptr p2 = createVar(Type::BOOL);
    Ptr p3 = createVar(Type::MEDIUM_INT);
    Ptr p4 = createVar(Type::CHAR);
    usleep(150 * 1000);
    freeElem(p1);
    freeElem(p3);
    Ptr p5 = createVar(Type::INT);
    cout << "p5.addr: " << p5.addr << endl;
    endScope();
    initScope();
    usleep(100 * 1000);
    p1 = createVar(Type::INT);
    p2 = createVar(Type::BOOL);
    p3 = createVar(Type::MEDIUM_INT);
    p4 = createVar(Type::CHAR);
    endScope();
    sleep(100);
    freeMem();
}

void testCompaction() {
    createMem(136);
    Ptr p1 = createVar(Type::INT);
    Ptr p2 = createVar(Type::INT);
    ArrPtr arr1 = createArr(Type::INT, 10);
    Ptr p3 = createVar(Type::INT);
    Ptr p4 = createVar(Type::INT);
    ArrPtr arr2 = createArr(Type::INT, 10);
    // usleep(1000);
    freeElem(p1);
    freeElem(p2);
    // freeElem(arr1);
    freeElem(p4);
    compactMem();
    int* arrptr1 = symTable->getPtr(arr1.addr >> 2);
    cout << "arrptr1: " << arrptr1 - mem->start << endl;
    int* ptr3 = symTable->getPtr(p3.addr >> 2);
    cout << "ptr3:" << ptr3 - mem->start << endl;
    int* arrptr2 = symTable->getPtr(arr2.addr >> 2);
    cout << "arrptr2: " << arrptr2 - mem->start << endl;
    cout << endl;
    int* p = mem->start;
    cout << (*(p) >> 1) << " " << (*(p)&1) << endl;
    cout << (*(p + 11) >> 1) << " " << (*(p + 11) & 1) << endl;
    cout << endl;
    cout << (*(p + 12) >> 1) << " " << (*(p + 12) & 1) << endl;
    cout << (*(p + 14) >> 1) << " " << (*(p + 14) & 1) << endl;
    cout << endl;
    cout << (*(p + 15) >> 1) << " " << (*(p + 15) & 1) << endl;
    cout << (*(p + 26) >> 1) << " " << (*(p + 26) & 1) << endl;
    cout << endl;
    cout << (*(p + 27) >> 1) << " " << (*(p + 27) & 1) << endl;
    p = mem->end - 1;
    cout << (*(p) >> 1) << " " << (*(p)&1) << endl;
}

void testCompactionCall() {
    createMem(136);
    initScope();
    Ptr p1 = createVar(Type::INT);
    Ptr p2 = createVar(Type::INT);
    ArrPtr arr1 = createArr(Type::INT, 8);
    Ptr p3 = createVar(Type::INT);
    Ptr p4 = createVar(Type::INT);
    ArrPtr arr2 = createArr(Type::INT, 6);
    freeElem(arr1);
    ArrPtr arr3 = createArr(Type::INT, 6);
    usleep(200 * 1000);
    // gc_run();
    int* ptr1 = symTable->getPtr(p1.addr >> 2);
    cout << "ptr1:" << ptr1 - mem->start << endl;
    int* ptr2 = symTable->getPtr(p2.addr >> 2);
    cout << "ptr2:" << ptr2 - mem->start << endl;
    int* ptr3 = symTable->getPtr(p3.addr >> 2);
    cout << "ptr3:" << ptr3 - mem->start << endl;
    int* ptr4 = symTable->getPtr(p4.addr >> 2);
    cout << "ptr4:" << ptr4 - mem->start << endl;
    int* arrptr2 = symTable->getPtr(arr2.addr >> 2);
    cout << "arrptr2: " << arrptr2 - mem->start << endl;
    int* arrptr3 = symTable->getPtr(arr3.addr >> 2);
    cout << "arrptr3: " << arrptr3 - mem->start << endl;
    cout << "Total free memory: " << mem->totalFreeMem << endl;
    cout << "Total free blocks: " << mem->totalFreeBlocks << endl;
    cout << "Biggest free block: " << mem->biggestFreeBlockSize << endl;
    endScope();
    usleep(100 * 1000);
    freeMem();
}

void testAssignArr() {
    createMem(1024 * 1024 * 512);  // 512 MB
    initScope();
    ArrPtr arr1 = createArr(Type::INT, 10);
    int arr[] = {1, 2, 3, 4, 5, 6, 7, 8, 9, 10};
    assignArr(arr1, arr, 10);
    cout << "arr1: " << arr1.addr << endl;
    for(int i = 0; i < 10; i++) {
        int val;
        cout << "arr[" << i << "]: ";
        getVar(arr1, i, &val);
        cout << val << endl;
    }
    // sleep(100);
    endScope();
    freeMem();
}

int main() {
    // // testSymbolTable();Type(Type::INT)
    // // testCreateVar();
    // testAssignVar();
    // testCode();
    // testCompaction();
    testCompactionCall();
    // testAssignArr();
    // ArrType a = ArrType(10, Type::INT);
    // Type t = a;
    // const ArrType& x = static_cast<const ArrType&>(t);
}

/**
 * TODO LIST:
 * Done 1. Write assignArr when an array needs to be copied
 * Done 2. Do locking in compaction
 * Done 3. Write a compaction initialisation scheme
 * 4. Write print statements as mentioned in the assignment pdf
 * 5. Write demo2 file and test everything together
 * 6. Write documentation
 * 7. Complete freeMem() without seg fault 
 */