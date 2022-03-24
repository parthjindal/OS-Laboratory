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

enum Type {
    INT,
    CHAR,
    MEDIUM_INT,
    BOOL,
    ARRAY
};

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

struct Ptr {
    Type type;
    int addr;
    int size;
    Ptr(const Type& _t, int _addr, int _size) : type(_t), addr(_addr), size(_size) {}
};

// valid, mark bit fields are stored as LSB's
struct Symbol {
    // word1 -> 31 bits for wordIdx, 1 bit for if symbol is allocated in symboltable memory
    // word2 -> 31 bits for offset, 1 bit for if symbol is in use (mark for garbage collection)
    unsigned int word1, word2;
};

#define MAX_SYMBOLS 1024
struct SymbolTable {
    unsigned int head, tail;
    Symbol symbols[MAX_SYMBOLS];
    int size;
    pthread_mutex_t mutex;
    void Init() {
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
    unsigned int alloc(unsigned int wordidx, unsigned int offset) {
        pthread_mutex_lock(&mutex);
        if (size == MAX_SYMBOLS) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }
        // DEBUG()
        cout << "Allocating symbol " << wordidx << ":" << offset << endl;
        unsigned int idx = head;
        head = (symbols[head].word2 & -2) >> 1;
        symbols[idx].word1 = (wordidx << 1) | 1;  // mark as allocated
        symbols[idx].word2 = (offset << 1) | 1;   // mark as in use
        size++;
        pthread_mutex_unlock(&mutex);
        return idx;  // local address
    }
    void free(unsigned int idx) {
        pthread_mutex_lock(&mutex);
        if (size == MAX_SYMBOLS) {
            head = tail = idx;
            symbols[idx].word1 = 0;
            symbols[idx].word2 = -2;  // sentinel
            size--;
            pthread_mutex_unlock(&mutex);
            return;
        }
        symbols[tail].word2 = idx << 1;
        symbols[idx].word1 = 0;
        symbols[idx].word2 = -2;  // sentinel
        tail = idx;
        size--;
        pthread_mutex_unlock(&mutex);
    }
    int getWordIdx(unsigned int idx) { return symbols[idx].word1 >> 1; }
    int getOffset(unsigned int idx) { return symbols[idx].word2 >> 1; }
};

SymbolTable* symTable;

#define MAX_STACK_SIZE 1024
struct Stack {
    int top;
    int _elems[MAX_STACK_SIZE];
    void Init() {
        top = -1;
    }
    void push(int elem) {
        _elems[top++] = elem;
    }
    int pop() {
        return _elems[--top];
    }
};

Stack* stack;

struct MemBlock {
    int *start, *end;
    int* mem;
    void Init(int _size) {
        int size = (((_size + 3) >> 2) << 2) + 8;  // align to 4 bytes
                                                   // and add 8 bytes for header, footer
        mem = (int*)malloc(size);
        start = mem;
        end = mem + (size >> 2);
        *start = (size >> 2) << 1;                      // 31 bits store size last bit for if free or not
        *(start + (size >> 2) - 1) = (size >> 2) << 1;  // footer
    }
    ~MemBlock() {
        free(mem);
    }
    int getMem(int size) {  // size in bytes (4 bytes aligned)
        int* p = start;
        int newsize = (((size + 3) >> 2) << 2) + 8;
        while ((p < end) &&
               ((*p & 1) ||
                ((*p << 1) < newsize)))
            p = p + (*p >> 1);
        if (p == end) {
            return -1;
        }
        addBlock((int*)p, newsize);
        return (p - start);
    }
    void addBlock(int* ptr, int size) {
        int oldsize = *ptr << 1;  // old size in bytes
        int words = size >> 2;
        *ptr = (words << 1) | 1;
        *(ptr + words - 1) = (words << 1) | 1;  // footer
        if (size < oldsize) {
            *(ptr + words) = (oldsize - size) >> 1;
            *(ptr + (oldsize >> 2) - 1) = (oldsize - size) >> 1;
        }
    }

    void freeBlock(int wordid) {
        int* ptr = start + wordid;
        int words = *ptr >> 1;
        *ptr = *ptr & -2;                // mark as free
        *(ptr + words - 1) = *ptr & -2;  // mark as free

        int* next = ptr + words;
        if (next != end && (*next & 1) == 0) {  // next is also free so coelesce
            words = words + (*next >> 1);
            *ptr = words << 1;                        // new size in words
            *(next + (*next >> 1) - 1) = words << 1;  // footer
        }
        if (ptr != start && (*(ptr - 1) & 1) == 0) {  // previous is also free so coelesce
            int prevwords = (*(ptr - 1) >> 1);
            *(ptr - prevwords) = (prevwords + words) << 1;  // new size in words
            words = words + prevwords;
            *(ptr + words - 1) = (prevwords + words) << 1;  // footer
        }
    }
};

MemBlock* mem = nullptr;
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
}

int translate(int local_addr) {
    return local_addr << 2;
}

Ptr createVar(const Type& t) {
    if (t == Type::ARRAY) {
        throw std::runtime_error("Array not supported using createVar, refer to createArray");
    }
    int _size = getSize(t);
    _size = (((_size + 3) >> 2) << 2);
    int wordid = mem->getMem(_size);
    if (wordid == -1)
        throw std::runtime_error("Out of memory");
    int local_addr = symTable->alloc(wordid, 0);
    return Ptr(t, translate(local_addr), getSize(t));
}

inline Type getType(const Ptr& p) {
    return p.type;
}

