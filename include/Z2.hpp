#ifndef Z2_HPP
#define Z2_HPP

#include <compare>
#include <boost/preprocessor/control/if.hpp>
#include <boost/preprocessor/comparison/equal.hpp>

struct Z2 {
    union {
        struct {
            union {
                uint16_t numerator_bits : 16; // Packed 16 bits for int_c and sqrt2_c
                struct {
                    int8_t int_c : 8;   // Lower 8 bits
                    int8_t sqrt2_c : 8; // Upper 8 bits
                };
            };
            int16_t denom_exp : 16;
        };
        uint32_t data; // Full 32-bit representation
    };
    
     // Constructor with parameters
    constexpr Z2(uint32_t data_ = 0) : data(data_) {};
    constexpr Z2(uint16_t numerator_, uint8_t denom_exp_) : numerator_bits(numerator_), denom_exp(denom_exp_) {};
    constexpr Z2(uint8_t int_c_, uint8_t sqrt2_c_, uint8_t denom_exp_) : int_c(int_c_), sqrt2_c(sqrt2_c_), denom_exp(denom_exp_) {};

    //======================================================================
    // Macros for Bitwise Operations
    //======================================================================

    /// Macro to perform a right shift while swapping byte order.
    /// - First, shifts `n` to the right by `shift`.
    /// - Adjusts for correct bit alignment.
    /// - Performs an arithmetic swap using addition, just like `LEFT_SHIFT_AND_SWAP`.
    #define LEFT_SHIFT_AND_SWAP(n) (__builtin_bswap16(n + (n & 0xFF00)))
    // #define RIGHT_SHIFT_AND_SWAP(n) (__bultin_bswap16(n - ((n & 0xFF)/2)))
    
    #define U_MIDDLE_MASK(s) (~((256 << s) - 256))
    #define L_MIDDLE_MASK(s) (256 - (256 >> s))

    #define LOWER_SIGN_EXTEND(x, s) \
        (((x & 128)<< (s+1)) - (x & 128)) | (x & U_MIDDLE_MASK(s))
    
    #define UPPER_SIGN_EXTEND(x, s) \
        ((x ^ UPPER_SIGN_MASK(s)) - UPPER_SIGN_MASK(s))    
    
