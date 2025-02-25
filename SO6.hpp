#ifndef SO6_HPP
#define SO6_HPP

#pragma once
#include <map>
#include <cstdint>
#include "./robin_hood.h"
#include "./Z2.hpp"

struct LUT;

using FrequencyMap = std::map<Z2, int>;
using FrequencyTable = std::map<FrequencyMap, std::vector<int>>;

// Use FNV-1a offset basis for the overall hash.
constexpr uint16_t offset = 0x811C;
constexpr uint16_t prime = 0x0101;

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
        uint8_t Col[6] = {0,1,2,3,4,5};
        uint8_t Row[6] = {0,1,2,3,4,5};
        static constexpr uint16_t col_mask = 0x3F;


        SO6();
        
        int get_index(const uint8_t row, const uint8_t col) const {return ((col<<2) + (col<<1) + row)&0x3F;}
        inline Z2& get_element(const uint8_t row, const uint8_t col) {return arr[get_index(row,col)];}  // Return the array element needed.
        inline const Z2 get_element(const uint8_t row, const uint8_t col) const {return arr[get_index(row,col)];}  // Return the array element needed.

        const std::strong_ordering operator<=>(const SO6& other) const;
        const bool operator==(const SO6& other) const {return ((*this) <=> other) == std::strong_ordering::equal;}
        friend std::ostream& operator<<(std::ostream&,const SO6&); 
                  
        static const SO6 identity() {
            static const SO6 I = []() {
            SO6 temp;
            for(int k = 0; k < 6; k++) {
                temp.arr[(k << 2) + (k << 1) + k] = Z2(1, 0, 0);
                temp.row_frequency[k][Z2(1, 0, 0)] = 1;
                temp.row_frequency[k][Z2(0, 0, 0)] = 5;
                temp.col_frequency[k][Z2(1, 0, 0)] = 1;
                temp.col_frequency[k][Z2(0, 0, 0)] = 5;
            }
            temp.canonical_form();
            temp.hash = SO6_hash(temp);
            return temp;
            }();
            return I;
        }

        FrequencyTable row_equivalence_classes();
        FrequencyTable col_equivalence_classes();
        bool get_next_equivalence_class(FrequencyTable& );

        SO6 operator*(const SO6&) const; 
        SO6 left_multiply_by_T(const uint8_t) const;
        
        template<int i> 
        static SO6 left_multiply_by_T(SO6 &S) {
            int row1, row2;

            switch (i) {
                case 0: row1 = 0; row2 = 1; break;
                case 1: row1 = 0; row2 = 2; break;
                case 2: row1 = 0; row2 = 3; break;
                case 3: row1 = 0; row2 = 4; break;
                case 4: row1 = 0; row2 = 5; break;
                case 5: row1 = 1; row2 = 2; break;
                case 6: row1 = 1; row2 = 3; break;
                case 7: row1 = 1; row2 = 4; break;
                case 8: row1 = 1; row2 = 5; break;
                case 9: row1 = 2; row2 = 3; break;
                case 10: row1 = 2; row2 = 4; break;
                case 11: row1 = 2; row2 = 5; break;
                case 12: row1 = 3; row2 = 4; break;
                case 13: row1 = 3; row2 = 5; break;
                case 14: row1 = 4; row2 = 5; break;
            }
            // Define a lambda function for frequency map decrement
            auto decrementFrequency = [](auto& freq_map, Z2 key) {
                auto it = freq_map.find(key);
                if (it != freq_map.end()) {
                    if (it->second == 1) freq_map.erase(it);  // Remove if count reaches zero
                    else it->second--;                         // Otherwise, decrement count
                }
            };

            // Now we only have one method that uses the calculated row1, row2, and p
            #pragma unroll
            for (int col = 0; col < 6; col++)
            {
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
                S.get_element(row1, col).denom_exp += (S.get_element(row1, col).int_c!=0);
                S.get_element(row2, col).denom_exp += (S.get_element(row2, col).int_c!=0);
                
                row1_element_abs = std::abs(S.get_element(row1, col));
                row2_element_abs = std::abs(S.get_element(row2, col));
                
                // Update frequencies
                S.row_frequency[row1][row1_element_abs]++;
                S.row_frequency[row2][row2_element_abs]++;
                S.col_frequency[col][row1_element_abs]++;
                S.col_frequency[col][row2_element_abs]++;
            }

            S.canonical_form();
            S.hash = SO6_hash(S);
            S.last_T = i;
            return S;
        }

        // Custom iterator class
        class Iterator {
            public:
                Iterator(const SO6& so6, int index, const uint8_t* Row = nullptr, const uint8_t* Col = nullptr)
                    : so6_(so6), index_(index), Row_(Row), Col_(Col) {}

                // Copy assignment operator
                Iterator& operator=(const Iterator& other) {
                    if (this != &other) {
                        index_ = other.index_;
                    }
                    return *this;
                }
                // Dereference operator to get the current element based on Row and Col permutation
                const Z2& operator*() const {
                    int row_index = (Row_ != nullptr) ? Row_[index_ % 6] : index_ % 6;
                    int col_index = (Col_ != nullptr) ? Col_[index_ / 6] : index_ / 6;              
                    return so6_.arr[(col_index << 2) + (col_index << 1) + row_index];  // Access based on permutations
                }

                // Increment operator to move to the next element
                Iterator& operator++() {
                    ++index_;
                    return *this;
                }

                // Comparison operator (for end of range checking)
                bool operator!=(const Iterator& other) const {
                    return index_ != other.index_;
                }

                // Comparison operator for equality
                bool operator==(const Iterator& other) const {
                    return index_ == other.index_;
                }
                // Decrement operator
                Iterator& operator--() {
                    --index_;
                    return *this;
                }

                // Add integer offset (for pointer arithmetic)
                Iterator operator+(int offset) const {
                    return Iterator(so6_, index_ + offset);
                }

                // Subtract integer offset (for pointer arithmetic)
                Iterator operator-(int offset) const {
                    return Iterator(so6_, index_ - offset);
                }

                // Difference between two iterators
                int operator-(const Iterator& other) const {
                    return index_ - other.index_;
                }

            private:
                const SO6& so6_;   // Reference to the SO6 object
                int index_;        // Current position in iteration
                const uint8_t *Row_;
                const uint8_t *Col_;
        };

        Iterator begin() const {return Iterator(*this, 0);}
        Iterator begin(uint8_t* Row) const {return Iterator(*this, 0, Row, Col);}
        Iterator end() const {return Iterator(*this, 36);}       
        Iterator end(uint8_t *Row) const {return Iterator(*this, 0, Row, Col) +6;}       

        bool is_better_permutation(const uint8_t*, const uint8_t*,const uint16_t curr_sc);

        std::pair<SO6::Iterator,SO6::Iterator> get_column(const uint8_t  col, const uint8_t* Row_ = nullptr, const uint8_t* Col_ = nullptr) const {
            return std::pair<SO6::Iterator,SO6::Iterator>(Iterator(*this, (col << 2) + (col << 1), Row_, Col_), Iterator(*this, (col << 2) + (col << 1) + 6, Row_, Col_));
        }
        std::pair<SO6::Iterator,SO6::Iterator> get_lex_column(const uint8_t  col) const {return get_column(col, Row, Col);}

    private:
        void canonical_form();
        FrequencyMap row_frequency[6];
        FrequencyMap col_frequency[6];

        static inline __attribute__((always_inline)) uint8_t mask_at_index(const uint16_t& mask, const int& index) {
            return (mask >> (2*index)) & 0b11;
         }

   // A fast column hash that is consistent with the lexicographic comparison:
    // - It examines the 6 elements (each 24 bits) in order.
    // - It determines a global “flip” flag based on the first nonzero element:
    //   if (first_nonzero.intPart < 0) then flip = true (if not already).
    // - Then it computes a hash by processing each element’s canonical value,
    //   where canonical = (flip ? -element.intPart : element.intPart) masked to 24 bits.
    // - The FNV‑1a–style mixing is done byte‐by‐byte so that all 24 bits are used.
    template <typename Iterator>
    static inline uint16_t column_hash(Iterator begin, Iterator end, uint16_t sign_mask = 0) {
        uint16_t hash = offset;

        int i = 0;
        auto it = begin;
        for (; it != end; ++it, ++i) {
            if ((*it).int_c == 0) continue;
            sign_mask = (((*it).int_c < 0)^(mask_at_index(sign_mask,i)==0b10) ) ? sign_mask^0xFFFF : sign_mask;
            break;
        }

        // Process each element in the range.
        #pragma unroll
        for (; it != end; ++it, ++i) {
            Z2 canonical = (mask_at_index(sign_mask, i) == 0b10) ? (-(*it)) : (*it);
            uint16_t combined = (static_cast<uint16_t>(canonical.int_c) * prime) +
                                (static_cast<uint16_t>(canonical.denom_exp) * (prime >> 2));
            hash = (hash * prime) ^ combined;
        }
        return hash;
    }

    // Now, in your SO6 class, you can define a member function that combines the
    // column hashes (assuming arr is stored in column-major order: each column occupies 6 consecutive elements).
    static inline uint16_t SO6_hash(const SO6 s) {
        uint16_t hash = offset;

        // There are 6 columns in an SO6.
        for (int col=0; col < 5; col++) {
            // Since get_index returns (col*6 + row), the columns are contiguous.
            auto column = s.get_column(col, s.Row, s.Col);

            // Compute the hash for this column.
            uint16_t col_hash = column_hash(column.first, column.second, s.sign_convention);
            
            // Combine into the overall hash.
            hash = (hash * prime) ^ col_hash;
        }
        return hash;
    };

};

namespace std {
    template <>
    struct hash<SO6> {
        uint16_t operator()(const SO6& so6) const {
            return so6.hash;
        }
    };
}

#endif
