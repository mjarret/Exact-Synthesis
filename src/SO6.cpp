#include <iomanip>  // For std::setw
#include <fstream>
#include <boost/format.hpp>
#include "SO6.hpp"
#include "Globals.hpp"
#include "utils.hpp"
#include "sort6.hpp"
#include "Z2.hpp"

// Alias the values of std::strong_ordering for cleaner code
constexpr auto Equal = std::strong_ordering::equal;
constexpr auto Less = std::strong_ordering::less;
constexpr auto Greater = std::strong_ordering::greater;
constexpr auto Equivalent = std::strong_ordering::equivalent;

/**
 * Basic constructor. Initializes Zero matrix.
 *
 */
SO6::SO6() : arr{Z2(static_cast<uint32_t>(0))}
{
    for(int i = 1; i < 36; i++) {arr[i] = Z2(static_cast<uint32_t>(0));}
}

// SO6Lite conversions removed in this build

const SO6& SO6::identity() {
    static const SO6 I = []() {
        SO6 temp;
        for (int k = 0; k < 6; k++) {
            // reserve() was a no-op for SmallFreqMap; removed for micro-optimization
            temp.arr[(k << 2) + (k << 1) + k] = Z2(1, 0, 0);
            temp.row_frequency[k][Z2(1, 0, 0)] = 1;
            temp.row_frequency[k][Z2(0, 0, 0)] = 5;
            temp.col_frequency[k][Z2(1, 0, 0)] = 1;
            temp.col_frequency[k][Z2(0, 0, 0)] = 5;
        }
        temp.canonical_form();
        temp.last_T = 15;
        temp.hash = prime;
        temp.col_hash = prime;
        return temp;
    }();
    return I;
}

/**
 * Overloads the * operator with matrix multiplication for SO6 objects
 * @param other reference to SO6 to be multiplied with (*this)
 * @return matrix multiplication of (*this) and other
 */
SO6 SO6::operator*(const SO6 &other) const
{
    SO6 prod;

    for (int row = 0; row < 6; ++row)
    {
        for (int k = 0; k < 6; ++k)
        {
            const Z2& left_element = get_element(row,k);
            if (left_element.int_c == 0) continue;
            for (int col = 0; col < 6; ++col)
            {
                if((other.get_element(k,col)).int_c == 0) continue;
                prod.get_element(row,col) += (left_element * other.get_element(k,col));
            }
        }
    }
    return prod;
}

SO6 SO6::left_multiply_by_T(const uint8_t i) const
{
    SO6 prod = *this;
    switch (i) {
        case 0: return left_multiply_by_T<0>(prod);
        case 1: return left_multiply_by_T<1>(prod);
        case 2: return left_multiply_by_T<2>(prod);
        case 3: return left_multiply_by_T<3>(prod);
        case 4: return left_multiply_by_T<4>(prod);
        case 5: return left_multiply_by_T<5>(prod);
        case 6: return left_multiply_by_T<6>(prod);
        case 7: return left_multiply_by_T<7>(prod);
        case 8: return left_multiply_by_T<8>(prod);
        case 9: return left_multiply_by_T<9>(prod);
        case 10: return left_multiply_by_T<10>(prod);
        case 11: return left_multiply_by_T<11>(prod);
        case 12: return left_multiply_by_T<12>(prod);
        case 13: return left_multiply_by_T<13>(prod);
        case 14: return left_multiply_by_T<14>(prod);
        default: throw std::invalid_argument("Invalid value for i");
    }
}

/**
 * @brief Transforms the current object into its canonical form.
 *
 * This function performs the following steps:
 * 1. Retrieves the row equivalence classes and copies them into the Row array.
 * 2. Retrieves the column equivalence classes and copies them into the Col array.
 * 3. Initializes row and column permutation arrays.
 * 4. Iterates over all possible sign conventions (32 in total).
 *    - For each sign convention:
 *      a. Copies the row equivalence classes into the row permutation array.
 *      b. Sorts each subset in the column equivalence classes independently based on the current sign convention.
 *      c. Copies the sorted column equivalence classes into the column permutation array.
 *      d. Checks if the current permutation is better than the previous one.
 *         - If it is, updates the Row and Col arrays with the current permutation and sets the sign convention.
 *    - Continues to the next equivalence class permutation.
 */


bool SO6::is_better_permutation(const uint8_t* row_perm, const uint8_t* col_perm, const uint16_t sign_perm) {
    for(int col = 0; col < 6; col++) {
        auto current = get_column(col, Row, Col);
        auto new_col = get_column(col, row_perm, col_perm);
        auto comparison = utils::lex_order(current, new_col, sign_convention, sign_perm);

        if (comparison == Equal) continue;
        return comparison == Greater;
    }
    return false;
}

/**
 * @brief Computes the row equivalence classes for the SO6 object.
 *
 * This function iterates through the rows of the SO6 object and groups them
 * into equivalence classes based on their frequency distribution. The result
 * is a map where the keys are maps representing the frequency distribution of
 * elements in each row, and the values are vectors containing the indices of
 * rows that share the same frequency distribution.
 *
 * @return A map where each key is a map of Z2 to int representing the frequency
 *         distribution of a row, and each value is a vector of row indices that
 *         have the same frequency distribution.
 */
// canonicalization and equivalence class helpers moved to src/algo/Canonicalizer.cpp

const std::strong_ordering SO6::operator<=>(const SO6 &other) const
{   
    std::strong_ordering comp = col_hash <=> other.col_hash;
    // return comp;

    if (comp == Equal) {   
        for (int col = 0; col < 5; ++col)
        {
            auto first_iterator_pair = get_column(col, Row, Col);
            auto other_iterator_pair = other.get_column(col, other.Row, other.Col);

            std::strong_ordering result = utils::lex_order(first_iterator_pair, other_iterator_pair, sign_convention, other.sign_convention);
            if(result != Equal) {
                return result;
            }
        }
    }
    return  comp;
}

/**
 * Overloads << function for SO6.
 * @param os reference to ostream object needed to implement <<
 * @param m reference to SO6 object to be displayed
 * @returns reference ostream with the matrix's display form appended
 */
std::ostream &operator<<(std::ostream &os, const SO6 &m) {
    int maxWidth = 0;

    // Find the maximum width of the elements
    for (int row = 0; row < 6; row++) {
        for (int col = 0; col < 6; col++) {
            std::stringstream ss;
            ss << m.get_element(row,col);
            maxWidth = std::max(maxWidth, static_cast<int>(ss.str().length()));
        }
    }

    const int width = maxWidth + 3; // Adjust the width by adding 2

    os << "\n";
    for (int row = 0; row < 6; row++) {
        std::string leftBorder = (row == 0) ? "⌈" : ((row == 5) ? "⌊" : "|");
        std::string rightBorder = (row == 0) ? "⌉" : ((row == 5) ? "⌋" : "|");

        os << leftBorder << "  ";
        for (int col = 0; col < 6; col++) {
            os << std::setw(width) << m.get_element(row,col);
        }
        os << "\t" << rightBorder << "\n";
    }
    os << "\n";

    return os;
}
