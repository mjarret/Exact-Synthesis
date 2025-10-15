#ifndef Z2_Array_HPP
#define Z2_Array_HPP

#include <iostream>
#include <array>
#include <cstdint>
#include <smmintrin.h>
#include "Z2.hpp"

struct Z2_Array {
    union {
        struct __attribute__((packed)) Packed192 {
            uint32_t el0 : 32;
            uint32_t el1 : 32;
            uint32_t el2 : 32;
            uint32_t el3 : 32;
            uint32_t el4 : 32;
            uint32_t el5 : 32;
        } packed;
        uint32_t arr[6];
    };

    // Constructor
    Z2_Array() : arr{0, 0, 0, 0, 0, 0} {}

    // Access elements
    uint32_t& operator[](size_t index) {
        return arr[index];
    }

    const uint32_t& operator[](size_t index) const {
        return arr[index];
    }

    // Size of the array
    constexpr size_t size() const {
        return 6;
    }

    // Equality operators
    bool operator==(const Z2_Array& other) const {
        for (size_t i = 0; i < size(); ++i) {
            if (arr[i] != other.arr[i]) {
                return false;
            }
        }
        return true;
    }

    bool operator!=(const Z2_Array& other) const {
        return !(*this == other);
    }

    // Stream output
    friend std::ostream& operator<<(std::ostream& os, const Z2_Array& array) {
        os << "[";
        for (size_t i = 0; i < array.size(); ++i) {
            os << array.arr[i];
            if (i < array.size() - 1) {
                os << ", ";
            }
        }
        os << "]";
        return os;
    }

    // Hash function
    friend struct std::hash<Z2_Array>;
};

namespace std {
    template <>
    struct hash<Z2_Array> {
        size_t operator()(const Z2_Array& array) const {
            size_t hash_value = 0;
            for (size_t i = 0; i < array.size(); ++i) {
                hash_value ^= std::hash<uint32_t>()(array.arr[i]) + 0x9e3779b9 + (hash_value << 6) + (hash_value >> 2);
            }
            return hash_value;
        }
    };
}

#endif // Z2_Array_HPP