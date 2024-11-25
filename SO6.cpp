#include <iomanip>  // For std::setw
#include <sstream>
#include <boost/format.hpp>
#include "SO6.hpp"
#include "pattern.hpp"
#include "Globals.hpp"
#include "utils.hpp"

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

SO6::SO6(pattern &other)
{
    for (int col = 0; col < 6; col++) {
        for (int row = 0; row < 6; row++) {
            if (other.get(row,col).first == 0 && other.get(row,col).second == 0) {
                continue;  // Skip this iteration if both `first` and `second` are zero
            }
            bool second_arg = other.get(row,col).first == 0 ? other.get(row,col).first : other.get(row,col).second;
            arr[(col << 2) + (col << 1) + row] = Z2(other.get(row,col).first || other.get(row,col).second, second_arg, other.get(row,col).first);
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

SO6 SO6::left_multiply_by_T(const int i) const
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


SO6 SO6::reconstruct(const std::string& name) {
    SO6 ret = SO6::identity();
    for(unsigned char i : name) {
        ret = ret.left_multiply_by_T((i & 15) -1);
        if(i>15) ret = ret.left_multiply_by_T((i>>4)-1);
    }
    ret.canonical_form();
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

const z2_int SO6::getLDE() const {
    return std::max_element(arr, arr + 36, [](const Z2& a, const Z2& b) {
        return a.exponent < b.exponent;
    })->exponent;
}

pattern SO6::to_pattern() const
{
    pattern ret = pattern();
    ret.hist.reserve(hist.size());
    ret.hist = hist;

    const int8_t lde = getLDE();
    for(int col = 0; col < 6; ++col) for(int row = 0; row < 6; ++row)
    {
        const auto z = get_element(row,col);
        if (z.exponent < lde - 1 || z.intPart == 0) continue;
        if (z.exponent == lde) { 
            ret.set(row, col, {1, z.sqrt2Part & 1});
            continue;
        }
        ret.set(row,col,{0,1});
    }
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

