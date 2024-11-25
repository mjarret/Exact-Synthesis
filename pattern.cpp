#include "pattern.hpp"

/**
 * @brief Default constructor for the pattern class.
 * 
 * This constructor initializes a new instance of the pattern class.
 */
pattern::pattern() {
    pattern_data = uint72_t(0,0);
}

/**
 * @brief Constructs a pattern object with the given low and high bits.
 * 
 * @param low_bits_ The lower 64 bits of the pattern.
 * @param high_bits_ The upper 8 bits of the pattern.
 */
pattern::pattern(const uint64_t low_bits_, const uint8_t high_bits_) {
    pattern_data = uint72_t(low_bits_, high_bits_);
}

/**
 * @brief Constructs a pattern object from a binary string.
 * 
 * This constructor initializes the pattern object by converting the given 
 * binary string into a uint72_t type and storing it in the pattern_data member.
 * 
 * @param binary_string A string representing the pattern.
 */
pattern::pattern(const std::string& binary_string) {
    pattern_data = uint72_t(binary_string);
}

/**
 * @brief Assignment operator for the pattern class.
 *
 * This operator assigns the values from another pattern object to this object.
 *
 * @param other The pattern object to be copied.
 */
void pattern::operator=(const pattern& other) {
    pattern_data = other.pattern_data;
    case_num_memo = other.case_num_memo;
}

/**
 * @brief Sets the bit at the specified position to the given value.
 * 
 * This function modifies the bit at the specified position in the pattern_data
 * to the provided boolean value. It first clears the bit at the given position
 * and then sets it to the new value.
 * 
 * @param bit_pos The position of the bit to be set.
 * @param value The boolean value to set the bit to (true for 1, false for 0).
 */
void pattern::set(const int bit_pos, const bool& value) {
    uint72_t clear = ~uint72_t(1ULL << bit_pos, 0);
    pattern_data = (pattern_data & clear) | (uint72_t(value << bit_pos, 0));
}

void pattern::set(const int row, const int col, uint8_t value) {
    int bit_pos = bit_position(row, col);
    pattern_data = pattern_data.set_pair(bit_pos, value & 0b11);
}

/**
 * @brief Sets the value at the specified row and column in the pattern.
 *
 * This function sets a pair of boolean values at the specified row and column
 * in the pattern. The pair is converted to an integer representation and stored
 * in the pattern data.
 *
 * @param row The row index where the value should be set.
 * @param col The column index where the value should be set.
 * @param value A pair of boolean values to be set at the specified position.
 */
void pattern::set(const int row, const int col, std::pair<bool, bool> value) {
    int bit_pos = bit_position(row, col);
    pattern_data = pattern_data.set_pair(bit_pos, value.first << 1 | value.second);
}

/**
 * @brief Retrieves the value from the pattern data at the specified row and column.
 * 
 * This function accesses the pattern data using the provided row and column indices,
 * converts the retrieved pair to an 8-bit unsigned integer, and returns it.
 * 
 * @param row The row index in the pattern data.
 * @param col The column index in the pattern data.
 * @return std::uint8_t The value at the specified row and column in the pattern data.
 */
std::uint8_t pattern::get_val(const int row, const int col) const {
    return static_cast<uint8_t>(pattern_data.get_pair(bit_position(row, col)));
}

/**
 * @brief Retrieves a pair of boolean values from the pattern data at the specified row and column.
 *
 * This function accesses the pattern data and extracts a pair of boolean values
 * from the specified row and column. The pair is represented as an 8-bit integer,
 * where the first boolean value is the most significant bit and the second boolean
 * value is the least significant bit.
 *
 * @param row The row index from which to retrieve the pair.
 * @param col The column index from which to retrieve the pair.
 * @return A std::pair containing two boolean values. The first element of the pair
 *         represents the most significant bit, and the second element represents
 *         the least significant bit.
 */
std::pair<bool, bool> pattern::get(const int row, const int col) const {
    uint8_t pair = pattern_data.get_pair(bit_position(row, col));
    return {pair >> 1, pair & 1};
}

template<const int row>
constexpr const uint72_t pattern::row_mask() const {
    static_assert(row >= 0 && row < 6, "Row index must be between 0 and 5");
    return row_mask_memoized<row>; // Return the precomputed result
}

