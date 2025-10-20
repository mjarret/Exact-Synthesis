/**
 * @file SO6.hpp
 * @brief Compact 6x6 matrix over Z[√2] with canonicalization, hashing, and
 *        permutation bookkeeping used for exact synthesis search.
 *
 * High level overview
 * - Storage: a flat array of 36 `Z2` entries in column-major order.
 * - Canonicalization: the matrix maintains row/col permutations and a sign
 *   convention to compare matrices lexicographically for a canonical form.
 * - Hashing: two 16-bit rolling signatures (`hash`, `col_hash`) combining
 *   per-column and per-row frequency-based summaries.
 * - Frequencies: each row/column maintains a tiny frequency map (`SmallFreqMap`)
 *   of absolute values to counts for fast delta updates under T-moves.
 */
#ifndef SO6_HPP
#define SO6_HPP

#pragma once
#include <map>
#include <vector>
#include <array>
#include <cstddef>
#include <cstdint>
#include <compare>
#include <algorithm>
#include <initializer_list>
#include <utility>
#include "policy/HashPolicy.hpp"
#include "Z2.hpp"
#include "config/FrequencyPolicy.hpp"
#include "ds/FrequencyTables.hpp"
#include "ds/Lehmer6.hpp"
#include "ds/Order6.hpp"
#include "ds/hash_containers.hpp"

// Small non-zero prime to seed signatures for the identity element
constexpr uint16_t prime = 0x0101;

/**
 * @class SO6
 * @brief Compact 6x6 matrix over Z[√2] supporting canonicalization and hashing.
 *
 * Data layout and invariants
 * - Elements are stored column-major in `arr` so that column scans are linear.
 * - `row_perm_lh_` and `col_perm_lh_` encode, in Lehmer form, the current best
 *   permutation found by canonicalization; these drive comparisons and ordering.
 * - `sign_convention` encodes a per-row sign choice used by lexicographic
 *   comparisons. Bit 0 corresponds to row 0, etc.
 * - `hash` and `col_hash` hold 16-bit signatures derived from row/column
 *   frequency summaries and are used as a fast pre-check in comparisons.
 * - `row_frequency`/`col_frequency` track absolute-value counts per row/column;
 *   they are incrementally updated by T-moves and used to compute signatures
 *   and equivalence classes.
 */
class SO6 {
public:
        // ---------- Storage ----------
        /// Flat column-major storage: index = col*6 + row
        uint8_t arr24_[36 * 3]{};     // packed 24-bit Z2

        /// Misc packed flags used during search/canonicalization
        union {
            struct {
                // Index (0..14) of the most recent T_{i} left-multiplication; 15 means "none"
                unsigned char last_T : 4;
                // Row sign mask used for lexicographic comparisons (bit l selects sign for row l)
                uint16_t sign_convention : 12 = 0b010101010101;
            };
        };

        /// Precomputed 16-bit signatures. Used for fast comparison pre-checks.
        uint16_t hash = 0, col_hash = 0;

        /// Lehmer-encoded permutations for the current canonical representative
        Lehmer6 col_perm_lh_{}, row_perm_lh_{};

        // ---------- Construction ----------
        /// Default constructs the zero matrix.
        SO6();

        /// Initialize from a 36-element list in column-major order.
        SO6(std::initializer_list<Z2> list) {
            size_t i = 0;
            for (const auto& z : list) {
                uint8_t row = i % 6;
                uint8_t col = i / 6;
                set_element(row, col, z);
                ++i;
                if (i >= 36) break;
            }
        }

        // ---------- Element access ----------
        /// Convert (row, col) to linear index into `arr` (column-major).
        uint8_t get_index(const uint8_t row, const uint8_t col) const { return col * 6 + row; }

        /// Read element (by value) from packed storage.
        inline Z2 get_element(const uint8_t row, const uint8_t col) const {
            int off = get_index(row, col) * 3;
            uint32_t v = arr24_[off]
                       | (uint32_t(arr24_[off + 1]) << 8)
                       | (uint32_t(arr24_[off + 2]) << 16);
            return Z2(v);
        }
        /// Write element into packed storage (stores lower 24 bits of z.data).
        inline void set_element(const uint8_t row, const uint8_t col, const Z2& z) {
            int off = get_index(row, col) * 3;
            uint32_t v = (z.data & 0xFFFFFFu);
            arr24_[off]     = v & 0xFFu;
            arr24_[off + 1] = (v >> 8) & 0xFFu;
            arr24_[off + 2] = (v >> 16) & 0xFFu;
        }

        // ---------- Ordering and identity ----------
        /// Three-way comparison using signatures then lexicographic columns with current permutations.
        const std::strong_ordering operator<=>(const SO6& other) const;
        /// Equality via the three-way comparison.
        bool operator==(const SO6& other) const { return ((*this) <=> other) == std::strong_ordering::equal; }

