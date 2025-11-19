/**
 * @file DyadicSqrt2.hpp
 * @brief Function-based variant of Z2 (dyadic numbers over sqrt(2)).
 *
 * Semantics mirror Z2.hpp but all helpers are implemented as inline
 * functions instead of preprocessor macros so we can benchmark the
 * macro-heavy vs function-heavy implementations.
 */
#ifndef DYADIC_SQRT2_HPP
#define DYADIC_SQRT2_HPP

#include <cstdint>
#include <compare>
#include <iostream>
#include <bit>

struct DyadicSqrt2 {
    // Layout constants (mirroring Z2.hpp)
    static constexpr uint8_t kBitsForNumerator = 16;
    static constexpr uint8_t kBitsForIntC      = 8;
    static constexpr uint8_t kBitsForSqrt2C    = 8;
    static constexpr uint8_t kBitsForDenomExp  = 8;

    static constexpr uint16_t kAxis           = uint16_t(1u << kBitsForIntC);
    static constexpr uint16_t kIntMask        = uint16_t(kAxis - 1u);
    static constexpr uint16_t kNumeratorMask  = uint16_t((uint32_t(1u) << kBitsForNumerator) - 1u);
    static constexpr uint16_t kSqrt2Mask      = uint16_t(kNumeratorMask & ~kIntMask);

    union {
        struct {
            union {
                uint16_t numerator_bits : kBitsForNumerator; // packed int_c + sqrt2_c
                struct {
                    int8_t int_c   : kBitsForIntC;
                    int8_t sqrt2_c : kBitsForSqrt2C;
                };
            };
            uint8_t denom_exp : kBitsForDenomExp;
        };
        uint32_t data : 24; // 16-bit numerator + 8-bit exponent
    };

    // ---------- Construction ----------

    constexpr DyadicSqrt2(uint32_t data_ = 0) : data(data_) {}

    constexpr DyadicSqrt2(uint16_t numerator, uint8_t denom)
        : numerator_bits(numerator), denom_exp(denom) {}

    constexpr DyadicSqrt2(uint8_t int_coeff, uint8_t sqrt2_coeff, uint8_t denom)
        : int_c(int_coeff), sqrt2_c(sqrt2_coeff), denom_exp(denom) {}

private:
    // ---------- Helper bit ops (function replacements for Z2 macros) ----------

    static inline uint16_t byte_swap(uint16_t v) {
        return __builtin_bswap16(v);
    }

    static inline uint16_t u_middle_mask(uint8_t s) {
        uint32_t tmp = (uint32_t(kAxis) << s);
        tmp = ~(tmp - kAxis);
        return static_cast<uint16_t>(tmp);
    }

    static inline uint16_t lower_sign_extend(uint16_t x, uint8_t s) {
        uint16_t low_sign = static_cast<uint16_t>(x & 128u);
        uint16_t part1 = static_cast<uint16_t>((low_sign << (s + 1)) - low_sign);
        return static_cast<uint16_t>(part1 | (x & u_middle_mask(s)));
    }

    static inline uint16_t left_shift_and_swap(uint16_t n) {
        return byte_swap(static_cast<uint16_t>(n + (n & kSqrt2Mask)));
    }

    static inline uint16_t numerator_left_shift(uint16_t n, uint8_t s) {
        uint8_t half = static_cast<uint8_t>(s / 2u);
        return static_cast<uint16_t>((n << half) & u_middle_mask(half));
    }

    static inline uint16_t add_numerators(uint16_t left, uint16_t right) {
        uint16_t sum   = static_cast<uint16_t>((left + right) & kNumeratorMask);
        uint16_t carry = static_cast<uint16_t>(((left & kIntMask) + (right & kIntMask)) >> 8);
        return static_cast<uint16_t>(sum - static_cast<uint16_t>(carry << 8));
    }

public:
    // ---------- Shift operators ----------

    DyadicSqrt2& operator>>=(uint8_t shift) {
        numerator_bits = static_cast<uint16_t>(
            static_cast<int16_t>(lower_sign_extend(numerator_bits, shift)) >> shift);
        denom_exp = static_cast<uint8_t>((denom_exp - 2u * shift) * (numerator_bits != 0));
        return *this;
    }

