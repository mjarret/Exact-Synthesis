#ifndef Z2_Array_HPP
#define Z2_Array_HPP

#include <iostream>
#include <array>
#include <cstdint>
#include <smmintrin.h>
#include "Z2.hpp"

struct Z2_Array {
    union {
        __uint128_t data; // 128-bit representation of the row
        struct{
            uint64_t low : 64; 
            uint64_t high : 64; // Upper 64 bits
        };
        struct{
            uint32_t el0 : 21;
            uint32_t el1 : 21;
            uint32_t el2 : 22;
            uint32_t el3 : 21;
            uint32_t el4 : 21;
            uint32_t el5 : 22;
        };
    };

    // Z2 operator[] (int index) const {
    //     switch(index) {
    //         case 0: return Z2(static_cast<sqrt2_int>(el0));
    //         case 1: return Z2(static_cast<sqrt2_int>(el1));
    //         case 2: return Z2(static_cast<sqrt2_int>(el2));
    //         case 3: return Z2(static_cast<sqrt2_int>(el3));
    //         case 4: return Z2(static_cast<sqrt2_int>(el4));
    //         case 5: return Z2(static_cast<sqrt2_int>(el5));
    //         default: throw std::out_of_range("Index out of range");
    //     }
    // }

    // Constructor
    Z2_Array(__uint128_t data_ = 0) : data(data_) {}

    // Setter for the 6 components
    void set_component(int index, const Z2& value) {
        switch(index) {
            case 0: el0 = value.data; return;
            case 1: el1 = value.data; return;
            case 2: el2 = value.data; return;
            case 3: el3 = value.data; return;
            case 4: el4 = value.data; return;
            case 5: el5 = value.data; return;
            default: throw std::out_of_range("Index out of range");
        }
    }

    // Z2_Array operator+=(const Z2_Array& other) {      
    //     set_component(0, (*this)[0] + other[0]);
    //     set_component(1, (*this)[1] + other[1]);
    //     set_component(2, (*this)[2] + other[2]);
    //     set_component(3, (*this)[3] + other[3]);
    //     set_component(4, (*this)[4] + other[4]);
    //     set_component(5, (*this)[5] + other[5]);
    //     return *this;
    // }

    // Z2_Array operator-() const {
    //     Z2_Array result;
    //     #pragma omp simd
    //     for(int i = 0; i < 6; i++) {
    //         result.set_component(i, -(*this)[i]);
    //     }
    //     return result;
    // }

    // Z2_Array operator+(const Z2_Array& other) const {
    //     Z2_Array result = *this;
    //     return result += other;
    // }

    // Z2_Array operator-=(const Z2_Array& other) {
    //     return *this += -other;
    // }

    // Z2_Array operator-(const Z2_Array& other) const {
    //     Z2_Array result = *this;
    //     return result -= other;
    // }

    // Z2_Array operator*=(const Z2_Array& other) {
    //     #pragma omp simd
    //     for(int i = 0; i < 6; i++) {
    //         set_component(i, (*this)[i] * other[i]);
    //     }
    //     return *this;
    // }

    // Z2_Array operator*(const Z2_Array& other) const {
    //     Z2_Array result;
    //     #pragma omp simd
    //     for(int i = 0; i < 6; i++) {
    //         result.set_component(i, (*this)[i] * other[i]);
    //     }
    //     return result;
    // }

    // Z2_Array& operator=(const Z2_Array& other) {
    //     data = other.data;
    //     return *this;
    // }

    // bool operator==(const Z2_Array& other) const {return data == other.data;}
    // const std::strong_ordering operator<=>(const Z2_Array& other) const {return data <=> other.data;}
    // friend std::ostream& operator<<(std::ostream& os, const Z2_Array& arr) {
    //     os << "[";
    //     for(int i = 0; i < 6; i++) {
    //         os << arr[i];
    //         if(i < 5) os << ", ";
    //     }
    //     os << "]";
    //     return os;
    // }
};

namespace std {
    template <>
    struct hash<Z2_Array> {
        size_t operator()(const Z2_Array& arr) const {
            return std::hash<uint16_t>()(arr.low) ^ (std::hash<uint64_t>()(arr.high) << 1);
        }
    };
}
#endif // Z2_Array_HPP