        /// Singleton identity matrix; pre-initialized with frequencies/signatures.
        static const SO6& identity();

        // ---------- Frequency helpers / equivalence classes ----------
        /// Build row equivalence classes keyed by row frequency signatures.
        FrequencyTable row_equivalence_classes();
        /// Build column equivalence classes keyed by column frequency signatures.
        FrequencyTable col_equivalence_classes();
        /// Iterate to the next permutation within each equivalence class; returns false on wrap.
        bool get_next_equivalence_class(FrequencyTable&);

        // ---------- Algebraic operations ----------
        /// Matrix multiply (this * other).
        SO6 operator*(const SO6&) const;

        /// Left-multiply by T_i (runtime index 0..14).
        SO6 left_multiply_by_T(const uint8_t) const;

        /// Left-multiply by T_i (compile-time index 0..14). Mutates and returns `S`.
        template<int i>
        static SO6 left_multiply_by_T(SO6 &S) {
            static_assert(i >= 0 && i < 15, "left_multiply_by_T: i out of range");
            static constexpr std::array<std::pair<int,int>, 15> pairs{{
                {0,1},{0,2},{0,3},{0,4},{0,5},
                {1,2},{1,3},{1,4},{1,5},
                {2,3},{2,4},{2,5},
                {3,4},{3,5},
                {4,5}
            }};
            constexpr int row1 = pairs[i].first;
            constexpr int row2 = pairs[i].second;

            size_t row_freq =
            #if (EXACT_FREQ_COLS_ONLY == 0) && (EXACT_FREQ_NONE == 0)
                frequency_hash(S.row_frequency[row1]) + frequency_hash(S.row_frequency[row2]);
            #else
                row_frequency_signature(S, row1) + row_frequency_signature(S, row2);
            #endif
            S.hash -= row_freq;

            for (int col = 0; col < 6; col++)
            {
                #if (EXACT_FREQ_NONE == 0)
                auto &cf = S.col_frequency[col];
                size_t col_freq = frequency_hash(cf);
                #else
                size_t col_freq = col_frequency_signature(S, col);
                #endif
                size_t col_sig = col_freq ^ (col_freq >> 1);
                S.hash     -= col_sig;
                S.col_hash -= col_sig;

                Z2 a = S.get_element(row1, col);
                Z2 b = S.get_element(row2, col);
                const Z2 a_old = a;
                const Z2 b_old = b;
                Z2 a_old_abs = std::abs(a_old);
                Z2 b_old_abs = std::abs(b_old);

                // To track the column sum, begin by decreasing the size by the elements that will be modified
                #if (EXACT_FREQ_COLS_ONLY == 0) && (EXACT_FREQ_NONE == 0)
                S.row_frequency[row1].decrement(a_old_abs);
                S.row_frequency[row2].decrement(b_old_abs);
                #endif
                #if (EXACT_FREQ_NONE == 0)
                cf.decrement(a_old_abs);
                cf.decrement(b_old_abs);
                #endif

                // Update elements
                a += b_old;
                b -= a_old;
                a.denom_exp += (a.int_c != 0);
                b.denom_exp += (b.int_c != 0);

                const Z2 a_abs = std::abs(a);
                const Z2 b_abs = std::abs(b);

                // Update frequencies
                #if (EXACT_FREQ_COLS_ONLY == 0) && (EXACT_FREQ_NONE == 0)
                S.row_frequency[row1][a_abs]++;
                S.row_frequency[row2][b_abs]++;
                #endif
                #if (EXACT_FREQ_NONE == 0)
                cf[a_abs]++;
                cf[b_abs]++;
                #endif

                // Write back updates
                S.set_element(row1, col, a);
                S.set_element(row2, col, b);

                #if (EXACT_FREQ_NONE == 0)
                col_freq = frequency_hash(cf);
                #else
                col_freq = col_frequency_signature(S, col);
                #endif
                col_sig = col_freq ^ (col_freq >> 1);
                S.hash     += col_sig;
                S.col_hash += col_sig;
            }

            S.canonical_form();
            row_freq =
            #if (EXACT_FREQ_COLS_ONLY == 0) && (EXACT_FREQ_NONE == 0)
                frequency_hash(S.row_frequency[row1]) + frequency_hash(S.row_frequency[row2]);
            #else
                row_frequency_signature(S, row1) + row_frequency_signature(S, row2);
            #endif
            S.hash +=  row_freq;
            S.last_T = i;
            return S;
        }

        // ---------- Iteration ----------
        /// Forward declaration for column iterator (implemented in iter/SO6Iterator.hpp)
        class Iterator;

        /// Get iterators over a (possibly permuted) column.
        std::pair<SO6::Iterator,SO6::Iterator> get_column(const uint8_t  col, const uint8_t* Row_ = nullptr, const uint8_t* Col_ = nullptr) const;

