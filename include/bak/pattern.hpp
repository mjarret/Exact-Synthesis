#ifndef PATTERN_HPP
#define PATTERN_HPP

#include <iostream>
#include <functional> // For std::hash
#include "SO6.hpp"
#include "uint72_t.hpp" // uint72_t for data
// #include "sort6.hpp"
#include "pattern.hpp"

class pattern{
    public:
        uint72_t pattern_data;
        std::vector<unsigned char> hist;

        pattern();
        pattern(const std::string &);
        pattern(const uint64_t, const uint8_t);
        void operator=(const pattern &);

        void transpose();
        void mod_row(const int);
        pattern pattern_mod();
        void canonical_form();

        // Output
        friend std::ostream& operator<<(std::ostream&, const pattern &);

        // Comparisons
        inline const std::strong_ordering operator<=>(const pattern &other) const {return pattern_data <=> other.pattern_data;};
        inline const bool operator==(const pattern &other) const {return pattern_data == other.pattern_data;}
        inline const bool operator!=(const pattern &other) const {return pattern_data != other.pattern_data;}
        inline const bool operator<(const pattern &other) const {return pattern_data < other.pattern_data;}
        inline const bool operator>(const pattern &other) const {return other.pattern_data < pattern_data;}
        inline const bool operator<=(const pattern &other) const {return !(other.pattern_data < pattern_data);}
        inline const bool operator>=(const pattern &other) const {return !(pattern_data < other.pattern_data);}

        // Constants
        static const pattern identity() {return pattern(0x4000040010001, 0x40);}

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
        template <typename T>
        void set(const int row, const int col, T value, const int bits_to_set = 2) {
            static_assert(std::is_integral<T>::value, "Value must be an integral type");
            int bit_pos = bit_position(row, col);
            pattern_data.set_bits(bit_pos, bit_pos + bits_to_set, value);
        }        

        void set_column(const int, const uint16_t);

        // Getters
        const uint8_t case_num() const;
        const uint16_t get_column(const int) const;
        
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
        const uint8_t get_element(const int row, const int col) const {
            return static_cast<uint8_t>((pattern_data >> bit_position(row, col)).low & 0b11);
        }

        static std::strong_ordering lex_order();

        inline void swap_elements(const int row_1, const int col_1, const int row_2, const int col_2) {
            const int bit_pos1 = bit_position(row_1, col_1);
            const int bit_pos2 = bit_position(row_2, col_2);
            auto swap_val = uint72_t(get_element(row_1, col_1)^get_element(row_2, col_2));
            pattern_data ^= (swap_val << bit_pos1) | (swap_val << bit_pos2);
        }

        inline void swap_columns(const int l, const int r) {
            auto col_xor = uint72_t(get_column(l)^get_column(r));
            pattern_data ^= (col_xor << bit_position(0,l)) | (col_xor << bit_position(0,r));
        }

        void sort_columns();   

        inline void swap_rows(const int row1_, const int row2_) {
            for(int col =0; col < 6; col++)  swap_elements(row1_, col, row2_, col);
        }

    private:
        mutable uint8_t case_num_memo = 0xFF;
        int8_t Col[6] = {0,1,2,3,4,5};
        int8_t Row[6] = {0,1,2,3,4,5};

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
        constexpr static const int bit_position(const int row, const int col) {
            return ((col << 3) + (col << 2) + (row << 1));
        }

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
        
        static const uint72_t col_mask(const int col) {
            switch (col) {
                case 0: return col_0;
                case 1: return col_1;
                case 2: return col_2;
                case 3: return col_3;
                case 4: return col_4;
                case 5: return col_5;
                default: return 0;
            }
        }

        const uint72_t get_row_intPart(const int) const;
        const uint72_t get_col_intPart(const int) const;
        const uint8_t column_weight(const int) const;
        const uint8_t row_weight(const int) const;
};

namespace std {
    template <>
    struct hash<pattern> {
        int operator()(const pattern& p) const {
            return std::hash<uint64_t>()(p.pattern_data.low);
        }
    };
}

#endif