#ifndef sqrt2_int_morton_HPP
#define sqrt2_int_morton_HPP

#include <iomanip>
#include <cstdint>
#include <iostream>
#include <bitset>
#include <cassert>
#include <compare>
#include <omp.h>
#include <atomic>
#include "./custom_morton_LUTs.h" // Include the libmorton library
#include <immintrin.h>

struct sqrt2_int_morton {
    uint16_t data; // Morton encoded data

    // Constructor
    explicit sqrt2_int_morton(int_fast16_t data_ = 0) : data(data_) {}
    // explicit sqrt2_int_morton(int_fast8_t a_coefficient, int_fast8_t b_coefficient) {data = MortonLookup::encodeMorton16_LUT(a_coefficient, b_coefficient);}

    // int_fast8_t get_int_c() const {
    //     int_fast8_t int_c;
    //     decodeX[data];
    //     MortonLookup::decodeMorton16_first(data, int_c);
    //     return int_c;
    // }

    // int_fast8_t get_sqrt2_c() const {
    //     int_fast8_t sqrt2_c;
    //     MortonLookup::decodeMorton16_second(data, sqrt2_c);
    //     return sqrt2_c;
    // }

    // // Overloaded stream insertion operator
    // friend std::ostream& operator<<(std::ostream& os, const sqrt2_int_morton& obj) {
    //     int_fast8_t a, b;
    //     MortonLookup::decodeMorton16_LUT(obj, a, b);
    //         os << static_cast<int>(a) << " + " << static_cast<int>(b) << "√2";
    //     return os;
    // }

    void series_print(int_fast32_t data) {
        bool firstTerm = true; // To handle the '+' for the first term
        std::bitset<16> bits(data);
        std::cout << "The series representation is: ";
        for (size_t i = 0; i < 16; ++i) {
            if (bits[i]) { // If the ith bit is 1
                if (!firstTerm) {
                    std::cout << " + ";
                }
                firstTerm = false;

                if (i == 0) {
                    // First term, sqrt(2)^0 = 1
                    std::cout << "1";
                } else if (i == 1) {
                    // Second term, sqrt(2)^1 = sqrt(2)
                    std::cout << "√2";
                } else {
                    // Higher powers
                    std::cout << "√2^" << i;
                }
            }
        }    
        std::cout << std::endl;    
    }

    inline __attribute__((always_inline)) sqrt2_int_morton operator+=(sqrt2_int_morton od) {
        asm (
            "1:\n\t"                      // Label for the start of the loop
            "movw %1, %%cx\n\t"           // Copy B to CX, so that CX = carry_{i-1}
            "andw %0, %1\n\t"           // Compute carry: A & carry_i -> carry_i = carry_i >>2
            "xorw %%cx, %0\n\t"             // Compute sum without carry: A XOR carry_{i-1} -> A
            "shlw $2, %1\n\t"            // Shift carry left by 2                 = carry_i
            "jnz 1b\n\t"                   // If carry is zero, exit the loop

            : "+r"(data), "+r"(od.data)   // Output: a and b are modified in place
            :                             // No additional inputs
            : "cx", "cc"                  // Clobbers
        );
        return *this;
    }

    sqrt2_int_morton& add_old(sqrt2_int_morton od) {
        for (int i = 0; i < 8; ++i) { // Unroll the loop for 8 iterations
            int carry = ((data & od.data) << 2) ; // Generate carry bits
            data ^= od.data;  // Sum without carry
            if(carry==0) return *this;
            od.data = carry;  // Update od to carry for the next iteration
        }
        return *this;
    }

    sqrt2_int_morton& operator-=(const sqrt2_int_morton other) {*this += (-other); return *this;} 
    sqrt2_int_morton operator+(const sqrt2_int_morton &other) const {return sqrt2_int_morton(data) += other;}
    sqrt2_int_morton operator-(const sqrt2_int_morton other) const {return *this + (-other);}

    sqrt2_int_morton operator*= (const sqrt2_int_morton other_) {
        sqrt2_int_morton other = other_;
        sqrt2_int_morton ret(0);

        #pragma unroll
        for (size_t i = 0; i < 16; i++) { // While there are still bits in other
            if(other.data == 0) return *this = ret;
            else if (other.data & 1) { // If the least significant bit of other is 1
                ret += *this; // Add the value of *this to the result
            }
            data <<= 1; // Shift *this left by 1 (multiply a by √2)
            other.data >>= 1; // Shift other right by 1 (divide b by √2) // sign extension makes this never break
        }

        return *this = ret;
    }

    std::pair<bool,bool> signs() const {
        uint16_t topTwoBits = (data >> 14) & 0b11; // Extract top two bits
        return std::make_pair((topTwoBits & 0b10) != 0, (topTwoBits & 0b01) != 0);
    }


    sqrt2_int_morton operator~() const {return sqrt2_int_morton(~data);}
    sqrt2_int_morton operator-() const {return ~(*this) + sqrt2_int_morton(0b11);} 

    sqrt2_int_morton operator*(const sqrt2_int_morton& other) const {return sqrt2_int_morton(*this)*=other;}
    sqrt2_int_morton& operator&=(const sqrt2_int_morton& other) {data &= other.data; return *this;} 
    sqrt2_int_morton& operator|=(const sqrt2_int_morton& other) {data |= other.data; return *this;} 
    sqrt2_int_morton& operator^=(const sqrt2_int_morton& other) {data ^= other.data; return *this;};
    sqrt2_int_morton operator^(const sqrt2_int_morton& other) const {sqrt2_int_morton ret = *this; ret.data ^= other.data; return ret;}

    inline sqrt2_int_morton sign_bits() const {return sqrt2_int_morton((data >> 14) & 0b11);}
    inline sqrt2_int_morton sign_extension() const {return -sqrt2_int_morton((data >> 14) & 0b11);}
    inline sqrt2_int_morton vector_abs() {return(*this ^ sign_extension()) + sign_bits();}

    sqrt2_int_morton& operator>>=(int shift) {data = (sign_extension().data<<((16-shift))) | (data >> (shift)); return *this;} // Right shift assignment
    sqrt2_int_morton operator>>(int shift) const {return sqrt2_int_morton(this->data)>>=shift;}
    sqrt2_int_morton& operator<<=(int shift) {data <<= shift; return *this;}; // Left shift assignment
    bool operator==(const sqrt2_int_morton& other) const=default;
    bool operator!=(const sqrt2_int_morton& other) const=default;
    std::strong_ordering operator<=>(const sqrt2_int_morton& other) const=default;
    operator int_fast16_t() const {return data;}
};

namespace std {
    template <>
    struct is_integral<sqrt2_int_morton> : std::true_type {};

    // This may not work correctly, need to check
    sqrt2_int_morton abs(const sqrt2_int_morton& num) {return (num < 0) ? ~num : num;}
    // int countr_zero(const sqrt2_int_morton& num) {return std::countr_zero(num.data);}
}

#endif // sqrt2_int_morton_HPP