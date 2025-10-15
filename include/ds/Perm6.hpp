// Compact permutation of 6 elements with fast access
#pragma once

#include <cstdint>
#include <array>

// Stores a permutation of {0..5} in 18 bits (6 entries * 3 bits).
// Provides O(1) get/set by index, and bulk encode/decode from/to array.
// Also provides optional Lehmer rank/unrank helpers (10-bit) for experiments.
struct Perm6 {
    uint32_t bits{0}; // only lower 18 bits used

    static constexpr uint8_t N = 6;

    // Pack 6 values (each 0..5) into bits
    static constexpr uint32_t pack(const std::array<uint8_t, N>& a) {
        uint32_t v = 0; uint32_t shift = 0;
        for (uint8_t i = 0; i < N; ++i, shift += 3) v |= (static_cast<uint32_t>(a[i] & 0x7) << shift);
        return v;
    }

    static constexpr std::array<uint8_t, N> unpack(uint32_t v) {
        std::array<uint8_t, N> a{}; uint32_t shift = 0;
        for (uint8_t i = 0; i < N; ++i, shift += 3) a[i] = static_cast<uint8_t>((v >> shift) & 0x7);
        return a;
    }

    static Perm6 from_array(const uint8_t p[6]) {
        std::array<uint8_t, N> a{}; for (int i=0;i<N;++i) a[i]=p[i];
        return Perm6{pack(a)};
    }

    void to_array(uint8_t out[6]) const {
        auto a = unpack(bits); for (int i=0;i<N;++i) out[i]=a[i];
    }

    // Fast get/set by index
    inline uint8_t get(int i) const { return static_cast<uint8_t>((bits >> (i*3)) & 0x7); }
    inline void set(int i, uint8_t v) {
        const uint32_t mask = ~(0x7u << (i*3));
        bits = (bits & mask) | (static_cast<uint32_t>(v & 0x7) << (i*3));
    }

    // Optional: Lehmer rank/unrank into 10 bits (720 states). Slower but minimal bits.
    static uint16_t rank10(const uint8_t p[6]) {
        // Compute Lehmer code then rank = sum c[i]*(5-i)!
        uint8_t used = 0; // bitset of used elements
        static constexpr int fact[6] = {120, 24, 6, 2, 1, 1};
        uint16_t r = 0;
        for (int i=0;i<6;++i) {
            int x = p[i];
            int less = 0; for (int v=0; v<x; ++v) less += ((used>>v)&1)==0;
            r += static_cast<uint16_t>(less * fact[i]);
            used |= (1u<<x);
        }
        return r; // 0..719
    }

    static void unrank10(uint16_t r, uint8_t out[6]) {
        // Inverse Lehmer: reconstruct permutation from rank
        static constexpr int fact[6] = {120, 24, 6, 2, 1, 1};
        uint8_t avail[6] = {0,1,2,3,4,5}; int n=6;
        for (int i=0;i<6;++i) {
            int idx = r / fact[i]; r %= fact[i];
            out[i] = avail[idx];
            for (int k=idx; k<n-1; ++k) avail[k]=avail[k+1];
            --n;
        }
    }
};

