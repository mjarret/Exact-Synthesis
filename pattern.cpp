#include <iostream>
#include <vector>
#include <algorithm>
#include <stdint.h>
#include "pattern.hpp"

pattern::pattern() {
}

pattern::pattern(const bool binary_rep[72]) {
    for (int col = 0; col < 6; col++)
    {
        for (int row = 0; row < 6; row++)
        {
            arr[col][row].first = binary_rep[2 * col + 12 * row]; 
            arr[col][row].second = binary_rep[2 * col + 12 * row + 1]; 
        }
    }
    lexicographic_order();
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
        lexicographic_order();
        return;
    }
    for (char digit : binary_string)
    {
        binary_rep[index] = (digit == '1'); // Convert char to boolean
        index+=iter;
    }
    *this = pattern(binary_rep);
    lexicographic_order();
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

const bool* pattern::to_binary() const{
    static bool binary_rep[72];
    for (int col = 0; col < 6; col++)
    {
        for (int row = 0; row < 6; row++)
        {
            binary_rep[2 * col + 12 * row] = arr[col][row].first; 
            binary_rep[2 * col + 12 * row + 1] = arr[col][row].second; 
        }
    }
    return binary_rep;
}

bool pattern::operator==(const pattern &other) const {
    for(int col = 0; col < 6; col++) {
        for(int row = 0; row < 6; row++) {
            if(arr[col][row] != other.arr[col][row]) return false;
        }
    }
    return true;
}

/**
 * Overloads the * operator with matrix multiplication for SO6 objects
 * @param other reference to pattern to be multiplied with (*this)
 * @return matrix multiplication of (*this) and other
 */
SO6 pattern::operator*(const SO6 &other) const
{
    SO6 prod;
    prod.hist = other.hist; 

    for (int col = 0; col < 6; col++)
    {
        for (int k = 0; k < 6; k++)
        {
            const Z2& left_element = other[col][k];            
            if (left_element.intPart == 0) continue;    
            
            Z2 smallerLDE = left_element;
            smallerLDE.exponent--; //decrease lde

            for (int row = 0; row < 6; row++)
            { 
                if(arr[k][row].first) prod[col][row] += left_element;
                if(arr[k][row].second) prod[col][row] += smallerLDE; 
            }
        }
    }
    return prod;
}

bool pattern::operator<(const pattern &other) const {
    for(int col = 0; col < 6; col++) {
        for(int row = 0; row < 6; row++) {
            if(arr[col][row] == other.arr[col][row]) {
                continue;
            }
            return arr[col][row] < other.arr[col][row];
        }
    }
    return false;
}

int8_t pattern::lex_order(const std::pair<bool,bool> first[6],const std::pair<bool,bool> second[6])
{
    for (int row = 0; row < 6; row++)
    {
        if (first[row] == second[row]) {
            continue;
        }
        if (first[row] > second[row])           // Reverse ordering is better for tests
            return -1;
        return 1;
    }
    return 0;
}

int8_t pattern::case_compare(const std::pair<bool,bool> first[6],const std::pair<bool,bool> second[6])
{
    for (int row = 0; row < 6; row++)
    {
        if (first[row].first == second[row].first) {
            continue;
        }
        if (first[row].first > second[row].first)           // Reverse ordering is better for tests
            return -1;
        return 1;
    }
    return 0;
}

bool pattern::lex_less(const std::pair<bool,bool> first[6],const std::pair<bool,bool> second[6])
{
    return (pattern::lex_order(first,second)<0);
}

bool pattern::case_less(const std::pair<bool,bool> first[6],const std::pair<bool,bool> second[6])
{
    return (pattern::case_compare(first,second)<0);
}

void pattern::lexicographic_order() {
    for (int i = 1; i < 6; i++)
    {
        for (int j = i; j > 0 && pattern::lex_less(arr[j],arr[j-1]); j--)  std::swap(arr[j], arr[j - 1]);
    }
}