    DyadicSqrt2& operator<<=(uint8_t shift) {
        numerator_bits = (numerator_bits << shift) & u_middle_mask(shift);
        denom_exp = (denom_exp + 2u * shift) * (numerator_bits != 0);
        return *this;
    }

    DyadicSqrt2 operator<<(uint8_t shift) const {
        DyadicSqrt2 ret = *this;
        ret <<= shift;
        return ret;
    }

    // ---------- Arithmetic ----------

    // Addition-assignment: mirrors Z2::operator+= switch on exponent difference
    DyadicSqrt2 operator+=(DyadicSqrt2 other) {
        const int8_t exp_diff = static_cast<int8_t>(denom_exp) - static_cast<int8_t>(other.denom_exp);
        switch (exp_diff) {
            // other shallower or equal (P cases, exp_diff >= 0)
            case 0: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(other.numerator_bits, 0));
                // N == 0 path includes reduce()
                reduce();
                break;
            }
            case 1: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(left_shift_and_swap(other.numerator_bits), 1));
                break;
            }
            case 2: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(other.numerator_bits, 2));
                break;
            }
            case 3: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(left_shift_and_swap(other.numerator_bits), 3));
                break;
            }
            case 4: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(other.numerator_bits, 4));
                break;
            }
            case 5: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(left_shift_and_swap(other.numerator_bits), 5));
                break;
            }
            case 6: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(other.numerator_bits, 6));
                break;
            }
            case 7: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(left_shift_and_swap(other.numerator_bits), 7));
                break;
            }
            case 8: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(other.numerator_bits, 8));
                break;
            }
            case 9: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(left_shift_and_swap(other.numerator_bits), 9));
                break;
            }
            case 10: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(other.numerator_bits, 10));
                break;
            }
            case 11: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(left_shift_and_swap(other.numerator_bits), 11));
                break;
            }
            case 12: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(other.numerator_bits, 12));
                break;
            }
            case 13: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(left_shift_and_swap(other.numerator_bits), 13));
                break;
            }
            case 14: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(other.numerator_bits, 14));
                break;
            }
            case 15: {
                numerator_bits = add_numerators(
                    numerator_bits,
                    numerator_left_shift(left_shift_and_swap(other.numerator_bits), 15));
                break;
            }

            // other deeper (N cases, exp_diff < 0)
            case -1: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(left_shift_and_swap(numerator_bits), 1));
                denom_exp = other.denom_exp;
                break;
            }
            case -2: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(numerator_bits, 2));
                denom_exp = other.denom_exp;
                break;
            }
            case -3: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(left_shift_and_swap(numerator_bits), 3));
                denom_exp = other.denom_exp;
                break;
            }
            case -4: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(numerator_bits, 4));
                denom_exp = other.denom_exp;
                break;
            }
            case -5: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(left_shift_and_swap(numerator_bits), 5));
                denom_exp = other.denom_exp;
                break;
            }
            case -6: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(numerator_bits, 6));
                denom_exp = other.denom_exp;
                break;
            }
            case -7: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(left_shift_and_swap(numerator_bits), 7));
                denom_exp = other.denom_exp;
                break;
            }
            case -8: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(numerator_bits, 8));
                denom_exp = other.denom_exp;
                break;
            }
            case -9: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(left_shift_and_swap(numerator_bits), 9));
                denom_exp = other.denom_exp;
                break;
            }
            case -10: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(numerator_bits, 10));
                denom_exp = other.denom_exp;
                break;
            }
            case -11: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(left_shift_and_swap(numerator_bits), 11));
                denom_exp = other.denom_exp;
                break;
            }
            case -12: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(numerator_bits, 12));
                denom_exp = other.denom_exp;
                break;
            }
            case -13: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(left_shift_and_swap(numerator_bits), 13));
                denom_exp = other.denom_exp;
                break;
            }
            case -14: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(numerator_bits, 14));
                denom_exp = other.denom_exp;
                break;
            }
            case -15: {
                numerator_bits = add_numerators(
                    other.numerator_bits,
                    numerator_left_shift(left_shift_and_swap(numerator_bits), 15));
                denom_exp = other.denom_exp;
                break;
            }

            default: {
                numerator_bits = 0;
                denom_exp = 0;
                break;
            }
        }
        return *this;
    }

    DyadicSqrt2 operator-=(const DyadicSqrt2& other) {
        if (other.numerator_bits == 0) {
            // Mirror Z2 quirk: construct from numerator_bits only
            return DyadicSqrt2(static_cast<uint32_t>(numerator_bits));
        }
        return *this += (-other);
    }

    DyadicSqrt2& operator*=(const DyadicSqrt2& other) {
        numerator_bits = numerator_bits & kIntMask;
        int_c   = int_c * other.int_c + ((sqrt2_c * other.sqrt2_c) << 1);
        sqrt2_c = int_c * other.sqrt2_c + sqrt2_c * other.int_c;
        denom_exp = denom_exp + other.denom_exp;
        return *this;
    }

    DyadicSqrt2 operator+(DyadicSqrt2 other) const {
        DyadicSqrt2 ret = *this;
        ret += other;
        return ret;
    }

    DyadicSqrt2 operator-(const DyadicSqrt2& other) const {
        DyadicSqrt2 ret = *this;
        return ret -= other;
    }

    DyadicSqrt2 operator*(const DyadicSqrt2& other) const {
        DyadicSqrt2 ret = *this;
        return ret *= other;
    }

    DyadicSqrt2 operator-() const {
        return DyadicSqrt2(
            static_cast<uint16_t>((kAxis - numerator_bits) * (numerator_bits != 0)),
            denom_exp);
    }

    // ---------- Comparison ----------

