#ifndef Z2_HPP
#define Z2_HPP

#include <array>

typedef int8_t z2_int;
typedef uint8_t uz2_int;

class Z2{
// elements of Z[1/sqrt(2)] are stored in the form (intPart + sqrt2Part*sqrt(2))/2^exponent
public:
    Z2(const z2_int,const z2_int,const z2_int); // the ints paseed form the entries of val
    Z2();// the entries of val are all 0
    // inline uint32_t as_uint32() const;
    Z2 operator+(const Z2&) const; //handles addition
    Z2& operator+=(const Z2&); //handles +=
    Z2& abs_add(const Z2&); //handles +=
    Z2& abs_subtract(const Z2&); //handles +=
    Z2& operator-=(const Z2&); //handles -=
    Z2 operator-() const; //handles negation
    Z2 operator-(Z2&); //handles subtraction
    bool operator<(const z2_int&);
    bool operator<(const Z2&) const;
    bool operator>(const z2_int&);
    bool operator>(Z2&);
    bool operator<=(Z2&);
    bool operator>=(Z2&);
    // Z2 operator*(const Z2); //function that handles multiplication
    // Z2 operator*(Z2&); //function that handles multiplication
    Z2 operator*(const Z2&) const; //function that handles multiplication
    void zero_mask_multiply(const Z2&);
    void zero_mask_divide(const Z2&);
    Z2 operator/(const Z2&) const; //function that handles multiplication
    bool operator==(const Z2&) const; //function that checks equality between two Z2
    bool operator==(Z2&);
    bool operator==(const z2_int&); //function that checks equality between two Z2
    bool operator!=(const Z2&) const; //function that checks equality between two Z2
    Z2& operator=(const z2_int&); //function that makes the operator have equal entries to parameter
    Z2& operator=(const Z2&); //function that makes the operator have equal entries to parameter
    Z2& operator=(Z2&); //function that makes the operator have equal entries to parameter
    Z2 abs(); //function that returns the magnitude of the operator
    Z2 abs() const; //function that returns the magnitude of the operator
    // Z2 zero_mask() {
    //     return Z2(3*(intPart==0), 0, 0);
    // }
    // static Z2 zero_mask(const Z2 &other) {
    //     if (other.intPart == 0) {
    //         return Z2(3, 0, 0);
    //     }
    // }
    bool abs_less(const Z2&);
    friend std::ostream& operator<<(std::ostream&,const Z2&); //display
    void negate(){intPart=-intPart;sqrt2Part=-sqrt2Part;}
    bool is_negative() const;

    void increaseDE() 
    {
        if(intPart!=0) exponent++;
    }

    z2_int intPart;
    z2_int sqrt2Part;
    z2_int exponent; //denominator exponent
private:
    Z2& reduce(); //auxiliary function to make sure every triad is in a consistent most reduced form
};

#endif // Z2_HPP
