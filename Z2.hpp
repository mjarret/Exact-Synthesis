#ifndef Z2_HPP
#define Z2_HPP

#include <array>

typedef int8_t z2_int;
typedef uint8_t uz2_int;

class Z2{
// elements of Z[1/sqrt(2)] are stored in the form (intPart + sqrt2Part*sqrt(2))/2^exponent
public:
    Z2();
    Z2(const z2_int, const z2_int, const z2_int); // the ints paseed form the entries of val
    // inline uint32_t as_uint32() const;
    Z2 operator+(const Z2&) const; //handles addition
    Z2& operator+=(const Z2&); //handles +=
    Z2& abs_add(const Z2&); //handles +=
    Z2& abs_subtract(const Z2&); //handles +=
    Z2& operator-=(const Z2&); //handles -=
    Z2 operator-() const; //handles negation
    Z2 operator-(const Z2&); //handles subtraction

    const bool operator==(const Z2&) const; //handles comparison
    const bool operator==(const z2_int &other) const;
    const std::strong_ordering operator<=>(const Z2&) const; //handles comparison
    const std::strong_ordering operator<=>(const int&) const; //handles comparison
    
    Z2 operator*(const Z2&) const; //function that handles multiplication
    void zero_mask_multiply(const Z2&);
    void zero_mask_divide(const Z2&);
    Z2 operator/(const Z2&) const; //function that handles multiplication
    Z2& operator=(const z2_int&); //function that makes the operator have equal entries to parameter
    Z2& operator=(const Z2&); //function that makes the operator have equal entries to parameter

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

    const Z2& increaseDE() 
    {
        if(intPart!=0) exponent++;
        return *this;
    }

    z2_int intPart;
    z2_int sqrt2Part;
    z2_int exponent; 
private:
    Z2& reduce(); //auxiliary function to make sure every triad is in a consistent most reduced form
};

#endif // Z2_HPP