#if __cpp_impl_three_way_comparison
    std::strong_ordering operator<=>(const DyadicSqrt2& other) const {
        return data * ((numerator_bits & kIntMask) != 0)
            <=> other.data * ((other.numerator_bits & kIntMask) != 0);
    }
#else
    bool operator<(const DyadicSqrt2& other) const {
        return data * (int_c != 0) < other.data * (other.int_c != 0);
    }
    bool operator>(const DyadicSqrt2& other) const {
        return data * (int_c != 0) > other.data * (other.int_c != 0);
    }
    bool operator<=(const DyadicSqrt2& other) const {
        return !(*this > other);
    }
    bool operator>=(const DyadicSqrt2& other) const {
        return !(*this < other);
    }
    bool operator!=(const DyadicSqrt2& other) const {
        return !(*this == other);
    }
#endif

    bool operator==(const DyadicSqrt2& other) const {
        return data * ((numerator_bits & kIntMask) != 0)
            == other.data * ((other.numerator_bits & kIntMask) != 0);
    }

    DyadicSqrt2& operator=(const DyadicSqrt2& other) {
        data = other.data;
        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const DyadicSqrt2& z) {
        return os << static_cast<int>(z.int_c) << ","
                  << static_cast<int>(z.sqrt2_c) << "e"
                  << static_cast<int>(z.denom_exp);
    }

    // ---------- Utilities ----------

    void reduce() {
        const uint8_t int_zeros = std::countr_zero(static_cast<uint8_t>(int_c & 0xFF));
        const uint8_t sq_zeros  = std::countr_zero(static_cast<uint8_t>(sqrt2_c & 0xFF));

        if (int_zeros > sq_zeros) {
            int_c >>= 1;
            numerator_bits = byte_swap(static_cast<uint16_t>(
                static_cast<int16_t>(lower_sign_extend(numerator_bits, sq_zeros)) >> sq_zeros));
            denom_exp = denom_exp - (2u * sq_zeros + 1u);
            return;
        }
        *this >>= int_zeros;
    }
};

namespace std {
    inline DyadicSqrt2 abs(const DyadicSqrt2& z) {
        int num = (z.int_c < 0) ? DyadicSqrt2::kAxis - z.numerator_bits : z.numerator_bits;
        return DyadicSqrt2(static_cast<uint16_t>(num), z.denom_exp);
    }

    template <>
    struct hash<DyadicSqrt2> {
        std::size_t operator()(const DyadicSqrt2& z) const {
            return z.data & 0xFFFFFFu;
        }
    };
}

#endif // DYADIC_SQRT2_HPP
