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

// Alias the values of std::strong_ordering for cleaner code
constexpr auto Equal = std::strong_ordering::equal;
constexpr auto Less = std::strong_ordering::less;
constexpr auto Greater = std::strong_ordering::greater;
constexpr auto Equivalent = std::strong_ordering::equivalent;

/**
 * Method to compare two Z2 arrays of length 6 lexicographically
 * We are given the guarantee that the first nonzero element in the row is non-negative
 * @param first array of Z2 of length 6
 * @param second array of Z2 of length 6
 * @return -1 if first < second, 0 if equal, 1 if first > second
 */
// std::strong_ordering SO6::lex_order(const Z2 first[6], const Z2 second[6]) const
// {
//     return utils::lex_order(first,first+6, second,second+6);
// }

// Canonical form function (recursive DP approach)
/**
 * @brief Computes the canonical form of a given SO6 matrix.
 *
 * This function recursively computes the canonical form of an SO6 matrix by 
 * iterating over all possible columns and assuming the rows are fixed. It uses 
 * memoization to store intermediate results and avoid redundant calculations.
 *
 * @param A The SO6 matrix for which the canonical form is to be computed.
 * @param dp A 2D vector used for memoization to store intermediate results.
 * @param size The current size of the matrix being processed.
 * @param diag_pos_enforced A boolean flag to enforce positive diagonal elements.
 * @return The maximum value of the canonical form for the given matrix.
 */

void SO6::canonical_form_test() {
    std::bitset<6> columns("111111");
    std::unordered_map<std::bitset<6>, int> dp = {};
    for (int i = 0; i < (1 << 6); ++i) {
       dp[std::bitset<6>(i)] = -1;
    }
    for (int i = 0; i < 6; ++i) {
        dp[std::bitset<6>(1 << i)] = i;
    }

    auto x = this->get_pivot_column(columns, dp);
    std::vector<int> sequence;
    while (columns.any()) {
        int memo_value = dp[columns];
        sequence.push_back(memo_value);
        columns.flip(memo_value);
    }
    
    std::cout << "Canonical form: " << std::endl;
    for (int i = 0; i < 6; ++i) {
        std::cout << sequence[i] << " ";
    }
    unpermuted_print();
}

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

void SO6::canonical_form() {
    row_sort();
    ecs = get_equivalence_classes();
    
    do {
        for(int curr_sc = 0; curr_sc < 32; curr_sc++) {
            // unpermuted_print();
            // Populate the row permutation
            int k = 0;
            uint8_t row_perm[6];
            for (const auto& vec : ecs) {
                for (int elem : vec) row_perm[k++] = elem;
            }

            // Sort column permutations based on lexicographical order
            uint8_t col_perm[6] = {0,1,2,3,4,5};
            uint8_t col_inverse[6];
            for (int i = 0; i < 6; ++i) {
                col_inverse[Col[i]] = i;
            }

            // std::copy(Col, Col+6, col_perm);
            std::sort(col_perm, col_perm+6, [this,row_perm,curr_sc,col_inverse](int i, int j) {
                auto left = get_column(col_inverse[i], row_perm); // This is absolute column i
                auto right = get_column(col_inverse[j], row_perm); // This is absolute column j
                std::strong_ordering comparison = utils::lex_order(left, right, curr_sc, curr_sc);
                return (Less == comparison); // Always returns Less for no good reason
            });        

            if (is_better_permutation(row_perm, col_perm, curr_sc)) {
                for (int i = 0; i < 6; ++i) {
                    this->Row[i] = row_perm[i];
                }
                for (int i = 0; i < 6; ++i) {
                    this->Col[i] = col_perm[i];
                }
                // std::copy(col_perm, col_perm+6, Col);
                sign_convention = curr_sc;
            }
        } 
    }  while (SO6::get_next_equivalence_class(ecs));
}

