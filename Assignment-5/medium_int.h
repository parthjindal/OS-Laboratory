#ifndef _MEDIUM_INT_H
#define _MEDIUM_INT_H
#include <ostream>

class medium_int {
   public:
    int to_int() const;
    unsigned char data[3];
    medium_int(int _val = 0);
    medium_int operator=(int _val);
    medium_int operator=(const medium_int& t);
    friend std::ostream& operator<<(std::ostream& os, const medium_int& t);
    medium_int operator+(const medium_int& t);
    medium_int operator-(const medium_int& t);
    medium_int operator*(const medium_int& t);
    medium_int operator/(const medium_int& t);
    bool operator==(const medium_int& t);
};
#endif  // _MEDIUM_INT_H