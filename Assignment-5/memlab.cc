#include "memlab.h"

#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

#include "debug.h"
#include "medium_int.h"
using namespace std;

bool gc_active = false;
pthread_t gcThread;
sem_t sem_gc;

MemBlock* mem = nullptr;
Stack* stack = nullptr;
SymbolTable* symTable = nullptr;

#if DEBUG_LEVEL >= _INFO_L
#define GC_LOG
#endif

#ifdef GC_LOG
FILE* logfile;
#endif

inline int translate2La(int local_addr) {
    return local_addr << 2;
}
inline int translate2Idx(int local_addr) {
    return local_addr >> 2;
}

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

void debugPrint(FILE* fp) {
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* p = mem->start;
    while (p < mem->end) {
        fprintf(fp, "%d, %d, %d\n", (int)(p - mem->start) << 2, ((int)(p - mem->start) << 2) + ((*p >> 1) << 2), *p & 1);
        p = p + (*p >> 1);
    }
    fprintf(fp, "total free memory: %d, biggest free hole: %d\n", mem->totalFreeMem, mem->biggestFreeBlockSize);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Returns the word index offset for an array
 *        of type T in logical memory from base
 * @param t: Base type of the array
 * @param idx: Index of the array
 * @return int: Word index offset
 */
int getWordForIdx(Type t, int idx) {
    int wordsize = t == Type::BOOL ? 32 : 4;
    int _count = wordsize / getSize(t);
    int _idx = idx / _count;
    return _idx;
}

/**
 * @brief Returns the byte/bit level offset for an array
 *        of type T in logical memory from base
 *
 * @param t: Base type of the array
 * @param idx: Index of the array
 * @return int: Byte/bit level offset
 */
int getOffsetForIdx(Type t, int idx) {
    int wordsize = t == Type::BOOL ? 32 : 4;
    int _count = wordsize / getSize(t);
    int _idx = idx / _count;
    return (idx - _idx * _count) * getSize(t);
}

/**
 * @brief Construct a new Symbol Table:: Symbol Table object
 * @param _size: size of the symbol table
 */
SymbolTable::SymbolTable(int _size) : size(0), head(0), tail(_size - 1), capacity(_size) {
    symbols = new Symbol[capacity];
    for (int i = 0; i < capacity; i++) {
        symbols[i].word1 = 0;
        symbols[i].word2 = ((i + 1)) << 1;
    }
    symbols[tail].word2 = -2;  // mark end of free list
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
    pthread_mutex_init(&mutex, &attr);
    LOG("SymbolTable", _COLOR_BLUE, "Created SymbolTable with size %d\n", _size);
}

/**
 * @brief Destroy the Symbol Table object and the mutex
 */
SymbolTable::~SymbolTable() {
    delete[] symbols;
    pthread_mutex_destroy(&mutex);
}

/**
 * @brief Allocates an entry in the symbol table and fills it with the given data
 *
 * @param wordidx: word index in the logic memory
 * @param offset: offset (byte-level) in the word
 * @return int: index of the allocated entry
 */
int SymbolTable::alloc(unsigned int wordidx, unsigned int offset) {
    if (size == capacity) {
        return -1;
    }
    unsigned int idx = head;
    head = (symbols[head].word2 & -2) >> 1;
    symbols[idx].word1 = (wordidx << 1) | 1;  // mark as allocated
    symbols[idx].word2 = (offset << 1) | 1;   // mark as in use
    size++;
    LOG("SymbolTable", _COLOR_BLUE, "Alloc symbol: %d at address: %d\n", idx, translate2La(wordidx) | offset);
    return idx;
}

/**
 * @brief Frees the given symbol if allocated else throws an exception
 * @throws std::exception: if the symbol is not allocated throw a runtime error
 */
void SymbolTable::free(unsigned int idx) {
    if (!isAllocated(idx)) {
        throw std::runtime_error("SymbolTable::free: symbol not allocated");
    }
    unsigned int wordidx = getWordIdx(idx);
    unsigned int offset = getOffset(idx);
    if (size == capacity) {
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
    LOG("SymbolTable", _COLOR_BLUE, "Freed symbol: %d at address: %d\n", idx, translate2La(wordidx) | offset);
}

/**
 * @brief Returns the pointer to the logical address of the symbol in the main memory
 *
 * @param idx: index of the symbol
 * @return int*: pointer to the logical address
 */
int* SymbolTable::getPtr(unsigned int idx) {
    int wordidx = getWordIdx(idx);
    int offset = getOffset(idx);
    int* ptr = mem->start + wordidx + 1;  // +1 to skip the header word
    ptr = (int*)((char*)ptr + offset);
    return ptr;
}

Stack::Stack(int size) : _top(-1), capacity(size) {
    _elems = new int[capacity];
}
Stack::~Stack() {
    delete[] _elems;
}
void Stack::push(int elem) {
    if (_top == capacity - 1) {
        throw std::runtime_error("Stack::push: stack overflow");
    }
    _elems[++_top] = elem;
    LOG("Stack", _COLOR_BLUE, "Pushed %d\n", elem);
}

int Stack::pop() {
    LOG("Stack", _COLOR_BLUE, "Popped %d\n", _elems[_top]);
    return _elems[_top--];
}

int Stack::top() { return _elems[_top]; }

/**
 * @brief Creates a memory block of given size (4 byte aligned)
 * @param _size: size of the memory block in bytes
 */
void MemBlock::Init(int _size) {
    int size = (((_size + 3) >> 2) << 2);  // align to 4 bytes
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
    LOG("MemBlock", _COLOR_BLUE, "Created Memory block with size %d bytes\n", size);
}

/**
 * @brief Destroy the Mem Block object, free the memory and the mutex
 *
 */
MemBlock::~MemBlock() {
    free(start);
    pthread_mutex_destroy(&mutex);
    LOG("MemBlock", _COLOR_BLUE, "Destroyed Memory block\n");
}

/**
 * @brief Find the first free block of size >= input_size
 *        and returns word-level offset from base pointer
 *
 * @param size: size of the free block required in bytes
 * @return int: word-level offset from base pointer
 */
int MemBlock::getMem(int size) {
    int* p = start;
    int newsize = (((size + 3) >> 2) << 2) + 8;  // align to 4 bytes + 8 bytes for header and footer
    // find the first free block of size >= newsize
    while ((p < end) &&
           ((*p & 1) ||
            ((*p << 1) < newsize))) {
        p = p + (*p >> 1);
    }
    // if no free block found, return -1
    if (p == end) {
        return -1;
    }
    // if free block found, split it into two blocks (allocate and free) if possible
    splitBlock((int*)p, newsize);
#ifdef GC_LOG
    fprintf(logfile, "%ld\n", ((end - start) - totalFreeMem));
#endif
    LOG("MemBlock", _COLOR_BLUE, "Alloc %d bytes at address: %d\n", newsize, (int)(p - start) << 2);
    return (p - start);
}

/**
 * @brief Splits a free block with first block allocated and second block free (if possible)
 *
 * @param ptr: pointer to the free block
 * @param size: size of the allocated block
 */
void MemBlock::splitBlock(int* ptr, int size) {
    int oldsize = *ptr << 1;  // old size in bytes
    int words = size >> 2;
    *ptr = (words << 1) | 1;
    *(ptr + words - 1) = (words << 1) | 1;  // footer
    if (size < oldsize) {
        *(ptr + words) = (oldsize - size) >> 1;
        *(ptr + (oldsize >> 2) - 1) = (oldsize - size) >> 1;
    }
    LOG("MemBlock", _COLOR_BLUE, "Split block at address: %d\n", (int)(ptr - start) << 2);
    // book keeping for compaction
    totalFreeMem -= words;
    if (size == oldsize) {
        totalFreeBlocks--;
    }
    if ((oldsize >> 2) == biggestFreeBlockSize) {
        biggestFreeBlockSize -= words;
    }
    biggestFreeBlockSize = max(biggestFreeBlockSize, totalFreeMem / (totalFreeBlocks + 1));
}

/**
 * @brief Free's the memory block at the given word-level offset,
 *        coalescing if possible at previous and next blocks
 *
 * @param wordid: word-level offset from base pointer
 */
void MemBlock::freeBlock(int wordid) {
    int* ptr = start + wordid;
    int words = *ptr >> 1;
    int orig_words = words;
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
#ifdef GC_LOG
    fprintf(logfile, "%ld\n", ((end - start) - totalFreeMem));
#endif
    LOG("MemBlock", _COLOR_BLUE, "Freed %d bytes at address: %d\n", orig_words << 2, wordid << 2);
}

/**
 * @brief Allocates heap-memory of given size
 *
 * @param size: size of the memory to be allocated
 * @param gc: if true, garbage collector is created
 */
void createMem(int size, bool gc) {
    if (mem != nullptr)
        throw std::runtime_error("Memory already created");
    mem = new MemBlock();
    mem->Init(size * EFFEC_MEM_RATIO);
    int symtable_size = min((1 << 15), (int)((size * EFFEC_MEM_RATIO) + 11) / 12);
    symTable = new SymbolTable(symtable_size);
    stack = new Stack(symtable_size);
    string fname = gc ? "gc" : "non_gc";
#ifdef GC_LOG
    logfile = fopen((fname + ".csv").c_str(), "w");
    fprintf(logfile, "%s\n", fname.c_str());
#endif
    if (gc) {
        sem_init(&sem_gc, 0, 0);  // initialize semaphore for garbage collector
                                  // thread to perform initializations
        signal(SIGUSR1, SIG_IGN);
        signal(SIGUSR2, SIG_IGN);
        int ret = pthread_create(&gcThread, nullptr, garbageCollector, nullptr);
        if (ret != 0) {
            throw std::runtime_error("Error creating garbage collector thread");
        }
        sem_wait(&sem_gc);
        LOG("createMem", _COLOR_BLUE, "Garbage collector thread created\n");
        gc_active = true;
    }
}

/**
 * @brief Create an object of given Type t and returns a Ptr struct object
 *
 * @param t: type of the object to be created
 * @return Ptr: Ptr to the created object
 */
Ptr createVar(const Type& t) {
    int _size = getSize(t);
    _size = (((_size + 3) >> 2) << 2);
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int wordid = mem->getMem(_size);
    if (wordid == -1) {
        PTHREAD_MUTEX_LOCK(&symTable->mutex);
        compactMem();
        PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
        wordid = mem->getMem(_size);
        if (wordid == -1)
            throw std::runtime_error("Out of memory");
    }
    PTHREAD_MUTEX_LOCK(&symTable->mutex);
    int local_addr = symTable->alloc(wordid, 0);
    if (local_addr == -1)
        throw std::runtime_error("Out of memory in symbol table");
    LOG("createVar", _COLOR_BLUE, "Created variable at local address: %d\n", translate2La(local_addr));
    PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
    stack->push(local_addr);
    return Ptr(t, translate2La(local_addr));
}

/**
 * @brief Creates an array object of baseType t and size: width and returns a Ptr struct object
 *
 * @param t: Base type of the array
 * @param width: size of the array
 * @return ArrPtr: Ptr to the created array
 */
ArrPtr createArr(const Type& t, int width) {
    int wordsize = t == Type::BOOL ? 32 : 4;
    int _count = wordsize / getSize(t);
    int _width = (width + _count - 1) / _count;  // round up
    int _size = _width << 2;
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int wordid = mem->getMem(_size);
    if (wordid == -1) {
        // In case of out of memory, try and compact the memory, if that also fails, throw exception
        PTHREAD_MUTEX_LOCK(&symTable->mutex);
        compactMem();
        PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
        wordid = mem->getMem(_size);
        if (wordid == -1)
            throw std::runtime_error("Out of memory");
    }
    PTHREAD_MUTEX_LOCK(&symTable->mutex);
    int local_addr = symTable->alloc(wordid, 0);
    if (local_addr == -1)
        throw std::runtime_error("Out of memory in symbol table");
    LOG("createArr", _COLOR_BLUE, "Created array at local address: %d\n", translate2La(local_addr));
    PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
    stack->push(local_addr);
    return ArrPtr(t, translate2La(local_addr), width);
}

/**
 * @brief Get the value of the object pointed by the Ptr and stores it in val
 *
 * @param[in] p: Ptr to the object
 * @param[out] val: storing the value of the object
 */
void getVar(const Ptr& p, void* val) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    int temp = *(int*)ptr;
    LOG("getVar", _COLOR_BLUE, "Copying 4 bytes from memory at logical address: %ld\n", (ptr - mem->start) << 2);
    if (p.type == Type::MEDIUM_INT) {
        if (temp & (1 << 23)) {
            temp = temp | 0xFF000000;
        }
    }
    memcpy(val, &temp, getSize(p.type));
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Get the value of the array pointed by the ArrPtr and stores it in val
 *
 * @param p: ArrPtr to the array
 * @param idx: index of the array
 * @param val: storing the value of the array
 */
void getVar(const ArrPtr& p, int idx, void* val) {
    int local_addr = p.addr >> 2;
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (idx < 0 || idx >= p.width)
        throw std::runtime_error("Index out of bounds");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    int word = getWordForIdx(p.type, idx);
    int offset = getOffsetForIdx(p.type, idx);
    ptr = ptr + word;
    int temp = *ptr;
    LOG("getVar", _COLOR_BLUE, "Copying 4 bytes from memory at logical address: %ld\n", (ptr - mem->start) << 2);
    if (p.type == Type::BOOL) {
        bool b = temp & (1 << offset);
        memcpy(val, &b, 1);
    } else
        memcpy(val, (char*)&temp + offset, getSize(p.type));
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign the value of val to the object pointed by the Ptr
 *
 * @param p: Ptr to the object
 * @param val: value to be assigned
 */
void assignVar(const Ptr& p, int val) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::INT)
        throw std::runtime_error("Assignment to non-int variable");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    memcpy((void*)ptr, &val, 4);
    LOG("assignVar", _COLOR_BLUE, "Assigned 4 bytes to memory at logical address: %ld\n", (ptr - mem->start) << 2);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign the value of val to the object pointed by the Ptr
 *
 * @param p: Ptr to the object
 * @param val: value to be assigned
 */
void assignVar(const Ptr& p, medium_int val) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::MEDIUM_INT)
        throw std::runtime_error("Assignment to non-int variable");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    int temp = val.to_int();
    memcpy((void*)ptr, &temp, 4);
    LOG("assignVar", _COLOR_BLUE, "Assigned 4 bytes to memory at logical address: %ld\n", (ptr - mem->start) << 2);

    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign the value of val to the object pointed by the Ptr
 *
 * @param p: Ptr to the object
 * @param val: value to be assigned
 */
void assignVar(const Ptr& p, bool f) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::BOOL)
        throw std::runtime_error("Assignment to non-bool variable");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    int temp = f ? 1 : 0;
    memcpy((void*)ptr, &temp, 4);
    LOG("assignVar", _COLOR_BLUE, "Assigned 4 bytes to memory at logical address: %ld\n", (ptr - mem->start) << 2);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign the value of val to the object pointed by the Ptr
 *
 * @param p: Ptr to the object
 * @param val: value to be assigned
 */
