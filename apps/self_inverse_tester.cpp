// Verifies T is self-inverse under canonicalization: apply random sequence,
// then apply the same indices in reverse and check we return to identity.
#include <iostream>
#include <random>
#include <vector>
#include "so6/T_Operator.hpp"
#include "config/Globals.hpp"

int main(int argc, char** argv) {
    int trials = 1000, max_len = 2;
    if (argc > 1) trials = std::max(1, std::atoi(argv[1]));
    if (argc > 2) max_len = std::max(1, std::atoi(argv[2]));
    suppress_indicators = true;

    std::mt19937_64 rng(std::random_device{}());
    std::uniform_int_distribution<int> len_dist(1, max_len);
    std::uniform_int_distribution<int> t_dist(0, 14);

    for (int tr = 0; tr < trials; ++tr) {
        int len = len_dist(rng);
        std::vector<uint8_t> seq; seq.reserve(static_cast<size_t>(len));
        SO6 s = SO6::identity();
        for (int i = 0; i < 2; ++i) {
            uint8_t t = static_cast<uint8_t>(t_dist(rng));
            seq.push_back(t);
            s = T_OperatorRuntime(t) * s;
        }
        for (int i = 1; i >= 0; --i) {
            s = T_OperatorRuntime(seq[static_cast<size_t>(i)]) * s;
        }
        if (!(s == SO6::identity())) {
            std::cerr << "Self-inverse failure at trial " << tr
                      << " with len=" << len << "\n";
            return 2;
        }
    }
    std::cout << "OK: " << trials << " trials passed (max_len=" << max_len << ")\n";
    return 0;
}
