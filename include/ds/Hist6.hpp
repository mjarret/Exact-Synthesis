// Compact encoder for histograms (c0..c5) with non-negative counts and sum ≤ 6.
// Number of weak compositions with up to 6 items across 6 slots equals
//   sum_{s=0..6} C(s+6-1, 6-1) = C(12,6) = 924 → fits in 10 bits.
// We encode by adding a slack part c6 = 6 - sum(ci), then encoding the 7-tuple
// (c0..c5,c6) as a composition of 6 into 7 parts.

#pragma once

#include <array>
#include <cstdint>
#include "util/assume.hpp"

struct Hist6 {
    uint16_t code{0}; // 0..923

    static constexpr uint8_t slots = 6;   // visible slots
    static constexpr uint8_t parts = 7;   // with slack
    static constexpr uint8_t total = 6;   // total items
    static constexpr uint16_t states = 924; // C(12,6)

    // Small safe binomial for n<=12, k<=6
    static inline uint16_t binom(int n, int k) {
        if (k < 0 || k > n) return 0;
        if (k == 0 || k == n) return 1;
        if (k > n - k) k = n - k;
        uint32_t r = 1;
        for (int i = 1; i <= k; ++i) {
            r = static_cast<uint32_t>((r * static_cast<uint32_t>(n - k + i)) / static_cast<uint32_t>(i));
        }
        return static_cast<uint16_t>(r);
    }

    // Number of weak compositions of n into r parts: C(n+r-1, r-1)
    static inline uint16_t compositions(int n, int r) {
        return binom(n + r - 1, r - 1);
    }

    // Encode counts (array of 6 non-negative integers with sum ≤ 6) into rank 0..923.
    static inline Hist6 from_counts_leq(const std::array<uint8_t, 6>& c) {
        int sum = 0;
        for (int i = 0; i < 6; ++i) sum += c[static_cast<size_t>(i)];
        ASSUME(sum <= total);
        // Build 7-part composition by appending slack = 6 - sum
        int slack = total - sum;
        uint16_t rank = 0;
        int n = total;    // remaining items
        int r = parts;    // remaining parts
        // Encode (c0..c5, slack) lexicographically
        for (int i = 0; i < 6; ++i) {
            int ci = c[static_cast<size_t>(i)];
            for (int x = 0; x < ci; ++x) {
                rank = static_cast<uint16_t>(rank + compositions(n - x, r - 1));
            }
            n -= ci;
            --r;
        }
        // Final part is slack; no contribution to rank needed
        Hist6 out{}; out.code = rank;
        return out;
    }

    // Decode to counts (c0..c5), ignoring slack.
    inline std::array<uint8_t, 6> to_counts() const {
        std::array<uint8_t, 6> c{};
        uint16_t rnk = code;
        int n = total; // remaining items
        int r = parts; // remaining parts
        for (int i = 0; i < 6; ++i) {
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
        // Final slack is n (ignored)
        return c;
    }
};

static_assert(Hist6::states == 924, "Expected 924 compositions with sum ≤ 6 across 6 slots");
static_assert(sizeof(Hist6) == 2, "Hist6 stores 10-bit code in 16-bit field");