void assignVar(const Ptr& p, char c) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::CHAR)
        throw std::runtime_error("Assignment to non-char variable");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    int temp = c;
    memcpy((void*)ptr, &temp, 4);
    LOG("assignVar", _COLOR_BLUE, "Assigned 4 bytes to memory at logical address: %ld\n", (ptr - mem->start) << 2);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign the value of val to an index of the array pointed by the ArrPtr
 *
 * @param p: ArrPtr to the array
 * @param idx: index of the array
 * @param val: value to be assigned
 */
void assignArr(const ArrPtr& p, int idx, int val) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::INT)
        throw std::runtime_error("Assignment to non-int array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    LOG("assignArr", _COLOR_BLUE, "Assigned 4 bytes to memory at logical address: %ld\n", (ptr - mem->start) << 2);
    int word = getWordForIdx(p.type, idx);
    int offset = getOffsetForIdx(p.type, idx);
    ptr = (int*)((char*)ptr + word * 4 + offset);
    memcpy((void*)ptr, &val, 4);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign the value of val to an index of the array pointed by the ArrPtr
 *
 * @param p: ArrPtr to the array
 * @param idx: index of the array
 * @param val: value to be assigned
 */
void assignArr(const ArrPtr& p, int idx, medium_int val) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::MEDIUM_INT)
        throw std::runtime_error("Assignment to non-medium-int array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    LOG("assignArr", _COLOR_BLUE, "Assigned 4 bytes to memory at logical address: %ld\n", (ptr - mem->start) << 2);
    int word = getWordForIdx(p.type, idx);
    int offset = getOffsetForIdx(p.type, idx);
    ptr = (int*)((char*)ptr + word * 4 + offset);
    int temp = val.to_int();
    memcpy((void*)ptr, &temp, 4);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign the value of val to an index of the array pointed by the ArrPtr
 *
 * @param p: ArrPtr to the array
 * @param idx: index of the array
 * @param val: value to be assigned
 */
void assignArr(const ArrPtr& p, int idx, char c) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::CHAR)
        throw std::runtime_error("Assignment to non-char array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    LOG("assignArr", _COLOR_BLUE, "Assigned 4 bytes to memory at logical address: %ld\n", (ptr - mem->start) << 2);
    int word = getWordForIdx(p.type, idx);
    int offset = getOffsetForIdx(p.type, idx);
    ptr = ptr + word;
    int temp = *ptr;
    memcpy((char*)&temp + offset, &c, 1);
    memcpy((void*)ptr, &temp, 4);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign the value of val to an index of the array pointed by the ArrPtr
 *
 * @param p: ArrPtr to the array
 * @param idx: index of the array
 * @param val: value to be assigned
 */
void assignArr(const ArrPtr& p, int idx, bool f) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::BOOL)
        throw std::runtime_error("Assignment to non-bool array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    LOG("assignArr", _COLOR_BLUE, "Assigned 4 bytes to memory at logical address: %ld\n", (ptr - mem->start) << 2);
    int word = getWordForIdx(p.type, idx);
    int offset = getOffsetForIdx(p.type, idx);
    ptr = ptr + word;
    int temp = *ptr;
    temp = temp & ~(1 << offset);
    temp = temp | (f << offset);
    memcpy((void*)ptr, &temp, 4);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign values of arr from 0 to n-1, to the array pointed by the ArrPtr
 *
 * @param p: ArrPtr to the array
 * @param arr: array of values to be assigned
 * @param n: size of the array
 */
void assignArr(const ArrPtr& p, int arr[], int n) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::INT)
        throw std::runtime_error("Assignment to non-int array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    for (int i = 0; i < n; i++) {
        int word = getWordForIdx(p.type, i);
        int offset = getOffsetForIdx(p.type, i);
        int* ptr_temp = (int*)((char*)ptr + word * 4 + offset);
        memcpy((void*)ptr_temp, &arr[i], 4);
    }
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign values of arr from 0 to n-1, to the array pointed by the ArrPtr
 *
 * @param p: ArrPtr to the array
 * @param arr: array of values to be assigned
 * @param n: size of the array
 */
