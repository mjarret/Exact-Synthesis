/**
 * @file SO6.hpp
 * @brief Compact 6x6 matrix over Z[√2] with canonicalization, hashing, and
 *        permutation bookkeeping used for exact synthesis search.
 *
 * High level overview
 * - Storage: a flat array of 36 `DyadicSqrt2` entries in column-major order.
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
#include <concepts>
#include <initializer_list>
#include <ostream>
#include <span>
#include <string_view>
#include <utility>
#include "sort/sort6.hpp"
#include "ds/SmallFreqMap.hpp"
#include "ds/Lehmer6.hpp"
#include "ds/Order6.hpp"
#include "ds/Clifford6.hpp"

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
private:
        /// Precomputed 16-bit signatures. Used for fast comparison pre-checks.
        /// A value of 0 for `hash` is treated as "not computed yet" and will
        /// trigger a lazy recomputation on first access via accessors.
        uint16_t hash = 0, col_hash = 0;

        /// Lehmer-encoded permutations for the current canonical representative.
        /// We use `bits() == Lehmer6::SENTINEL` as a sentinel meaning "not yet
        /// canonicalized"; canonical_form() overwrites these with valid codes.
        Lehmer6 col_perm_lh_{Lehmer6::from_index(Lehmer6::SENTINEL)},
                row_perm_lh_{Lehmer6::from_index(Lehmer6::SENTINEL)};
public:
        // ---------- Storage ----------
        /// Flat column-major storage: index = col*6 + row
        // uint8_t arr24_[36 * 3]{};     // previous packed 24-bit DyadicSqrt2
        std::array<DyadicSqrt2, 36> arr_{};   // direct Dyadic storage

        /// Misc packed flags used during search/canonicalization
        union {
            struct {
                // Index (0..14) of the most recent T_{i} left-multiplication; 15 means "none"
                unsigned char last_T : 4;
                // Row sign mask: 1 bit per row (0 = POS, 1 = NEG); we use only 6 bits
                uint8_t sign_convention : 6 = 0;
            };
        };

        // ---------- Construction ----------
        /// Default constructs the zero matrix.
        SO6();

        /// Initialize from a 36-element list in column-major order.
        SO6(std::initializer_list<DyadicSqrt2> list) {
            size_t i = 0;
            for (const auto& z : list) {
                uint8_t row = i % 6;
                uint8_t col = i / 6;
                set_element(row, col, z);
                ++i;
                if (i >= 36) break;
            }
        }

        /// Parse a Mathematica-style 6x6 matrix string: {{a,b,c,d,e,f},{...},...}.
        explicit SO6(std::string_view s);

        // ---------- Element access ----------
        /// Convert (row, col) to linear index into `arr` (column-major).
        uint8_t get_index(const uint8_t row, const uint8_t col) const { return col * 6 + row; }

        /// Read element (by value) from packed storage.
        inline DyadicSqrt2 get_element(const uint8_t row, const uint8_t col) const {
            return arr_[get_index(row, col)];
        }
        /// Write element into packed storage (stores lower 24 bits of z.data).
        inline void set_element(const uint8_t row, const uint8_t col, const DyadicSqrt2& z) {
            arr_[get_index(row, col)] = z;
            // Mark cached hashes as invalid; they will be recomputed lazily.
            hash = 0;
            col_hash = 0;
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

        // Lazily computed accessors for the hash fields. A zero value in
        // `hash` indicates "not computed yet" and will trigger a full
        // recomputation the first time one of these is called.
        inline uint16_t primary_hash() const {
            if (hash == 0) {
                const_cast<SO6*>(this)->recompute_hash();
            }
            return hash;
        }

        inline uint16_t column_hash() const {
            if (hash == 0) {
                const_cast<SO6*>(this)->recompute_hash();
            }
            return col_hash;
        }

        // Accessors for canonicalization metadata; prefer these over reaching
        // into the underlying fields directly. When the internal Lehmer code
        // is left at the sentinel value, we materialize the canonical
        // form on first access even though these methods are const.
        inline const Lehmer6& row_perm_lh() const {
            if (row_perm_lh_.bits() == Lehmer6::SENTINEL) {
                const_cast<SO6*>(this)->canonical_form();
            }
            return row_perm_lh_;
        }

        inline const Lehmer6& col_perm_lh() const {
            if (col_perm_lh_.bits() == Lehmer6::SENTINEL) {
                const_cast<SO6*>(this)->canonical_form();
            }
            return col_perm_lh_;
        }
        inline uint8_t sign_mask() const { return sign_convention; }
        inline void canonical_reset() {
            row_perm_lh_ = Lehmer6::from_index(Lehmer6::SENTINEL);
            col_perm_lh_ = Lehmer6::from_index(Lehmer6::SENTINEL);
        }

        inline void set_row_perm_lh(const Lehmer6& p) { row_perm_lh_ = p; }
        inline void set_col_perm_lh(const Lehmer6& p) { col_perm_lh_ = p; }
        inline void set_sign_mask(uint8_t m) { sign_convention = m; }

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
        inline bool is_better_permutation(const Row& cand_row, const Col& cand_col, const uint16_t sign_perm) {
            uint8_t row_a[6];
            uint8_t col_a[6];
            for (int i = 0; i < 6; ++i) {
                row_a[i] = static_cast<uint8_t>(cand_row[i]);
                col_a[i] = static_cast<uint8_t>(cand_col[i]);
            }
            // Force resolution to the pointer overload, avoiding recursion
            // back into this template for array arguments.
            bool (SO6::*ptr_overload)(const uint8_t*, const uint8_t*, const uint16_t) = &SO6::is_better_permutation;
            return (this->*ptr_overload)(row_a, col_a, sign_perm);
        }

        /// Transform into canonical form (updates permutations and sign convention).
        void canonical_form();

        // Debug/inspection helpers
        void print_raw(std::ostream& os) const;
        void print_with_perms(std::ostream& os) const;
        void print_mathematica(std::ostream& os) const;

        /// Materialize the canonical view (perms/sign) into raw storage and return it.
        /// The returned SO6 has identity row/col perms and zero sign_convention.
        SO6 materialize_canonical() const;

        /// Column sign mask: bit c is 0 if the top non-zero element in column c has int_c > 0,
        /// and 1 if that top non-zero element has int_c < 0. Columns with all zeros contribute 0.
        uint8_t col_sign() const;
        
        // Helpers: compute frequency signatures by scanning entries (no stored maps)
        static inline uint16_t row_frequency_signature(const SO6& s, int row) {
            std::array<DyadicSqrt2, 6> vals{};
            for (int c = 0; c < 6; ++c) vals[c] = std::abs(s.get_element(row, c));
            return signature(vals);
        }

        static inline uint16_t col_frequency_signature(const SO6& s, int col) {
            std::array<DyadicSqrt2, 6> vals{};
            for (int r = 0; r < 6; ++r) vals[r] = std::abs(s.get_element(r, col));
            return signature(vals);
        }

        static inline uint16_t signature(std::array<DyadicSqrt2, 6>& vals) {
            size_t acc = 0;
            uint8_t seen = 0;                    // bit i == 1 -> vals[i] already accounted for

            for (int i = 0; i < 6; ++i) {
                const uint8_t bit_i = uint8_t(1u << i);
                if (seen & bit_i) continue;

                // Start a new group with leader i
                seen |= bit_i;
                int cnt = 1;

                const auto& leader = vals[i];
                const auto leader_key = leader.data;   // pull once; helps compilers keep it in a register

                // Count duplicates of leader among the remaining elements
                for (int j = i + 1; j < 6; ++j) {
                    const uint8_t bit_j = uint8_t(1u << j);
                    // (seen check is cheap and removes re-visiting already grouped items)
                    if (!(seen & bit_j) && vals[j].data == leader_key) {
                        seen |= bit_j;
                        ++cnt;
                    }
                }

                // One call per distinct value with its frequency
                const uint16_t h = z_freq_hash(leader, cnt);
                acc += h + h * h;
            }

            // same light finalization you had
            acc ^= acc >> 3;
            acc ^= acc >> 1;
            return static_cast<uint16_t>(acc);   
        }

        // ---------- Hash helpers ----------
        /// Hash a single (DyadicSqrt2, count) contribution using the configured policy.
        static inline uint16_t z_freq_hash(const DyadicSqrt2 z, const int i) {
            constexpr auto mix64_variant = [](uint16_t x) {
                x ^= x >> 5;
                return x;
            };
            uint16_t seed = (std::hash<DyadicSqrt2>{}(std::abs(z)) << 3) | (i & 0x7);
            return mix64_variant(seed);
        }
        /// Combine all entries of a SmallFreqMap into a stable signature.
        // static inline size_t frequency_hash(const FrequencyMap& f);
        /// Report the current sizeof(SO6) in bytes (compile-time constant)
        static constexpr std::size_t size_bytes() { return sizeof(SO6); }
};

namespace std {
    template <>
    struct hash<SO6> {
        uint16_t operator()(const SO6& s) const { return s.primary_hash(); }
    };
}
#endif
