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

#include <bit>
#include <cassert>
#include <charconv>
#include <climits>
#include <compare>
#include <cstdint>
#include <iostream>
#include <string>
#include <string_view>
#include <type_traits>

using word_t = uint64_t;
using limb_t = uint32_t;
using int_t  = int16_t;

struct DyadicSqrt2 {
    // Layout constants (mirroring Z2.hpp)
    static constexpr uint8_t kBitsForNumerator = sizeof(limb_t) * CHAR_BIT;
    static constexpr uint8_t kBitsForIntC      = kBitsForNumerator/2;
    static constexpr uint8_t kBitsForSqrt2C    = kBitsForNumerator/2;
    static constexpr uint8_t kBitsForDenomExp  = 8;

    static constexpr limb_t kAxis           = limb_t(1ull << kBitsForIntC);
    static constexpr limb_t kIntMask        = limb_t(kAxis - 1ull);
    static constexpr limb_t kNumeratorMask  = limb_t((word_t(1ull) << kBitsForNumerator) - 1u);
    static constexpr limb_t kSqrt2Mask      = limb_t(kNumeratorMask & ~kIntMask);

    union {
        struct {
            union {
                limb_t numerator_bits : kBitsForNumerator; // packed int_c + sqrt2_c
                struct {
                    int_t int_c   : kBitsForIntC;
                    int_t sqrt2_c : kBitsForSqrt2C;
                };
            };
            uint8_t denom_exp : kBitsForDenomExp;
        };
        word_t data : kBitsForNumerator + kBitsForDenomExp; // 16-bit numerator + 8-bit exponent
    };

    // ---------- Construction ----------

    constexpr DyadicSqrt2(word_t data_ = 0) : data(data_) {}

    constexpr DyadicSqrt2(limb_t numerator, uint8_t denom)
        : numerator_bits(numerator), denom_exp(denom) {}

    constexpr DyadicSqrt2(int_t int_coeff, int_t sqrt2_coeff, uint8_t denom)
        : int_c(int_coeff), sqrt2_c(sqrt2_coeff), denom_exp(denom) {}

    explicit DyadicSqrt2(std::string_view s) : data(0) {
        const char* p = s.data();
        const char* end = p + s.size();

        int ic = 0;
        auto r1 = std::from_chars(p, end, ic);
        assert(r1.ec == std::errc{});
        p = r1.ptr;

        assert(p < end && *p == ',');
        ++p;

        int sc = 0;
        auto r2 = std::from_chars(p, end, sc);
        assert(r2.ec == std::errc{});
        p = r2.ptr;

        assert(p < end && *p == 'e');
        ++p;

        unsigned de = 0;
        auto r3 = std::from_chars(p, end, de);
        assert(r3.ec == std::errc{});
        p = r3.ptr;

        assert(p == end);

        int_c = static_cast<int_t>(ic);
        sqrt2_c = static_cast<int_t>(sc);
        denom_exp = static_cast<uint8_t>(de);
        if (numerator_bits == 0) denom_exp = 0;
    }

    explicit DyadicSqrt2(const std::string& s) : DyadicSqrt2(std::string_view{s}) {}

private:
    // ---------- Helper bit ops (function replacements for Z2 macros) ----------

    static inline limb_t swap_numerator(limb_t v) {
        return std::rotr(v, kBitsForIntC);
    }

    static inline limb_t u_middle_mask(uint8_t s) {
        word_t tmp = (word_t(kAxis) << s);
        tmp = ~(tmp - kAxis);
        return static_cast<limb_t>(tmp);
    }

    static inline limb_t lower_sign_extend(limb_t x, uint8_t s) {
        // Sign bit for the low coefficient lives in the top bit of the
        // int_c field, whose width is kBitsForIntC.
        const limb_t sign_bit = static_cast<limb_t>(limb_t(1u) << (kBitsForIntC - 1u));
        limb_t low_sign = static_cast<limb_t>(x & sign_bit);
        limb_t part1 = static_cast<limb_t>((low_sign << (s + 1u)) - low_sign);
        return static_cast<limb_t>(part1 | (x & u_middle_mask(s)));
    }

    static inline limb_t left_shift_and_swap(limb_t n) {
        return swap_numerator(static_cast<limb_t>(n + (n & kSqrt2Mask)));
    }

    static inline limb_t numerator_left_shift(limb_t n, uint8_t s) {
        uint8_t half = static_cast<uint8_t>(s / 2u);
        return static_cast<limb_t>((n << half) & u_middle_mask(half));
    }

    static inline limb_t add_numerators(limb_t left, limb_t right) {
        limb_t sum   = static_cast<limb_t>((left + right) & kNumeratorMask);
        // Carry is the overflow from the int_c half; shift by the number
        // of bits in that half rather than a hard-coded 8.
        limb_t carry = static_cast<limb_t>(((left & kIntMask) + (right & kIntMask)) >> kBitsForIntC);
        return static_cast<limb_t>(sum - static_cast<limb_t>(carry << kBitsForIntC));
    }

public:
    // ---------- Shift operators ----------

