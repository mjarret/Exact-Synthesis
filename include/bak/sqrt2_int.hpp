#ifndef SQRT2_INT_HPP
#define SQRT2_INT_HPP

// Standard library headers
#include <bit>
#include <cassert>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>

// A constant that might be used in modular arithmetic.
static constexpr uint8_t MOD = std::numeric_limits<uint8_t>::max();

struct sqrt2_int {
    //======================================================================
    // Data Members
    //======================================================================
    union {
        struct {
            uint8_t int_c : 8;   // Coefficient for the integer part (assumed odd unless zero)
            uint8_t sqrt2_c : 8; // Coefficient for the √2 part
        };
        uint16_t data;           // Combined raw data representation
    };

    //======================================================================
    // Constructors
    //======================================================================
    
    /// Constructs from a 16-bit integer (defaults to 0).
    explicit constexpr sqrt2_int(int16_t data_ = 0)
        : data(static_cast<uint16_t>(data_)) {}
    
    /// Constructs from two 8-bit coefficients.
    explicit constexpr sqrt2_int(int8_t a_coefficient, int8_t b_coefficient)
        : int_c(a_coefficient), sqrt2_c(b_coefficient) {}

    //======================================================================
    // Utility Methods
    //======================================================================
    
    /// Swaps the byte order of the underlying 16-bit data.
    void swap() {
        data = __builtin_bswap16(data);
    }
    
    /// Returns the raw 16-bit data.
    uint16_t get_data() const {
        return data;
    }

    //======================================================================
    // Stream Output
    //======================================================================
    
    // Overloaded output operator.
    friend std::ostream& operator<<(std::ostream& os, const sqrt2_int& obj) {
        // Convert to signed integers.
        int r = static_cast<int8_t>(obj.int_c);
        int s = static_cast<int8_t>(obj.sqrt2_c);
        os << r;
        if (s >= 0) {
            os << " + " << s << "√2";
        } else {
            os << " - " << (-s) << "√2";
        }
        return os;
    }

    //======================================================================
    // Scientific Notation Normalization
    //======================================================================
    
    /**
     * @brief Adjusts the internal representation into a normalized form.
     *
     * The function shifts the internal coefficients until at least one is odd.
     *
     * @return A value representing the power of √2 shifted out.
     */
    uint8_t scientific_notation() {
        uint8_t intPartZeros   = std::countr_zero(int_c);
        uint8_t sqrt2PartZeros = std::countr_zero(sqrt2_c);

        if (intPartZeros > sqrt2PartZeros) {
            int_c   >>= (sqrt2PartZeros + 1);
            sqrt2_c >>= sqrt2PartZeros;
            swap();
            return (2 * sqrt2PartZeros + 1);
        }
        data >>= intPartZeros;
        return 2*intPartZeros;
    }

    //======================================================================
    // Arithmetic Operators
    //======================================================================
    
    /// Addition-assignment operator.
    sqrt2_int& operator+=(sqrt2_int other) {
        data += other.data - ((data & 0xFF) < (other.data & 0xFF))<<8; //This only works when neither both - or +.
        return *this;
    }
    
    /// Binary addition operator.
    sqrt2_int operator+(const sqrt2_int &other) const {
        return sqrt2_int(data + other.data);
    }
    
    /// Unary negation operator.
    sqrt2_int operator-() const {
        return sqrt2_int((256 - data)); // Works great except for when data is 0...
    }

    /// Subtraction-assignment operator.
    sqrt2_int& operator-=(const sqrt2_int other) {
        data -= other.data + ((data & 0xFF) < (other.data & 0xFF))<<8;
        return *this;
    }

    /// Binary subtraction operator.
    sqrt2_int operator-(const sqrt2_int other) const {
        return sqrt2_int(data - other.data + 256*((data & 0xFF) < (other.data & 0xFF)));
    }    

    /// Multiplication-assignment operator.
    sqrt2_int& operator*=(const sqrt2_int other_) {
        int8_t new_int_c = int_c * other_.int_c + ((sqrt2_c * other_.sqrt2_c) << 1);
        sqrt2_c = int_c * other_.sqrt2_c + sqrt2_c * other_.int_c;
        int_c   = new_int_c;
        return *this;
    }
    
    /// Binary multiplication operator.
    sqrt2_int operator*(const sqrt2_int& other) const {
        return sqrt2_int(*this) *= other;
    }
    
    /// A helper function for addition on raw 16-bit values.
    static uint16_t& add_to(uint16_t& first, uint16_t& second) {
        return first += static_cast<uint16_t>(sqrt2_int(first) + sqrt2_int(second));
    }

    //======================================================================
    // Shift Operators
    //======================================================================
    
    /// Right shift assignment operator.
    sqrt2_int& operator>>=(uint shift) {
        data >>= (shift >> 1);
        if (!(shift & 1))
            return *this;
        int_c >>= 1;
        swap();
        return *this;
    }
    
    /// Left shift assignment operator.
    sqrt2_int& operator<<=(uint shift) {
        data <<= (shift >> 1);
        if (!(shift & 1))
            return *this;
        sqrt2_c <<= 1;
        swap();
        return *this;
    }
    
    /// Binary right shift operator.
    sqrt2_int operator>>(int shift) const {
        return sqrt2_int(data) >>= shift;
    }
    
    /// Binary left shift operator.
    sqrt2_int operator<<(int shift) const {
        return sqrt2_int(data) <<= shift;
    }

    //======================================================================
    // Conversion Operators
    //======================================================================
    
    /// Implicit conversion to uint16_t.
    operator uint16_t() const {
        return data;
    }
};

namespace std {
    /// Specialization to mark sqrt2_int as an integral type.
    template <>
    struct is_integral<sqrt2_int> : std::true_type {};

    /// Overloaded abs() function for sqrt2_int.
    inline sqrt2_int abs(const sqrt2_int& num) {
        return sqrt2_int(std::abs(num.int_c), std::abs(num.sqrt2_c));
    }
    
    /// Overloaded countr_zero() function for sqrt2_int.
    inline int countr_zero(const sqrt2_int& num) {
        return std::countr_zero(num.data);
    }
}

#endif // SQRT2_INT_HPP
