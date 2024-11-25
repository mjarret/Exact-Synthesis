#ifndef PATTERN_HPP
#define PATTERN_HPP

#include <iostream>
#include <functional> // For std::hash
#include "SO6.hpp"
#include "uint72_t.hpp" // uint72_t for data

class SO6;

class pattern{
    public:
        pattern();
        pattern(const std::string &);
        pattern(const uint64_t, const uint8_t);

        void mod_row(const int &);
        pattern pattern_mod();
        
        friend std::ostream& operator<<(std::ostream&, const pattern &);

        std::vector<unsigned char> hist;
        std::string id;
        std::string name() const; 
        std::string case_string();
        std::string human_readable();
        std::string generateBinaryString(const std::string&);

        // Comparisons
        void operator=(const pattern &);
        bool operator==(const pattern &) const;
        bool operator<(const pattern &) const;
        std::strong_ordering operator<=>(const pattern &) const;
        static std::strong_ordering lex_order(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        static std::strong_ordering case_compare(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        static bool lex_less(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        static bool case_less(const std::pair<bool,bool>[6],const std::pair<bool,bool>[6]);
        bool case_equals(const pattern &) const;

        static const pattern identity() {return pattern(0x4000040010001, 0x40);}

        // Getter: Retrieve std::pair<bool, bool> at (row, col)
        void set(const int bit_position, const bool& value);
        void set(const int row, const int col, const uint8_t value);
        void set(const int row, const int col, const std::pair<bool, bool> value);
        std::pair<bool, bool> get(const int row, const int col) const;
        std::uint8_t get_val(const int row, const int col) const;

        const uint8_t case_num() const;
        const uint16_t get_column(const int) const;
        const uint72_t get_masked_row(const int) const;
        
        template<const int>
        constexpr const uint72_t row_mask() const;

        uint72_t pattern_data;
        
        template<const int row>
        constexpr static uint72_t row_mask_memoized = []() constexpr {
            static_assert(row >= 0 && row < 6, "Row index must be between 0 and 5");

            uint72_t mask;
            for (int col = 0; col < 6; ++col) {
                int pos = (col << 3) + (col << 2) + (row << 1); // Compute bit position
                mask.set_pair(pos, 0b10);                      // Set bits in the pair
            }
            return mask;
        }();

    private:
        mutable uint8_t case_num_memo = 0xFF;
        const uint64_t first_mask = 0xAAAAAAAAAAAAAAAAULL;
        std::map<bool,int> row_frequency;
        std::map<bool,int> col_frequency;

        // Helper function to compute the bit position
        constexpr inline int bit_position(const int row, const int col) const { return ((col <<3) + (col <<2)  + (row <<1 )); }
};

namespace std {
    template <>
    struct hash<pattern> {
        int operator()(const pattern& p) const {
            return std::hash<uint64_t>()(p.pattern_data.low_bits);
        }
    };
}

struct PatternHash {
    std::size_t operator()(const pattern& p) const {
        return hash(p);
    }

    std::size_t hash(const pattern& p) const {
        return std::hash<uint64_t>()(p.pattern_data.low_bits);
    }

    bool equal(const pattern& a, const pattern& b) const {
        return a == b;  // Relies on `operator==` in `pattern`
    }
};

#endif