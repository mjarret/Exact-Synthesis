#include <iomanip>  // For std::setw
#include <fstream>
#include <boost/format.hpp>
#include "SO6.hpp"
#include "Globals.hpp"
#include "utils.hpp"
#include "sort6.hpp"
#include "./include/Z2.hpp"

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
void SO6::canonical_form() {

    // Get equivalence classes and put into a consistent form
    auto row_ecs = row_equivalence_classes();

    uint8_t* ptr = Row;
    for (const auto& [row_freq_map, rows] : row_ecs) {
        ptr = std::copy(rows.begin(), rows.end(), ptr);
    }

    ptr = Col;
    auto col_ecs = col_equivalence_classes();
    for (const auto& [col_freq_map, cols] : col_ecs) {
        ptr = std::copy(cols.begin(), cols.end(), ptr);
    }

    uint8_t row_perm[6];
    uint8_t col_perm[6];
    
    do { 
        ptr = row_perm;
        for (const auto&[key, group] : row_ecs) {
            ptr = std::copy(group.begin(), group.end(), ptr);
        }
        
        // for(auto sc : utils::all_row_masks(*this, row_perm, col_ecs)) {
        for(uint8_t k = 0; k < 32; ++k) {
            uint16_t sc = utils::POS;
            for(int l = 1; l < 6; ++l) {
                if (k & (1 << (l-1))) {
                    sc = utils::set_mask_sign(sc, l, utils::NEG);
                } else {
                    sc = utils::set_mask_sign(sc, l, utils::POS);
                }
            }

            auto comparator = [&](int i, int j) {
                auto left = get_column(i, row_perm);
                auto right = get_column(j, row_perm);
                return Less == utils::lex_order(left, right, sc, sc);
            };

            ptr = col_perm;
            for (auto &[key, col_class] : col_ecs) {
                sort6::sorting_network_dispatch(col_class, comparator);
                ptr = std::copy(col_class.begin(), col_class.end(), ptr);
            }

            if (is_better_permutation(row_perm, col_perm, sc)) {
                std::copy(row_perm, row_perm + 6, Row);
                std::copy(col_perm, col_perm + 6, Col);
                sign_convention = sc;
            }
        }
    }  while (get_next_equivalence_class(row_ecs));
}

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
std::map<std::map<Z2, int>, std::vector<int>> SO6::row_equivalence_classes() {
    std::map<std::map<Z2, int>, std::vector<int>> ret;

    // Go in order to maintain sort
    for (int row = 0; row < 6; ++row) {
        std::map<Z2, int> &key = row_frequency[row];
        ret[key].push_back(row);
    }
    return ret;
}

std::map<std::map<Z2, int>, std::vector<int>> SO6::col_equivalence_classes() {
    std::map<std::map<Z2, int>, std::vector<int>> ret;

    // Iterate in order to maintain sort
    for (int col = 0; col < 6; ++col) {
        std::map<Z2, int> key = col_frequency[col];
        ret[key].push_back(col);
    }

    return ret;
}

// This function doesn't work.
// bool SO6::get_next_equivalence_class(std::vector<std::vector<int>>& row_equivalence_classes) {
bool SO6::get_next_equivalence_class(std::map<std::map<Z2, int>, std::vector<int>>& row_equivalence_classes) {
    bool more_permutations = false;
    for (auto&[key,group] : row_equivalence_classes) {
        if (std::next_permutation(group.begin(), group.end())) {
            more_permutations = true;
            break;
        } else {
            std::sort(group.begin(), group.end());
        }
    }

    return more_permutations;
}

const std::strong_ordering SO6::operator<=>(const SO6 &other) const
{
    // I think we can assume they have the same hash value at this point
    for (int col = 0; col < 5; ++col)
    {
        auto first_iterator_pair = get_column(col, Row, Col);
        auto other_iterator_pair = other.get_column(col, other.Row, other.Col);

        std::strong_ordering result = utils::lex_order(first_iterator_pair, other_iterator_pair, sign_convention, other.sign_convention);
        if(result != Equal) return result;
    }

    return Equal;
}

const uint8_t SO6::getLDE() const {
    return std::max_element(arr, arr + 36, [](const Z2& a, const Z2& b) {
        return a.denom_exp < b.denom_exp;
    })->denom_exp;
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

void SO6::unpermuted_print(const uint8_t Row_[6], const uint8_t Col_[6]) const {
    int maxWidth = 0;

    // Determine the maximum width of elements
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 6; ++col) {
            std::stringstream ss;
            ss << arr[get_index(Row_[row], Col_[col])];
            maxWidth = std::max(maxWidth, static_cast<int>(ss.str().length()));
        }
    }

    for (int col = 0; col < 6; ++col) {
        std::stringstream ss;
        maxWidth = std::max(maxWidth, static_cast<int>(std::string("Col [" + std::to_string(col) + "] =" + std::to_string(Col_[col])).length()));
    }

    const int width = maxWidth + 2;  // Adjust the width by adding some padding

    std::stringstream precomputed_output;

    // Print column headers using Boost.Format
    precomputed_output << "\n";
    precomputed_output << boost::format("%-" + std::to_string(width) + "s") % "";  // Adjust spacing for row labels
    for (int col = 0; col < 6; ++col) {
        std::stringstream ss;
        ss << ("Col[" + std::to_string(col) + "] =" + std::to_string(Col_[col]));
        precomputed_output << boost::format("%-" + std::to_string(width) + "s") % ss.str();
    }
    precomputed_output << "\n";

    // Print matrix rows and elements
    for (int row = 0; row < 6; ++row) {
        // Print row label with left border
        precomputed_output << boost::format("Row %-2d ") % (int) Row_[row];

        // Select border style for the row
        std::string leftBorder = (row == 0) ? "⌈ " : ((row == 5) ? "⌊ " : "| ");
        std::string rightBorder = (row == 0) ? " ⌉" : ((row == 5) ? " ⌋" : " |");

        precomputed_output << leftBorder;

        // Precompute each element in the row and format it using Boost.Format
        for (int col = 0; col < 6; ++col) {
            std::stringstream ss;
            ss << get_element(Row_[row], Col_[col]);
            precomputed_output << boost::format("%-" + std::to_string(width) + "s") % ss.str();
        }

        // Print right border
        precomputed_output << rightBorder << "\n";
    }
    precomputed_output << "\n";

    // Output everything at once after precomputing
    std::cout << precomputed_output.str();
}

void SO6::unpermuted_print() const {
    unpermuted_print(this->Row, this->Col);
}
