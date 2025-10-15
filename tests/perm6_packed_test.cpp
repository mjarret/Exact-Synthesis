// Brute-force correctness test for ds::Perm6Packed against arrays
#include <array>
#include <algorithm>
#include <iostream>
#include <cstdint>
#include "ds/Perm6Packed.hpp"

int main() {
    using ds::Perm6Packed;
    std::array<uint8_t,6> a = {0,1,2,3,4,5};
    size_t idx = 0;
    do {
        Perm6Packed p = Perm6Packed::from_array(a);
        for (int i=0;i<6;++i) {
            if (p[i] != a[i]) {
                std::cerr << "Index mismatch at perm=" << idx << " i=" << i << " got=" << (int)p[i] << " exp=" << (int)a[i] << "\n";
                return 1;
            }
        }
        uint8_t out[6]; p.to_array(out);
        for (int i=0;i<6;++i) {
            if (out[i] != a[i]) {
                std::cerr << "to_array mismatch at perm=" << idx << " i=" << i << " got=" << (int)out[i] << " exp=" << (int)a[i] << "\n";
                return 2;
            }
        }
        ++idx;
    } while (std::next_permutation(a.begin(), a.end()));
    if (idx != 720) { std::cerr << "Expected 720 perms, got " << idx << "\n"; return 3; }
    std::cout << "Perm6Packed correctness OK (720 perms validated)\n";
    return 0;
}

