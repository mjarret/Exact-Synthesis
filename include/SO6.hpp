/**
 * @file SO6.hpp
 * @brief SO6 matrix representation and operations over Z[√2].
 */
#ifndef SO6_HPP
#define SO6_HPP

#pragma once
#include <map>
#include <array>
#include <cstdint>
#include "policy/HashPolicy.hpp"
#include "Z2.hpp"
#include "ds/SmallFreqMap.hpp"

struct LUT;

using FrequencyMap = SmallFreqMap;

struct FrequencyKey {
    std::array<std::pair<Z2, uint8_t>, 6> entries{};
    uint8_t size = 0;
    bool operator<(FrequencyKey const& other) const {
        if (size != other.size) return size < other.size;
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

        uint8_t Col[6] = {0,1,2,3,4,5};
        uint8_t Row[6] = {0,1,2,3,4,5};

        SO6();
        SO6(std::initializer_list<Z2> list) {
            std::copy(list.begin(), list.end(), arr);
        }

        int get_index(const uint8_t row, const uint8_t col) const {return ((col<<2) + (col<<1) + row)&0x3F;}
        inline Z2& get_element(const uint8_t row, const uint8_t col) {return arr[get_index(row,col)];}  // Return the array element needed.
        inline const Z2 get_element(const uint8_t row, const uint8_t col) const {return arr[get_index(row,col)];}  // Return the array element needed.

        const std::strong_ordering operator<=>(const SO6& other) const;
        const bool operator==(const SO6& other) const {return ((*this) <=> other) == std::strong_ordering::equal;}
        friend std::ostream& operator<<(std::ostream&,const SO6&); 

        static const SO6& identity();

        FrequencyTable row_equivalence_classes();
        FrequencyTable col_equivalence_classes();
        bool get_next_equivalence_class(FrequencyTable& );

        SO6 operator*(const SO6&) const; 
        SO6 left_multiply_by_T(const uint8_t) const;

        template<int i> 
        static SO6 left_multiply_by_T(SO6 &S) {
            int row1, row2;

            switch (i) {
                case 0:     row1 = 0; row2 = 1; break;
                case 1:     row1 = 0; row2 = 2; break;
                case 2:     row1 = 0; row2 = 3; break;
                case 3:     row1 = 0; row2 = 4; break;
                case 4:     row1 = 0; row2 = 5; break;
                case 5:     row1 = 1; row2 = 2; break;
                case 6:     row1 = 1; row2 = 3; break;
                case 7:     row1 = 1; row2 = 4; break;
                case 8:     row1 = 1; row2 = 5; break;
                case 9:     row1 = 2; row2 = 3; break;
                case 10:    row1 = 2; row2 = 4; break;
                case 11:    row1 = 2; row2 = 5; break;
                case 12:    row1 = 3; row2 = 4; break;
                case 13:    row1 = 3; row2 = 5; break;
                case 14:    row1 = 4; row2 = 5; break;
                default:    __builtin_unreachable();
            }

            // Define a lambda function for frequency map decrement
            auto decrementFrequency = [](auto& freq_map, Z2 key) {
                auto it = freq_map.find(key);
                if (it != freq_map.end()) {
                    if ((*it).second == 1) freq_map.erase(it);  // Remove if count reaches zero
                    else (*it).second--;                         // Otherwise, decrement count
                }
            };

            size_t row_freq = frequency_hash(S.row_frequency[row1])+frequency_hash(S.row_frequency[row2]);
            S.hash -= row_freq;

            #pragma unroll
            for (int col = 0; col < 6; col++)
            {
                size_t col_freq = frequency_hash(S.col_frequency[col]);
                S.hash -= col_freq^(col_freq>>1);
                S.col_hash -= col_freq^(col_freq>>1);

                const Z2 row1_element = S.get_element(row1, col);
                const Z2 row2_element = S.get_element(row2, col);
                Z2 row1_element_abs = std::abs(row1_element);
                Z2 row2_element_abs = std::abs(row2_element);

                // To track the column sum, begin by decreasing the size by the elements that will be modified
                decrementFrequency(S.row_frequency[row1], row1_element_abs);
                decrementFrequency(S.row_frequency[row2], row2_element_abs);
                decrementFrequency(S.col_frequency[col], row1_element_abs);
                decrementFrequency(S.col_frequency[col], row2_element_abs);

                // Update elements
                S.get_element(row1, col) += row2_element;
                S.get_element(row2, col) -= row1_element;
                S.get_element(row1, col).denom_exp += (S.get_element(row1, col).int_c != 0);
                S.get_element(row2, col).denom_exp += (S.get_element(row2, col).int_c != 0);
                
                row1_element_abs = std::abs(S.get_element(row1, col));
                row2_element_abs = std::abs(S.get_element(row2, col));
                
                // Update frequencies
                S.row_frequency[row1][row1_element_abs]++;
                S.row_frequency[row2][row2_element_abs]++;
                S.col_frequency[col][row1_element_abs]++;
                S.col_frequency[col][row2_element_abs]++;

                col_freq = frequency_hash(S.col_frequency[col]);
                S.hash += col_freq^(col_freq>>1);
                S.col_hash += col_freq^(col_freq>>1);
            }

            S.canonical_form();
            row_freq = frequency_hash(S.row_frequency[row1])+frequency_hash(S.row_frequency[row2]);
            S.hash +=  row_freq;
            S.last_T = i;
            return S;
        }

        // Iterator forward declaration (defined out-of-class)
        class Iterator;

        Iterator begin() const;
        Iterator begin(uint8_t* Row) const;
        Iterator end() const;
        Iterator end(uint8_t *Row) const;

        bool is_better_permutation(const uint8_t*, const uint8_t*,const uint16_t curr_sc);

        std::pair<SO6::Iterator,SO6::Iterator> get_column(const uint8_t  col, const uint8_t* Row_ = nullptr, const uint8_t* Col_ = nullptr) const;
        std::pair<SO6::Iterator,SO6::Iterator> get_lex_column(const uint8_t  col) const;
        void canonical_form();

        std::string serialize() const;
        static SO6 deserialize(const std::string& data);
    public: // expose maps for controlled benchmarks only
        FrequencyMap row_frequency[6];
        FrequencyMap col_frequency[6];

    public: // expose for benchmarks (read-only intended)

        // Centralized in policy/HashPolicy.hpp (wrappers keep call sites stable)
        static inline size_t mix64_variant(uint64_t x) { return hashpolicy::mix64_variant(x); }
        static inline uint64_t mix64_variant_rt(uint64_t x, int v) { return hashpolicy::mix64_variant_rt(x, v); }
        static inline size_t z_freq_hash(const Z2 z, const int i) { return hashpolicy::z_freq_hash(z, i); }
        static inline size_t z_freq_hash_variant(const Z2 z, const int i, int variant) { return hashpolicy::z_freq_hash_variant(z, i, variant); }

        static inline size_t frequency_hash_variant(const FrequencyMap& f, int variant) {
            size_t hash = 0;
            for (const auto& kv : f) {
                size_t h = z_freq_hash_variant(kv.first, kv.second, variant);
                hash += h + h * h;
            }
            hash ^= hash >> 3;
            hash ^= hash >> 1;
            return hash;
        }

        static inline void signature_row_col_variant(const SO6& s, int variant, size_t& row_sig, size_t& col_sig) {
            row_sig = 0; col_sig = 0;
            for (int r = 0; r < 6; ++r) row_sig += frequency_hash_variant(s.row_frequency[r], variant);
            for (int c = 0; c < 6; ++c) {
                size_t cf = frequency_hash_variant(s.col_frequency[c], variant);
                col_sig += (cf ^ (cf >> 1));
            }
        }

        static inline size_t frequency_hash(const FrequencyMap& f) {
            size_t hash = 0;
            for (const auto& kv : f) {
                size_t h = z_freq_hash(kv.first, kv.second);
                hash += h + h * h;
            }
            hash ^= hash >> 3;
            hash ^= hash >> 1;
            return hash;
        }
};

// Define iterator out-of-class
#include "iter/SO6Iterator.hpp"

// Inline definitions that depend on Iterator completeness
inline SO6::Iterator SO6::begin() const { return Iterator(*this, 0); }
inline SO6::Iterator SO6::begin(uint8_t* Row) const { return Iterator(*this, 0, Row, Col); }
inline SO6::Iterator SO6::end() const { return Iterator(*this, 36); }
inline SO6::Iterator SO6::end(uint8_t* Row) const { return Iterator(*this, 0, Row, Col) + 6; }
inline std::pair<SO6::Iterator,SO6::Iterator> SO6::get_column(const uint8_t col, const uint8_t* Row_, const uint8_t* Col_) const {
    return std::pair<SO6::Iterator,SO6::Iterator>(Iterator(*this, (col << 2) + (col << 1), Row_, Col_), Iterator(*this, (col << 2) + (col << 1) + 6, Row_, Col_));
}
inline std::pair<SO6::Iterator,SO6::Iterator> SO6::get_lex_column(const uint8_t col) const { return get_column(col, Row, Col); }

namespace std {
    template <>
    struct hash<SO6> {
        uint16_t operator()(const SO6& s) const {
            return s.hash;
        }
    };
}

#endif
