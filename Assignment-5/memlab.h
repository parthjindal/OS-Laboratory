#ifndef _MEM_LAB_H
#define _MEM_LAB_H

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

#define GC_PERIOD_MS 100
#define MAX_SYMBOLS 1024
#define MAX_STACK_SIZE 1024

enum Type {
    INT,
    CHAR,
    MEDIUM_INT,
    BOOL,
    ARRAY
};

struct Ptr {
    Type type;
    int addr;
    Ptr(const Type& _t, int _addr) : type(_t), addr(_addr) {}
};

struct ArrPtr : public Ptr {
    int width;
    ArrPtr(const Type& t, int _addr, int _width) : Ptr(t, _addr), width(_width) {}
};

// valid, mark bit fields are stored as LSB's
struct Symbol {
    // word1 -> 31 bits for wordIdx, 1 bit for if symbol is allocated in symboltable memory
    // word2 -> 31 bits for offset, 1 bit for if symbol is in use (mark for garbage collection)
    unsigned int word1, word2;
};

struct SymbolTable {
    unsigned int head, tail;
    Symbol symbols[MAX_SYMBOLS];
    int size;
    pthread_mutex_t mutex;
    SymbolTable();
    ~SymbolTable();
    int alloc(unsigned int wordidx, unsigned int offset);
    void free(unsigned int idx);
    inline int getWordIdx(unsigned int idx) { return symbols[idx].word1 >> 1; }
    inline int getOffset(unsigned int idx) { return symbols[idx].word2 >> 1; }
    inline void setMarked(unsigned int idx) { symbols[idx].word2 |= 1; }     // mark as in use
    inline void setUnmarked(unsigned int idx) { symbols[idx].word2 &= -2; }  // mark as free
    inline void setAllocated(unsigned int idx) { symbols[idx].word1 |= 1; }  // mark as allocated
    inline void setUnallocated(unsigned int idx) { symbols[idx].word1 &= -2; }
    inline bool isMarked(unsigned int idx) { return symbols[idx].word2 & 1; }
    inline bool isAllocated(unsigned int idx) { return symbols[idx].word1 & 1; }
    int* getPtr(unsigned int idx);
};

struct Stack {
    int _top;
    int _elems[MAX_STACK_SIZE];
    Stack();
    void push(int elem);
    int pop();
    int top();
};

struct MemBlock {
    int *start, *end;
    int* mem;
    int totalFreeMem;
    int totalFreeBlocks;
    int biggestFreeBlockSize;
    pthread_mutex_t mutex;
    void Init(int _size);
    ~MemBlock();
    int getMem(int size);
    void addBlock(int* ptr, int size);
    void freeBlock(int wordid);
};

int getSize(const Type& type);
void createMem(int size);
Ptr createVar(const Type& t);
void getVar(const Ptr& p, void* val);
void assignVar(const Ptr& p, int val);
void assignVar(const Ptr& p, bool f);
void assignVar(const Ptr& p, char c);
ArrPtr createArr(const Type& t, int width);
void initScope();
void endScope();
void freeElem(const Ptr& p);
void* garbageCollector(void*);
void assignArr(const ArrPtr& p, int idx, int val);
void assignArr(const ArrPtr& p, int idx, char c);
void assignArr(const ArrPtr& p, int idx, bool f);
void getVar(const ArrPtr& p, int idx, void* _mem);
void freeMem();
void gcActivate();

#endif  // _MEM_LAB_H