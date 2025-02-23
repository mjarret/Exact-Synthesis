#include <iostream>
#include <array>
#include <algorithm>
#include <cassert>
#include "../PermutationCoder.hpp"

int main() {
    std::array<uint8_t, 6> perm = {0, 1, 2, 3, 4, 5}; // Initial permutation
    uint16_t rank = 0;

    do {
        // Extract elements
        uint8_t a = perm[0], b = perm[1], c = perm[2];
        uint8_t d = perm[3], e = perm[4], f = perm[5];

        // **1. Encode the permutation**
        uint16_t encodedRank = PermutationCoder::encode(a, b, c, d, e, f);
        // assert(encodedRank == rank && "❌ Encoding mismatch!");

        // **2. Decode full permutation**
        uint8_t da, db, dc, dd, de, df;
        PermutationCoder::decode(encodedRank, da, db, dc, dd, de, df);

        // **3. Verify correctness**
        assert(a == da && b == db && c == dc && d == dd && e == de && f == df && "❌ Decoding failed!");

        // **4. Print progress every 50 ranks**
        if (rank % 50 == 0) {
            std::cout << "✅ Checked rank: " << rank << " (Valid)" << std::endl;
        }

        rank++;

    } while (std::next_permutation(perm.begin(), perm.end()));

    std::cout << "🎉 All 720 permutations successfully verified! ✅" << std::endl;
    return 0;
}
