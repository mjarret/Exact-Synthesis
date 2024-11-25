#ifndef UTILS_HPP
#define UTILS_HPP

#include <vector>
#include <set>
#include <algorithm>
#include <random>
#include <sstream>
#include <bitset>
#include <tbb/concurrent_set.h>
#include <tbb/parallel_for.h>
#include "Z2.hpp"
#include "SO6.hpp"
#include "utils.hpp"


/**
 * @file utils.hpp
 * @brief Utility functions for set operations and conversions.
 */
class utils {
public:

    static constexpr uint16_t NEG = 0b10;
    static constexpr uint16_t POS = 0b01;
    static constexpr uint16_t DISAGREE = 0b11;
    static constexpr uint16_t AGREE = 0b00;
    static constexpr uint16_t UNSET = 0b00;
    static constexpr uint8_t BITS = 0b11;


    static constexpr auto Equal = std::strong_ordering::equal;
    static constexpr auto Less = std::strong_ordering::less;
    static constexpr auto Greater = std::strong_ordering::greater;
    static constexpr auto Equivalent = std::strong_ordering::equivalent;

    static void print_sign_mask(const uint16_t& mask) {
        for(int i=0; i<6; ++i) {
            uint16_t tmp = mask_at_index(mask, i);
            
            if( tmp == NEG) std::cout << "-";
            else if( tmp == POS) std::cout << "+";
            else if( tmp == DISAGREE) std::cout << "\u00BF";
            else if( tmp == AGREE) std::cout << "?";
        }
    }

    inline static uint8_t mask_at_index(const uint16_t& mask, const int& index) {
        return (mask >> (2*index)) & BITS;
    }

    inline static uint16_t& set_mask_sign(uint16_t& mask, const int& index, const uint16_t& sign) {
        mask ^= ((mask >> (2 * index)) & BITS) << (2 * index);  // Clear the two bits at the position
        mask |= (sign << (2 * index));  // Set the two bits to `sign`
        return mask;
    }

    /**
     * @brief Calculate the free multiply depth.
     * @param target_T_count Total T count.
     * @param stored_depth_max Maximum depth to store.
     * @return Free multiply depth.
     */
    static int free_multiply_depth(int target_T_count, int stored_depth_max) {
        return target_T_count - stored_depth_max;
    }

    /**
     * @brief Converts a set of SO6s to a shuffled vector and clears the set.
     * @param s Set of SO6 to be converted.
     * @return A shuffled vector containing the elements originally in the set.
     */
    static std::vector<SO6> convert_to_vector_and_clear(tbb::concurrent_set<SO6>& s) {
        std::vector<SO6> v;
        v.reserve(s.size()); // Reserve space to avoid reallocations
        
        // Explicitly move elements from the set into the vector
        for (auto it = s.begin(); it != s.end(); ++it) {
            v.push_back(std::move(*it));
        }
        
        s.clear(); // Clear the set after moving elements
        
        // Shuffle the vector
        if (!v.empty()) {
            static thread_local std::mt19937 g(std::random_device{}());
            std::shuffle(v.begin(), v.end(), g);
        }
        
        return v; // Return the shuffled vector
    }

    static std::string convert_csv_line_to_binary(const std::string& line) {
        std::stringstream ss(line);
        std::string item;
        std::string binaryString;

        while (std::getline(ss, item, ',')) {
            int number = std::stoi(item);
            binaryString += std::bitset<2>(number).to_string();
        }
        if(binaryString.length() !=72) {
            std::cout << "shit.";
            std::exit(0);
        }
        return binaryString;
    }

    /**
     * @brief Performs the set difference operation (A \ B).
     * Erases elements from set A that are also in set B.
     * @param A Set from which elements will be erased.
     * @param B Set containing elements to be removed from A.
     */
    // template<typename T>
    // static void setDifference(tbb::concurrent_set<T>& A, const tbb::concurrent_set<T>& B) {
    //     for (auto it = B.begin(); it != B.end(); ++it) {
    //         A.unsafe_erase(*it);
    //     }
    // }
    template<typename T>
    static void setDifference(tbb::concurrent_set<T>& A, const tbb::concurrent_set<T>& B) {
        tbb::concurrent_set<T> result;
        for (const auto& elem : A) {
            if (B.find(elem) == B.end()) {
                result.insert(elem);
            }
        }
        A.swap(result); // Efficiently replace A with the new set
    }

