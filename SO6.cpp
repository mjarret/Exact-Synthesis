#include <iostream>
#include <iomanip>  // For std::setw
#include <sstream>
#include <stdint.h>
#include <vector>
#include <algorithm>
#include <bitset>
#include <bit>
#include <variant>
#include <ranges>
#include <compare>
#include <concepts>
#include <omp.h>
#include <boost/format.hpp>
#include "SO6.hpp"
#include "pattern.hpp"
#include "Globals.hpp"
#include "utils.hpp"
#include <map>

// ANSI color codes
#define RESET "\033[0m"
#define RED "\033[31m"
#define GREEN "\033[32m"
#define YELLOW "\033[33m"
#define BLUE "\033[34m"
#define MAGENTA "\033[35m" // For elements currently under consideration


// Alias the values of std::strong_ordering for cleaner code
constexpr auto Equal = std::strong_ordering::equal;
constexpr auto Less = std::strong_ordering::less;
constexpr auto Greater = std::strong_ordering::greater;
constexpr auto Equivalent = std::strong_ordering::equivalent;

int SO6::get_pivot_column(std::bitset<6>& columns, std::unordered_map<std::bitset<6>, int>& memo) {
    std::cout << "Considering submatrix: " << columns << std::endl;
    unpermuted_print(columns);

    // Memo already contains base cases. Return memo if non-empty
    if (!memo[columns] == -1) return memo[columns];

    int submatrix_size = columns.count();

    // Lexicographically sort the row
    std::vector<int> col;
    for(int i = 0; i < 6; i++) {
        if(columns.test(i)) {
            col.push_back(i);
        }
    }

    int lex_row = 6 - submatrix_size;

    std::vector<int> pivots;
    for(int i = 0; i < 6; ++i) {
        if(!columns.test(i)) pivots.push_back(i);
    }

    // Find best pivot column. Logic should sort/memoize it while finding the maximum
    int pivot = *std::max_element(pivots.begin(), pivots.end(), [&](int a, int b) {
        // return lex_order(a, b, columns, memo, lex_row);
        return true;
    });

    std::cout << "Pivot found.";
    // Check if the pivot element corresponds to Z2(0,0,0)
    if (get_lex_element(pivot, lex_row).intPart == 0) {
        memo[columns] = -2;
    } else {
        memo[columns] = pivot;
    }
    return memo[columns];
}

std::vector<int> get_permutation(const std::bitset<6>& columns, std::unordered_map<std::bitset<6>, std::vector<int>>& memo) {
    if(memo.contains(columns)) return memo[columns]; // If it's already computed, return it
    // Logic to compute the memo for a given column set.
    // This is where you perform your submatrix computation if necessary.
    // For the example, we'll return an empty vector.
    return {};
}

bool SO6::submatrix_lex_less(std::vector<int> &left_columns, std::vector<int> &right_columns, int start_row) {
    if (start_row == 6) return false;

    // These were equal, move onto the next submatrix
    std::vector<int> left_columns_copy = left_columns;
    std::vector<int> right_columns_copy = right_columns;

    left_columns_copy.erase(left_columns_copy.begin());
    right_columns_copy.erase(right_columns_copy.begin());
    // Recursively check the next submatrix
    return submatrix_lex_less(left_columns_copy, right_columns_copy, start_row + 1);
}

/**
 * Basic constructor. Initializes Zero matrix.
 *
 */
SO6::SO6()
{
    for(int i = 0; i < 36; i++) {
        arr[i] = Z2(0,0,0);
    }
    int equivalence_class_size = 1;

}

/**
 * Basic constructor. Initializes to other
 *
 */
SO6::SO6(Z2 other[6][6])
{
    for(int col =0; col<6; col++) {
        for(int row=0; row <6; row++) {
            *this[col][row]=other[col][row];
        }
    }
}

