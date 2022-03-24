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

#define MAX_SYMBOLS 1024
#define MAX_STACK_SIZE 1024

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

int getSize(const Type& type);

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

class SymbolTable {
   public:
    int head, tail;
    Symbol symbols[MAX_SYMBOLS];
    int size;
    pthread_mutex_t mutex;
    void Init();
    int alloc(int wordidx, int offset);
    void free(int idx);
    int getWordIdx(int idx);
    int getOffset(int idx);
};

struct Stack {
    int top;
    int _elems[MAX_STACK_SIZE];
    void Init();
    void push(int elem);
    int pop();
};

class MemBlock {
   public:
    int *start, *end;
    int* mem;
    void Init(int _size);
    ~MemBlock();
    int getMem(int size);
    void addBlock(int* ptr, int size);
    void freeBlock(int wordid);
};

typedef int medium_int;

void createMem(int size);
Ptr createVar(const Type& t);
void assignVar(const Ptr& p, int val);
void assignVar(const Ptr& p, bool val);
void assignVar(const Ptr& p, char val);
void assignVar(const Ptr& p, medium_int val);

