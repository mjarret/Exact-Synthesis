#include <algorithm>
#include <iostream>
#include <cmath>
#include <stdint.h>
#include "Z2.hpp"

/**
 * Constructor that initializes object corresponding to (a+sqrt(2)*b)/(2^c)
 * @param a integer part of numerator
 * @param b sqrt(2) part of numerator
 * @param c log_2 of the denominator
 */
Z2::Z2(const int8_t a, const int8_t b, const int8_t c){
    val[0] = a;
    val[1] = b;
    val[2] = c;
    reg[0] = 0;
    reg[1] = 0;
    reg[2] = 0;
    Z2::reduce();
}

/**
 * Constructor to initialize Z2 corresponding to 0.
 */
Z2::Z2(){
    val[0] = 0;
    val[1] = 0;
    val[2] = 0;
    reg[0] = 0;
    reg[1] = 0;
    reg[2] = 0;
}

/**
 * Overloads + operator for Z2
 * @param other reference to Z2 object to be added
 * @return summation the sum of other and *this
 */
Z2 Z2::operator+(Z2& other){
    int8_t k = std::max(val[2], other[2]);
    int8_t* scaled0 = scale(k);
    int8_t* scaled1 = other.scale(k);
    Z2 summation = Z2(scaled0[0]+scaled1[0], scaled0[1]+scaled1[1], k);
    return summation;
}

/**
 * Overloads += operator for Z2
 * @param other reference to Z2 object to be added to *this
 * @return *this reference to this object, onto which other has been added
 */
Z2& Z2::operator+=(Z2& other){
    int8_t k = std::max(val[2], other[2]);
    int8_t* scaled0 = scale(k);
    int8_t* scaled1 = other.scale(k);
    val[0] = scaled0[0] + scaled1[0];
    val[1] = scaled0[1] + scaled1[1];
    val[2] = k;
    reduce();
    return *this;
}

/**
 * Overloads the - operator for Z2
 * @param 
 * @return negation of Z2
 */
Z2 Z2::operator-(){
    return Z2(-val[0],-val[1],val[2]);
}

/**
 * Overloads the - operator for Z2
 * @param other reference to Z2 object to be subtracted
 * @return summation the subtraction *this - other
 */
Z2 Z2::operator-(Z2& other){
    int8_t k = std::max(val[2], other[2]);
    int8_t* scaled0 = scale(k);
    int8_t* scaled1 = other.scale(k);
    Z2 subtraction = Z2(scaled0[0]-scaled1[0], scaled0[1]-scaled1[1], k);
    return subtraction;
}

/**
 * Overloads the * operator for Z2
 * @param other reference to Z2 object to be multiplied
 * @return
 */
Z2 Z2::operator*(const Z2& other){
    return Z2(val[0]*other[0]+2*val[1]*other[1], val[0]*other[1]+val[1]*other[0], val[2]+other[2]);
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
bool Z2::operator==(const int8_t& i){return *this == Z2(i,0,0);}
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
        int8_t a2 = diff.val[0]*diff.val[0];                  // compute a^2    
        int8_t b2 = (diff.val[1]*diff.val[1]) << 1;           // compute 2b^2
        if(a2 > b2) return true;                                    // a<0, b>0, and a^2 > 2 b^2 implies that a+sqrt(2)b <0
        return false;                                               // a<0, b>0, and a^2 <= 2 b^2 implies that a+sqrt(2)b >= 0
    }
    if(diff.val[1] >= 0) return false;                              // a>=0 and b>=0 means that diff >=0
    int8_t a2 = diff.val[0]*diff.val[0];                      // compute a^2    
    int8_t b2 = (diff.val[1]*diff.val[1]) << 1;               // compute 2b^2
    if(a2 < b2) return true;                                        // a>0, b<0, and a^2 < 2b^2 implies that a + sqrt(2) b < 0
    return false;                                                   // a>0, b<0, and a^2 >= 2b^2 implies that a+sqrt(2)b >= 0
}
bool Z2::operator>(Z2& other)  {return other < *this;}
bool Z2::operator>=(Z2& other) {return !(*this < other);}
bool Z2::operator<=(Z2& other) {return !(*this > other);}

/**
 * Overloads the < operator for Z2
 * @param other reference to Z2 object to be compared to
 * @return true if this < other and false otherwise
 */
const bool Z2::operator<(const Z2& other) const{
    int8_t k = std::max(val[2],other[2]);
    int8_t a = val[0] << (k-val[2]);
    int8_t b = val[1] << (k-val[2]);
    a = a - (other[0] << (k-other[2]));
    b = b - (other[1] << (k-other[2]));
    if(a < 0) {                                    
        if(b <= 0) return true;                           // a<0 and b<=0 means that diff < 0
        a*=a;                  // compute a^2    
        b = (b*b) << 1;           // compute 2b^2
        if(a > b) return true;                                    // a<0, b>0, and a^2 > 2 b^2 implies that a+sqrt(2)b <0
        return false;                                               // a<0, b>0, and a^2 <= 2 b^2 implies that a+sqrt(2)b >= 0
    }
    if(b >= 0) return false;                              // a>=0 and b>=0 means that diff >=0
    a*=a;                      // compute a^2    
    b = (b*b) << 1;               // compute 2b^2
    if(a < b) return true;                                        // a>0, b<0, and a^2 < 2b^2 implies that a + sqrt(2) b < 0
    return false;                                                   // a>0, b<0, and a^2 >= 2b^2 implies that a+sqrt(2)b >= 0
}

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
    return *this;
}

/**
 * Scales the Z2 to a different denominator exponent
 * @param k the exponent to scale to
 * @return reg the workspace array containing the scaled values
 */
int8_t* Z2::scale(const int8_t& k){
    // scales a Z2's entries to a given denominator exponent
    int8_t expdiff = k - val[2];
    reg[0] = val[0] << expdiff;
    reg[1] = val[1] << expdiff;
    reg[2] = k;
    return reg;
}
 
/**
 * Overloads << function for Z2
 * @param os reference to ostream object needed to implement <<
 * @param z reference to Z2 object to be displayed
 * @returns reference ostream with the Z2's display form appended
 */
/*std::ostream& operator<<(std::ostream& os, const Z2& z){
    if(!z[0]) {
        if(!z[1]) return (os << 0);
        os << z[1] << "\u221A2";
        if(!z[2]) return os;
        os << "e" << -z[2];
        return os;
    }
    if(!z[1]) {
        os << z[0];
        if(!z[2]) return os;
        os << "e" << -z[2];
        return os;
    }
    if(!z[2]) {
        os << z[0] << '+' << z[1] << "\u221A2";
        return os;    
    }
    os << '(' << z[0] << '+' << z[1] << "\u221A2)e" << -z[2];
    return os;
}*/
