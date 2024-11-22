#include <iostream>
#include <vector>
#include <array>
#include <algorithm>
#include <stdint.h>
#include <cstdint>
#include <functional>
#include <compare>
#include "pattern.hpp"

pattern::pattern() {
}

pattern::pattern(const bool binary_rep[72]) {
    for (int col = 0; col < 6; col++) for (int row = 0; row < 6; row++) {
        // Extract the first and second bits for the current row and column
        bool first = binary_rep[2 * col + 12 * row];
        bool second = binary_rep[2 * col + 12 * row + 1];

        // Use the set method to store the values
        set(row, col, {first, second});
    }
}

pattern::pattern(const uint64_t first_rows, const uint8_t final_row) {
    this->first_rows = first_rows;
    this->final_row = final_row;
}


pattern::pattern(const std::string &binary_string) {
    bool binary_rep[72] = {0}; // Array representing binary pattern
    int index = 0; // Index for binaryArray
    float iter = 72/binary_string.length();
    if(iter != 2 && iter != 1) {
        std::string copyOfBS = binary_string;
        copyOfBS = generateBinaryString(binary_string);
        if (copyOfBS.length() != 72 && copyOfBS.length()!= 36) {
            std::cout << "Cannot parse pattern. Exiting.";
            std::exit(0);
        }
        *this = pattern(copyOfBS);
        return;
    }
    for (char digit : binary_string)
    {
        binary_rep[index] = (digit == '1'); // Convert char to boolean
        index+=iter;
    }
    *this = pattern(binary_rep);
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
    std::strong_ordering result = first_rows <=> other.first_rows;
    if(result != std::strong_ordering::equal) return result;
    else return final_row <=> other.final_row;
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
            auto value = ret.get(row,col);
            if(value.first == 0) continue;
            ret.set(row,col,{value.first,!value.second});
        }
    }
    return ret;
}

void pattern::mod_row(const int &row) {
    for (int col = 0; col < 6; col++) {
        auto value = get(row,col);
        if(value.first == 0) continue;
        set(row,col,{value.first,!value.second});
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

// If needed, we can make this routine a bit faster by eliminating lexicographic ordering
// since we can just count 1s to distinguish most of these
const uint8_t pattern::case_number() {

    // This is now case 3,4. Case three has two rows with only two 1s. Column 2 will always distinguish these.
    int row_sum=0;
    for(int row = 0; row<5; row++) row_sum+=get(row,2).first;
    return row_sum>2 ? 3 : 4;
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