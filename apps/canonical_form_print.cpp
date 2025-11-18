/**
 * Quick canonical form print tester:
 *  - Start from identity.
 *  - Apply a single random T operator.
 *  - Print raw storage and the canonical view (perms/sign).
 */

#include <random>
#include <iostream>
#include <bitset>

#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"

int main(int argc, char** argv) {
    uint64_t seed = 0;
    if (argc > 1) seed = std::strtoull(argv[1], nullptr, 10);
    std::mt19937_64 rng(seed ? seed : std::random_device{}());
    std::uniform_int_distribution<int> d(0, 14);

    for (int t = 0; t < 15; ++t) {
        SO6 s = SO6::identity();
        s = T_OperatorRuntime(static_cast<uint8_t>(t)) * s;

        std::cout << "==== T" << t << " ====\n";
        std::cout << "Raw storage:\n";
        s.print_raw(std::cout);
        std::cout << "\nCanonical view (perms/sign):\n";
        s.print_with_perms(std::cout);
        std::cout << "\nrow_perm=";
        for (auto v : Lehmer6::decode_ref(s.row_perm_lh_.bits())) std::cout << int(v) << ' ';
        std::cout << "\ncol_perm=";
        for (auto v : Lehmer6::decode_ref(s.col_perm_lh_.bits())) std::cout << int(v) << ' ';
        std::cout << "\nsign_mask=0b" << std::bitset<6>(s.sign_convention) << "\n\n";
    }
    return 0;
}
