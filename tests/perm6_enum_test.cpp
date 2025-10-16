// Brute-force correctness test for ds::Perm6Enum
#include <array>
#include <algorithm>
#include <iostream>
#include <cstdint>
#include "ds/Perm6Enum.hpp"

int main() {
    using ds::Perm6Enum;
    std::array<uint8_t,6> a = {0,1,2,3,4,5};
    // Verify table and rank alignment across all 720 perms
    size_t idx = 0;
    do {
        auto r = Perm6Enum::rank_of(a.data());
        if (r != idx) {
            std::cerr << "Rank mismatch at idx=" << idx << " r=" << r << "\n";
            return 1;
        }
        Perm6Enum p = Perm6Enum::from_array(a);
        for (int i=0;i<6;++i) {
            if (p[i] != a[i]) {
                std::cerr << "Index mismatch at idx=" << idx << " i=" << i << " got=" << (int)p[i] << " exp=" << (int)a[i] << "\n";
                return 2;
            }
        }
        ++idx;
    } while (std::next_permutation(a.begin(), a.end()));

    if (idx != 720) {
        std::cerr << "Expected 720 perms, got " << idx << "\n"; return 3;
    }
    std::cout << "Perm6Enum correctness OK (720 perms validated)\n";
    return 0;
}

