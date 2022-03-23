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

class Type {
   public:
    enum TypeEnum {
        INT,
        CHAR,
        MEDIUM_INT,
        BOOL,
        ARRAY
    };
    TypeEnum type;
    Type(TypeEnum _type) : type(_type) {}
    Type(const Type& other) : type(other.type) {}
    virtual ~Type() {}
};

class ArrType : public Type {
   public:
    Type base;
    int width;
    ArrType(int _width, const Type::TypeEnum& _base) : Type(Type::ARRAY), base(_base), width(_width) {}
};

int getSize(const Type& type) {
    switch (type.type) {
        case Type::INT:
            return 4;
        case Type::CHAR:
            return 1;
        case Type::MEDIUM_INT:
            return 2;
        case Type::BOOL:
            return 1;
        case Type::ARRAY: {
            const ArrType& arr = static_cast<const ArrType&>(type);
            return arr.width * getSize(arr.base);
        }
        default:
            return 0;
    }
}

struct Ptr {
    Type t;
    int addr;
    Ptr(const Type& _t, int _addr) : t(_t), addr(_addr) {}
};

// valid, mark bit fields are stored as LSB's
struct Symbol {
    // word1 -> 31 bits for wordIdx, 1 bit for if symbol is allocated in symboltable memory
    // word2 -> 31 bits for offset, 1 bit for if symbol is in use (mark for garbage collection)
    unsigned int word1, word2;
};

#define MAX_SYMBOLS 1024
struct SymbolTable {
    int head, tail;
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
            symbols[i].word2 = ((i + 1) * sizeof(Symbol)) << 1;
        }
        symbols[tail].word2 = -2;  // mark end of free list
        pthread_mutexattr_t attr;
        pthread_mutexattr_init(&attr);
        pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_ERRORCHECK_NP);
        pthread_mutex_init(&mutex, &attr);
    }
    int alloc(int wordidx, int offset) {
        pthread_mutex_lock(&mutex);
        if (size == MAX_SYMBOLS) {
            pthread_mutex_unlock(&mutex);
            return -1;
        }
        int idx = head;
        head = symbols[head].word2 & -2;
        symbols[idx].word1 = (wordidx << 1) | 1;  // mark as allocated
        symbols[idx].word2 = (offset << 1) | 1;   // mark as in use
        size++;
        pthread_mutex_unlock(&mutex);
        return idx;  // local address
    }
    void free(int idx) {
        pthread_mutex_lock(&mutex);
        symbols[tail].word2 = idx << 1;
        symbols[idx].word1 = 0;
        symbols[idx].word2 = -2;
        tail = idx;
        size--;
        pthread_mutex_unlock(&mutex);
    }
    int getWordIdx(int idx) { return symbols[idx].word1 >> 1; }
    int getOffset(int idx) { return symbols[idx].word2 >> 1; }
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

Ptr createVar(const Type& t) {
    int _size = getSize(t);
    _size = (((_size + 3) >> 2) << 2) + 8;
    int wordid = mem->getMem(_size);
    if (wordid == -1)
        throw std::runtime_error("Out of memory");
    int local_addr = symTable->alloc(wordid, 0);
    return Ptr(t, local_addr);
}

void assignVar(const Ptr& p, int val) {
    if (p.t.type != Type::INT)
        throw std::runtime_error("Assignment to non-int variable");
    int local_addr = p.addr;
    int wordId = symTable->getWordIdx(local_addr);
    int offset = symTable->getOffset(local_addr);
    int* ptr = mem->start + wordId;
    ptr = ptr + offset;
    memcpy(ptr, &val, sizeof(int));
}

void assignVar(const Ptr& p, bool f) {
    if (p.t.type != Type::BOOL)
        throw std::runtime_error("Assignment to non-bool variable");
    int local_addr = p.addr;
    int wordId = symTable->getWordIdx(local_addr);
    int offset = symTable->getOffset(local_addr);
    int* ptr = mem->start + wordId;
    ptr = ptr + offset;
    memcpy(ptr, &f, sizeof(bool));
}

void assignVar(const Ptr& p) {
}

int main() {
    MemBlock* mem = new MemBlock();
    mem->Init(1024 * 1024);
    int word1 = mem->getMem(512);
    cout << "word1: " << word1 << endl;
    int word2 = mem->getMem(256);
    cout << "word2: " << word2 << endl;
    int word3 = mem->getMem(128);
    cout << "word3: " << word3 << endl;
    // mem->freeBlock(word3);
    // cout << "freeing word3" << endl;
    // mem->freeBlock(word2);
    // cout << "freeing word2" << endl;
    mem->freeBlock(word1);
    cout << "freeing word1" << endl;

    word1 = mem->getMem(32);
    cout << "word1: " << word1 << endl;
    int word4 = mem->getMem(64);
    cout << "word4: " << word4 << endl;
}