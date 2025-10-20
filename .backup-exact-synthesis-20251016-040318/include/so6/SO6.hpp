/**
 * @file SO6.hpp
 * @brief SO6 matrix representation and operations over Z[√2].
 */
#ifndef SO6_HPP
#define SO6_HPP

#pragma once
#include <map>
#include <vector>
#include <array>
#include <cstdint>
#include <compare>
#include <algorithm>
#include <initializer_list>
#include <utility>
#include "policy/HashPolicy.hpp"
#include "Z2.hpp"
#include "ds/SmallFreqMap.hpp"
#include "ds/Perm6.hpp"


using FrequencyMap = SmallFreqMap;

struct FrequencyKey {
    std::array<std::pair<Z2, uint8_t>, 6> entries{};
    uint8_t size = 0;
    bool operator<(FrequencyKey const& other) const {
        if (size != other.size) return size < other.size;
        #pragma unroll 6
        for (uint8_t i = 0; i < size; ++i) {
            if (auto c = entries[i].first <=> other.entries[i].first; c != 0) return c < 0;
            if (entries[i].second != other.entries[i].second) return entries[i].second < other.entries[i].second;
        }
        return false;
    }
};

using FrequencyTable = std::map<FrequencyKey, std::vector<int>>;

constexpr uint16_t prime = 0x0101;

/**
 * @brief Compact 6x6 matrix over Z[√2] with canonicalization and hashing.
 */
class SO6 {
    public:
        Z2 arr[36];
        union {
            struct {
                unsigned char last_T : 4;
                uint16_t sign_convention : 12 = 0b010101010101;
            };
        };
        uint16_t hash = 0;
        uint16_t col_hash = 0;

        Perm6 Col;
        Perm6 Row;

        SO6();
        SO6(std::initializer_list<Z2> list) {
            std::copy(list.begin(), list.end(), arr);
        }

        int get_index(const uint8_t row, const uint8_t col) const { return static_cast<int>(col) * 6 + static_cast<int>(row); }
        inline Z2& get_element(const uint8_t row, const uint8_t col) {return arr[get_index(row,col)];}  // Return the array element needed.
        inline const Z2& get_element(const uint8_t row, const uint8_t col) const {return arr[get_index(row,col)];}  // Return the array element needed.

        const std::strong_ordering operator<=>(const SO6& other) const;
        bool operator==(const SO6& other) const { return ((*this) <=> other) == std::strong_ordering::equal; }

        static const SO6& identity();

        FrequencyTable row_equivalence_classes();
        FrequencyTable col_equivalence_classes();
        bool get_next_equivalence_class(FrequencyTable& );

        SO6 operator*(const SO6&) const; 
        SO6 left_multiply_by_T(const uint8_t) const;

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

            size_t row_freq = frequency_hash(S.row_frequency[row1]) + frequency_hash(S.row_frequency[row2]);
            S.hash -= row_freq;

            #pragma unroll 6
            for (int col = 0; col < 6; col++)
            {
                auto &cf = S.col_frequency[col];
                size_t col_freq = frequency_hash(cf);
                size_t col_sig = col_freq ^ (col_freq >> 1);
                S.hash -= col_sig;
                S.col_hash -= col_sig;

                auto &a = S.get_element(row1, col);
                auto &b = S.get_element(row2, col);
                const Z2 a_old = a;
                const Z2 b_old = b;
                Z2 a_old_abs = std::abs(a_old);
                Z2 b_old_abs = std::abs(b_old);

                // To track the column sum, begin by decreasing the size by the elements that will be modified
                S.row_frequency[row1].decrement(a_old_abs);
                S.row_frequency[row2].decrement(b_old_abs);
                cf.decrement(a_old_abs);
                cf.decrement(b_old_abs);

                // Update elements
                a += b_old;
                b -= a_old;
                a.denom_exp += (a.int_c != 0);
                b.denom_exp += (b.int_c != 0);

                const Z2 a_abs = std::abs(a);
                const Z2 b_abs = std::abs(b);

                // Update frequencies
                S.row_frequency[row1][a_abs]++;
                S.row_frequency[row2][b_abs]++;
                cf[a_abs]++;
                cf[b_abs]++;

                col_freq = frequency_hash(cf);
                col_sig = col_freq ^ (col_freq >> 1);
                S.hash += col_sig;
                S.col_hash += col_sig;
            }

            S.canonical_form();
            row_freq = frequency_hash(S.row_frequency[row1])+frequency_hash(S.row_frequency[row2]);
            S.hash +=  row_freq;
            S.last_T = i;
            return S;
        }

        // Iterator forward declaration (defined out-of-class)
        class Iterator;

        // Basic column iterators are exposed via get_column

        bool is_better_permutation(const uint8_t*, const uint8_t*,const uint16_t curr_sc);

        std::pair<SO6::Iterator,SO6::Iterator> get_column(const uint8_t  col, const uint8_t* Row_ = nullptr, const uint8_t* Col_ = nullptr) const;
        void canonical_form();

        // Serialization removed (unused)

    public: // expose maps for controlled benchmarks only
        FrequencyMap row_frequency[6];
        FrequencyMap col_frequency[6];

    public: // expose for benchmarks (read-only intended)

        // Centralized in policy/HashPolicy.hpp (wrappers keep call sites stable)
        static inline size_t z_freq_hash(const Z2 z, const int i) { return hashpolicy::z_freq_hash(z, i); }
        // Variant-based helpers removed with profiling code

        static inline size_t frequency_hash(const FrequencyMap& f);
};

// Define iterator out-of-class
#include "iter/SO6Iterator.hpp"

// Inline definitions split out for clarity (no logic changes)
#include "so6/Signatures.inl"

inline std::pair<SO6::Iterator,SO6::Iterator> SO6::get_column(const uint8_t col, const uint8_t* Row_, const uint8_t* Col_) const {
    return std::pair<SO6::Iterator,SO6::Iterator>(Iterator(*this, (col << 2) + (col << 1), Row_, Col_), Iterator(*this, (col << 2) + (col << 1) + 6, Row_, Col_));
}

namespace std {
    template <>
    struct hash<SO6> {
        uint16_t operator()(const SO6& s) const {
            // Keep original behavior: use precomputed 16-bit signature
            return s.hash;
        }
    };
}

#endif