void assignArr(const ArrPtr& p, medium_int arr[], int n) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::MEDIUM_INT)
        throw std::runtime_error("Assignment to non-medium-int array");
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    for (int i = 0; i < n; i++) {
        int word = getWordForIdx(p.type, i);
        int offset = getOffsetForIdx(p.type, i);
        int* ptr_temp = (int*)((char*)ptr + word * 4 + offset);
        memcpy((void*)ptr_temp, &arr[i].data, 4);
    }
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign values of arr from 0 to n-1, to the array pointed by the ArrPtr
 *
 * @param p: ArrPtr to the array
 * @param arr: array of values to be assigned
 * @param n: size of the array
 */
void assignArr(const ArrPtr& p, char arr[], int n) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::CHAR)
        throw std::runtime_error("Assignment to non-char array");

    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    for (int i = 0; i < n; i++) {
        int word = getWordForIdx(p.type, i);
        int offset = getOffsetForIdx(p.type, i);
        int* ptr_temp = ptr + word;
        int temp = *ptr_temp;
        memcpy((char*)&temp + offset, &arr[i], 1);
        memcpy(ptr_temp, &temp, 4);
    }
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

/**
 * @brief Assign values of arr from 0 to n-1, to the array pointed by the ArrPtr
 *
 * @param p: ArrPtr to the array
 * @param arr: array of values to be assigned
 * @param n: size of the array
 */
