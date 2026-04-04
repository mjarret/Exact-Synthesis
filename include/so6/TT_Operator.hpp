#pragma once

#include <array>
#include <cstdint>
#include "so6/T_Operator.hpp"

/**
 * @brief Entry in the 165-element paired T-gate alphabet.
 *
 * Each entry represents the compound operation T_{t_second} * T_{t_first},
 * i.e., apply t_first first, then t_second.
 */
struct TT_Entry {
    uint8_t t_first;
    uint8_t t_second;
};

/**
 * @brief 165 compound alphabets for paired T-gate BFS.
 *
 * 45 commuting pairs (disjoint row sets, canonical order i<j)
 * + 120 non-commuting pairs (both orderings)
 * = 165 total.
 *
 * Commuting pairs satisfy: rows(T_i) ∩ rows(T_j) = ∅,
 * meaning T_i * T_j = T_j * T_i, so we keep only i < j.
 */
inline constexpr auto TT_ALPHABET = []() {
    constexpr std::array<std::pair<int,int>, 15> T_PAIRS = {{
        {0,1},{0,2},{0,3},{0,4},{0,5},
        {1,2},{1,3},{1,4},{1,5},
        {2,3},{2,4},{2,5},
        {3,4},{3,5},{4,5}
    }};

    std::array<TT_Entry, 165> result{};
    int count = 0;
    for (int i = 0; i < 15; ++i) {
        for (int j = 0; j < 15; ++j) {
            if (i == j) continue;  // T^2 = I
            auto [r1i, r2i] = T_PAIRS[i];
            auto [r1j, r2j] = T_PAIRS[j];
            bool disjoint = (r1i != r1j && r1i != r2j &&
                             r2i != r1j && r2i != r2j);
            if (disjoint && i > j) continue;  // commuting: keep i<j only
            result[count++] = {static_cast<uint8_t>(i),
                               static_cast<uint8_t>(j)};
        }
    }
    return result;
}();

static_assert(TT_ALPHABET.size() == 165);

/**
 * @brief Precomputed row pairs for each of the 165 TT alphabets.
 *
 * For each alphabet entry, stores the 4 row indices (r1a, r2a, r1b, r2b)
 * and whether the two T gates act on disjoint rows.
 */
struct TT_RowInfo {
    uint8_t r1a, r2a;   // rows for t_first
    uint8_t r1b, r2b;   // rows for t_second
    bool disjoint;       // true if {r1a,r2a} ∩ {r1b,r2b} = ∅
};

// Row pair lookup: T_PAIRS[index] = (row1, row2)
inline constexpr std::array<std::pair<uint8_t,uint8_t>, 15> TT_ROW_PAIRS = {{
    {0,1},{0,2},{0,3},{0,4},{0,5},
    {1,2},{1,3},{1,4},{1,5},
    {2,3},{2,4},{2,5},
    {3,4},{3,5},{4,5}
}};

inline constexpr auto TT_ROW_INFO = []() {
    std::array<TT_RowInfo, 165> result{};
    for (int a = 0; a < 165; ++a) {
        auto [r1a, r2a] = TT_ROW_PAIRS[TT_ALPHABET[a].t_first];
        auto [r1b, r2b] = TT_ROW_PAIRS[TT_ALPHABET[a].t_second];
        result[a] = {r1a, r2a, r1b, r2b,
                     (r1a != r1b && r1a != r2b && r2a != r1b && r2a != r2b)};
    }
    return result;
}();

/**
 * @brief Apply a single T-gate kernel to two rows of S for one column,
 * writing directly to arr_ (bypasses set_element hash invalidation).
 */
inline __attribute__((always_inline))
void apply_T_kernel_raw(SO6& S, uint8_t r1, uint8_t r2, uint8_t col) {
    const uint8_t idx1 = col * 6 + r1;
    const uint8_t idx2 = col * 6 + r2;
    DyadicSqrt2 a = S.arr_[idx1];
    DyadicSqrt2 b = S.arr_[idx2];
    const DyadicSqrt2 a_old = a;
    a += b;
    b -= a_old;
    b = -b;
    a.denom_exp += (a.int_c != 0);
    b.denom_exp += (b.int_c != 0);
    S.arr_[idx1] = a;
    S.arr_[idx2] = b;
}

/**
 * @brief Runtime wrapper for paired T-gate application (fused kernel).
 *
 * Optimizations over two sequential T_OperatorRuntime calls:
 *   - Single SO6 copy (not two)
 *   - Single 6-column loop (both T gates per column)
 *   - Direct arr_ writes (no per-element hash invalidation)
 *   - One canonical_reset + hash invalidation at the end
 */
class TT_OperatorRuntime {
public:
    explicit constexpr TT_OperatorRuntime(uint8_t alpha_idx)
        : alpha_idx_(alpha_idx) {}

    uint8_t t_first()  const { return TT_ALPHABET[alpha_idx_].t_first; }
    uint8_t t_second() const { return TT_ALPHABET[alpha_idx_].t_second; }

    inline SO6 apply(const SO6& S) const {
        SO6 result = S;   // single copy
        const auto& info = TT_ROW_INFO[alpha_idx_];

        // Both disjoint and overlapping: apply T_first then T_second per column.
        // For disjoint, order doesn't matter (independent rows).
        // For overlapping, T_second reads rows already updated by T_first — correct.
        for (uint8_t col = 0; col < 6; ++col) {
            apply_T_kernel_raw(result, info.r1a, info.r2a, col);
            apply_T_kernel_raw(result, info.r1b, info.r2b, col);
        }

        // Invalidate hash + canonical once at the end.
        // Use set_element on one element to trigger hash invalidation,
        // then canonical_reset.
        result.set_element(0, 0, result.get_element(0, 0));
        result.canonical_reset();
        result.last_T = t_second();
        return result;
    }

private:
    uint8_t alpha_idx_;
};

inline SO6 operator*(const TT_OperatorRuntime& op, const SO6& rhs) {
    return op.apply(rhs);
}
