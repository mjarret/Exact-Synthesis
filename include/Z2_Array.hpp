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

    // Method to get a Z2 element at a specific index
    Z2 get_element(int index) const {
        switch(index) {
            case 0: return Z2(el0);
            case 1: return Z2(el1);
            case 2: return Z2(el2);
            case 3: return Z2(el3);
            case 4: return Z2(el4);
            case 5: return Z2(el5);
            default: throw std::out_of_range("Index out of range");
        }
    }

    // Method to set a Z2 element at a specific index
    void set_element(int index, const Z2& value) {
        switch(index) {
            case 0: el0 = value.data; break;
            case 1: el1 = value.data; break;
            case 2: el2 = value.data; break;
            case 3: el3 = value.data; break;
            case 4: el4 = value.data; break;
            case 5: el5 = value.data; break;
            default: throw std::out_of_range("Index out of range");
        }
    }

    // Method to access the array data as a reinterpreted Z2 array
    Z2* as_Z2_array() {
        return reinterpret_cast<Z2*>(&data);
    }

    const Z2* as_Z2_array() const {
        return reinterpret_cast<const Z2*>(&data);
    }

    // Addition-assignment operator
    Z2_Array& operator+=(const Z2_Array& other) {
        auto* this_data = as_Z2_array();
        auto* other_data = other.as_Z2_array();
        for (int i = 0; i < 6; ++i) {
            this_data[i] += other_data[i];
        }
        return *this;
    }

    // Subtraction-assignment operator
    Z2_Array& operator-=(const Z2_Array& other) {
        auto* this_data = as_Z2_array();
        auto* other_data = other.as_Z2_array();
        for (int i = 0; i < 6; ++i) {
            this_data[i] -= other_data[i];
        }
        return *this;
    }

    // Multiplication-assignment operator
    Z2_Array& operator*=(const Z2_Array& other) {
        auto* this_data = as_Z2_array();
        auto* other_data = other.as_Z2_array();
        for (int i = 0; i < 6; ++i) {
            this_data[i] *= other_data[i];
        }
        return *this;
    }

    // Addition operator
    Z2_Array operator+(const Z2_Array& other) const {
        Z2_Array result = *this;
        result += other;
        return result;
    }

    // Subtraction operator
    Z2_Array operator-(const Z2_Array& other) const {
        Z2_Array result = *this;
        result -= other;
        return result;
    }

    // Multiplication operator
    Z2_Array operator*(const Z2_Array& other) const {
        Z2_Array result = *this;
        result *= other;
        return result;
    }

    // Negation operator
    Z2_Array operator-() const {
        Z2_Array result;
        auto* this_data = as_Z2_array();
        auto* result_data = result.as_Z2_array();
        for (int i = 0; i < 6; ++i) {
            result_data[i] = -this_data[i];
        }
        return result;
    }

    // Equality operator
    bool operator==(const Z2_Array& other) const {
        return data == other.data;
    }

    // Three-way comparison operator
    std::strong_ordering operator<=>(const Z2_Array& other) const {
        return data <=> other.data;
    }

    // Stream insertion operator for printing
    friend std::ostream& operator<<(std::ostream& os, const Z2_Array& arr) {
        auto* arr_data = arr.as_Z2_array();
        os << "[";
        for(int i = 0; i < 6; i++) {
            os << arr_data[i];
            if(i < 5) os << ", ";
        }
        os << "]";
        return os;
    }
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