void printVar(const Ptr& p) {
    int local_addr = p.addr >> 2;  // TODO: write a function here
    int wordId = symTable->getWordIdx(local_addr);
    int offset = symTable->getOffset(local_addr);
    int* ptr = mem->start + wordId + 1;  // +1 for header
    if (p.type == Type::INT) {
        ptr = (int*)((char*)ptr + offset);
        printf("%d\n", *ptr);
    } else if (p.type == Type::BOOL) {
        bool* ptr2 = (bool*)((char*)ptr + offset);
        printf("%d\n", *ptr2);
    } else if (p.type == Type::CHAR) {
        char* ptr3 = (char*)((char*)ptr + offset);
        printf("%c\n", *ptr3);
    } else if (p.type == Type::MEDIUM_INT) {
        ptr = (int*)((char*)ptr + offset);
        int val = *ptr;
        if (val & (1 << 23)) {
            val = val | 0xFF000000;
        }
        printf("%d\n", val);
    } else
        throw std::runtime_error("Invalid type");
    // memcpy((void*)ptr, &val, sizeof(int));
}

void assignVar(const Ptr& p, int val) {
    if (getType(p) != Type::INT && getType(p) != Type::MEDIUM_INT)
        throw std::runtime_error("Assignment to non-int variable");
    int local_addr = p.addr >> 2;  // TODO: write a function here
    int wordId = symTable->getWordIdx(local_addr);
    int offset = symTable->getOffset(local_addr);
    int* ptr = mem->start + wordId + 1;  // +1 for header
    ptr = (int*)((char*)ptr + offset);
    memcpy((void*)ptr, &val, getSize(p.type));
}

void assignVar(const Ptr& p, bool f) {
    if (getType(p) != Type::BOOL)
        throw std::runtime_error("Assignment to non-bool variable");
    int local_addr = p.addr >> 2;  // TODO: write a function here
    int wordId = symTable->getWordIdx(local_addr);
    int offset = symTable->getOffset(local_addr);
    int* ptr = mem->start + wordId + 1;  // +1 for header
    ptr = (int*)((char*)ptr + offset);
    memcpy((void*)ptr, &f, sizeof(bool));
}

void assignVar(const Ptr& p, char c) {
    if (getType(p) != Type::CHAR)
        throw std::runtime_error("Assignment to non-char variable");
    int local_addr = p.addr >> 2;  // TODO: write a function here
    int wordId = symTable->getWordIdx(local_addr);
    int offset = symTable->getOffset(local_addr);
    int* ptr = mem->start + wordId + 1;  // +1 for header
    ptr = (int*)((char*)ptr + offset);
    memcpy((void*)ptr, &c, sizeof(char));
}

Ptr createArr(const Type& t, int width) {
    int _count = (1 << 2) / getSize(t);
    int _width = (width + _count - 1) / _count;  // round up
    int _size = _width << 2;
    int wordid = mem->getMem(_size);
    if (wordid == -1)
        throw std::runtime_error("Out of memory");
    int local_addr = symTable->alloc(wordid, 0);
    return Ptr(t, translate(local_addr), getSize(t) * _width);
}

void assignArr(const Ptr& p, int idx, int val) {
    if (getType(p) != Type::INT && getType(p) != Type::MEDIUM_INT)
        throw std::runtime_error("Assignment to non-int variable");
    int local_addr = p.addr >> 2;  // TODO: write a function here
    int wordId = symTable->getWordIdx(local_addr);
    int offset = symTable->getOffset(local_addr);
    int* ptr = mem->start + wordId + 1;  // +1 for header
    ptr = (int*)((char*)ptr + offset);
    ptr = (int*)((char*)ptr + (idx << 2));
    memcpy((void*)ptr, &val, getSize(p.type));
}

void assignArr(const Ptr& p, int vals[], int n) {
    if (getType(p) != Type::INT && getType(p) != Type::MEDIUM_INT)
        throw std::runtime_error("Assignment to non-int variable");
    int local_addr = p.addr >> 2;  // TODO: write a function here
    int wordId = symTable->getWordIdx(local_addr);
    int offset = symTable->getOffset(local_addr);
    int* ptr = mem->start + wordId + 1;  // +1 for header
    ptr = (int*)((char*)ptr + offset);
    for (int i = 0; i < n; i++) {
        ptr = (int*)((char*)ptr + (i << 2));
        memcpy((void*)ptr, &vals[i], getSize(p.type));
    }
}

int getVar(const Ptr& p, int idx) {
    if (getType(p) !=)
}

// void assignVar(const Ptr& p) {
// }

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
    cout << "here" << endl;
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
    Ptr p1 = createVar(Type(Type::INT));
    Ptr p2 = createVar(Type(Type::BOOL));
    Ptr p3 = createVar(Type(Type::CHAR));
    cout << p1.addr << endl;
    cout << p2.addr << endl;
    cout << p3.addr << endl;
}

void testAssignVar() {
    createMem(1024 * 1024);
    Ptr p1 = createVar(Type(Type::INT));
    Ptr p2 = createVar(Type(Type::BOOL));
    Ptr p3 = createVar(Type(Type::MEDIUM_INT));
    Ptr p4 = createVar(Type(Type::CHAR));
    cout << p1.addr << " " << p2.addr << " " << p3.addr << " " << p4.addr
         << endl;
    assignVar(p1, 123);
    assignVar(p2, false);
    assignVar(p3, INT32_MAX);
    assignVar(p4, 'z');
    printVar(p1);
    printVar(p2);
    printVar(p3);
    printVar(p4);
}

int main() {
    // // testSymbolTable();Type(Type::INT)
    // // testCreateVar();
    testAssignVar();
    // ArrType a = ArrType(10, Type::INT);
    // Type t = a;
    // const ArrType& x = static_cast<const ArrType&>(t);
    // cout << x.width << endl;
}

/**
 * TODO LIST:
 * 1. Write garbage collector and compaction
 * 2.
 *
 */