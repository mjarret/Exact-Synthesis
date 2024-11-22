#ifndef PATTERN_HPP
#define PATTERN_HPP

#include <iostream>
#include <vector>
#include <utility> // For std::pair
#include <compare>
#include <functional> // For std::hash
#include <cstddef> // for std::size_t
#include "SO6.hpp"


class SO6;

class pattern{
    public:
        pattern();
        pattern(const bool[72]);
        pattern(const std::string &);
        pattern(const uint64_t, const uint8_t);

        void mod_row(const int &);
        pattern pattern_mod();
        
        
        friend std::ostream& operator<<(std::ostream&, const pattern &);

        const uint8_t case_number();
        std::vector<unsigned char> hist;
        std::string id;
        std::string name() const; 
        std::string case_string();
        std::string human_readable();
        std::string generateBinaryString(const std::string&);


        // Comparisons
        bool operator==(const pattern &) const;
        bool operator<(const pattern &) const;
        std::strong_ordering operator<=>(const pattern &) const;
        static std::strong_ordering lex_order(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        static std::strong_ordering case_compare(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        static bool lex_less(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        static bool case_less(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        bool case_equals(const pattern &) const;

        static const pattern identity() {return pattern(0x4000040010001, 0x40);}
        
        uint64_t first_rows;
        uint8_t final_row;

        // Getter: Retrieve std::pair<bool, bool> at (row, col)
        std::pair<bool, bool> get(size_t row, size_t col) const {
            size_t bit_pos = bit_position(row, col);
            if (bit_pos < 64) {
                // Bits are in first_rows
                return {
                    static_cast<bool>((first_rows >> bit_pos) & 1),
                    static_cast<bool>((first_rows >> (bit_pos + 1)) & 1)
                };
            } else {
                // Bits are in final_row; adjust position using %64 or &63
                size_t adjusted_pos = bit_pos & 63; // Faster than bit_pos - 64 or bit_pos % 64
                return {
                    static_cast<bool>((final_row >> adjusted_pos) & 1),
                    static_cast<bool>((final_row >> (adjusted_pos + 1)) & 1)
                };
            }
        }

        // Setter: Update std::pair<bool, bool> at (row, col)
        void set(size_t row, size_t col, std::pair<bool, bool> value) {
            size_t bit_pos = bit_position(row, col);
            if (bit_pos < 64) {
                // Update first_rows
                first_rows = (first_rows & ~(1ULL << bit_pos)) | (static_cast<uint64_t>(value.first) << bit_pos);
                first_rows = (first_rows & ~(1ULL << (bit_pos + 1))) | (static_cast<uint64_t>(value.second) << (bit_pos + 1));
            } else {
                // Use bit-shifting to constrain bit_pos to [0, 63]
                size_t adjusted_pos = bit_pos & 63;
                final_row = (final_row & ~(1U << adjusted_pos)) | (static_cast<uint8_t>(value.first) << adjusted_pos);
                final_row = (final_row & ~(1U << (adjusted_pos + 1))) | (static_cast<uint8_t>(value.second) << (adjusted_pos + 1));
            }
        }



    private:
        std::map<bool,int> row_frequency[6];
        std::map<bool,int> col_frequency[6];

        // Helper function to compute the bit position
        size_t bit_position(const size_t& row, const size_t& col) const { return (col * 6 + row) << 1 ; }
};

namespace std {
    template <>
    struct hash<pattern> {
        size_t operator()(const pattern& p) const {
            return std::hash<uint64_t>()(p.first_rows);
        }
    };
}

struct PatternHash {
    std::size_t operator()(const pattern& p) const {
        return hash(p);
    }

    std::size_t hash(const pattern& p) const {
        return std::hash<uint64_t>()(p.first_rows);
    }

    bool equal(const pattern& a, const pattern& b) const {
        return a == b;  // Relies on `operator==` in `pattern`
    }
};


#endif