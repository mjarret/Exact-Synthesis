#pragma once
#include <array>
#include <cstdint>
#include <cassert>
#include <ostream>
#include <bit>
#include "util/assume.hpp"

/// Lehmer6
/// --------
/// Compact representation of permutations of {0,1,2,3,4,5} using exactly 10 bits
/// of information (the lexicographic rank 0..719 via Lehmer/factoradic code).
///
/// - operator[](i) returns the element at position i
/// - next_permutation() steps to the next code (wraps after 719 -> 0)
/// - encode()/decode() translate between the 6-element permutation and the 10-bit code
///
/// Note: As with all C++ bit-fields, the physical object size is implementation-defined.
/// The *information content* is exactly 10 bits. For tightly packed arrays, store
/// `bits()` (0..719) into a custom 10-bit bit-packed container.
class Lehmer6 {
    // Exactly 10 information bits (rank in [0,719]).
    uint16_t code_ : 10;

    static constexpr uint16_t FACT[7] = {1, 1, 2, 6, 24, 120, 720};

    // On-demand global decoding table: rank (0..719) -> permutation array
    static inline const std::array<std::array<uint8_t, 6>, 720>& decoding_table() {
        static const std::array<std::array<uint8_t, 6>, 720> tbl = [] {
            std::array<std::array<uint8_t, 6>, 720> t{};
            for (uint16_t r = 0; r < 720; ++r) {
                t[static_cast<size_t>(r)] = decode(r);
            }
            return t;
        }();
        return tbl;
    }

    // popcount for <= 6-bit masks using hardware popcnt (C++20 <bit>)
    static inline uint16_t popcount6(uint16_t x) {
        return std::popcount(x & 0x3Fu);
    }

    // Select the k-th set bit (0-based) using TZCNT/CTZ; mask uses only bits 0..5
    static inline uint16_t select_kth(uint16_t mask, uint16_t k) {
        mask &= 0x3Fu;
        // Remove k lowest set bits, then return index of next set bit
        while (k--) {
            // precondition: k < popcount(mask)
            mask &= (mask - 1);
        }
        ASSUME(mask != 0);
        return static_cast<uint16_t>(std::countr_zero(mask));
    }

public:
    // ----- Basics -----

    // Identity permutation [0,1,2,3,4,5] has rank 0.
    constexpr Lehmer6() : code_(0) {}

    // Construct from raw 10-bit value; reduced modulo 720 for safety.
    explicit constexpr Lehmer6(uint16_t bits) : code_(bits % 720u) {}

    // Factory: from lexicographic index (0..719).
    static constexpr Lehmer6 from_index(uint16_t idx) {
        Lehmer6 p;
        p.code_ = static_cast<uint16_t>(idx % 720u);
        return p;
    }

    // Factory: from a permutation array (must be a permutation of 0..5).
    static Lehmer6 from_perm(const std::array<uint8_t, 6>& perm) {
        return Lehmer6(encode(perm));
    }

    // Read back the stored 10-bit value (0..719).
    constexpr uint16_t bits() const { return static_cast<uint16_t>(code_); }

    // Synonym for bits() when you want the lexicographic rank.
    constexpr uint16_t index() const { return bits(); }

    // ----- Access -----

    // Element at position i (0..5) via table lookup.
    uint8_t operator[](size_t i) const {
        ASSUME(i < 6);
        return decoding_table()[static_cast<size_t>(code_)][i];
    }

    // Materialize the whole permutation via table lookup.
    std::array<uint8_t, 6> to_array() const {
        return decoding_table()[static_cast<size_t>(code_)];
    }

    // ----- Iteration -----

    // Steps to the next rank; returns false on wrap-around (after 719 -> 0).
    [[nodiscard]] bool next_permutation() {
        if (code_ + 1u < 720u) { ++code_; return true; }
        code_ = 0u;
        return false;
    }

    // Optional: previous in this enumeration; false on wrap-around.
    [[nodiscard]] bool prev_permutation() {
        if (code_ > 0u) { --code_; return true; }
        code_ = 719u;
        return false;
    }

    // ----- Translation: permutation <-> 10-bit code -----

    // Encode permutation (array of 6 unique values in 0..5) to rank in [0,719].
    static uint16_t encode(const std::array<uint8_t, 6>& perm) {
        uint16_t mask = 0b111111u;
        uint16_t idx = 0;
        for (int i = 0; i < 6; ++i) {
            const uint16_t x = perm[static_cast<size_t>(i)];
            ASSUME(x < 6);
            // rank of x among remaining symbols:
            const uint16_t r = popcount6(mask & ((1u << x) - 1u));
            ASSUME(mask & (1u << x));     // x must be available
            idx = static_cast<uint16_t>(idx + r * FACT[5 - i]);
            mask &= ~(1u << x);
        }
        ASSUME(idx < 720);
        return idx; // fits in exactly 10 bits
    }

    // Decode rank in [0,1023] (only 0..719 are used) back to permutation.
    static std::array<uint8_t, 6> decode(uint16_t bits) {
        bits = static_cast<uint16_t>(bits % 720u);
        std::array<uint8_t, 6> out{};
        uint16_t mask = 0b111111u;
        for (int i = 0; i < 6; ++i) {
            const uint16_t f = FACT[5 - i];
            const uint16_t q = bits / f;
            bits %= f;
            const uint16_t v = select_kth(mask, q);
            out[static_cast<size_t>(i)] = static_cast<uint8_t>(v);
            mask &= ~(1u << v);
        }
        return out;
    }

    // ----- Comparison / streaming -----

    friend bool operator==(const Lehmer6& a, const Lehmer6& b) { return a.code_ == b.code_; }
    friend bool operator!=(const Lehmer6& a, const Lehmer6& b) { return !(a == b); }

    friend std::ostream& operator<<(std::ostream& os, const Lehmer6& p) {
        auto a = p.to_array();
        os << '[' << int(a[0]);
        for (int i = 1; i < 6; ++i) os << ' ' << int(a[static_cast<size_t>(i)]);
        os << ']';
        return os;
    }
};

static_assert((1u << 10) >= 720u, "Need at most 10 bits for 720 states.");
