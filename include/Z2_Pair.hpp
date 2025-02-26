#ifndef Z2_PAIR_HPP
#define Z2_PAIR_HPP

#include <cstdint>
#include "Z2.hpp"

constexpr __uint128_t MASK_21BIT_BUCKETS = (__uint128_t(0x1FFFFF00) << 64) | __uint128_t(0x1FFFFF001FFFFF);
constexpr __uint128_t CARRY_MASK = (__uint128_t(0x20000000) << 64) | __uint128_t(0x2000000020000000);
constexpr __uint128_t FINAL_MASK = (__uint128_t(0x3FFFFFFFFFFFFFFF) << 64) | __uint128_t(0xFFFFFFFFFFFF);
constexpr __uint128_t DENOM_EXP_MASK =
    (__uint128_t(0x1F0000) << 0)  | (__uint128_t(0x1F0000) << 21) |  
    (__uint128_t(0x1F0000) << 42) | (__uint128_t(0x1F0000) << 63) |  
    (__uint128_t(0x1F0000) << 84) | (__uint128_t(0x1F0000) << 105);  

struct Z2_Pair {
    union {
        struct {
            uint32_t first : 21;
            uint32_t second : 21;
            uint32_t third : 21;
            uint32_t fourth : 21;
            uint32_t fifth : 21;
            uint32_t sixth : 23;
        };
        __uint128_t data;
    };

    Z2_Pair(__uint128_t data_ = 0) : data(data_) {};

    Z2_Pair operator= (const Z2_Pair& other) {
        data = other.data;
        return *this;
    }

    Z2_Pair operator= (const uint64_t other) {
        data = other;
        return *this;
    }

    #define ADD_NUMERATORS(LEFT, RIGHT) \
        (LEFT + RIGHT - (((LEFT & MASK_21BIT_BUCKETS) + (RIGHT & MASK_21BIT_BUCKETS)) & CARRY_MASK)) & FINAL_MASK
 


    Z2_Pair operator+= (const Z2_Pair& other) {
        __uint128_t differing_bits = (data & DENOM_EXP_MASK) - (other.data & DENOM_EXP_MASK);
        data = ADD_NUMERATORS(data, other.data);
        return *this;
    }

};

#endif // Z2_PAIR_HPP