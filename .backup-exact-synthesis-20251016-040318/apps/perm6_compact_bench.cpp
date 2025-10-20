// Micro-benchmark: Perm6 (10-bit) operator[] vs raw array and Perm6Enum
#include <array>
#include <algorithm>
#include <vector>
#include <chrono>
#include <iostream>
#include <cstdint>
#include "ds/Perm6.hpp"
#include "ds/Perm6Enum.hpp"

using Clock = std::chrono::high_resolution_clock;

static std::vector<std::array<uint8_t,6>> make_arrays() {
    std::vector<std::array<uint8_t,6>> v; v.reserve(720);
    std::array<uint8_t,6> a = {0,1,2,3,4,5};
    do { v.push_back(a); } while (std::next_permutation(a.begin(), a.end()));
    return v;
}

static std::vector<ds::Perm6Enum> make_perm_enum() {
    std::vector<ds::Perm6Enum> v; v.reserve(720);
    for (int i=0;i<720;++i) v.emplace_back(static_cast<uint16_t>(i));
    return v;
}

static std::vector<Perm6> make_perm10() {
    std::vector<Perm6> v; v.reserve(720);
    for (int i=0;i<720;++i) v.emplace_back(static_cast<uint16_t>(i));
    return v;
}

int main() {
    auto arrays = make_arrays();
    auto perm_enum  = make_perm_enum();
    auto perm10  = make_perm10();

    volatile uint64_t sink = 0;
    const int rounds = 20000; // ~ 720*6*20000 ~ 86M ops per test

    {
        auto t0 = Clock::now();
        for (int r=0;r<rounds;++r) {
            for (int k=0;k<720;++k) {
                const auto& a = arrays[k];
                for (int i=0;i<6;++i) sink += a[i];
            }
        }
        auto t1 = Clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
        std::cout << "array_access_us=" << us << " sink=" << sink << "\n";
    }

    sink = 0;
    {
        auto t0 = Clock::now();
        for (int r=0;r<rounds;++r) {
            for (int k=0;k<720;++k) {
                const auto& p = perm_enum[k];
                for (int i=0;i<6;++i) sink += p[i];
            }
        }
        auto t1 = Clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
        std::cout << "perm6enum_access_us=" << us << " sink=" << sink << "\n";
    }

    sink = 0;
    {
        auto t0 = Clock::now();
        for (int r=0;r<rounds;++r) {
            for (int k=0;k<720;++k) {
                const auto& p = perm10[k];
                for (int i=0;i<6;++i) sink += p[i];
            }
        }
        auto t1 = Clock::now();
        auto us = std::chrono::duration_cast<std::chrono::microseconds>(t1-t0).count();
        std::cout << "perm6_10bit_access_us=" << us << " sink=" << sink << "\n";
    }
    return 0;
}

