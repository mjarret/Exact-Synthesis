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
#include <span>
#include <concepts>
#include <utility>
#include "sort/sort6.hpp"
#include "Z2.hpp"
#include "ds/SmallFreqMap.hpp"
#include "ds/Lehmer6.hpp"
#include "ds/Order6.hpp"

// Flat, stack-only table specialized for tiny n=6
#include "ds/FlatFrequencyTable.hpp"

using FrequencySignature = uint16_t;
using FrequencyMap = SmallFreqMap;
using FrequencyTable = ds::FlatFrequencyTable;
inline constexpr const char* kHashBackendName = "flat";

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
                // Row sign mask: 1 bit per row (0 = POS, 1 = NEG); we use only 6 bits
                uint8_t sign_convention : 6 = 0;
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

        /// Recompute `hash` and `col_hash` from the current matrix contents.
        /// Uses the same scheme as identity():
        ///  - hash accumulates row_frequency_signature over rows and (col_freq ^ (col_freq>>1)) over cols
        ///  - col_hash accumulates (col_freq ^ (col_freq>>1)) over cols
        void recompute_hash();

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

        // ---------- Canonicalization / comparison helpers ----------
        /// Compare using Lehmer-encoded permutations for current vs candidate.
        bool is_better_permutation(const Lehmer6& row_perm, const Lehmer6& col_perm, const uint16_t sign_perm);

        /// Compare using raw candidate arrays; current comes from stored Lehmer.
        bool is_better_permutation(const uint8_t* row_perm, const uint8_t* col_perm, const uint16_t sign_perm);

        /// Compare using contiguous views (zero-copy) for candidate permutations.
        inline bool is_better_permutation(std::span<const uint8_t, 6> row_perm,
                                          std::span<const uint8_t, 6> col_perm,
                                          const uint16_t sign_perm) {
            return is_better_permutation(row_perm.data(), col_perm.data(), sign_perm);
        }

        /// Compare using indexable, sized candidate permutations; copies 6 bytes
        /// to bridge to the pointer fast-path. Works with std::array and custom
        /// types exposing operator[] for indices 0..5.
        template <class Row, class Col>
        requires (std::convertible_to<decltype(std::declval<const Row&>()[0]), uint8_t> &&
                  std::convertible_to<decltype(std::declval<const Row&>()[5]), uint8_t> &&
                  std::convertible_to<decltype(std::declval<const Col&>()[0]), uint8_t> &&
                  std::convertible_to<decltype(std::declval<const Col&>()[5]), uint8_t>)
        inline bool is_better_permutation(const Row& cand_row,
                                          const Col& cand_col,
                                          const uint16_t sign_perm) {
            uint8_t row_a[6];
            uint8_t col_a[6];
            for (int i = 0; i < 6; ++i) {
                row_a[i] = static_cast<uint8_t>(cand_row[static_cast<std::size_t>(i)]);
                col_a[i] = static_cast<uint8_t>(cand_col[static_cast<std::size_t>(i)]);
            }
            // Force resolution to the pointer overload, avoiding recursion
            // back into this template for array arguments.
            bool (SO6::*ptr_overload)(const uint8_t*, const uint8_t*, const uint16_t) = &SO6::is_better_permutation;
            return (this->*ptr_overload)(row_a, col_a, sign_perm);
        }

        /// Transform into canonical form (updates permutations and sign convention).
        void canonical_form();
        
        // ---------- Frequency bookkeeping ----------
        // No stored row/column frequency maps in the simplified policy.

        // Helpers: compute frequency signatures by scanning entries (no stored maps)
        static inline uint16_t row_frequency_signature(const SO6& s, int row) {
            std::array<Z2, 6> vals{};
            for (int c = 0; c < 6; ++c) vals[c] = std::abs(s.get_element(row, c));
            return signature_from_sorted(vals);
        }

        static inline uint16_t col_frequency_signature(const SO6& s, int col) {
            std::array<Z2, 6> vals{};
            for (int r = 0; r < 6; ++r) vals[r] = std::abs(s.get_element(r, col));
            return signature_from_sorted(vals);
        }

        static inline uint16_t signature_from_sorted(std::array<Z2, 6>& vals) {
            auto comp = [](const Z2& a, const Z2& b){ return a.data < b.data; };
            sort6::sorting_network_dispatch(vals, comp);
            size_t acc = 0;
            int i = 0;
            while (i < 6) {
                int j = i + 1;
                while (j < 6 && vals[j].data == vals[i].data) ++j;
                size_t h = z_freq_hash(vals[i], j-i);
                acc += h + h * h;
                i = j;
            }
            acc ^= acc >> 3;
            acc ^= acc >> 1;
            return acc;
        }

        // ---------- Hash helpers ----------
        /// Hash a single (Z2, count) contribution using the configured policy.
        static inline size_t z_freq_hash(const Z2 z, const int i) {
            auto mix64_variant = [](uint64_t x) {
                x ^= x >> 12;
                x ^= x << 25;
                x ^= x >> 27;
                x *= 0x2545F4914F6CDD1DULL;
                return x;
            };
            const uint64_t seed = (static_cast<uint64_t>(std::hash<Z2>{}(std::abs(z))) << 3)
                                | static_cast<uint64_t>(i & 0x7);
            return static_cast<size_t>(mix64_variant(seed));
        }
        /// Combine all entries of a SmallFreqMap into a stable signature.
        static inline size_t frequency_hash(const FrequencyMap& f);
        /// Report the current sizeof(SO6) in bytes (compile-time constant)
        static constexpr std::size_t size_bytes() { return sizeof(SO6); }
};

// Inline definitions split out for clarity (no logic changes)
#include "so6/Signatures.inl"

namespace std {
    template <>
    struct hash<SO6> {
        uint16_t operator()(const SO6& s) const { return s.hash; }
    };
}
#endif
