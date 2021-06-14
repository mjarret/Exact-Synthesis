#include <algorithm>
#include <iostream>
// #include <cmath>
#include <stdint.h>
#include "Z2.hpp"

/**
 * Constructor that initializes object corresponding to (a+sqrt(2)*b)/(2^c)
 * @param a integer part of numerator
 * @param b sqrt(2) part of numerator
 * @param c log_2 of the denominator
 */
Z2::Z2(const int8_t a, const int8_t b, const int8_t c){
    // val[2] = c<<2;
    // if(c%2) {
    //     val[0] = b;
    //     val[1] = a<<1;
    //     reduce();
    //     return;
    // }
    val[0] = a;
    val[1] = b;
    val[2] = c;
    reduce();
}

/**
 * Constructor to initialize Z2 corresponding to 0.
 */
Z2::Z2(){
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
}

/**
 * Overloads + operator for Z2
 * @param other reference to Z2 object to be added
 * @return summation the sum of other and *this
 */
Z2 Z2::operator+(Z2& other){
    Z2 tmp = *this;
    tmp += other;
    return tmp;
}

/**
 * Overloads += operator for Z2
 * @param other reference to Z2 object to be added to *this
 * @return *this reference to this object, onto which other has been added
 */
Z2& Z2::operator+=(Z2& other){
    int8_t exp_diff = val[2]-other.val[2];
    if(exp_diff > 0) {
        val[0] += other.val[0]<<exp_diff;
        val[1] += other.val[1]<<exp_diff;
        reduce();
        return *this;
    }
    val[0] <<= -exp_diff;
    val[1] <<= -exp_diff;
    val[0] += other.val[0];
    val[1] += other.val[1];
    val[2] = other.val[2];
    reduce();
    return *this;
}

/**
 * Overloads the - operator for Z2
 * @param 
 * @return negation of Z2
 */
Z2 Z2::operator-(){ return Z2(-val[0],-val[1],val[2]);}

/**
 * Overloads the - operator for Z2
 * @param other reference to Z2 object to be subtracted
 * @return summation the subtraction *this - other
 */
Z2 Z2::operator-(Z2& other) {return -other + *this;}

/**
 * Overloads the * operator for Z2
 * @param other reference to Z2 object to be multiplied
 * @return
 */
Z2 Z2::operator*(const Z2& other){
    return Z2(val[0]*other[0]+(val[1]*other[1]<<1), val[0]*other[1]+val[1]*other[0], val[2]+other[2]);
}

/**
 * Overloads the == operator for Z2
 * @param other reference to Z2 object to be compared to
 * @return whether or not the entries of the two Z2s are equal
 */
bool Z2::operator==(const Z2& other){    
    return (val[0]==other[0] && val[1]==other[1] && val[2]==other[2]);
}

/**
 * Overloads the == operator for Z2
 * @param other reference to Z2 object to be compared to
 * @return whether or not the entries of the two Z2s are equal
 */
bool Z2::operator==(const int8_t& i){return val[0]==i && val[1]==0 && val[2]==0;}

bool Z2::operator!=(const Z2& other){return !(*this == other);}

/**
 * Overloads the < operator for Z2
 * @param other reference to Z2 object to be compared to
 * @return true if this < other and false otherwise
 */
bool Z2::operator<(Z2& other){
    Z2 diff = *this - other;                                        // Find difference and store as Z2
    if(diff.val[0] < 0) {                                    
        if(diff.val[1] <= 0) return true;                           // a<0 and b<=0 means that diff < 0
        int8_t a2 = diff.val[0]*diff.val[0];                        // compute a^2    
        int8_t b2 = (diff.val[1]*diff.val[1]) << 1;                 // compute 2b^2
        if(a2 > b2) return true;                                    // a<0, b>0, and a^2 > 2 b^2 implies that a+sqrt(2)b <0
        return false;                                               // a<0, b>0, and a^2 <= 2 b^2 implies that a+sqrt(2)b >= 0
    }
    if(diff.val[1] >= 0) return false;                              // a>=0 and b>=0 means that diff >=0
    int8_t a2 = diff.val[0]*diff.val[0];                      // compute a^2    
    int8_t b2 = (diff.val[1]*diff.val[1]) << 1;               // compute 2b^2
    if(a2 < b2) return true;                                        // a>0, b<0, and a^2 < 2b^2 implies that a + sqrt(2) b < 0
    return false;                                                   // a>0, b<0, and a^2 >= 2b^2 implies that a+sqrt(2)b >= 0
}