    /**
     * @brief Rotates and clears sets for the next iteration.
     * @param prior Set to be cleared.
     * @param current Set to be moved to prior.
     * @param next Set to be moved to current.
     */
    static void rotate_and_clear(tbb::concurrent_set<SO6>& prior, tbb::concurrent_set<SO6>& current, tbb::concurrent_set<SO6>& next) {
        tbb::concurrent_set<SO6>().swap(prior); // Clear prior
        prior.swap(current); // Move current to prior
        current.swap(next); // Move next to current
    }

    /**
     * @brief Calculate the number of generating sets.
     * @param tt Total T count.
     * @param sdm Stored depth maximum.
     * @return Number of generating sets.
     */
    static int num_generating_sets(int total_T_count, int max_stored_depth) {
        return std::min(total_T_count - 1 - max_stored_depth, max_stored_depth); 
    }

    template <typename ForwardIt, typename Compare = std::less<>>
    static std::vector<typename std::iterator_traits<ForwardIt>::value_type>
    find_all_maxima(ForwardIt first, ForwardIt last, Compare comp = Compare()) {
        using T = typename std::iterator_traits<ForwardIt>::value_type;
        
        if (first == last) return {};  // Return empty if range is empty

        std::vector<T> maxima;
        auto maxElem = *first;
        
        for (auto it = first; it != last; ++it) {
            if (comp(maxElem, *it)) {
                maxElem = *it;
                maxima.clear();
                maxima.push_back(*it);
            } else if (!comp(*it, maxElem)) {
                maxima.push_back(*it);
            }
        }
        
        return maxima;
    }

    /**
     * @brief Compares two ranges lexicographically, considering their sign.
     *
     * This function compares two ranges, `first` and `second`, taking into account their respective signs.
     * The comparison is done lexicographically:
     * - If both ranges have the same sign, the comparison is straightforward.
     * - If the signs differ, the function determines the order based on the sign and value.
     *
     * @tparam Iterator Type of the iterator.
     * @param first_begin Iterator to the beginning of the first range.
     * @param first_end Iterator to the end of the first range.
     * @param second_begin Iterator to the beginning of the second range.
     * @param second_end Iterator to the end of the second range.
     * @return std::strong_ordering::less if the first range is less than the second range,
     *         std::strong_ordering::equal if the ranges are equal,
     *         std::strong_ordering::greater if the first range is greater than the second range.
     */
    template <typename Iterator>
    static std::strong_ordering lex_order(Iterator &first_it, Iterator &first_end, Iterator &second_it, Iterator &second_end, uint16_t first_sign_mask = 0, uint16_t second_sign_mask = 0) {
        // Find the first non-zero element in both ranges and determine sign

        int i = 0;

        std::strong_ordering comp1 = Equal;
        std::strong_ordering comp2 = Equal;

        for (; first_it != first_end && second_it != second_end; ++first_it, ++second_it, ++i) {
            comp1 = (*first_it).intPart <=> 0;
            comp2 = (*second_it).intPart <=> 0;
            if (comp1 == Equal && comp2 == Equal) continue;
            if (comp1 == Equal) return Greater;
            if (comp2 == Equal) return Less;

            uint8_t fsm = mask_at_index(first_sign_mask, i);
            uint8_t ssm = mask_at_index(second_sign_mask, i);

            if((comp1 == Less)^(fsm==NEG)) first_sign_mask ^= 0xFFFF;
            if((comp2 == Less)^(ssm==NEG)) second_sign_mask ^= 0xFFFF;
                
            break;
        }

        bool first_is_neg = false;
        bool second_is_neg = false;

        std::strong_ordering comparison = Equal;

        for (; first_it != first_end && second_it != second_end; ++first_it, ++second_it, ++i) {
            
            first_is_neg = mask_at_index(first_sign_mask, i) == NEG;
            second_is_neg = mask_at_index(second_sign_mask, i) == NEG;
            comparison = (second_is_neg ? -*second_it : *second_it) <=> (first_is_neg ? -*first_it : *first_it);
        
            if (comparison == Equal) continue;
            if ((*first_it).intPart == 0) return Greater;
            if ((*second_it).intPart == 0) return Less;

            return comparison;
        }
        
        return Equal;  // All elements are equal
    }

