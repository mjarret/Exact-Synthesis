#ifndef PATTERN_HPP
#define PATTERN_HPP

#include <iostream>
#include <vector>
#include <utility> // For std::pair
#include <functional> // For std::hash
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
        const bool* to_binary() const;

        bool operator==(const pattern &) const;

        SO6 operator*(const SO6 &) const;
        bool operator<(const pattern &) const;
        friend std::ostream& operator<<(std::ostream&, const pattern &);

        std::pair<bool,bool> arr[6][6];
        std::vector<unsigned char> hist;
        std::string name(); 
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
};

namespace std {
    template <>
    struct hash<pattern> {
        size_t operator()(const pattern& p) const {
            // Assuming p.to_binary() returns an array of 72 booleans
            const bool* binaryArray = p.to_binary();
            
            // Calculate the hash value based on the binaryArray
            size_t hashValue = 0;
            for (int i = 0; i < 64; i++) {
                hashValue = (hashValue << 1) | binaryArray[i];
            }
            
            return hashValue;
        }
    };
}


#endif