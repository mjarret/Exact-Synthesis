#ifndef PATTERN_HPP
#define PATTERN_HPP

#include <iostream>
#include <functional> // For std::hash
#include "SO6.hpp"
#include "uint72_t.hpp" // uint72_t for data

class pattern{
    public:
        uint72_t pattern_data;
        std::vector<unsigned char> hist;

        pattern();
        pattern(const std::string &);
        pattern(const uint64_t, const uint8_t);

        void operator=(const pattern &);
        void mod_row(const int);
        pattern pattern_mod();
        
        // Output
        friend std::ostream& operator<<(std::ostream&, const pattern &);

        // Comparisons
        const std::strong_ordering operator<=>(const pattern &) const;
        const bool operator==(const pattern &other) const {return pattern_data == other.pattern_data;}
        const bool operator<(const pattern &other) const {return (*this <=> other) == std::strong_ordering::less;}
        const bool operator>(const pattern &other) const {return (*this <=> other) == std::strong_ordering::greater;}
        const bool operator<=(const pattern &other) const {return (*this <=> other) != std::strong_ordering::greater;}
        const bool operator>=(const pattern &other) const {return (*this <=> other) != std::strong_ordering::less;}

        // Constants
        static const pattern identity() {return pattern(0x4000040010001, 0x40);}

        // Setters
        void set(const int bit_position, const bool& value);
        void set(const int row, const int col, const uint8_t value);
        void set(const int row, const int col, const std::pair<bool, bool> value);

        // Getters
        const uint8_t case_num() const;
        const uint16_t get_column(const int) const;
        std::pair<bool, bool> get(const int row, const int col) const;
        std::uint8_t get_val(const int row, const int col) const;

    private:
        mutable uint8_t case_num_memo = 0xFF;

        // Various masks for rows/columns
        constexpr static uint72_t row_0 = uint72_t(0x3003003003003003,0x00);
        constexpr static uint72_t row_1 = uint72_t(0xc00c00c00c00c00c,0x00);
        constexpr static uint72_t row_2 = uint72_t(0x0030030030030030,0x03);
        constexpr static uint72_t row_3 = uint72_t(0x00c00c00c00c00c0,0x0c);
        constexpr static uint72_t row_4 = uint72_t(0x0300300300300300,0x30);
        constexpr static uint72_t row_5 = uint72_t(0x0c00c00c00c00c00,0xc0);

        constexpr static uint72_t col_0 = uint72_t(0x0000000000000fff,0x00);
        constexpr static uint72_t col_1 = uint72_t(0x0000000000fff000,0x00);
        constexpr static uint72_t col_2 = uint72_t(0x0000000fff000000,0x00);
        constexpr static uint72_t col_3 = uint72_t(0x0000fff000000000,0x00);
        constexpr static uint72_t col_4 = uint72_t(0x0fff000000000000,0x00);
        constexpr static uint72_t col_5 = uint72_t(0xf000000000000000,0xff);

        constexpr static uint72_t int_part     = uint72_t(0xAAAAAAAAAAAAAAAAULL, 0xAA);
        constexpr static uint72_t sqrt2_part   = uint72_t(0x5555555555555555ULL, 0x55);

        // Helper function to compute the bit position
        constexpr inline int bit_position(const int row, const int col) const { return ((col<<3) + (col<<2)  + (row<<1)); }

        template<const int row>
            constexpr static const uint72_t row_int_part() {
                switch (row) {
                    case 0: return (row_0 & int_part);
                    case 1: return (row_1 & int_part);
                    case 2: return (row_2 & int_part);
                    case 3: return (row_3 & int_part);
                    case 4: return (row_4 & int_part);
                    case 5: return (row_5 & int_part);
                    default: return 0;
                }
            }

        template<const int col>
            constexpr static const uint72_t col_int_part() {
                switch (col) {
                    case 0: return (col_0 & int_part);
                    case 1: return (col_1 & int_part);
                    case 2: return (col_2 & int_part);
                    case 3: return (col_3 & int_part);
                    case 4: return (col_4 & int_part);
                    case 5: return (col_5 & int_part);
                    default: return 0;
                }
                // static_assert(row >= 0 && row < 6, "Row index must be between 0 and 5");
                // return row_mask_memoized<row>; // Return the precomputed result
            }

        const uint72_t get_masked_row(const int) const;
        const uint72_t get_masked_col(const int) const;
        const uint8_t column_weight(const int) const;
};

namespace std {
    template <>
    struct hash<pattern> {
        int operator()(const pattern& p) const {
            return std::hash<uint64_t>()(p.pattern_data.low_bits);
        }
    };
}

#endif