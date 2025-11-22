// Compact encoding for an ordered list of distinct indices from {0..5}.
// Representation = (mask, rank)
//  - mask: 6-bit selection of elements (bit v set iff v is present)
//  - rank: Lehmer-like rank of the order among the k! permutations of the selected
//          elements, where k = popcount(mask). Elements are ordered with respect
//          to the ascending-sorted list of selected values.
#pragma once

#include <array>
#include <cstdint>
#include <bit>

namespace order6 {

static constexpr uint16_t FACT[7] = {1, 1, 2, 6, 24, 120, 720};

struct Order6 {
    // Packed 16-bit storage: [rank:10][mask:6]
    union {
        uint16_t packed{0};
        struct {
            // Lower 6 bits store the mask, upper 10 bits store the rank
            uint16_t mask_bf : 6;
            uint16_t rank_bf : 10;
        } bf;
    };

    static constexpr uint8_t popcnt(uint8_t x) { return static_cast<uint8_t>(std::popcount(x & 0x3Fu)); }

    static inline uint8_t select_kth(uint8_t mask, uint8_t k) {
        mask &= 0x3Fu;
        while (k--) mask &= (mask - 1);
        return static_cast<uint8_t>(std::countr_zero(mask));
    }

    // Accessors
    inline uint8_t mask() const { return static_cast<uint8_t>(bf.mask_bf & 0x3Fu); }
    inline uint16_t rank() const { return static_cast<uint16_t>(bf.rank_bf & 0x03FFu); }
    inline void set_mask_rank(uint8_t m, uint16_t r) {
        bf.mask_bf = static_cast<uint16_t>(m & 0x3Fu);
        bf.rank_bf = static_cast<uint16_t>(r & 0x03FFu);
    }
    inline void set_rank(uint16_t r) { bf.rank_bf = r & 0x03FFu; }

    uint8_t size() const { return popcnt(mask()); }

    // Encode from an array of size k with distinct values in 0..5
    static inline Order6 from_array(const uint8_t* a, uint8_t k) {
        Order6 o{};
        // Build mask
        uint8_t m = 0;
        for (uint8_t i = 0; i < k; ++i) { m |= static_cast<uint8_t>(1u << a[i]); }
        // Rank among permutations of the ascending-sorted selected values
        uint8_t rem = m;
        uint16_t r = 0;
        for (uint8_t i = 0; i < k; ++i) {
            uint8_t v = a[i];
            // Count how many remaining values < v
            uint8_t less = static_cast<uint8_t>(
                std::popcount(static_cast<unsigned>(rem & static_cast<uint8_t>((1u << v) - 1)))
            );
            r = static_cast<uint16_t>(r + less * FACT[k - 1 - i]);
            rem = static_cast<uint8_t>(rem & static_cast<uint8_t>(~(1u << v)));
        }
        o.set_mask_rank(m, r);
        return o;
    }

    // Decode into array of length k=popcount(mask) per current rank
    inline void to_array(uint8_t* out) const {
        uint8_t k = size();
        uint16_t bits = rank();
        uint8_t rem = mask();
        for (uint8_t i = 0; i < k; ++i) {
            uint16_t f = FACT[k - 1 - i];
            uint8_t q = static_cast<uint8_t>(bits / f);
            bits = static_cast<uint16_t>(bits % f);
            uint8_t v = select_kth(rem, q);
            out[i] = v;
            rem = static_cast<uint8_t>(rem & static_cast<uint8_t>(~(1u << v)));
        }
    }

    // Advance to next permutation order for the same mask; returns false on wrap
    inline bool next_permutation() {
        uint8_t k = size();
        uint16_t max = FACT[k];
        uint16_t r = rank();
        if (r+1 < max) { set_rank(r+1); return true; }
        set_rank(0u); return false;
    }

    // Reset order to ascending (rank=0)
    inline void reset() { set_rank(0u); }
};

static_assert(sizeof(Order6) == 2, "Order6 must remain 2 bytes (mask:6 + rank:10)");

} // namespace order6
