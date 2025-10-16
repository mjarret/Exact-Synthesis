// Enumerated 6-permutation with LUT-backed operator[] and rank/unrank
#pragma once

#include <array>
#include <cstdint>
#include <algorithm>

namespace ds {

class Perm6Enum {
public:
    using Table = std::array<std::array<uint8_t,6>, 720>;

    Perm6Enum() : r_(0) {}
    explicit Perm6Enum(uint16_t rank) : r_(rank % 720) {}

    static Perm6Enum from_array(const uint8_t p[6]) {
        return Perm6Enum(rank_of(p));
    }

    static Perm6Enum from_array(const std::array<uint8_t,6>& p) {
        return Perm6Enum(rank_of(p.data()));
    }

    uint8_t operator[](int i) const {
        return table()[r_][i & 7];
    }

    void to_array(uint8_t out[6]) const {
        const auto& row = table()[r_];
        for (int i=0;i<6;++i) out[i] = row[i];
    }

    uint16_t rank() const { return r_; }

    // Lehmer rank (lexicographic), must match table() enumeration
    static uint16_t rank_of(const uint8_t p[6]) {
        uint8_t avail[6] = {0,1,2,3,4,5};
        int n = 6;
        uint16_t rank = 0;
        static constexpr int fact[6] = {120,24,6,2,1,1};
        for (int i=0;i<6;++i) {
            int x = p[i];
            int idx = 0;
            for (int k=0;k<n;++k) {
                if (avail[k] == x) { idx = k; break; }
            }
            rank += static_cast<uint16_t>(idx * fact[i]);
            for (int k=idx; k<n-1; ++k) avail[k] = avail[k+1];
            --n;
        }
        return rank;
    }

    static const Table& table() {
        static Table t = build_table();
        return t;
    }

private:
    static Table build_table() {
        Table t{};
        std::array<uint8_t,6> a = {0,1,2,3,4,5};
        size_t idx = 0;
        do {
            for (int i=0;i<6;++i) t[idx][i] = a[i];
            ++idx;
        } while (std::next_permutation(a.begin(), a.end()));
        return t;
    }

    uint16_t r_;
};

} // namespace ds

