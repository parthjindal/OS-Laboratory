#include "medium_int.h"

#include <cstring>

using namespace std;

medium_int::medium_int(int _val) {
    data[0] = _val & 0xff;
    data[1] = (_val >> 8) & 0xff;
    data[2] = (_val >> 16) & 0xff;
}
int medium_int::to_int() const {
    int val = ((int)this->data[0]) | ((int)this->data[1] << 8) | ((int)this->data[2] << 16);
    if (val & (1 << 23)) {
        val |= 0xff << 24;
    }
    return val;
}

medium_int medium_int::operator=(int _val) {
    data[0] = _val & 0xff;
    data[1] = (_val >> 8) & 0xff;
    data[2] = (_val >> 16) & 0xff;
    return *this;
}

medium_int medium_int::operator=(const medium_int& t) {
    memcpy(this->data, t.data, 3);
    return *this;
}

ostream& operator<<(ostream& os, const medium_int& t) {
    int val = t.to_int();
    os << val;
    return os;
}

medium_int medium_int::operator+(const medium_int& t) {
    int v = t.to_int();
    int val = this->to_int();
    medium_int res(val + v);
    return res;
}

medium_int medium_int::operator-(const medium_int& t) {
    medium_int res;
    int v = t.to_int();
    int val = this->to_int();
    return medium_int(val - v);
}

medium_int medium_int::operator*(const medium_int& t) {
    medium_int res;
    int v = t.to_int();
    int val = this->to_int();
    return medium_int(val * v);
}

medium_int medium_int::operator/(const medium_int& t) {
    medium_int res;
    int v = t.to_int();
    int val = this->to_int();
    return medium_int(val / v);
}

bool medium_int::operator==(const medium_int& t2) {
    return memcmp(this->data, t2.data, 3) == 0;
}