const uint72_t pattern::get_masked_row(const int row) const {
    switch (row) {
        case 0: return (pattern_data & row_mask<0>());
        case 1: return (pattern_data & row_mask<1>());
        case 2: return (pattern_data & row_mask<2>());
        case 3: return (pattern_data & row_mask<3>());
        case 4: return (pattern_data & row_mask<4>());
        case 5: return (pattern_data & row_mask<5>());
        default: return 0;
    }
}

const uint16_t pattern::get_column(const int col) const {
    return (pattern_data.get_bits(bit_position(0, col)) & 0x0FFF); // Assume contiguous in columns
}


/**
 * @brief Computes the case number for the current pattern.
 *
 * This function calculates a case number based on the Hamming weight of the pattern data.
 * The case number is memoized to avoid redundant calculations.
 *
 * @return uint8_t The case number for the current pattern.
 *
 * The function uses the following logic to determine the case number:
 * - If the memoized case number is not 0xFF, it returns the memoized value.
 * - Otherwise, it calculates the Hamming weight of the pattern data masked with a specific mask.
 * - Based on the Hamming weight, it determines the case number using a series of conditions:
 *   - If the Hamming weight is 4, the case number is 1.
 *   - If the Hamming weight is 24, the case number is 8.
 *   - If the Hamming weight is 16, it checks the number of 1s in each column and row:
 *     - If any column or row has exactly 2 ones, the case number is 6.
 *     - Otherwise, the case number is 3.
 *   - If the Hamming weight is 12, it checks the number of 1s in each column:
 *     - If any column has 4 or 0 ones, the case number is 4.
 *     - Otherwise, the case number is 7.
 *   - If the Hamming weight is 8, it checks the number of 1s in each column and row:
 *     - If any column or row has 4 ones, the case number is 2.
 *     - If more than 2 columns or rows have 0 ones, the case number is 2.
 *     - Otherwise, the case number is 5.
 *   - For any other Hamming weight, the case number is 0.
 */
const uint8_t pattern::case_num() const {
    if(case_num_memo != 0xFF) return case_num_memo;
    constexpr uint72_t mask = uint72_t(0xAAAAAAAAAAAAAAAAULL, 0xAA);
    const int hamming_weight = (pattern_data & mask).popcount();

    // Helper lambda: Count 1s in a column with a specific mask
    auto count_column_ones = [&](int col) {
        return __builtin_popcount(get_column(col) & 0xAAAA);
    };

    // Helper lambda: Count total 1s in a row
    auto count_row_ones = [&](const int row) {
        return get_masked_row(row).popcount();
    };

    switch (hamming_weight) {
        case 4: return (case_num_memo = 1);
        case 24: return (case_num_memo = 8);
        case 16: {
            for(int col = 0; col < 3; col++) if (count_column_ones(col) == 2) return (case_num_memo = 6);
            for(int row = 0; row < 3; row++) if (count_row_ones(row) == 2) return (case_num_memo = 6);
            return (case_num_memo = 3);
        }
        case 12: {
            for(int col = 0; col < 3; col++) {
                int num_ones = count_column_ones(col);
                if(num_ones == 4 || num_ones == 0) return (case_num_memo = 4);
            }
            return (case_num_memo = 7);
        }
        case 8: {
            int zero_cols = 0;
            for(int col = 0; col < 4; col++) {          
                int num_ones = count_column_ones(col);
                if(num_ones == 4) return (case_num_memo = 2);
                else if(num_ones == 0) zero_cols++;
                if(zero_cols > 2) return (case_num_memo = 2);
            }
            
            int zero_rows = 0;
            for(int row = 0; row < 4; row++) {
                int num_ones = count_row_ones(row);
                if(num_ones == 4) return (case_num_memo = 2);
                else if(num_ones == 0) zero_rows++;
                if(zero_rows > 2) return (case_num_memo = 2);
            }

            return (case_num_memo = 5);
        }
        default: return (case_num_memo = 0);  // This will hit when we hit the identity case
    }
}

std::string pattern::generateBinaryString(const std::string& text) {
    std::string binaryString;
    for (char c : text) {
        if (c == '0' || c == '1') {
            binaryString += c;
        }
    }
    return binaryString;
}

std::strong_ordering pattern::operator<=>(const pattern &other) const
{
    // First compare the case numbers
    std::strong_ordering result = case_num() <=> other.case_num();
    if(result != std::strong_ordering::equal) return result;

    // Need to further distinguish patterns based on sqrt(2) part here
    return result;
}