        // ---------- Canonicalization / comparison helpers ----------
        /// Compare using Lehmer-encoded permutations for current vs candidate.
        bool is_better_permutation(const Lehmer6& row_perm, const Lehmer6& col_perm, const uint16_t sign_perm);

        /// Compare using raw candidate arrays; current comes from stored Lehmer.
        bool is_better_permutation(const uint8_t* row_perm, const uint8_t* col_perm, const uint16_t sign_perm);

        /// Transform into canonical form (updates permutations and sign convention).
        void canonical_form();
        
        // ---------- Frequency bookkeeping (exposed for benchmarks) ----------
        /// Absolute-value frequency per row.
        #if (EXACT_FREQ_COLS_ONLY == 0) && (EXACT_FREQ_NONE == 0)
        FrequencyMap row_frequency[6];
        #endif
        /// Absolute-value frequency per column.
        #if (EXACT_FREQ_NONE == 0)
        FrequencyMap col_frequency[6];
        #endif

        // Helpers: compute frequency signatures by scanning entries (no stored maps)
        static inline size_t row_frequency_signature(const SO6& s, int row) {
            // gather abs values
            Z2 vals[6];
            for (int c = 0; c < 6; ++c) vals[c] = std::abs(s.get_element(row, static_cast<uint8_t>(c)));
            // sort by raw data
            std::sort(vals, vals + 6, [](const Z2& a, const Z2& b){ return a.data < b.data; });
            // run-length encode and fold like frequency_hash
            size_t acc = 0;
            int i = 0;
            while (i < 6) {
                int j = i + 1;
                while (j < 6 && vals[j].data == vals[i].data) ++j;
                int cnt = j - i;
                size_t h = z_freq_hash(vals[i], cnt);
                acc += h + h * h;
                i = j;
            }
            acc ^= acc >> 3;
            acc ^= acc >> 1;
            return acc;
        }

        static inline size_t col_frequency_signature(const SO6& s, int col) {
            Z2 vals[6];
            for (int r = 0; r < 6; ++r) vals[r] = std::abs(s.get_element(static_cast<uint8_t>(r), col));
            std::sort(vals, vals + 6, [](const Z2& a, const Z2& b){ return a.data < b.data; });
            size_t acc = 0;
            int i = 0;
            while (i < 6) {
                int j = i + 1;
                while (j < 6 && vals[j].data == vals[i].data) ++j;
                int cnt = j - i;
                size_t h = z_freq_hash(vals[i], cnt);
                acc += h + h * h;
                i = j;
            }
            acc ^= acc >> 3;
            acc ^= acc >> 1;
            return acc;
        }

        // ---------- Hash helpers ----------
        /// Hash a single (Z2, count) contribution using the configured policy.
        static inline size_t z_freq_hash(const Z2 z, const int i) { return hashpolicy::z_freq_hash(z, i); }
        /// Combine all entries of a SmallFreqMap into a stable signature.
        static inline size_t frequency_hash(const FrequencyMap& f);
        /// Report the current sizeof(SO6) in bytes (compile-time constant)
        static constexpr std::size_t size_bytes() { return sizeof(SO6); }
};

// Define iterator out-of-class
#include "iter/SO6Iterator.hpp"

// Inline definitions split out for clarity (no logic changes)
#include "so6/Signatures.inl"

inline std::pair<SO6::Iterator,SO6::Iterator> SO6::get_column(const uint8_t col, const uint8_t* Row_, const uint8_t* Col_) const {
    return std::pair<SO6::Iterator,SO6::Iterator>(Iterator(*this, (col << 2) + (col << 1), Row_, Col_), Iterator(*this, (col << 2) + (col << 1) + 6, Row_, Col_));
}

// (no Lehmer overload; comparisons use decode-only local arrays)

namespace std {
    template <>
    struct hash<SO6> {
        uint16_t operator()(const SO6& s) const { return s.hash; }
    };
}
// Verify SO6 layout against the end of the last data member present under the active policy
#include <cstddef>
#if (EXACT_FREQ_NONE == 1)
// No frequency maps present; last data member is row_perm_lh_
constexpr std::size_t __so6_expected_size = offsetof(SO6, row_perm_lh_) + sizeof(((SO6*)0)->row_perm_lh_);
#elif (EXACT_FREQ_COLS_ONLY == 1)
// Only column maps present; last data member is col_frequency
constexpr std::size_t __so6_expected_size = offsetof(SO6, col_frequency) + sizeof(((SO6*)0)->col_frequency);
#else
// Both row and column maps present; last data member is col_frequency
constexpr std::size_t __so6_expected_size = offsetof(SO6, col_frequency) + sizeof(((SO6*)0)->col_frequency);
#endif
static_assert(sizeof(SO6) == __so6_expected_size, "SO6 size changed; check packing/layout");

#endif
