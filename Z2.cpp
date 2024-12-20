#include <iostream>
#include <algorithm>
#include <stdint.h>
#include <compare>
#include "Z2.hpp"

// /**
//  * Default Constructor
//  * Initializes a Z2 object to represent the number 0.
//  * This is achieved by setting all components of the number (integer part, sqrt(2) part, and log base √2 of the denominator) to 0.
//  */
Z2::Z2()
{
    intPart = 0;
    sqrt2Part = 0;
    exponent = 0;
}

/**
 * Parameterized Constructor
 * Initializes a Z2 object to represent the number (a+√2*b)/(√2)^c.
 * @param a Integer part of the numerator. Represents the coefficient of the rational component.
 * @param b Coefficient of the √2 component in the numerator. Represents the irrational component multiplied by √2.
 * @param c The exponent c in the denominator, representing the power of √2. Affects the scaling of the number.
 * This constructor allows for the creation of a Z2 number with specific components, enabling the representation of a wide range of values.
 */
Z2::Z2(const z2_int a, const z2_int b, const z2_int c)
{
    intPart = a;
    sqrt2Part = b;
    exponent = c;
}



/**
 * Overloads the + operator for Z2 objects.
 * Performs the addition of two Z2 objects.
 * @param other The Z2 object to add to the current object.
 * @return The result of adding the current object and the 'other' object.
 */
Z2 Z2::operator+(const Z2 &other) const
{
    Z2 tmp = *this;
    tmp += other;
    return tmp;
}

Z2& Z2::operator+=(const Z2 &other) {
    if(other.intPart==0) {
        return *this;
    }
    if(intPart==0) {
        *this = other;
        return *this;
    }

    uz2_int exp_diff = std::abs(exponent - other.exponent);       

    if(other.exponent < exponent) {
        if(exp_diff & 1) {
            intPart += other.sqrt2Part << ((exp_diff + 1) >> 1);
            sqrt2Part += other.intPart << (exp_diff >> 1);
        } else {
            intPart += other.intPart << (exp_diff >> 1);
            sqrt2Part += other.sqrt2Part << (exp_diff >> 1);
            if(!exp_diff) reduce();
        }
    } else {
        if(exp_diff & 1) {
            std::swap(intPart, sqrt2Part);
            intPart <<= 1; // multiply intPart by 2
            exp_diff--; // already multiplied by 2, exp_diff is now even
        }
        intPart <<= (exp_diff >> 1);
        sqrt2Part <<= (exp_diff >> 1);
        exponent = other.exponent;
        intPart += other.intPart;
        sqrt2Part += other.sqrt2Part;
        exponent = other.exponent;
        if(!exp_diff) reduce();
    }
    return *this;
}

/**
 * Overloads the -= operator for Z2 objects.
 * Subtracts the 'other' Z2 object from the current object.
 * @param other The Z2 object to be subtracted from the current object.
 * @return A reference to the current object after subtraction.
 */
Z2 &Z2::operator-=(const Z2 &other){
    *this += -other;
    return *this;
}

/**
 * Overloads the - operator for negating a Z2 object.
 * @return The negated Z2 object.
 */
Z2 Z2::operator-() const { 
    return Z2(-intPart, -sqrt2Part, exponent); 
}


/**
 * Overloads the - operator for Z2 objects.
 * Performs the subtraction of 'other' from the current object.
 * @param other The Z2 object to be subtracted from the current object.
 * @return The result of the subtraction.
 */
Z2 Z2::operator-(const Z2 &other) { return *this + (-other); }

// /**
//  * Overloads the * operator for Z2
//  * @param other reference to Z2 object to be multiplied
//  * @return
//  */
// Z2 Z2::operator*(const Z2 other)
// {   
//     return Z2(intPart * other.intPart + ((sqrt2Part * other.sqrt2Part) << 1), intPart * other.sqrt2Part + sqrt2Part * other.intPart, exponent + other.exponent);
//     // Z2 tmp = Z2(intPart * other.intPart + ((sqrt2Part * other.sqrt2Part) << 1), intPart * other.sqrt2Part + sqrt2Part * other.intPart, exponent + other.exponent);
//     // tmp.reduce();
//     // if(intPart == 0) return Z2(0,0,0);              // This check may not be necessary if we're careful
//     // return tmp;
// }