bool pattern::operator==(const pattern &other) const {
    return (*this <=> other) == std::strong_ordering::equal;
}

bool pattern::operator<(const pattern &other) const {
    return (*this <=> other) == std::strong_ordering::less;
}

std::strong_ordering pattern::lex_order(const std::pair<bool,bool> first[6],const std::pair<bool,bool> second[6])
{
    std::strong_ordering result = std::strong_ordering::equal;
    for (int row = 0; row < 6; row++)
    {
        result = first[row] <=> second[row];
        if (result == std::strong_ordering::equal) continue;
        return result;
    }
    return std::strong_ordering::equal;
}

std::strong_ordering pattern::case_compare(const std::pair<bool,bool> first[6],const std::pair<bool,bool> second[6])
{
    std::strong_ordering result = lex_order(first,second);
    if(result != std::strong_ordering::equal) {
        return (result == std::strong_ordering::less) ? std::strong_ordering::greater : std::strong_ordering::less;
    }
    return std::strong_ordering::equal;
}

bool pattern::lex_less(const std::pair<bool,bool> first[6],const std::pair<bool,bool> second[6])
{
    return (pattern::lex_order(first,second)==std::strong_ordering::less);
}

bool pattern::case_less(const std::pair<bool,bool> first[6],const std::pair<bool,bool> second[6])
{
    return (pattern::case_compare(first,second)==std::strong_ordering::less);
}

/// @brief 
/// @return 
pattern pattern::pattern_mod() {
    pattern ret = *this;
    for (int col = 0; col < 6; col++) {
        for(int row = 0; row < 6; row++) {
            uint8_t value = ret.get_val(row,col);
            if(value < 2) continue;
            ret.set(row, col, value^1);
        }
    }
    return ret;
}

void pattern::mod_row(const int &r) {
    int row = r;
    for (int col = 0; col < 6; col++) {
        auto value = get_val(row,col);
        if(value < 2) continue;
        value ^=1;
        set(row,col,value);
    }
}

/**
 * Overloads << function for SO6.
 * @param os reference to ostream object needed to implement <<
 * @param m reference to SO6 object to be displayed
 * @returns reference ostream with the matrix's display form appended
 */
std::ostream &operator<<(std::ostream &os, const pattern &m)
{
    os << "\n";
    for (int row = 0; row < 6; row++)
    {
        if (row == 0)
            os << "⌈ ";
        else if (row == 5)
            os << "⌊ ";
        else
            os << "| ";
        for (int col = 0; col < 6; col++)
            os << m.get(row,col).first << ',' << m.get(row,col).second << ' ';
        if (row == 0)
            os << "⌉\n";
        else if (row == 5)
        {
            os << "⌋\n";
        }
        else
        {
            os << "|\n";
        }
    }
    os << "\n";
    return os;
}

/**
 * Overloads << function for SO6.
 * @param os reference to ostream object needed to implement <<
 * @param m reference to SO6 object to be displayed
 * @returns reference ostream with the matrix's display form appended
 */
std::string pattern::case_string()
{
    std::string os = "\n";
    for (int row = 0; row < 6; row++)
    {
        if (row == 0)
            os += "⌈ ";
        else if (row == 5)
            os += "⌊ ";
        else
            os += "| ";
        for (int col = 0; col < 6; col++)
            os += get(row,col).first ? "\xCE\x94 " : "  " ;
        if (row == 0)
            os += "⌉\n";
        else if (row == 5)
        {
            os += "⌋\n";
        }
        else
        {
            os += "|\n";
        }
    }
    os += "\n";
    return os;
}

std::string pattern::name() const
{
    std::string ret = "";
    for (char i : hist)
    {
        ret.append(1,i);
    }
    return ret;
}

std::string pattern::human_readable() 
{
    std::string ret = "";
    for(int row=0; row<6; row++) {
        ret+= "[";
        for(int col=0; col<6; col++) {
            ret += std::to_string(get(row,col).first) + " " + std::to_string(get(row,col).second);
            if(col<5) ret += ",";
        }
        ret+= "]";
    }
    return ret;
}

bool pattern::case_equals(const pattern & other) const {
    for(int col = 0; col<6; col++) {
        for(int row = 0; row <6; row++) {
            if(other.get(row,col).first != get(row,col).first) return false;
        }
    }
    return true;
}