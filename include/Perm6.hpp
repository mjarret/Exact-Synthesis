#ifndef PERM6_NAMESPACE_H
#define PERM6_NAMESPACE_H

#include <array>
#include <cstdint>

#if defined(__GNUC__) || defined(__clang__)
    #define ALWAYS_INLINE inline __attribute__((always_inline))
#else
    #define ALWAYS_INLINE inline
#endif

namespace perm6 {

// Precomputed factorial lookup table for 0..6.
ALWAYS_INLINE constexpr std::array<int, 7> fact = {1, 1, 2, 6, 24, 120, 720};

// Internal helper: given a 6-bit mask, compute the index (0–5) of its n-th set bit.
// (For valid input, mask is assumed to have at least n+1 bits set.)
ALWAYS_INLINE constexpr int compute_nth_set_bit(uint8_t mask, int n) {
    int count = 0;
    for (int i = 0; i < 6; ++i) {
        if (mask & (1 << i)) {
            if (count == n)
                return i;
            ++count;
        }
    }
    return -1; // Not expected with valid input.
}

// Build a compile-time lookup table: for every 6-bit mask (0..63) and for n = 0..5,
// nth_set_bit_table[mask][n] yields the index of the n-th set bit.
ALWAYS_INLINE constexpr std::array<std::array<int, 6>, 64> make_nth_set_bit_table() {
    std::array<std::array<int, 6>, 64> table = {};
    for (int mask = 0; mask < 64; ++mask) {
        for (int n = 0; n < 6; ++n) {
            table[mask][n] = compute_nth_set_bit(static_cast<uint8_t>(mask), n);
        }
    }
    return table;
}

ALWAYS_INLINE constexpr auto nth_set_bit_table = make_nth_set_bit_table();

// Internal helper: return the index of the n-th set bit in a 6-bit mask using the lookup table.
ALWAYS_INLINE constexpr int nth_set_bit(uint8_t mask, int n) {
    return nth_set_bit_table[mask][n];
}

// Internal helper: decode the Lehmer code 'code' and return the element at permutation position 'pos'.
// (That is, for a permutation P, if P[pos] = j then decode_element(code, pos) returns j.)
ALWAYS_INLINE constexpr int decode_element(uint16_t code, int pos) {
    uint16_t temp = code;
    uint8_t mask = 0x3F; // All 6 numbers (bits 0..5) are initially available.
    for (int j = 0; j < pos; ++j) {
        int d = temp / fact[5 - j];
        temp %= fact[5 - j];
        int bit_index = nth_set_bit(mask, d);
        mask &= ~(1 << bit_index);
    }
    int d = temp / fact[5 - pos];
    int bit_index = nth_set_bit(mask, d);
    return bit_index;
}

// Public API: Given a Lehmer code (a uint16_t with value in [0,720)),
// compute and return the Lehmer code of its inverse permutation.
// (If the original permutation maps i -> j then the inverse maps j -> i.)
ALWAYS_INLINE constexpr uint16_t inverse(uint16_t code) {
    uint16_t invCode = 0;
    uint8_t avail = 0x3F; // All positions (0..5) available.
    for (int k = 0; k < 6; ++k) {
        int pos = -1;
        for (int i = 0; i < 6; ++i) {
            if (decode_element(code, i) == k) {
                pos = i;
                break;
            }
        }
        int digit = 0;
        for (int i = 0; i < pos; ++i) {
            if (avail & (1 << i))
                ++digit;
        }
        invCode = invCode * (6 - k) + digit;
        avail &= ~(1 << pos);
    }
    return invCode;
}

} // namespace perm6

#endif // PERM6_NAMESPACE_H