    static void reverse_sign_mask(uint16_t& mask) {
        mask = ~mask;
    }

    static std::strong_ordering lex_order(std::pair<SO6::Iterator,SO6::Iterator>& first, std::pair<SO6::Iterator,SO6::Iterator>& second, const uint16_t& first_sign = 0, const uint16_t& second_sign = 0) {
        return lex_order(first.first, first.second, second.first, second.second, first_sign, second_sign);
    }

    /**
     * @brief Computes a mask indicating the sign of rows based on the elements of a matrix.
     *
     * This function iterates through the rows and columns of the given matrix `s` and determines
     * the sign of each row based on the elements in the matrix. The sign is determined by comparing
     * the elements to zero. If the total count of positive elements in a row is greater than the
     * count of negative elements, the row is considered positive. Otherwise, it is considered negative.
     *
     * @param s The matrix object of type SO6 from which elements are retrieved.
     * @param r_eq_c A map where keys are maps of Z2 to integers, and values are vectors of row indices.
     * @param col_eq_c A map where keys are maps of Z2 to integers, and values are vectors of column indices.
     * @return A uint8_t value representing the sign mask of the rows. Each bit in the returned value
     *         corresponds to a row, where a set bit indicates a negative row and an unset bit indicates
     *         a positive row.
     */
    static std::pair<uint16_t,uint16_t> sign_masks (SO6& s, uint8_t* Row, std::map<std::map<Z2, int>, std::vector<int>>& col_eq_c) {
        uint16_t row_mask = POS;    // This fixes global sign
        uint16_t prior = row_mask;
        uint16_t col_mask = 0;
        uint8_t curr_row_mask;
        bool is_changed = true;

        while(true) {
            for (int r = 0; r < 6; ++r) {
                curr_row_mask = mask_at_index(row_mask, Row[r]);

                if(curr_row_mask == AGREE || curr_row_mask == DISAGREE) set_mask_sign(row_mask, Row[r], majority_vote(s, Row[r], col_mask, col_eq_c)); 

                for (int c = 0; c < 6; ++c) {
                    uint8_t curr_col_mask = mask_at_index(col_mask, c);
                    if (curr_col_mask == AGREE || curr_col_mask == DISAGREE) {
                        int intPart = s.get_element(Row[r], c).intPart;

                        if(intPart == 0) continue;
                        else if(intPart < 0) set_mask_sign(col_mask, c, ~curr_row_mask);    // This can produce a col_mask of DISAGREE 
                        else if(intPart > 0) set_mask_sign(col_mask, c, curr_row_mask);
                    }
                }
            }
            if(prior == row_mask) break;
        }    

        return std::make_pair(row_mask, col_mask);
    }