/**
 * Overloads the * operator for Z2 objects.
 * Multiplies the current object with 'other'.
 * @param other The Z2 object to be multiplied with the current object.
 * @return The result of the multiplication.
 */
Z2 Z2::operator*(const Z2 &other) const
{   
    return Z2(intPart * other.intPart + ((sqrt2Part * other.sqrt2Part) << 1), intPart * other.sqrt2Part + sqrt2Part * other.intPart, exponent + other.exponent);
}

/**
 * Overloads the * operator for Z2 objects.
 * Multiplies the current object with 'other'.
 * @param other The Z2 object to be multiplied with the current object.
 * @return The result of the multiplication.
 */
void Z2::zero_mask_multiply(const Z2 &other) 
{
    // Maybe slightly faster to bit twiddle.
    if(other.intPart == 0) {
        intPart *=3;
        return;
    }
    intPart = intPart * other.intPart + ((sqrt2Part * other.sqrt2Part) << 1);
    sqrt2Part = intPart * other.sqrt2Part + sqrt2Part * other.intPart;
    exponent =  other.exponent;
}

/**
 * Overloads the / operator for Z2 objects.
 * Divides the current object by 'other'.
 * @param other The Z2 object to divide the current object by.
 * @return The result of the division.
 */
Z2 Z2::operator/(const Z2 &other) const
{   
    // If this is 0, we can't do division.     
    // If other is 0, then by virtue of other generating this, this is also 0, so don't need to check
    if(intPart == 0) return Z2(0,0,0);   
 
    // Neither is 0 now, so we can do division. Assume that other was one of the terms in the 
    // original product and we did not reduce it. This will fail if we reduced it and we would need
    // add further checks.
    int denominator = (other.intPart * other.intPart << 1) - (other.sqrt2Part * other.sqrt2Part);

    int intPartNew = (intPart << 1) * other.intPart + sqrt2Part * other.sqrt2Part;
    int sqrt2PartNew = (sqrt2Part << 1) * other.intPart - intPart * other.sqrt2Part;

    intPartNew /= denominator;
    sqrt2PartNew /= denominator;
    return Z2(intPartNew, sqrt2PartNew, exponent - other.exponent);
}

/**
 * Overloads the * operator for Z2 objects.
 * Multiplies the current object with 'other'.
 * @param other The Z2 object to be multiplied with the current object.
 * @return The result of the multiplication.
 */
void Z2::zero_mask_divide(const Z2 &other) 
{
    // If trying to divide by 0, actually divide by the mask 3. No other work needed.
    if(other.intPart == 0) {
        intPart /= 3;
        return;
    } 
    int denominator = (other.intPart * other.intPart << 1) - (other.sqrt2Part * other.sqrt2Part);
    int intPartNew = (intPart << 1) * other.intPart + sqrt2Part * other.sqrt2Part;
    int sqrt2PartNew = (sqrt2Part << 1) * other.intPart - intPart * other.sqrt2Part;
    intPart = intPartNew / denominator;
    sqrt2Part = sqrt2PartNew / denominator;
    exponent -= other.exponent;
}

bool Z2::abs_less(const Z2 &other) {
    if(intPart < other.intPart) return true;
    if(intPart == other.intPart) {
        if(sqrt2Part < other.sqrt2Part) return true;
        if(sqrt2Part == other.sqrt2Part && exponent < other.exponent) return true;
    }
    return false;
}

/**
 * Overloads the < operator for Z2 objects.
 * Compares the current object with 'other' for less-than relation.
 * @param other The Z2 object to compare with the current object.
 * @return True if the current object is less than 'other', false otherwise.
 */
bool Z2::is_negative() const
{
    if(intPart<0) 
        return true;
    return intPart==0 && sqrt2Part<0;
}

const bool Z2::operator==(const Z2 &other) const
{
    return (intPart == other.intPart && sqrt2Part == other.sqrt2Part && exponent == other.exponent);
}

const bool Z2::operator==(const z2_int &other) const
{
    return (intPart == other && exponent == 0);
}


