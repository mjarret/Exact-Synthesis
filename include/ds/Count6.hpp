// Compact encoder for histograms (c0..c5) with non-negative counts summing to at most 6.
// Number of weak compositions with sum ≤ 6 into 6 parts equals C(6+6, 6) = C(12,6) = 924 → fits in 10 bits.
//
// Encoding order: lexicographic on (c0,c1,c2,c3,c4,c5).
// API is intentionally minimal and meant for cold paths (finalized/serialized forms),
// not for hot ++/-- updates (SmallFreqMap remains optimal there).

#pragma once

#include <array>
#include <cstdint>
#include "util/assume.hpp"

struct Count6 {
    // Encoded rank in [0, states-1]
    uint16_t code{0};

    static constexpr uint8_t parts = 6;   // number of slots
    static constexpr uint8_t total = 6;   // maximum sum of counts (≤ total)
    static constexpr uint16_t states = 924; // C(12,6) = sum_{s=0..6} C(s+5,5)

    // Binomial for small ranges (preconditions held by caller): n<=11, 0<=k<=min(n,6)
    // No runtime checks; encode assumptions for the compiler.
    static inline uint16_t binom(int n, int k) {
        ASSUME(n >= 0);
        ASSUME(k >= 0);
        ASSUME(k <= n);
        ASSUME(n <= 11);
        ASSUME(k <= 6);
        int kk = k;
        if (kk > n - kk) kk = n - kk; // use symmetry; kk in [0, floor(n/2)]
        uint32_t r = 1;
        for (int i = 1; i <= kk; ++i) {
            r = static_cast<uint32_t>( (r * static_cast<uint32_t>(n - kk + i)) / static_cast<uint32_t>(i) );
        }
        return static_cast<uint16_t>(r);
    }

    // Number of weak compositions of n into r parts: C(n+r-1, r-1)
    static inline uint16_t compositions(int n, int r) {
        ASSUME(n >= 0);
        ASSUME(r >= 1);
        return binom(n + r - 1, r - 1);
    }

    // Encode counts (array of 6 non-negative integers with sum ≤ 6) into rank 0..(states-1).
    static inline Count6 from_counts(const std::array<uint8_t, 6>& c) {
        int sum = 0;
        for (int i = 0; i < 6; ++i) sum += c[static_cast<size_t>(i)];
        ASSUME(sum <= total);
        Count6 out{};
        uint16_t rank = 0;
        // Model sum≤total by adding a final implicit slack part so that
        // we rank as exact compositions over (parts+1) with total = 6.
        int n = total;        // remaining total to distribute (including slack)
        int r = parts + 1;    // remaining parts including slack
        for (int i = 0; i < parts; ++i) {
            const int ci = c[static_cast<size_t>(i)];
            // add all compositions with leading value < ci
            for (int x = 0; x < ci; ++x) {
                rank = static_cast<uint16_t>(rank + compositions(n - x, r - 1));
            }
            n -= ci;
            --r;
        }
        // Final slack is determined by remaining n
        out.code = rank;
        return out;
    }

    // Decode rank (0..states-1) to counts with sum ≤ 6.
    inline std::array<uint8_t, 6> to_counts() const {
        std::array<uint8_t, 6> c{};
        uint16_t rnk = code;
        int n = total;      // remaining total (including slack)
        int r = parts + 1;  // remaining parts including slack
        for (int i = 0; i < parts; ++i) {
            int x = 0;
            while (x <= n) {
                uint16_t cnt = compositions(n - x, r - 1);
                if (cnt <= rnk) { rnk = static_cast<uint16_t>(rnk - cnt); ++x; }
                else { break; }
            }
            c[static_cast<size_t>(i)] = static_cast<uint8_t>(x);
            n -= x;
            --r;
        }
        // Remaining n is the implicit slack (ignored)
        return c;
    }
};

static_assert(Count6::states == 924, "Expected 924 weak compositions of ≤6 into 6 parts");
static_assert(sizeof(Count6) == 2, "Count6 stores ≤10-bit code in 16-bit field");