    inline __attribute__((always_inline))
    DyadicSqrt2& operator>>=(uint8_t shift) {
        numerator_bits = static_cast<limb_t>(
            static_cast<std::make_signed_t<limb_t>>(lower_sign_extend(numerator_bits, shift)) >> shift);
        denom_exp = static_cast<uint8_t>((denom_exp - 2u * shift) * (numerator_bits != 0));
        return *this;
    }

    inline __attribute__((always_inline))
    DyadicSqrt2& operator<<=(uint8_t shift) {
        limb_t mask = static_cast<limb_t>(~((kAxis << shift) - kAxis));
        numerator_bits = static_cast<limb_t>((numerator_bits << shift) & mask);
        denom_exp = static_cast<uint8_t>((denom_exp + 2u * shift) * (numerator_bits != 0));
        return *this;
    }

    inline __attribute__((always_inline))
    DyadicSqrt2 operator<<(uint8_t shift) const {
        DyadicSqrt2 ret = *this;
        ret <<= shift;
        return ret;
    }

    // ---------- Arithmetic ----------

    // Addition-assignment: generic, parity-driven implementation
    inline __attribute__((always_inline))
    DyadicSqrt2 operator+=(DyadicSqrt2 other) {
        const auto diff = denom_exp - other.denom_exp;
        const auto N = diff >= 0 ? diff : -diff;
        if (N >= kBitsForNumerator) {
            numerator_bits = 0;
            denom_exp = 0;
            return *this;
        }
        if (diff >= 0) {
            limb_t r = other.numerator_bits;
            if (N & 1u) r = left_shift_and_swap(r);
            r = numerator_left_shift(r, static_cast<uint8_t>(N));
            numerator_bits = add_numerators(numerator_bits, r);
            if (N == 0) reduce();
        } else {
            limb_t l = numerator_bits;
            if (N & 1u) l = left_shift_and_swap(l);
            l = numerator_left_shift(l, static_cast<uint8_t>(N));
            numerator_bits = add_numerators(other.numerator_bits, l);
            denom_exp = other.denom_exp;
        }
        return *this;
    }

    inline __attribute__((always_inline))
    DyadicSqrt2 operator-=(const DyadicSqrt2& other) {
        if (other.numerator_bits == 0) {
            return *this;
        }
        return *this += (-other);
    }

    inline __attribute__((always_inline))
    DyadicSqrt2& operator*=(const DyadicSqrt2& other) {
        numerator_bits = numerator_bits & kIntMask;
        int_c   = int_c * other.int_c + ((sqrt2_c * other.sqrt2_c) << 1);
        sqrt2_c = int_c * other.sqrt2_c + sqrt2_c * other.int_c;
        denom_exp = denom_exp + other.denom_exp;
        return *this;
    }

    inline __attribute__((always_inline))
    DyadicSqrt2 operator+(DyadicSqrt2 other) const {
        DyadicSqrt2 ret = *this;
        ret += other;
        return ret;
    }

    inline __attribute__((always_inline))
    DyadicSqrt2 operator-(const DyadicSqrt2& other) const {
        DyadicSqrt2 ret = *this;
        return ret -= other;
    }

    inline __attribute__((always_inline))
    DyadicSqrt2 operator*(const DyadicSqrt2& other) const {
        DyadicSqrt2 ret = *this;
        return ret *= other;
    }

    inline __attribute__((always_inline))
    DyadicSqrt2 operator-() const {
        return DyadicSqrt2(
            static_cast<limb_t>((kAxis - numerator_bits) * (numerator_bits != 0)),
            denom_exp);
    }

    // ---------- Comparison ----------

#if __cpp_impl_three_way_comparison
    inline __attribute__((always_inline))
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

    inline __attribute__((always_inline))
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

    inline __attribute__((always_inline))
    void reduce() {
        // Count leading zeros within each coefficient's logical width
        // (kBitsForIntC) instead of assuming 8 bits.
        const auto coeff_mask = static_cast<limb_t>((limb_t(1u) << kBitsForIntC) - 1u);
        const uint8_t int_zeros =
            std::countr_zero(static_cast<limb_t>(static_cast<limb_t>(int_c) & coeff_mask));
        const uint8_t sq_zeros  =
            std::countr_zero(static_cast<limb_t>(static_cast<limb_t>(sqrt2_c) & coeff_mask));

        if (int_zeros > sq_zeros) {
            int_c >>= 1;
            numerator_bits = swap_numerator(static_cast<limb_t>(
                static_cast<std::make_signed_t<limb_t>>(lower_sign_extend(numerator_bits, sq_zeros)) >> sq_zeros));
            denom_exp = denom_exp - (2u * sq_zeros + 1u);
            return;
        }
        *this >>= int_zeros;
    }
};

namespace std {
    inline DyadicSqrt2 abs(const DyadicSqrt2& z) {
        int num = (z.int_c < 0) ? DyadicSqrt2::kAxis - z.numerator_bits : z.numerator_bits;
        return DyadicSqrt2(static_cast<limb_t>(num), z.denom_exp);
    }

    template <>
    struct hash<DyadicSqrt2> {
        std::size_t operator()(const DyadicSqrt2& z) const {
            return z.data & ((word_t(1) << (DyadicSqrt2::kBitsForNumerator + DyadicSqrt2::kBitsForDenomExp)) - 1);
        }
    };
}

#endif // DYADIC_SQRT2_HPP