SO6::SO6(pattern &other)
{
    for (int col = 0; col < 6; col++) {
        for (int row = 0; row < 6; row++) {
            if (other.arr[col][row].first == 0 && other.arr[col][row].second == 0) {
                continue;  // Skip this iteration if both `first` and `second` are zero
            }
            bool second_arg = other.arr[col][row].first == 0 ? other.arr[col][row].first : other.arr[col][row].second;
            arr[(col << 2) + (col << 1) + row] = Z2(other.arr[col][row].first || other.arr[col][row].second, second_arg, other.arr[col][row].first);
        }
    }

}

// Something much faster than this would be a "multiply by T" method that explicitly does the matrix multiplication given a particular T matrix instead of trying to compute it naively

/**
 * Overloads the * operator with matrix multiplication for SO6 objects
 * @param other reference to SO6 to be multiplied with (*this)
 * @return matrix multiplication of (*this) and other
 */
SO6 SO6::operator*(const SO6 &other) const
{
    // multiplies operators assuming COLUMN,ROW indexing
    SO6 prod;

    // let's see what happens if i turn off history printing
    prod.hist.reserve(hist.size() + other.hist.size());  // Reserve instead of resize
    std::copy(other.hist.begin(), other.hist.end(), std::back_inserter(prod.hist));
    std::copy(hist.begin(), hist.end(), std::back_inserter(prod.hist));

    for (int row = 0; row < 6; ++row)
    {
        for (int k = 0; k < 6; ++k)
        {
            const Z2& left_element = *this[k][row];
            if (left_element.intPart == 0) continue;
            for (int col = 0; col < 6; ++col)
            {
                if((other[col][k]).intPart == 0) continue;
                prod[col][row] += (left_element * other[col][k]);
            }
        }
    }
    return prod;
}

/**
 * Overloads the * operator with matrix multiplication for SO6 objects
 * @param other reference to pattern to be multiplied with (*this)
 * @return matrix multiplication of (*this) and other
 */
SO6 SO6::operator*(const pattern &other) const
{
    // multiplies operators assuming COLUMN,ROW indexing
    SO6 prod;
    prod.hist = hist; // patterns don't typically have histories in this code base, could be changed but currently not so

    for (int row = 0; row < 6; ++row)
    {
        for (int k = 0; k < 6; ++k)
        {
            const Z2& left_element = *this[k][row];
            if (left_element.intPart == 0) continue;

            Z2 smallerLDE = left_element;
            smallerLDE.exponent--; //decrease lde

            for (int col = 0; col < 6; col++)
            {
                if(other.arr[col][k].first) prod[col][row] += left_element;
                if(other.arr[col][k].second) prod[col][row] += smallerLDE;
            }
        }
    }
    return prod;
}

SO6 SO6::left_multiply_by_T(const int &i) const
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
    }
    // Handle invalid input case
    throw std::invalid_argument("Invalid value for i");
}

/// @brief left multiply this by a circuit
/// @param circuit circuit listed as a compressed vector of gates
/// @return the result circuit * this
SO6 SO6::left_multiply_by_circuit(std::vector<unsigned char> &circuit)
{
    SO6 prod = *this;
    for (unsigned char i : circuit)
    {
        prod = prod.left_multiply_by_T((i & 15) -1);
        if(i>15) {
            prod = prod.left_multiply_by_T((i>>4)-1);
        }
    }
    return prod;
}

void SO6::update_history(const unsigned char &p) {
    // Check if we need to start a new history entry
    if (hist.empty() || (hist.back() & 0xF0) != 0) {
        hist.reserve(hist.size() + 1);  // Reserve space for one more element
        hist.push_back(p);              // Add the new entry
    } else {
        // Pack the new entry into the higher 4 bits of the last byte
        hist.back() |= (p << 4);
    }
}