bool SO6::is_better_permutation(const uint8_t* row_perm, const uint8_t* col_perm, const int &sign_perm) {
    SO6::Iterator current_permutation(*this, 0, Row);
    SO6::Iterator new_permuation(*this, 0, row_perm);
    for(int col = 0; col < 6; col++) {
        auto current = get_column(col, Row);
        auto new_col = get_column(col, row_perm);
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
 * @brief Sorts the rows based on their frequency counts.
 * 
 * This function initializes the Row array with indices from 0 to 5.
 * It then counts the frequency of each row using a map and sorts the
 * Row array based on these frequency counts in ascending order.
 */
void SO6::row_sort() {    
    std::sort(Row, Row+6, [this](int a, int b) {
        return row_frequency[a] < row_frequency[b];
    });
}


std::vector<std::vector<int>> SO6::get_equivalence_classes() const {
    std::vector<int> ec;
    std::vector<std::vector<int>> ecs;
    for (int i = 0; i < 6; i++) {
        ec.push_back(Row[i]);
        if (i == 5 || row_frequency[Row[i]] != row_frequency[Row[i+1]]) {
            ecs.push_back(ec);
            ec.clear();
        }
    }
    return ecs;
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
bool SO6::get_next_equivalence_class(std::vector<std::vector<int>>& row_equivalence_classes) {
    bool more_permutations = false;
    for (auto& ec : row_equivalence_classes) {
        if (std::next_permutation(ec.begin(), ec.end())) {
            more_permutations = true;
        } else {
            std::sort(ec.begin(), ec.end());
        }
    }
    
    return more_permutations;
}

// bool SO6::get_next_ec_permutation(std::vector<int>& ec) {
//     return std::next_permutation(ec.begin(), ec.end());
// }

std::string SO6::name()
{
    return std::string(hist.begin(),hist.end());
}

SO6 SO6::reconstruct() {
    return SO6::reconstruct(std::string(hist.begin(), hist.end()));
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
    // ret.unpermuted_print();
    // Iterate over each integer in the string
    while (iss >> number) {
        // Process each number, for example, print it
        ret = ret.left_multiply_by_T(number);
    }    
    // ret.unpermuted_print();
    return ret;
}

bool SO6::operator<(const SO6 &other) const
{   
    std::cout << "Comparing SO6 objects" << std::endl;
    unpermuted_print();
    other.unpermuted_print();
    std::cin.get();
    for (int col = 0; col < 5; ++col)
    { // There is no need to check the final column due to constraints
        std::cout << "Comparing column " << col << std::endl;
        std::strong_ordering result = utils::lex_order(get_column(col),other.get_column(col),sign_convention,other.sign_convention);
        if(result != Equal) return result == Less;
    }
    // std::cout << "SO6 objects are equal" << std::endl;
    return false;
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

// void SO6::unpermuted_print() const {
//     int maxWidth = 0;

//     // Find the maximum width of the elements
//     for (int row : Row) {
//         for (int col : Col) {
//             std::stringstream ss;
//             ss << arr[get_index(row,col)];
//             maxWidth = std::max(maxWidth, static_cast<int>(ss.str().length()));
//         }
//     }

//     const int width = maxWidth + 2; // Adjust the width by adding 2

//     // Print column headers
//     std::cout << "\n";
//     std::cout << std::setw(6) << "";  // Adjust spacing for row labels
//     for (int col = 0; col < 6; ++col) {
//         std::cout << std::setw(width + 3) << "Col " + std::to_string(Col[col]);
//     }
//     std::cout << "\n";

//     // Print the matrix rows and elements
//     for (int row = 0; row < 6; ++row) {
//         // Print row label with left border
//         std::cout << "Row " << std::setw(2) << (int) Row[row] << " ";

//         // Select border style for the row
//         std::string leftBorder = (row == 0) ? "⌈ " : ((row == 5) ? "⌊ " : "| ");
//         std::string rightBorder = (row == 0) ? " ⌉" : ((row == 5) ? " ⌋" : " |");

//         std::cout << leftBorder;

//         // Print matrix elements
//         for (int col = 0; col < 6; ++col) {
//             std::cout << std::setw(width) << arr[get_index(Row[row], Col[col])] << " ";
//         }

//         // Print right border
//         std::cout << rightBorder << "\n";
//     }
//     std::cout << "\n";
// }

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

