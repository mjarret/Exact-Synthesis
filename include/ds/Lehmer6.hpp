#pragma once
#include <array>
#include <cstdint>
#include <cassert>
#include <ostream>
#include <bit>
#include <span>
#include <algorithm>

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
    // Exactly 10 information bits (rank in [0,719]) plus one sentinel (720).
    uint16_t code_ : 10;

public:
    // Number of real permutation ranks and the reserved sentinel code.
    static constexpr uint16_t NUM_RANKS = 720u;        // valid ranks: 0..719
    static constexpr uint16_t SENTINEL  = 720u;        // reserved "not yet canonicalized"

private:

    static constexpr uint16_t FACT[7] = {1, 1, 2, 6, 24, 120, 720};

    // -------- Fast LUTs from bak-2 (packed-15 digits and encode5) --------
    // Pack only the first five digits as a base-6 number (MSD first).
    static inline uint16_t pack_base6_5(std::span<const uint8_t, 6> perm) {
        uint32_t key = 0;
        for (int i = 0; i < 5; ++i) key = key * 6u + perm[static_cast<size_t>(i)];
        return static_cast<uint16_t>(key); // 6^5 = 7776 < 2^16
    }

    // Encode LUT keyed by the first five digits (0..7775). Invalid prefixes -> 0xFFFF.
    static inline const std::array<uint16_t, 7776>& encode5_table() {
        static const std::array<uint16_t, 7776> tbl = [] {
            std::array<uint8_t, 6> perms{0,1,2,3,4,5};
            std::array<uint16_t, 7776> t{};
            t.fill(0xFFFFu);
            uint16_t r = 0;
            do {
                uint32_t key = 0;
                for (int i = 0; i < 5; ++i) key = key * 6u + perms[static_cast<size_t>(i)];
                t[key] = r++;
            } while (std::next_permutation(perms.begin(), perms.end()));
            return t;
        }();
        return tbl;
    }

    // Packed-15 table: rank -> five 3-bit digits (p[0..4])
    static inline const std::array<uint16_t, 720>& packed15_table() {
        static const std::array<uint16_t, 720> T = []{
            std::array<uint16_t, 720> a{};
            std::array<uint8_t, 6> perms{0,1,2,3,4,5};
            uint16_t r = 0;
            do {
                uint16_t b = static_cast<uint16_t>((perms[0]) | (perms[1] << 3) | (perms[2] << 6) | (perms[3] << 9) | (perms[4] << 12));
                a[r++] = b;
            } while (std::next_permutation(perms.begin(), perms.end()));
            return a;
        }();
        return T;
    }

    // Final digit LUT: 15-bit pack -> p[5] (0..5). Unused entries are 0xFF.
    static inline const std::array<uint8_t, (1u << 15)>& final_digit_by_pack() {
        static const std::array<uint8_t, (1u << 15)> L = []{
            std::array<uint8_t, (1u << 15)> a{};
            a.fill(0xFFu);
            const auto& P15 = packed15_table();
            for (uint16_t r = 0; r < 720; ++r) {
                const uint16_t pack = P15[r];
                uint16_t used = 0;
                used |= 1u << ( ( pack       ) & 0x7u );
                used |= 1u << ( ( pack >>  3 ) & 0x7u );
                used |= 1u << ( ( pack >>  6 ) & 0x7u );
                used |= 1u << ( ( pack >>  9 ) & 0x7u );
                used |= 1u << ( ( pack >> 12 ) & 0x7u );
                const uint16_t missing = static_cast<uint16_t>((~used) & 0x3Fu);
                a[pack] = static_cast<uint8_t>(std::countr_zero(missing));
            }
            return a;
        }();
        return L;
    }

    // On-demand global decoding table: rank (0..719) -> permutation array, plus
    // one extra entry at SENTINEL used only as a safe fallback.
    static inline const std::array<std::array<uint8_t, 6>, 721>& decoding_table() {
        static const std::array<std::array<uint8_t, 6>, 721> tbl = [] {
            std::array<std::array<uint8_t, 6>, 721> t{};
            const auto& P15 = packed15_table();
            const auto& F   = final_digit_by_pack();
            for (uint16_t r = 0; r < NUM_RANKS; ++r) {
                const uint16_t b = P15[r];
                std::array<uint8_t,6> p{};
                p[0] = static_cast<uint8_t>( b        & 0x7u);
                p[1] = static_cast<uint8_t>((b >> 3) & 0x7u);
                p[2] = static_cast<uint8_t>((b >> 6) & 0x7u);
                p[3] = static_cast<uint8_t>((b >> 9) & 0x7u);
                p[4] = static_cast<uint8_t>((b >>12) & 0x7u);
                p[5] = F[b];
                t[r] = p;
            }
            // Sentinel rank decodes to identity; this value is never used for
            // canonical permutations, only as a "not yet canonicalized" marker.
            t[SENTINEL] = {0,1,2,3,4,5};
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
        return static_cast<uint16_t>(std::countr_zero(mask));
    }

public:
    // ----- Basics -----

    // Identity permutation [0,1,2,3,4,5] has rank 0.
    constexpr Lehmer6() : code_(0) {}

    // Construct from raw 10-bit value; reduced modulo NUM_RANKS+1 for safety
    // so that SENTINEL (720) can be preserved.
    explicit constexpr Lehmer6(uint16_t bits) : code_(bits % (NUM_RANKS + 1u)) {}

    // Factory: from lexicographic index (0..719).
    static constexpr Lehmer6 from_index(uint16_t idx) {
        Lehmer6 p;
        p.code_ = static_cast<uint16_t>(idx % (NUM_RANKS + 1u));
        return p;
    }

    // Factory: from a permutation array (must be a permutation of 0..5).
    static Lehmer6 from_perm(const std::array<uint8_t, 6>& perm) {
        return Lehmer6(encode(std::span<const uint8_t, 6>(perm)));
    }

    static Lehmer6 from_perm(const uint8_t (&perm)[6]) {
        return Lehmer6(encode(std::span<const uint8_t, 6>(perm)));
    }

    // Generic contiguous-view factory: accepts any contiguous block convertible
    // to span<const uint8_t, 6> with zero overhead.
    static Lehmer6 from_perm(std::span<const uint8_t, 6> perm) {
        return Lehmer6(encode(perm));
    }

    // Read back the stored 10-bit value (0..719).
    constexpr uint16_t bits() const { return static_cast<uint16_t>(code_); }

    // Synonym for bits() when you want the lexicographic rank.
    constexpr uint16_t index() const { return bits(); }

    // ----- Access -----

    // Element at position i (0..5) using packed-15 + final-digit LUT (no array materialization)
    uint8_t operator[](size_t i) const {
        const uint16_t b = packed15_table()[code_];
        return i<5 ? static_cast<uint8_t>((b >> (3 * i)) & 0x7u) : final_digit_by_pack()[b];
    }

    // Materialize the whole permutation via packed-15 + final-digit LUT.
    std::array<uint8_t, 6> to_array() const {
        std::array<uint8_t, 6> out{};
        const uint16_t idx = static_cast<uint16_t>(code_ % (NUM_RANKS + 1u));
        if (idx == SENTINEL) {
            out = {0,1,2,3,4,5};
            return out;
        }
        const uint16_t b = packed15_table()[idx];
        out[0] = static_cast<uint8_t>( b        & 0x7u);
        out[1] = static_cast<uint8_t>((b >> 3) & 0x7u);
        out[2] = static_cast<uint8_t>((b >> 6) & 0x7u);
        out[3] = static_cast<uint8_t>((b >> 9) & 0x7u);
        out[4] = static_cast<uint8_t>((b >>12) & 0x7u);
        out[5] = final_digit_by_pack()[b];
        return out;
    }

    // Zero-copy accessors to the decoding row to avoid by-value array copies
    static inline const std::array<uint8_t, 6>& decode_ref(uint16_t bits) {
        return decoding_table()[static_cast<size_t>(bits % (NUM_RANKS + 1u))];
    }
    static inline const uint8_t* decode_ptr(uint16_t bits) {
        return decoding_table()[static_cast<size_t>(bits % (NUM_RANKS + 1u))].data();
    }

    // Apply this permutation to an input array of length 6 (in-place).
    template <typename T>
    void apply(T (&arr)[6]) const {
        auto perm = to_array();
        T tmp[6];
        for (int i = 0; i < 6; ++i) tmp[i] = arr[perm[i]];
        for (int i = 0; i < 6; ++i) arr[i] = tmp[i];
    }

    // Inverse permutation: p_inv such that p_inv[p[i]] = i.
    Lehmer6 inverse() const {
        auto p = to_array();
        std::array<uint8_t,6> inv{};
        for (uint8_t i = 0; i < 6; ++i) inv[p[i]] = i;
        return Lehmer6::from_perm(inv);
    }

    // ----- Iteration -----

    // Steps to the next rank; returns false on wrap-around (after 719 -> 0).
    [[nodiscard]] bool next_permutation() {
        if (code_ + 1u < NUM_RANKS) { ++code_; return true; }
        code_ = 0u;
        return false;
    }

    // Optional: previous in this enumeration; false on wrap-around.
    [[nodiscard]] bool prev_permutation() {
        if (code_ > 0u) { --code_; return true; }
        code_ = static_cast<uint16_t>(NUM_RANKS - 1u);
        return false;
    }

    // ----- Translation: permutation <-> 10-bit code -----

    // Encode permutation (array of 6 unique values in 0..5) to rank in [0,719].
    static uint16_t encode(std::span<const uint8_t, 6> perm) {
        const uint32_t key5 = pack_base6_5(perm);
        return encode5_table()[key5];
    }

    // Decode rank in [0,1023] (only 0..719 are used) back to permutation (packed-15 + final-digit LUT).
    static std::array<uint8_t, 6> decode(uint16_t bits) {
        bits = static_cast<uint16_t>(bits % (NUM_RANKS + 1u));
        if (bits == SENTINEL) {
            return {0,1,2,3,4,5};
        }
        std::array<uint8_t, 6> out{};
        const uint16_t b = packed15_table()[bits];
        out[0] = static_cast<uint8_t>( b        & 0x7u);
        out[1] = static_cast<uint8_t>((b >> 3) & 0x7u);
        out[2] = static_cast<uint8_t>((b >> 6) & 0x7u);
        out[3] = static_cast<uint8_t>((b >> 9) & 0x7u);
        out[4] = static_cast<uint8_t>((b >>12) & 0x7u);
        out[5] = final_digit_by_pack()[b];
        return out;
    }

    // ----- Comparison / streaming -----

    friend bool operator==(const Lehmer6& a, const Lehmer6& b) { return a.code_ == b.code_; }
    friend bool operator!=(const Lehmer6& a, const Lehmer6& b) { return !(a == b); }
    friend bool operator==(const Lehmer6& a, uint16_t b) { return a.code_ == b; }

    // Composition: applies the left-hand permutation to the right-hand permutation.
    // Result r satisfies r[i] = lhs[rhs[i]] for i in 0..5.
    friend inline Lehmer6 operator*(const Lehmer6& lhs, const Lehmer6& rhs) {
        std::array<uint8_t, 6> out{};
        const auto& L = Lehmer6::decode_ref(lhs.bits());
        const auto& R = Lehmer6::decode_ref(rhs.bits());
        for (int i = 0; i < 6; ++i) {
            out[static_cast<size_t>(i)] = L[static_cast<size_t>(R[static_cast<size_t>(i)])];
        }
        return Lehmer6::from_perm(out);
    }

    friend std::ostream& operator<<(std::ostream& os, const Lehmer6& p) {
        auto a = p.to_array();
        os << '[' << int(a[0]);
        for (int i = 1; i < 6; ++i) os << ' ' << int(a[static_cast<size_t>(i)]);
        os << ']';
        return os;
    }
};

static_assert((1u << 10) >= 721u, "Need at most 10 bits for 721 states (including sentinel).");
