#ifndef UTILS_HPP
#define UTILS_HPP

#include <vector>
#include <set>
#include <algorithm>
#include <random>
#include <sstream>
#include <bitset>
#include "Z2.hpp"
#include "SO6.hpp"


/**
 * @file utils.hpp
 * @brief Utility functions for set operations and conversions.
 */
class utils {
public:
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
    static std::vector<SO6> convert_to_vector_and_clear(std::set<SO6>& s) {
        std::vector<SO6> v(std::make_move_iterator(s.begin()), std::make_move_iterator(s.end()));
        s.clear(); // Clear the set
        std::random_device rd;
        std::mt19937 g(rd());
        std::shuffle(v.begin(), v.end(), g);
        return v;  // Return vector
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

    


    // /**
    //  * @brief Performs the set difference operation (A \ B).
    //  * Erases elements from set A that are also in set B.
    //  * @param A Set from which elements will be erased.
    //  * @param B Set containing elements to be removed from A.
    //  */
    // static void setDifference(std::set<SO6>& A, const std::set<SO6>& B) {
    //     for (auto it = B.begin(); it != B.end(); ++it) {
    //         A.erase(*it);
    //     }
    // }

    /**
     * @brief Performs the set difference operation (A \ B).
     * Erases elements from set A that are also in set B.
     * @param A Set from which elements will be erased.
     * @param B Set containing elements to be removed from A.
     */
    template<typename T>
    static void setDifference(std::set<T>& A, const std::set<T>& B) {
        for (auto it = B.begin(); it != B.end(); ++it) {
            A.erase(*it);
        }
    }


    /**
     * @brief Rotates and clears sets for the next iteration.
     * @param prior Set to be cleared.
     * @param current Set to be moved to prior.
     * @param next Set to be moved to current.
     */
    static void rotate_and_clear(std::set<SO6>& prior, std::set<SO6>& current, std::set<SO6>& next) {
        std::set<SO6>().swap(prior); // Clear prior
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

    template <bool first_is_negative, bool second_is_negative>
    static bool lex_comparator(const Z2 &first, const Z2 &second) {
        if constexpr (first_is_negative) {
            if constexpr (second_is_negative) {
                return (-second < -first);
            } else {
                return (second < -first);
            }
        } else {
            if constexpr (second_is_negative) {
                return (-second < first);
            } else {
                return (second < first);
            }
        }
    }

    using LexComparatorType = bool (*)(const Z2&, const Z2&);
    static std::pair<LexComparatorType, LexComparatorType> get_comparators(const bool &first_is_negative, const bool &second_is_negative) {
        auto comparator_fs = lex_comparator<false, false>;
        auto comparator_sf = lex_comparator<false, false>;

        if (first_is_negative && second_is_negative) {
            comparator_fs = lex_comparator<true, true>;
            comparator_sf = lex_comparator<true, true>;
        } else if (first_is_negative && !second_is_negative) {
            comparator_fs = lex_comparator<true, false>;
            comparator_sf = lex_comparator<false, true>;
        } else if (second_is_negative && !first_is_negative) {
            comparator_fs = lex_comparator<false, true>;
            comparator_sf = lex_comparator<true, false>;
        }

        return std::make_pair(comparator_fs, comparator_sf);
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
    static std::strong_ordering lex_order(Iterator first_begin, Iterator first_end, Iterator second_begin, Iterator second_end, const uint8_t& first_sign = 0, const uint8_t& second_sign = 0) {
        uint8_t first_sign_mask = (first_sign << 1); // Pad with 0 for first element
        uint8_t second_sign_mask = (second_sign << 1); // Pad with 0 for first element

        // Find the first non-zero element in both ranges and determine sign
        Iterator first_it = first_begin;
        Iterator second_it = second_begin;
        int i = 0;
        for (; first_it != first_end && second_it != second_end; ++first_it, ++second_it, ++i) {
            if ((*first_it).intPart == 0) {
            if ((*second_it).intPart == 0) {
                continue;  // Both are zero, continue to the next element
            }
            return std::strong_ordering::greater;  // First is zero, second is non-zero => first > second
            }
            if ((*second_it).intPart == 0) {
            return std::strong_ordering::less;  // First is non-zero, second is zero => first < second
            }

            // Both first and second are non-zero, now determine their signs
            if (((*first_it).intPart < 0 && ((first_sign_mask >> i) & 1) == 0) || 
            ((*first_it).intPart > 0 && ((first_sign_mask >> i) & 1) == 1)) {
                first_sign_mask = ~first_sign_mask;
            }
            if (((*second_it).intPart < 0 && ((second_sign_mask >> i) & 1) == 0) || 
            ((*second_it).intPart > 0 && ((second_sign_mask >> i) & 1) == 1)) {
                second_sign_mask = ~second_sign_mask;
            }
            break;
        }
        // Compare the remaining elements
        for (; first_it != first_end && second_it != second_end; ++first_it, ++second_it, ++i) {
            auto comparator = get_comparators((first_sign_mask >> i) & 1, (second_sign_mask >> i) & 1);
            if (comparator.first(*first_it, *second_it)) {
                return std::strong_ordering::less;  // first < second
            } 
            else if (comparator.second(*second_it, *first_it)) {
                return std::strong_ordering::greater;  // first > second;
            }
        }

        return std::strong_ordering::equal;  // All elements are equal
    }

    static std::strong_ordering lex_order(const std::pair<SO6::Iterator,SO6::Iterator> first, const std::pair<SO6::Iterator,SO6::Iterator> second, const uint8_t& first_sign = 0, const uint8_t& second_sign = 0) {
        return lex_order(first.first, first.second, second.first, second.second, first_sign, second_sign);
    }

    // static std::strong_ordering lex_order(const SO6 &left, const SO6 &right, const int &col)
    // {
    //     return lex_order(left_begin, left_begin + 6,  right_begin, right_begin + 6,left.sign_convention, right.sign_convention);
    // }

    template <typename Iterator>
    static bool lex_less(Iterator first_begin, Iterator first_end, Iterator second_begin, Iterator second_end) {
        return (lex_order(first_begin, first_end, second_begin, second_end) == std::strong_ordering::less);
    }


};


#endif // UTILS_HPP