/**
 * Overloads the < operator for Z2
 * @param other reference to Z2 object to be compared to
 * @return true if this < other and false otherwise
 */
const bool Z2::operator<(const Z2& other) const{
    int8_t k = std::max(val[2],other[2]);
    int8_t a = val[0] << (k-val[2]);
    int8_t b = val[1] << (k-val[2]);
    a -= other[0] << (k-other[2]);
    b -= other[1] << (k-other[2]);
    if(a < 0) {                                    
        if(b <= 0) return true;                             // a<0 and b<=0 means that diff < 0
        a*=a;                                               // compute a^2    
        b = (b*b) << 1;                                     // compute 2b^2
        if(a > b) return true;                              // a<0, b>0, and a^2 > 2 b^2 implies that a+sqrt(2)b <0
        return false;                                       // a<0, b>0, and a^2 <= 2 b^2 implies that a+sqrt(2)b >= 0
    }
    if(b >= 0) return false;                                // a>=0 and b>=0 means that diff >=0
    a*=a;                                                   // compute a^2    
    b = (b*b) << 1;                                         // compute 2b^2
    if(a < b) return true;                                  // a>0, b<0, and a^2 < 2b^2 implies that a + sqrt(2) b < 0
    return false;                                           // a>0, b<0, and a^2 >= 2b^2 implies that a+sqrt(2)b >= 0
}

bool Z2::operator>(Z2& other)  {return other < *this;}
bool Z2::operator>=(Z2& other) {return !(*this < other);}
bool Z2::operator<=(Z2& other) {return !(*this > other);}

/**
 * Overloads the < operator for Z2
 * @param other reference to an integer
 * @return true if this < other and false otherwise
 */
bool Z2::operator<(const int8_t& i){
    Z2 tmp = Z2(i,0,0);
    return *this < tmp;
}  

bool Z2::operator>(const int8_t& i){
    Z2 tmp = Z2(i,0,0);
    return *this > tmp;
}  

/**
 * Overloads the = operator for Z2
 * @param other reference to object make *this equal to
 * @return *this reference to this object which has been made equal to other
 */
Z2& Z2::operator=(const int8_t& other){
    //assigns an operator
    val[0] = other;
    val[1] = 0;
    val[2] = 0;
    return *this;
}

Z2 Z2::abs() {
    if(*this < 0) return -*this;
    return *this;
}

/**
 * Overloads the = operator for Z2
 * @param other reference to object make *this equal to
 * @return *this reference to this object which has been made equal to other
 */
Z2& Z2::operator=(const Z2& other){
    //assigns an operator
    val[0] = other[0];
    val[1] = other[1];
    val[2] = other[2];
    return *this;
}

/**
 * Reduces this Z2 to simplest form
 * @return *this reference to simplified object
 */
Z2& Z2::reduce(){
    if(val[0] == 0 && val[1] == 0) {
        val[2] = 0;
        return *this;
    }
    // reduces a Z2 to its lowest denominator exponent expression, can be negative
    while(val[0]%2 == 0 && val[1]%2 == 0){
        val[0] >>= 1;
        val[1] >>= 1;
        val[2]--;
    }
    // while(val[0]%2 == 0 && val[1]%2 == 0){
    //     val[0] >>= 1;
    //     val[1] >>= 1;
    //     val[2] -= 2;
    // }
    // if(val[0]%2) {
    //     val[0] >>= 1;
    //     val[2] --;
    // }
    return *this;
}

Z2 Z2::pattern(int LDE) {
    switch(getLDE() - LDE) {
        case 0:
            return Z2(val[0]%2, val[1]%2, val[2]);
            break;
        case 1:
            return Z2(0, val[1]%2, val[2]);
            break;
        default:
            return Z2();
    }
}