const std::strong_ordering Z2::operator<=>(const Z2 &other) const
{
    std::strong_ordering result = intPart<=>other.intPart;
    if(result != std::strong_ordering::equal) return result;

    result = sqrt2Part<=>other.sqrt2Part;
    if(result != std::strong_ordering::equal) return result;
    
    return exponent<=>other.exponent;
}

const std::strong_ordering Z2::operator<=>(const int &other) const
{
    if(intPart != other) return intPart<=>other;
    std::strong_ordering exponent_comparison = exponent<=>0;
    if(exponent_comparison != std::strong_ordering::equal) return (exponent_comparison == std::strong_ordering::greater) ? std::strong_ordering::less : std::strong_ordering::greater;
    return sqrt2Part<=>0;
}

/**
 * Overloads the = operator for Z2
 * @param other reference to object make *this equal to
 * @return *this reference to this object which has been made equal to other
 */
Z2 &Z2::operator=(const Z2 &other)
{
    // // assigns an operator
    intPart = other.intPart;
    sqrt2Part = other.sqrt2Part;
    exponent = other.exponent;
    return *this;
}

/**
 * Overloads the = operator for Z2 objects.
 * Assigns the values from another Z2 object to the current object.
 * This assignment makes the current object a copy of the 'other' object.
 * @param other The Z2 object whose values are to be copied.
 * @return A reference to the current object after the assignment.
 */
Z2 &Z2::operator=(const z2_int &other)
{
    intPart = other;
    sqrt2Part = 0;
    exponent = 0;
    return *this;
}

/**
 * Reduces this Z2 to its simplest form.
 * This function normalizes the Z2 object by adjusting its components, 
 * ensuring it represents the number in its most reduced form.
 * The reduction process involves manipulating the integer and sqrt(2) parts
 * of the number and adjusting the denominator exponent accordingly.
 * This is particularly useful for maintaining a standard form of Z2 numbers 
 * after arithmetic operations.
 * 
 * @return A reference to this object in its simplified form.
 */
Z2 &Z2::reduce()
{
    // Assume that reduce() doesn't get called if intPart == 0;
    if (!(intPart) && !(sqrt2Part))          // If it's 0, it's reduced.
    { 
        exponent = 0;         
        return *this;       // Can we make all of these point to the same thing? Where would this go wrong? There may be a lot of 0s.
    }

    // reduces a Z2 to its lowest denominator exponent (DE) expression, can be negative, but maybe should require positive?
    // If a is even, (a + b√2)/√2^k ↦ (b + (a/2)√2)/√2^(k-1) in each iteration
    // bool integer_part_index = 0;
    
    // loop until one of the two parts becomes odd
    while (!(intPart & 1) && !(sqrt2Part & 1)) {
        intPart >>= 1;
        sqrt2Part >>= 1;
        exponent -= 2;
    }
    if (!(intPart & 1)) {
        std::swap(intPart, sqrt2Part);
        sqrt2Part >>= 1;
        exponent--;
    }
    return *this;
}

/**
 * Overloads the << operator for Z2 objects.
 * This operator allows Z2 objects to be output to an ostream, such as std::cout.
 * It outputs the Z2 object in a specific format, displaying its components.
 * The format used is (integer_part,sqrt(2)_part)e(log_base_2_of_denominator).
 * This representation helps in understanding the state of a Z2 object.
 * 
 * @param os The output stream to which the Z2 object is to be sent.
 * @param z The Z2 object to be output.
 * @return A reference to the output stream.
 */
std::ostream& operator<<(std::ostream& os, const Z2& z){
    os << (int) z.intPart << "," << (int) z.sqrt2Part << "e" << (int) z.exponent;
    return os;
}


/**
 * @brief Computes the absolute value of the Z2 object.
 * 
 * This method returns a new Z2 object that represents the absolute value
 * of the current object. If the integer part of the current object is 
 * negative, it returns the negation of the current object; otherwise, 
 * it returns the current object itself.
 * 
 * @return Z2 A new Z2 object representing the absolute value.
 */

Z2 Z2::abs() const{
    return (intPart < 0) ? -*this : *this;
}
