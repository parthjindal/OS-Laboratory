#include <stdlib.h>
#include <unistd.h>

#include <cstdio>
#include <cstring>
#include <exception>
#include <iostream>
#include <string>
#include <vector>

using namespace std;

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
    SymbolTable() : size(0), head(0), tail(MAX_SYMBOLS - 1) {
        // Explicit free list
        for (int i = 0; i < MAX_SYMBOLS; i++) {
            symbols[i].word1 = 0;
            symbols[i].word2 = ((i + 1) * sizeof(Symbol)) << 1;
        }
        symbols[tail].word2 = -2;  // mark end of free list
    }
    int alloc(int wordidx, int offset) {
        if (size == MAX_SYMBOLS) {
            return -1;
        }
        int idx = head;
        head = symbols[head].word2 & -2;
        symbols[idx].word1 = (wordidx << 1) | 1;  // mark as allocated
        symbols[idx].word2 = (offset << 1) | 1;   // mark as in use
        size++;
        return idx;  // local address
    }
    void free(int idx) {
        symbols[tail].word2 = idx << 1;
        symbols[idx].word1 = 0;
        symbols[idx].word2 = -2;
        tail = idx;
        size--;
    }
};

#define MAX_STACK_SIZE 1024
struct Stack {
    int top;
    int _elems[MAX_STACK_SIZE];
    Stack() : top(0) {}
    void push(int elem) {
        _elems[top++] = elem;
    }
    int pop() {
        return _elems[--top];
    }
};

struct MemBlock {
    int *start, *end;
    int* mem;
    void init(int _size) {
        int size = (((_size + 3) >> 2) << 2) + 8;  // align to 4 bytes
                                                   // and add 8 bytes for header, footer
        cout << "Allocating " << size << " bytes" << endl;
        mem = (int*)malloc(size);
        start = mem;
        end = mem + (size >> 2);
        *start = (size >> 2) << 1;                      // 31 bits store size last bit for if free or not
        *(start + (size >> 2) - 1) = (size >> 2) << 1;  // footer
    }
    int getMem(int size) {  // size in bytes (4 bytes aligned)
        int* p = start;
        int newsize = (((size + 3) >> 2) << 2) + 8;
        cout << "gettting " << newsize << " bytes" << endl;
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
        *ptr = (size >> 2) << 1;
        int words = size >> 2;
        if (size < oldsize) {
            *(ptr + words) = (oldsize >> 2) << 1;  // old size in words
            *(ptr + words + (oldsize >> 2) - 1) = (oldsize >> 2) << 1;
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

int main() {
    MemBlock* mem = new MemBlock();
    mem->init(1024);
    int word1 = mem->getMem(512);
    cout << "word1: " << word1 << endl;
    int word2 = mem->getMem(256);
    cout << "word2: " << word2 << endl;
    int word3 = mem->getMem(128);
    cout << "word3: " << word3 << endl;
    mem->freeBlock(word3);
    cout << "freeing word3" << endl;
    mem->freeBlock(word2);
    cout << "freeing word2" << endl;
    mem->freeBlock(word1);
    cout << "freeing word1" << endl;

    word1 = mem->getMem(512);
    cout << "word1: " << word1 << endl;
}

// void* mem;

// void createMem(size_t bytes) {
//     mem = malloc(bytes);
//     sizeof(SymbolTable);
// }

// Ptr createVar(const Type& t) {
//     Ptr p(t, 0);
// }

// int main() {
//     Type t1 = Type(Type::INT);
//     Type t2 = Type(Type::CHAR);
//     Type t3 = Type(Type::BOOL);
//     Type t4 = Type(Type::MEDIUM_INT);
//     ArrType t5 = ArrType(10, Type::INT);
// }

// size_t
// getSize(const Type& type) {
// }

// struct Ptr {
//     int addr;
//     Type type;
// };

// void createVar(const Type& type, const std::string& name,
//                const std::string& value) {
//     if (type.type == TypeEnum::INT) {
//     }
// }

// // extern void* mem;

// struct Mem {
//     void* mem;
//     size_t size;
//     pthread_mutex_t mutex;
// };

// struct hole {
//     int base;
//     size_t size;
//     hole* next;
//     hole* prev;
//     hole(int _base, size_t _size) : base(_base), size(_size), next(nullptr), prev(nullptr) {}
// };

// struct holeList {
//     hole* head;
//     hole* tail;
//     holeList() : head(nullptr), tail(nullptr) {}
// };

// size_t findTypeSize(TypeEnum type) {
//     switch (type) {
//         case INT:
//             return 32;
//         case CHAR:
//             return 8;
//         case BOOL:
//             return 1;
//         case MEDIUM_INT:
//             return 24;
//         case ARRAY:
//             return 0;  // pointer type
//     }
// }

// struct Type {
//     TypeEnum type;
//     Type* subtype;
//     size_t width;
//     // subtype is only used for array and pointer
//     Type(TypeEnum& type, Type* subtype = nullptr, size_t width = 0) : type(type), subtype(subtype), width(width) {}
//     Type(const Type& other) {
//         type = other.type;
//         subtype = other.subtype;
//         width = other.width;
//     }
// };

// struct Symbol {
//     std::string name;
//     const Type& type;
//     int offset;
//     Symbol(std::string& name, const Type& type, int offset) : name(name), type(type), offset(offset) {}
// };

// struct SymbolTable {
// }

// void
// createMem(size_t size) {
//     mem = malloc(size);
//     if (mem == NULL) {
//         std::cout << "Error: malloc failed" << std::endl;
//         exit(1);
//     }
//     memset(mem, 0, size);
// }

// class SymType;

// class Symbol {
//     std::string name;
//     const SymType& type;
//     size_t offset;
//     Symbol(const std::string& _name, const SymType& _type)
//         : name(_name), type(_type) {}
//     ~Symbol() {}
//     Symbol(const Symbol& symbol)
//         : name(symbol.name), type(symbol.type), offset(symbol.offset) {}

//     std::ostream& operator<<(std::ostream& os) const {
//         os << name << ": " << type << " @ " << offset;
//         return os;
//     }
// };

// struct