void assignArr(const ArrPtr& p, bool arr[], int n) {
    int local_addr = translate2Idx(p.addr);
    if (!(symTable->isAllocated(local_addr) && symTable->isMarked(local_addr)))
        throw std::runtime_error("Variable not in symbol table");
    if (p.type != Type::BOOL)
        throw std::runtime_error("Assignment to non-bool array");

    PTHREAD_MUTEX_LOCK(&mem->mutex);
    int* ptr = symTable->getPtr(local_addr);
    for (int i = 0; i < n; i++) {
        int word = getWordForIdx(p.type, i);
        int offset = getOffsetForIdx(p.type, i);
        int* ptr_temp = ptr + word;
        int temp = *ptr_temp;
        temp = temp & ~(1 << offset);
        temp = temp | (arr[i] << offset);
        memcpy((void*)ptr_temp, &temp, 4);
    }
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

// marker for start of scope
void initScope() {
    LOG("initScope", _COLOR_BLUE, "Initializing scope\n");
    stack->push(-1);
}

// pop elements from stack until -1
void endScope() {
    LOG("endScope", _COLOR_BLUE, "Ending scope");
    while (stack->top() != -1) {
        int local_addr = stack->pop();
        PTHREAD_MUTEX_LOCK(&symTable->mutex);
        if (symTable->isAllocated(local_addr)) {
            LOG("Endscope", _COLOR_BLUE, "Popping local variable at address, unmarking for GC: %d", translate2La(local_addr));
            symTable->setUnmarked(local_addr);
        }
        PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    }
    stack->pop();  // pop -1
}

void _freeElem(int local_addr) {
    int wordId = symTable->getWordIdx(local_addr);
    mem->freeBlock(wordId);
    symTable->free(local_addr);
}

/**
 * @brief Free the memory allocated to the variable pointed by the Ptr
 *
 * @param p: Ptr to the variable
 */
void freeElem(const Ptr& p) {
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    PTHREAD_MUTEX_LOCK(&symTable->mutex);
    int local_addr = translate2Idx(p.addr);
    if (symTable->isAllocated(local_addr)) {
        LOG("FreeElem", _COLOR_BLUE, "Freeing variable at address %d", p.addr);
        _freeElem(local_addr);
    } else {
        throw std::runtime_error("double free called");
    }
    PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void calcOffset() {
    int* p = mem->start;
    int offset = 0;
    while (p < mem->end) {
        if ((*p & 1) == 0) {
            offset += (*p >> 1);
        } else {
            *(p + (*p >> 1) - 1) = (((p - offset) - mem->start) << 1) | 1;
        }
        p = p + (*p >> 1);
    }
    LOG("Garbage Collector", _COLOR_GREEN, "Compaction: Updating memory offsets\n");
}

void updateSymbolTable() {
    for (int i = 0; i < symTable->capacity; i++) {
        if (symTable->isAllocated(i)) {
            int* p = symTable->getPtr(i) - 1;
            int newWordId = *(p + (*p >> 1) - 1) >> 1;
            symTable->symbols[i].word1 = (newWordId << 1) | 1;
        }
    }
    LOG("Garbage Collector", _COLOR_GREEN, "Compaction: Updating symbol table with new logical address\n");
}

void compactMem() {
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
    LOG("Garbage Collector", _COLOR_GREEN, "Compaction: Compact memory complete\n");
    p = mem->start;
    while (p < mem->end) {
        *(p + (*p >> 1) - 1) = *p;
        p = p + (*p >> 1);
    }
    mem->biggestFreeBlockSize = mem->totalFreeMem;
    mem->totalFreeBlocks = 1;
}

void gc_run() {
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    PTHREAD_MUTEX_LOCK(&symTable->mutex);
    for (int i = 0; i < symTable->capacity; i++) {
        if (symTable->isAllocated(i) && !symTable->isMarked(i)) {
            LOG("Garbage Collector", _COLOR_GREEN, "Collecting out of scope variable at addr %d\n", translate2La(i));
            _freeElem(i);
        }
    }
    mem->totalFreeMem = max(mem->totalFreeMem, 1);
    double free_ratio = (double)mem->totalFreeMem / (double)(mem->biggestFreeBlockSize);
    if (free_ratio >= COMPACT_THRESHOLD) {
        LOG("Garbage Collector", _COLOR_GREEN, "Free ratio: %f, compacting memory\n", free_ratio);
        compactMem();
    }
    PTHREAD_MUTEX_UNLOCK(&symTable->mutex);
    PTHREAD_MUTEX_UNLOCK(&mem->mutex);
}

void handlSigUSR1(int sig) {
    gc_run();
}
void handleSigUSR2(int sig) {
    pthread_exit(0);
}

void* garbageCollector(void*) {
    signal(SIGUSR1, handlSigUSR1);
    signal(SIGUSR2, handleSigUSR2);
    sem_post(&sem_gc);
    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGUSR1);
    while (true) {
        usleep(GC_PERIOD_US);
        pthread_sigmask(SIG_BLOCK, &set, NULL);
        gc_run();
        pthread_sigmask(SIG_UNBLOCK, &set, NULL);
    }
    pthread_exit(0);
}

void gcActivate() {
    LOG("Garbage Collector", _COLOR_GREEN, "Signalled garbage collector\n");
    if (gc_active)
        pthread_kill(gcThread, SIGUSR1);
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
    SymbolTable* symtable = new SymbolTable(1024 * 1024);
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
    assignVar(p3, medium_int(-123));
    assignVar(p4, 'z');
    int val;
    getVar(p1, &val);
    cout << val << endl;
    bool f;
    getVar(p2, &f);
    cout << f << endl;
    medium_int val2;
    getVar(p3, &val2.data);
    cout << val2 << endl;
    char c;
    getVar(p4, &c);
    cout << c << endl;
    freeMem();
}

void freeMem() {
    PTHREAD_MUTEX_LOCK(&mem->mutex);
    PTHREAD_MUTEX_LOCK(&symTable->mutex);
    if (gc_active) {
        pthread_kill(gcThread, SIGUSR2);
        pthread_join(gcThread, nullptr);
        gc_active = false;
        sem_destroy(&sem_gc);
    }
    delete mem;
    delete symTable;
    delete stack;
    mem = NULL;
    symTable = NULL;
    stack = NULL;
#ifdef GC_LOG
    fclose(logfile);
#endif
    // cout << "Done" << endl;
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
    usleep(100 * 1000);
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
    ArrPtr arr1 = createArr(Type::MEDIUM_INT, 10);
    // int arr[] = {1, 2, 3, 4, 5, 6, -7, -8, -9, -10};
    // char arr[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j'};
    medium_int arr[] = {1, 2, 3, 4, 5, 6, -7, -8, -9, -10};
    assignArr(arr1, arr, 10);
    cout << "arr1: " << arr1.addr << endl;
    for (int i = 0; i < 10; i++) {
        medium_int val;
        cout << "arr[" << i << "]: ";
        getVar(arr1, i, &val);
        cout << val << endl;
    }
    freeElem(arr1);
    usleep(150 * 1000);
    int* p = mem->start;
    ArrPtr arr2 = createArr(Type::MEDIUM_INT, 10);
    for (int i = 0; i < 10; i++) {
        assignArr(arr2, i, arr[i]);
    }
    cout << "Array 2 starts here " << endl;
    for (int i = 0; i < 10; i++) {
        medium_int val;
        cout << "arr[" << i << "]: ";
        getVar(arr2, i, &val);
        cout << val << endl;
    }
    // sleep(100);
    endScope();
    sleep(1);
    freeMem();
}

// int main() {
//     testAssignVar();
//     testAssignArr();
// }

void test2() {
    createMem(100, false);
    initScope();
    ArrPtr p1 = createArr(Type::INT, 10);  // 48 bytes
    Ptr p2 = createVar(Type::INT);         // 12 bytes
    Ptr p3 = createVar(Type::INT);         // 12 bytes
}