#include "uint72_t.hpp"

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
void pattern::set_column(const int col, uint16_t value) {
    int bit_pos = bit_position(0, col);
    pattern_data.set_bits(bit_pos, bit_pos + 12, value);
}

const uint72_t pattern::get_row_intPart(const int row) const {
    switch (row) {
        case 0: return (pattern_data & row_int_part<0>());
        case 1: return (pattern_data & row_int_part<1>());
        case 2: return (pattern_data & row_int_part<2>());
        case 3: return (pattern_data & row_int_part<3>());
        case 4: return (pattern_data & row_int_part<4>());
        case 5: return (pattern_data & row_int_part<5>());
        default: return 0;
    }
}

const uint72_t pattern::get_col_intPart(const int col) const {
    switch (col) {
        case 0: return (pattern_data & col_int_part<0>());
        case 1: return (pattern_data & col_int_part<1>());
        case 2: return (pattern_data & col_int_part<2>());
        case 3: return (pattern_data & col_int_part<3>());
        case 4: return (pattern_data & col_int_part<4>());
        case 5: return (pattern_data & col_int_part<5>());
        default: return 0;
    }
}

const uint8_t pattern::column_weight(const int col) const {
    return static_cast<uint8_t>(((pattern_data & int_part).popcount()<<3) + (pattern_data & sqrt2_part).popcount());
}

const uint8_t pattern::row_weight(const int col) const {
    return static_cast<uint8_t>(((pattern_data & int_part).popcount()<<3) + (pattern_data & sqrt2_part).popcount());
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

    const int hamming_weight = (pattern_data & int_part).popcount();

    switch (hamming_weight) {
        case 4: return (case_num_memo = 1);
        case 24: return (case_num_memo = 8);
        case 16: {
            for(int col = 0; col < 3; col++) if (get_col_intPart(col).popcount() == 2) return (case_num_memo = 6);
            for(int row = 0; row < 3; row++) if (get_row_intPart(row).popcount() == 2) return (case_num_memo = 6);
            return (case_num_memo = 3);
        }
        case 12: {
            for(int col = 0; col < 3; col++) {
                int num_ones = get_col_intPart(col).popcount();
                if(num_ones == 4 || num_ones == 0) return (case_num_memo = 4);
            }
            return (case_num_memo = 7);
        }
        case 8: {
            int zero_cols = 0;
            for(int col = 0; col < 4; col++) {          
                int num_ones = get_col_intPart(col).popcount();
                if(num_ones == 4) return (case_num_memo = 2);
                else if(num_ones == 0) zero_cols++;
                if(zero_cols > 2) return (case_num_memo = 2);
            }
            
            int zero_rows = 0;
            for(int row = 0; row < 4; row++) {
                int num_ones = get_row_intPart(row).popcount();
                if(num_ones == 4) return (case_num_memo = 2);
                else if(num_ones == 0) zero_rows++;
                if(zero_rows > 2) return (case_num_memo = 2);
            }

            return (case_num_memo = 5);
        }
        default: return (case_num_memo = 0);  // This will hit when we hit the identity case
    }
}

/// @brief 
/// @return 
pattern pattern::pattern_mod() {
    pattern ret = *this;
    for (int col = 0; col < 6; col++) {
        for(int row = 0; row < 6; row++) {
            uint8_t value = ret.get_element(row,col);
            if(value < 2) continue;
            ret.set(row, col, (value^1) & 0b11);
        }
    }
    return ret;
}

void pattern::mod_row(const int r) {
    int row = r;
    for (int col = 0; col < 6; col++) {
        auto value = get_element(row,col);
        if(value < 2) continue;
        value ^=1;
        set(row,col,value);
    }
}

const uint16_t pattern::get_column(const int col) const {
    return static_cast<uint16_t>((pattern_data >> bit_position(0,col)).low & 0xFFF);
}

// void pattern::sort_columns() {
//     sort6::network(*this);
// }

void pattern::canonical_form() {
    sort_columns();
    uint72_t smallest_pattern = pattern_data;  // Initialize with the current pattern

    // Define the lambda function
    auto perform_swap_and_compare = [&](int x, int y) {
        swap_rows(x,y);
        if(row_weight(x) < row_weight(y)) return;
        sort_columns();
        if (smallest_pattern < pattern_data) smallest_pattern = pattern_data;
    };

    perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 5); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 5); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 5); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(3, 5); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(4, 5); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 4); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(1, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(2, 3); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1); perform_swap_and_compare(0, 2); perform_swap_and_compare(0, 1);

    // Set the pattern to the smallest canonical form
    pattern_data = smallest_pattern;
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
        for (int col = 0; col < 6; col++) {
            uint8_t element = m.get_element(row,col);
            bool intPart = element >> 1;
            bool sqrt2Part = element & 1;
            os << intPart << ',' << sqrt2Part << ' ';
        }
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
};

void pattern::transpose() {
    #pragma unroll
    for(int row = 0; row < 6; row ++) {
        # pragma unroll
        for(int col = row+1; col < 6; col++) {
            swap_elements(row,col,col,row);
        }
    }
}