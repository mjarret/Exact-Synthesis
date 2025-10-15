// Brute-force equivalence tests for Perm6 (10-bit rank) vs raw arrays
#include <array>
#include <algorithm>
#include <iostream>
#include <cstdint>
#include <vector>
#include "ds/Perm6.hpp"

static std::vector<std::array<uint8_t,6>> all_perms() {
    std::vector<std::array<uint8_t,6>> v; v.reserve(720);
    std::array<uint8_t,6> a = {0,1,2,3,4,5};
    do { v.push_back(a); } while (std::next_permutation(a.begin(), a.end()));
    return v;
}

int main() {
    auto perms = all_perms();
    size_t cases = 0;
    for (size_t idx=0; idx<perms.size(); ++idx) {
        const auto &arr = perms[idx];
        // Encode array -> rank (must match enumeration index)
        uint16_t r = Perm6::encode(arr.data());
        if (r != idx) {
            std::cerr << "encode_mismatch at rank " << idx << ": got " << r << "\n";
            return 1;
        }
        Perm6 p(static_cast<uint16_t>(r));

        // operator[] equivalence
        for (int i=0;i<6;++i) {
            if (p[i] != arr[i]) {
                std::cerr << "operator[] mismatch at rank " << idx << ", i=" << i
                          << ": p[i]=" << int(p[i]) << " arr[i]=" << int(arr[i]) << "\n";
                return 2;
            }
        }

        // to_array equivalence
        uint8_t out[6]{}; p.to_array(out);
        for (int i=0;i<6;++i) if (out[i] != arr[i]) {
            std::cerr << "to_array mismatch at rank " << idx << ", i=" << i << "\n"; return 3;
        }

        // Simulate iterator access pattern used by SO6::Iterator
        // row_index = Row[idx%6], col_index = Col[idx/6]
        uint8_t row_a[6]; uint8_t col_a[6];
        for (int i=0;i<6;++i) row_a[i]=arr[i];
        for (int i=0;i<6;++i) col_a[i]=arr[i]; // reuse same perm for column case
        Perm6 row_p = Perm6::from_array(row_a);
        Perm6 col_p = Perm6::from_array(col_a);
        for (int k=0;k<36;++k) {
            int ri = k % 6; int ci = k / 6;
            if (row_a[ri] != row_p[ri]) { std::cerr << "row index mismatch rank=" << idx << " k=" << k << "\n"; return 4; }
            if (col_a[ci] != col_p[ci]) { std::cerr << "col index mismatch rank=" << idx << " k=" << k << "\n"; return 4; }
        }

        // Serialization pattern (range-like): collect 6 ints
        // Compare arr values and p.to_array output vector
        uint8_t ser[6]{}; row_p.to_array(ser);
        for (int i=0;i<6;++i) if (ser[i] != arr[i]) { std::cerr << "serialize mismatch\n"; return 5; }

        ++cases;
    }
    std::cout << "Perm6 (10-bit) equivalence OK for " << cases << " permutations\n";
    return 0;
}