    #define RIGHT_SHIFT_AND_SWAP(n, shift) (                            \
            ((static_cast<int>(n) >> 8) & 0xFF) |                       \
            ((((static_cast<int>(n) << 8)&0xFF00) >> 1) &0xFF00)        \
    );
    
    constexpr inline __attribute__((always_inline)) Z2& operator>>=(uint8_t shift) {
        numerator_bits = static_cast<uint16_t>(static_cast<int16_t>(LOWER_SIGN_EXTEND(numerator_bits, shift))>>shift);
        denom_exp = (denom_exp-2*shift)*(numerator_bits != 0);
        return *this;
    }

    #define NUMERATOR_LEFT_SHIFT(n, s) ( (n << (s/2)) & U_MIDDLE_MASK(s/2) )

    constexpr inline __attribute__((always_inline)) Z2& operator<<=(uint8_t shift) {
        numerator_bits = (numerator_bits << shift) & U_MIDDLE_MASK(shift);
        denom_exp = (denom_exp+2*shift)*(numerator_bits != 0);
        return *this;
    }

    constexpr inline __attribute__((always_inline)) void reduce() { 
        const uint8_t int_zeros = std::countr_zero(static_cast<uint8_t>(int_c)), sq_zeros = std::countr_zero(static_cast<uint8_t>(sqrt2_c));

        if(int_zeros > sq_zeros) {
            int_c >>= 1;
            numerator_bits = __builtin_bswap16(static_cast<uint16_t>(static_cast<int16_t>(LOWER_SIGN_EXTEND(numerator_bits, sq_zeros))>>sq_zeros));
            denom_exp -= 2*sq_zeros + 1;
            return;
        }
        *this >>= int_zeros;
    }

    //======================================================================
    // Macros for Arithmetic Operations
    //======================================================================

    /// Macro to perform a left shift and byte swap.
    /// This adds the least significant byte of `n` to `n`, then swaps the byte order.

    /// Macro to add numerators while handling exponent differences.
    /// - `RIGHT` is first adjusted for borrow conditions.
    /// - `RIGHT` is then **shifted into place** based on `SHIFT`, correctly handling sign extensions.
    /// - The adjusted `RIGHT` is added to `LEFT`.
    #define LEFT_SHIFT_COEFFICIENTS(x, SHIFT) (x << (SHIFT>>1))
    #define ADD_SHIFTED_NUMERATORS(LEFT, RIGHT, SHIFT) \
        (LEFT + (RIGHT << (SHIFT>>1)) - ((((LEFT&0xFF) + (RIGHT&0xFF))>>8)<<8) ) & 0xFFFF
    // LEFT + (RIGHT << (SHIFT>>1)) - ((LEFT^RIGHT) < 0)*((LEFT & 0xFF) < ((RIGHT << (SHIFT >> 1))& 0xFF)) * 0x100 & 0xFFFF

    //======================================================================
    // Macros for Handling Addition With Different Denominator Exponents
    //======================================================================

    /// Handles the case when `other` has a denominator exponent less than `this` and `N` is even.
    #define EVEN_CASE_P(N) case N: { \
        numerator_bits = ADD_SHIFTED_NUMERATORS(numerator_bits, NUMERATOR_LEFT_SHIFT(other.numerator_bits, N), 0); \
        BOOST_PP_IF(BOOST_PP_EQUAL(N, 0), reduce(); , ) \
        break; \
    }

    /// Handles the case when `other` has a denominator exponent less than `this` and `N` is odd.
    #define ODD_CASE_P(N) case N: { \
        numerator_bits = ADD_SHIFTED_NUMERATORS(numerator_bits, NUMERATOR_LEFT_SHIFT(LEFT_SHIFT_AND_SWAP(other.numerator_bits), N),0); \
        break; \
    }

    /// Handles the case when `other` has a denominator exponent greater than `this` and `N` is even.
    #define EVEN_CASE_N(N) case -N: { \
        numerator_bits = ADD_SHIFTED_NUMERATORS(other.numerator_bits, NUMERATOR_LEFT_SHIFT(numerator_bits, N), 0); \
        denom_exp = other.denom_exp; \
        break; \
    }

    /// Handles the case when `other` has a denominator exponent greater than `this` and `N` is odd.
    #define ODD_CASE_N(N) case -N: { \
        numerator_bits = ADD_SHIFTED_NUMERATORS(other.numerator_bits, NUMERATOR_LEFT_SHIFT(LEFT_SHIFT_AND_SWAP(numerator_bits), N),0); \
        denom_exp = other.denom_exp; \
        break; \
    }

    //======================================================================
    // Addition Operator Overload
    //======================================================================

    /// Addition-assignment operator.
    /// - This function adds `other` to `this`, shifting its numerator into place based on exponent differences.
    /// - Uses a switch-case structure to efficiently handle all possible exponent differences.
    /// - If `denom_exp` differs, `other.numerator_bits` is **shifted into place** before being added.
    /// - Handles both positive and negative exponent differences, ensuring correct bit alignment.
    /// - If the exponent difference is too large, sets `numerator_bits` and `denom_exp` to zero.
    /// @returns The updated numerator bits.
    constexpr inline __attribute__((always_inline)) Z2 operator+=(Z2 other) {
        const int8_t exp_diff = denom_exp - other.denom_exp;
        switch (exp_diff) {
            EVEN_CASE_P(0)  ODD_CASE_P(1)  EVEN_CASE_P(2)  ODD_CASE_P(3)  
            EVEN_CASE_P(4)  ODD_CASE_P(5)  EVEN_CASE_P(6)  ODD_CASE_P(7)  
            EVEN_CASE_P(8)  ODD_CASE_P(9)  EVEN_CASE_P(10) ODD_CASE_P(11) 
            EVEN_CASE_P(12) ODD_CASE_P(13) EVEN_CASE_P(14) ODD_CASE_P(15) 
            ODD_CASE_N(1)   EVEN_CASE_N(2) ODD_CASE_N(3)   EVEN_CASE_N(4)  
            ODD_CASE_N(5)   EVEN_CASE_N(6) ODD_CASE_N(7)   EVEN_CASE_N(8)  
            ODD_CASE_N(9)   EVEN_CASE_N(10) ODD_CASE_N(11) EVEN_CASE_N(12)  
            ODD_CASE_N(13)  EVEN_CASE_N(14) ODD_CASE_N(15)  
            default: {
                numerator_bits = 0;
                denom_exp = 0;
                break;
            }
        }
        return *this;
    }
    
    constexpr inline __attribute__((always_inline)) Z2 operator-=(const Z2& other) {
        if(other.numerator_bits == 0) return (*this).numerator_bits;
        return *this += (-other);
    }

    Z2& operator*=(const Z2& other) {
        int_c = int_c * other.int_c + ((sqrt2_c * other.sqrt2_c) << 1);
        sqrt2_c = int_c * other.sqrt2_c + sqrt2_c * other.int_c;
        denom_exp += other.denom_exp;
        return *this;
    }
    
    Z2 operator+(const Z2 other) const {
        Z2 ret = *this;
        ret += other;
        return ret;
    }

    constexpr inline __attribute__((always_inline)) Z2 operator-() const {return Z2(static_cast<uint16_t>((256-numerator_bits)*(numerator_bits != 0)), denom_exp);}
    Z2 operator-(const Z2& other) const {Z2 ret = *this; return ret -= other;}
    std::strong_ordering operator<=>(const Z2& other) const {return data*(int_c != 0) <=> other.data*(other.int_c != 0);} // This won't necessarily work, because of the case where int_c == 0

    Z2 operator*(const Z2& other) const {Z2 ret = *this; return ret *= other;}
    Z2& operator=(const Z2& other) {data = other.data; return *this;};
    friend std::ostream& operator<<(std::ostream& os, const Z2& z) {return os << static_cast<int>(z.int_c) << "," << static_cast<int>(z.sqrt2_c) << "e" << static_cast<int>(z.denom_exp);}
};

namespace std {
    inline Z2 abs(const Z2& z) {
        return Z2(std::abs(z.int_c), std::abs(z.sqrt2_c), z.denom_exp);
    }

    template <>
    struct hash<Z2> {
        std::size_t operator()(const Z2& z) const {
            return std::hash<uint32_t>{}(z.data);
        }
    };
}

#endif // Z2_HPP