void pattern::case_order() {
    for (int i = 1; i < 6; i++)
    {
        for (int j = i; j > 0 && pattern::case_less(arr[j],arr[j-1]); j--)  std::swap(arr[j], arr[j - 1]);
    }
}

/// @brief 
/// @return 
pattern pattern::pattern_mod() {
    pattern ret = *this;
    for (int col = 0; col < 6; col++) {
        for(int row = 0; row < 6; row++) {
            if(ret.arr[col][row].first == 0) continue;
            ret.arr[col][row].second = !ret.arr[col][row].second; 
        }
    }
    ret.lexicographic_order();
    return ret;
}

void pattern::mod_row(const int &row) {
    for (int col = 0; col < 6; col++) {
        if(this->arr[col][row].first == 0) continue;
        this->arr[col][row].second = !this->arr[col][row].second; 
    }
}


pattern pattern::transpose() {
    pattern ret = *this;
    for (int col = 0; col < 6; col++) {
        for(int row = col+1; row < 6; row++) {
            std::swap(ret.arr[col][row],ret.arr[row][col]);
        }
    }
    ret.lexicographic_order();
    return ret;
}

// void pattern::row_sort_by_column(const int & col) {

// }

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
            os << m.arr[col][row].first << ',' << m.arr[col][row].second << ' ';
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
            os += arr[col][row].first ? "\xCE\x94 " : "  " ;
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

std::string pattern::name()
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
const int pattern::case_number() {
    this->lexicographic_order();
    // Case 1,2,5,7 all have at most 2 entries per row
    // Thus, after lexicographic ordering of the columns
    // these two entries will always be in columns 0 or 1
    if(!arr[2][0].first) {
        // This is now case 1,2,5,7
        // Case 1,2 both have 4 empty columns.
        // Thus, after lex ordering, columns 2-5 of case 5,7 will be empty
        if(arr[2][2].first) {
            // This is now case 5,7
            // Case 5 has 2 empty columns. 
            // Thus, after lex ordering, column 4,5 will be empty
            return arr[4][4].first ? 7 : 5;
        }
        // This is now case 1,2
        // Case 1 has 4 empty rows and case 2 has 2 empty rows
        // Thus, we can't return this as easily without some more work
        int row_sum = 0;
        for(int row = 0; row < 5; row ++) row_sum+= arr[0][row].first;
        return row_sum > 2 ? 2 : 1;
    }
    // This is now case 3,4,6,8
    // Case 3,4 has two empty columns
    if(arr[4][2].first) {
        // This is now case 6,8. Case 6 has two empty rows
        for(int row=0; row <5; row++) {
            if(!arr[0][row].first) {
                for(int col=0; col <5; col ++) {
                    if(arr[col][row].first) break;
                }
                return 6;
            }
            return 8;
        }
    }
    // This is now case 3,4. Case three has two rows with only two 1s. Column 2 will always distinguish these.
    int row_sum=0;
    for(int row = 0; row<5; row++) row_sum+=arr[2][row].first;
    return row_sum>2 ? 3 : 4;
}

// std::string pattern::human_readable() 
// {
//     std::string ret = "";
//     bool* tmp = to_binary();
//     for(int i=0; i<72; i+=2) {
//         if(i%12 == 0) ret += "[";
//         ret += std::to_string(tmp[i]) + " " + std::to_string(tmp[i+1]);
//         if((i+2)%12 == 0) ret += "]";
//         else ret += ",";
//     }
//     return ret;
// }

std::string pattern::human_readable() 
{
    std::string ret = "";
    const bool* tmp = to_binary();
    for(int row=0; row<6; row++) {
        ret+= "[";
        for(int col=0; col<6; col++) {
            ret += std::to_string(arr[col][row].first) + " " + std::to_string(arr[col][row].second);
            if(col<5) ret += ",";
        }
        ret+= "]";
    }
    return ret;
}

bool pattern::case_equals(const pattern & other) const {
    for(int col = 0; col<6; col++) {
        for(int row = 0; row <6; row++) {
            if(other.arr[col][row].first != arr[col][row].first) return false;
        }
    }
    return true;
}