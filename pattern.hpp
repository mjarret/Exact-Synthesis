#ifndef PATTERN_HPP
#define PATTERN_HPP

#include <iostream>
#include <vector>
#include <utility> // For std::pair
#include <functional> // For std::hash
#include <cstddef> // for std::size_t
#include "SO6.hpp"


class SO6;

class pattern{
    public:
        pattern();
        pattern(const bool[72]);
        pattern(const std::string &);

        void lexicographic_order();
        void case_order();
        pattern pattern_mod();
        void mod_row(const int &);
        pattern transpose();
        // const bool* to_binary() const;
        const std::array<bool, 72> to_binary() const;

        bool operator==(const pattern &) const;

        SO6 operator*(const SO6 &) const;
        bool operator<(const pattern &) const;
        friend std::ostream& operator<<(std::ostream&, const pattern &);

        std::pair<bool,bool> arr[6][6];
        std::vector<unsigned char> hist;
        std::string name() const; 
        std::string human_readable();
        std::string generateBinaryString(const std::string&);

        static int8_t lex_order(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        static int8_t case_compare(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);

        static bool lex_less(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        static bool case_less(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        bool case_equals(const pattern &) const;

        const int case_number();

        std::string case_string();

        static const pattern identity() {
            pattern I;
            for(int k =0; k<6; k++) {
                I.arr[k][k].first = 1;
            }
            return I;
        }
        std::string id;

    private:
        uint64_t first_rows;
        uint8_t final_row;

        // Helper function to compute the bit position
        size_t bit_position(size_t row, size_t col) const { return (col * 6 + row)<<1; }

        // Getter: Retrieve std::pair<bool, bool> at (row, col)
        std::pair<bool, bool> get(size_t row, size_t col) const {
            size_t bit_pos = bit_position(row, col);
            if (bit_pos < 64) {
                // Bits are in first 5 rows
                bool first = (first_rows >> bit_pos) & 1;
                bool second = (first_rows >> (bit_pos + 1)) & 1;
                return {first, second};
            } else {
                // Bits are in final_row
                bit_pos >>= 6; // Adjust for final_row offset
                bool first = (final_row >> bit_pos) & 1;
                bool second = (final_row >> (bit_pos + 1)) & 1;
                return {first, second};
            }
        }

        // Setter for the `std::pair<bool, bool>` for matrix element (row, col)
        void set(size_t row, size_t col, std::pair<bool, bool> value) {
            // Update the first bit in first_rows
            if (value.first) {
                first_rows |= (1ULL << (row * 6 + col)); // Set the bit
            } else {
                first_rows &= ~(1ULL << (row * 6 + col)); // Clear the bit
            }

            // Update the second bit in final_row
            if (value.second) {
                final_row |= (1U << col); // Set the bit
            } else {
                final_row &= ~(1U << col); // Clear the bit
            }
        }
};

namespace std {
    template <>
    struct hash<pattern> {
        size_t operator()(const pattern& p) const {
            std::array<bool,72> binary = p.to_binary();
            uint64_t hash = 0;

            // Hash the first 64 bits
            for (int i = 0; i < 64; i++) {
                hash = (hash << 1) | binary[i];
            }

            return std::hash<uint64_t>()(hash);
        }
    };
}

struct PatternHash {
    std::size_t operator()(const pattern& p) const {
        return hash(p);
    }

    std::size_t hash(const pattern& p) const {
        std::array<bool,72> binary = p.to_binary();
        uint64_t hash = 0;

        // Hash the first 64 bits
        for (int i = 0; i < 64; i++) {
            hash = (hash << 1) | binary[i];
        }

        return std::hash<uint64_t>()(hash);
    }

    bool equal(const pattern& a, const pattern& b) const {
        return a == b;  // Relies on `operator==` in `pattern`
    }
};


#endif