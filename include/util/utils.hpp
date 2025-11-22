/**
 * @file utils.hpp
 * @brief Utility helpers for sign masks and lexicographic comparisons on SO6.
 */
#ifndef UTILS_HPP
#define UTILS_HPP

#include "so6/SO6.hpp"

/**
 * @file utils.hpp
 * @brief Utility functions for set operations and conversions.
 */
namespace utils {
    // Compact sign mask: 1 bit per row (0 = POS, 1 = NEG)
    static constexpr uint16_t NEG = 0b1;
    static constexpr uint16_t POS = 0b0;
    static constexpr uint8_t  BITS = 0b1;

    static constexpr auto Equal = std::strong_ordering::equal;
    static constexpr auto Less = std::strong_ordering::less;
    static constexpr auto Greater = std::strong_ordering::greater;
    static constexpr auto Equivalent = std::strong_ordering::equivalent;

    inline static uint8_t mask_at_index(const uint16_t& mask, const int& index) {
        return static_cast<uint8_t>((mask >> index) & BITS);
    }

    inline static uint16_t& set_mask_sign(uint16_t& mask, const int& index, const uint16_t& sign) {
        // Branchless: set bit at position `index` to (sign & 1)
        const uint16_t bit = static_cast<uint16_t>(1u << index);
        const uint16_t sel = static_cast<uint16_t>(-static_cast<int>(sign & 0x1u)); // 0x0000 or 0xFFFF
        mask = static_cast<uint16_t>((mask & ~bit) | (sel & bit));
        return mask;
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
            comp1 = (*first_it).int_c <=> 0;
            comp2 = (*second_it).int_c <=> 0;
            if (comp1 == Equal && comp2 == Equal) continue;
            if (comp1 == Equal) return Greater;
            if (comp2 == Equal) return Less;

            uint8_t fsm = mask_at_index(first_sign_mask, i);
            uint8_t ssm = mask_at_index(second_sign_mask, i);

            if ((comp1 == Less) ^ (fsm == NEG)) first_sign_mask  ^= 0x3Fu; // flip all 6 bits
            if ((comp2 == Less) ^ (ssm == NEG)) second_sign_mask ^= 0x3Fu;
            break;
        }

        bool first_is_neg = false;
        bool second_is_neg = false;

        std::strong_ordering comparison = Equal;

        for (; first_it != first_end && second_it != second_end; ++first_it, ++second_it, ++i) {
            
            first_is_neg = (mask_at_index(first_sign_mask, i) == NEG);
            second_is_neg = (mask_at_index(second_sign_mask, i) == NEG);
            comparison = (second_is_neg ? -*second_it : *second_it) <=> (first_is_neg ? -*first_it : *first_it);
        
            if (comparison == Equal) continue;
            if ((*first_it).int_c == 0) return Greater;
            if ((*second_it).int_c == 0) return Less;

            return comparison;
        }
        
        return Equal;  // All elements are equal
    }

    // Removed unused helpers (apply_sign_mask, lex_less) to reduce dead code
};
#endif // UTILS_HPP