// Function to check consistency between two maps
bool is_consistent_column(const std::map<Z2, int>& mapA, const std::map<Z2, int>& mapB) {
    for (const auto& [key, valueA] : mapA) {
        auto it = mapB.find(key);
        if (it == mapB.end()) {
            // Key from mapA does not exist in mapB
            return false;
        }
        if (it->second < valueA) {
            // Frequency in mapB is less than in mapA
            return false;
        }
    }
    return true;
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
                if ( k & (1 << (l-1))) {
                    sc = utils::set_mask_sign(sc, l, utils::NEG);
                } else {
                    sc = utils::set_mask_sign(sc, l, utils::POS);
                }
            }
    
            ptr = col_perm;
            for (auto &[key, col_class] : col_ecs) {
                std::sort(col_class.begin(), col_class.end(), [&](int i, int j) {
                    auto left = get_column(i, row_perm);
                    auto right = get_column(j, row_perm);
                    return Less == utils::lex_order(left, right, sc, sc);
                });
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

bool SO6::is_better_permutation(const uint8_t* row_perm, const uint8_t* col_perm, const int &sign_perm) {
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
 * @brief Sorts the physical array to match the lexicographical order.
 *
 * This function sorts the physical array `arr` such that it matches the
 * lexicographical order defined by the `Row` and `Col` arrays.
 */
void SO6::sort_physical_array() {
    Z2 temp[36];
    std::map<Z2,int> temp_rf[6];
    for(size_t row = 0; row<6; row++) {
        for(size_t col = 0; col<6; col++) {
            temp[(col<<2) + (col<<1) + row] = arr[(Col[col]<<2) + (Col[col] <<1) + Row[row]];
        }
        temp_rf[row] = row_frequency[Row[row]];
    }
    for(int row =0; row <6 ; row++) {
        Row[row] = row;
        Col[row] = row;
    }

    std::copy(std::begin(temp), std::end(temp), std::begin(arr));
    std::copy(std::begin(temp_rf), std::end(temp_rf), std::begin(row_frequency));
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
    for (int row = 0; row < 6; row++) {
        std::map<Z2, int> &key = row_frequency[row];
        ret[key].push_back(row);
    }
    return ret;
}

// Return negative values to make life easier later
std::map<std::map<Z2, int>, int> SO6::entry_frequency_in_cols(std::vector<int>& cols) {
    std::map<std::map<Z2, int>, int> ret;

    // Go in order to maintain sort
    for (int row = 0; row < 6; row++) {
        std::map<Z2, int> next_row;
        for(int col : cols) {
            next_row[get_element(row, col).abs()]--;
        }
        ret[next_row]++;
    }
    return ret;
}

std::map<std::map<Z2, int>, std::vector<int>> SO6::col_equivalence_classes() {
    std::map<std::map<Z2, int>, std::vector<int>> ret;

    // Iterate in order to maintain sort
    for (int col = 0; col < 6; col++) {
        std::map<Z2, int> key = col_frequency[col];
        ret[key].push_back(col);
    }

    return ret;
}


/**
 * @brief Negates all elements in a specified row of a 6x6 matrix.
 *
 * This function iterates through each column of the specified (lex) row
 * and negates the element at that position.
 *
 * @param row Reference to the row index to be negated.
 */
void SO6::negate_row(int& row) {
    // std::cout << "Negating row " << row << std::endl;
    for (int col = 0; col < 6; ++col) {
        get_element(row,col).negate();
    }
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

// bool SO6::get_next_ec_permutation(std::vector<int>& ec) {
//     return std::next_permutation(ec.begin(), ec.end());
// }

std::string SO6::name() const
{
    return std::string(hist.begin(),hist.end());
}


SO6 SO6::reconstruct(const std::string& name) {
    SO6 ret = SO6::identity();
    for(unsigned char i : name) {
        ret = ret.left_multiply_by_T((i & 15) -1);
        if(i>15) ret = ret.left_multiply_by_T((i>>4)-1);
    }
    ret.canonical_form();
    return ret;
}

std::string SO6::name_as_num(const std::string name) {
    std::string ret;
    for(unsigned char i : name)
    {
        ret.append(std::to_string((uint) ((i & 15) -1)) + " ");
        if(i>15) ret.append(std::to_string((uint)((i>>4)-1)) + " ");
    }
    ret.pop_back();
    return ret;
}

std::string SO6::circuit_string() {
    std::string ret;
    for (unsigned char byte : hist) {
        int lower = (byte & 15) - 1;  // Lower 4 bits
        ret.append(std::to_string(lower) + " ");

        // Check for upper 4 bits
        if (byte > 15) {
            int upper = (byte >> 4) - 1;  // Upper 4 bits
            ret.append(std::to_string(upper) + " ");
        }
    }
    ret.pop_back();
    return ret;
}

SO6 SO6::reconstruct_from_circuit_string(const std::string& input) {
    std::istringstream iss(input);
    int number;
    SO6 ret = SO6::identity();
    // Iterate over each integer in the string
    while (iss >> number) {
        // Process each number, for example, print it
        ret = ret.left_multiply_by_T(number);
    }
    return ret;
}

const std::strong_ordering SO6::operator<=>(const SO6 &other) const
{
    for (int col = 0; col < 5; ++col)
    {

        auto first_iterator_pair = get_column(col, Row, Col);
        auto other_iterator_pair = other.get_column(col, other.Row, other.Col);

        std::strong_ordering result = utils::lex_order(first_iterator_pair, other_iterator_pair, sign_convention, other.sign_convention);
        if(result != Equal) return result;
    }

    return Equal;
}

const z2_int SO6::getLDE() const
{
    z2_int ret;
    for (int i = 0; i < 6; i++)
    {
        for (int j = 0; j < 6; j++)
        {
            ret = std::max(ret, (*this)[i][j].exponent);
        }
    }
    return ret;
}

pattern SO6::to_pattern() const
{
    pattern ret;
    ret.hist.reserve(hist.size());
    ret.hist = hist;

    const int8_t& lde = getLDE();
    for (int col = 0; col < 6; col++)
    {
        for (int row = 0; row < 6; row++)
        {
            if (arr[row + (col<<2) + (col<<1)].exponent < lde - 1 || arr[row + (col<<2) + (col<<1)].intPart==0) {
                continue;
            }
            if (arr[row + (col<<2) + (col<<1)].exponent == lde)
            {
                ret.arr[col][row].first = 1;
                ret.arr[col][row].second = arr[row + (col<<2) + (col<<1)].sqrt2Part % 2;
                continue;
            }
            ret.arr[col][row].second = 1;
        }
    }
    ret.lexicographic_order();
    return ret;
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
            ss << m[col][row];
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
            os << std::setw(width) << m[col][row];
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

void SO6::unpermuted_print(const uint8_t Row_[6], const uint8_t Col_[6], const std::vector<int>& pivotRows, const std::vector<int>& pivotCols) const {
    int maxWidth = 0;

    // Lambda to check if a value exists in a vector
    auto contains = [](const std::vector<int>& vec, int value) {
        return std::find(vec.begin(), vec.end(), value) != vec.end();
    };

    // Determine max width of elements without color codes
    for (int row = 0; row < 6; ++row) {
        for (int col = 0; col < 6; ++col) {
            std::stringstream ss;
            ss << arr[get_index(Row_[row], Col_[col])];
            maxWidth = std::max(maxWidth, static_cast<int>(ss.str().length()));
        }
    }

    // Determine max width for column headers
    for (int col = 0; col < 6; ++col) {
        std::stringstream ss;
        ss << "Col[" << col << "] =" << Col_[col];
        maxWidth = std::max(maxWidth, static_cast<int>(ss.str().length()));
    }

    const int width = maxWidth + 2;  // Add padding for consistency

    std::stringstream precomputed_output;

    // Print column headers
    precomputed_output << "\n" << boost::format("%-" + std::to_string(width) + "s") % ""; // Spacing for row labels
    for (int col = 0; col < 6; ++col) {
        std::string header = "Col[" + std::to_string(col) + "] =" + std::to_string(Col_[col]);
        if (contains(pivotCols, col)) {
            header = MAGENTA + header + RESET; // Add color if it's a pivot column
        } else {
            header = YELLOW + header + RESET; // Regular column color
        }
        precomputed_output << boost::format("%-" + std::to_string(width) + "s") % header;
    }
    precomputed_output << "\n";

    // Print matrix rows and elements
    for (int row = 0; row < 6; ++row) {
        std::string rowLabel = "Row " + std::to_string((int) Row_[row]);
        rowLabel = contains(pivotRows, row) ? MAGENTA + rowLabel + RESET : rowLabel;

        // Row label and border style
        precomputed_output << boost::format("%-" + std::to_string(width) + "s") % rowLabel;
        std::string leftBorder = (row == 0) ? "⌈ " : ((row == 5) ? "⌊ " : "| ");
        std::string rightBorder = (row == 0) ? " ⌉" : ((row == 5) ? " ⌋" : " |");
        precomputed_output << leftBorder;

        // Format each element in the row
        for (int col = 0; col < 6; ++col) {
            std::stringstream ss;
            ss << get_element(Row_[row], Col_[col]);
            std::string elementStr = ss.str();
            
            // Add color coding based on sign and pivot status without affecting width
            if (contains(pivotRows, row) && contains(pivotCols, col)) {
                elementStr = MAGENTA + elementStr + RESET;
            } else if (elementStr[0] == '-') { // Negative elements
                elementStr = RED + elementStr + RESET;
            } else if (elementStr[0] != '0') { // Positive elements
                elementStr = GREEN + elementStr + RESET;
            }
            precomputed_output << boost::format("%-" + std::to_string(width) + "s") % elementStr;
        }

        // Print right border
        precomputed_output << rightBorder << "\n";
    }

    // Output everything after precomputing
    std::cout << precomputed_output.str();
}

void SO6::physical_print() const {
    int maxWidth = 0;

    // Find the maximum width of the elements
    for (int row = 0; row < 6 ; row++) {
        for (int col =0 ; col <6 ; col++) {
            std::stringstream ss;
            ss << arr[get_index(row,col)];
            maxWidth = std::max(maxWidth, static_cast<int>(ss.str().length()));
        }
    }

    const int width = maxWidth + 2; // Adjust the width by adding 2

    std::cout << "\n";
    for (int row= 0 ; row <6 ; row++) {
        std::string leftBorder = "Row: " + std::to_string(row) + " ";
        leftBorder += (row == 0) ? "⌈" : ((row == 5) ? "⌊" : "|");
        std::string rightBorder = (row == 0) ? "⌉" : ((row == 5) ? "⌋" : "|");

        std::cout << leftBorder << "\t";
        for (int col = 0; col < 6 ; col++) {
            std::cout << std::setw(width) << arr[get_index(row,col)];
        }
        std::cout << "\t" << rightBorder << "\n";
    }
    std::cout << "\n";
}

void SO6::print_sign_mask(uint16_t& mask) {
    for(int i=0; i<6; ++i) {
        uint8_t tmp = mask_of_column(i);
            
        if( tmp == utils::NEG) std::cout << "-";
        else if( tmp == utils::POS) std::cout << "+";
        else if( tmp == utils::DISAGREE) std::cout << "\u00BF";
        else if( tmp == utils::AGREE) std::cout << "?";
    }
}

inline uint8_t SO6::mask_of_column(const int& c) {
    return (col_mask >> (2*c)) & utils::DISAGREE;
}

inline uint16_t SO6::set_mask_sign(const int& index, const uint8_t& sign) {
    return (col_mask & ~(utils::DISAGREE << (2*index))) | (sign << (2*index));
}


void SO6::unpermuted_print(const std::bitset<6>& columns_to_print) const {
    int maxWidth = 0;

    // Find the maximum width of the elements in the specified columns
    for (int row : Row) {
        for (int col : Col) {
            if (columns_to_print.test(col)) {
                std::stringstream ss;
                ss << arr[get_index(row, col)];
                maxWidth = std::max(maxWidth, static_cast<int>(ss.str().length()));
            }
        }
    }

    const int width = maxWidth + 2; // Adjust the width by adding 2

    std::cout << "\n";
    for (int row : Row) {
        std::string leftBorder = (row == 0) ? "⌈" : ((row == 5) ? "⌊" : "|");
        std::string rightBorder = (row == 0) ? "⌉" : ((row == 5) ? "⌋" : "|");

        std::cout << leftBorder << "\t";
        for (int col = 0; col < 6; ++col) {
            if (columns_to_print.test(col)) {
                std::cout << std::setw(width) << arr[get_index(row, col)];
            }
        }
        std::cout << "\t" << rightBorder << "\n";
    }
    std::cout << "\n";
}