    /**
     * @brief Computes the majority vote for a given row in a matrix.
     *
     * This function iterates through a map of column equivalence classes and 
     * calculates the majority vote based on the elements of the matrix `s` 
     * and the provided column mask. The function returns a 2-bit value 
     * representing the majority vote.
     *
     * @param s The matrix object of type SO6.
     * @param row The row index for which the majority vote is being computed.
     * @param proc An array of uint8_t representing the pivot elements.
     * @param col_mask A uint16_t bitmask representing the columns to be considered.
     * @param col_eq_c A map where the key is a map of Z2 to int, and the value is a vector of column indices.
     * 
     * @return uint16_t A 2-bit value representing the majority vote:
     *         - 0b00: Indicates that the function needs to try something else.
     *         - AGREE: Indicates a positive majority vote.
     *         - DISAGREE: Indicates a negative majority vote.
     */
    static uint16_t majority_vote (const SO6& s, const uint8_t& row, const uint16_t& col_mask, std::map<std::map<Z2, int>, std::vector<int>>& col_eq_c) {
        // Z2 row_total = Z2(0,0,0);    
        int row_total = 0;      
        // Needs to depend upon the row mask of the previous row. Try both options, because it won't matter which option we go with ultimately.
        for(auto &[z2, cols] : col_eq_c) {
            for (auto c : cols) {

                uint16_t sign = mask_at_index(col_mask, c);

                // Pivot elements can't be used to determine signs
                if (sign == AGREE || sign == DISAGREE)  continue;

                const Z2 &curr_el = s.get_element(row, c);

                if(curr_el.intPart == 0) continue; 

                bool negative_col = sign == NEG; 
                bool negative_el = curr_el.intPart < 0;
                
                if(negative_col == negative_el) row_total ++;
                else row_total --;

            }

            if (row_total != 0) return (row_total < 0) ? NEG : POS;
        }
        return UNSET;
    }

    // static const uint8_t* pivot_of_col(const SO6& s, const uint8_t* Row, const uint8_t& col) {
    //     for(auto r = Row; r != Row + 6; ++r) {
    //         std::strong_ordering comp = s.get_element(*r, col).intPart <=> 0;
    //         if(comp == std::strong_ordering::equal) continue; 
    //         return r;
    //     }
    //     return nullptr;
    // }

    static SO6& apply_sign_mask (SO6& s, uint16_t row_sign_mask, uint16_t col_sign_mask, uint8_t* row_perm) {
        for (auto r = row_perm; r < row_perm +6; ++r) {
            uint8_t row = *r;
            for(auto col = 0; col < 6; ++col)  {
                uint8_t cm = mask_at_index(col_sign_mask, col);
                uint8_t rm = mask_at_index(row_sign_mask, row);
                if (cm == NEG || cm == DISAGREE) s.get_element(row,col).negate();
                if (rm == NEG || rm == DISAGREE) s.get_element(row,col).negate();
            }
        }
        return s;
    }

    template <typename Iterator>
    static bool lex_less(Iterator first_begin, Iterator first_end, Iterator second_begin, Iterator second_end) {
        return (lex_order(first_begin, first_end, second_begin, second_end) == std::strong_ordering::less);
    }


    static std::vector<uint16_t> all_row_masks(SO6& s, uint8_t* Row, std::map<std::map<Z2, int>, std::vector<int>> col_eq_c) {
        std::vector<uint16_t> ret = {0};
        uint16_t rsm = (utils::sign_masks(s, Row, col_eq_c)).first;
        for(int i=0; i<6; ++i) {
            const uint8_t tmp = mask_at_index(rsm, i);
            if (tmp == AGREE || tmp == DISAGREE){
                // Duplicate each entry in `ret`, setting bit `i` in the new copies
                size_t size = ret.size();
                for (size_t k = 0; k < size; ++k) {
                    ret.push_back(set_mask_sign(ret[k], i, NEG));
                    ret.push_back(set_mask_sign(ret[k], i, POS));
                }
            }
        }
        return ret;
    }

};

// static bool is_valid_pattern(pattern pat) {
//     SO6 S, St;
//     for(int col = 0; col < 6; col++) {
//         for(int row = 0; row < 6; row++) {
//             S[col][row].intPart=pat.arr[col][row].first;
//             S[col][row].sqrt2Part=pat.arr[col][row].second;
//         }
//     }
//     for(int col = 0; col < 6; col++) {
//         for(int row = 0; row < 6; row++) {
//             St[col][row].intPart=pat.arr[row][col].first;
//             St[col][row].sqrt2Part=pat.arr[row][col].second;
//         }
//     }

//     S = St*S;
//     for(int col = 0; col < 6; col++) {
//         for(int row = 0; row < 6; row++) {
//             if(!(St[col][row] == 0)) return false;
//         }
//     }
//     return true;
// }



        
#endif // UTILS_HPP


