#ifndef UTIL_AMY_CIRCUIT_HPP
#define UTIL_AMY_CIRCUIT_HPP

#include <cstdint>
#include <ostream>
#include <random>
#include <string>
#include <utility>
#include <vector>

namespace amy {

struct CircuitParams {
    int tcount = 4;
    double prob_t = 0.2;
    bool allow_cnot = false;
};

struct Circuit {
    std::vector<std::string> row0;
    std::vector<std::string> row1;
    int tdepth = 0;

    int depth() const { return static_cast<int>(row0.size()); }
};

Circuit random_circuit(std::mt19937_64& rng, const CircuitParams& params);

void write_rows(std::ostream& os, const Circuit& circ);
void write_searches_block(std::ostream& os, const std::string& label, const Circuit& circ);

} // namespace amy

#endif // UTIL_AMY_CIRCUIT_HPP
