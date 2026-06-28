#include "util/amy_circuit.hpp"

#include <algorithm>
#include <stdexcept>

namespace amy {
namespace {

CircuitParams normalize_params(const CircuitParams& in) {
    CircuitParams out = in;
    out.tcount = std::max(0, out.tcount);
    if (out.prob_t < 0.0) out.prob_t = 0.0;
    if (out.prob_t > 1.0) out.prob_t = 1.0;
    return out;
}

void write_row(std::ostream& os, const std::vector<std::string>& row) {
    for (size_t i = 0; i < row.size(); ++i) {
        if (i) os << " ";
        os << row[i];
    }
    os << "\n";
}

} // namespace

Circuit random_circuit(std::mt19937_64& rng, const CircuitParams& params) {
    CircuitParams p = normalize_params(params);

    // Randomly construct a circuit by sampling layers until exactly p.tcount
    // T gates are placed. Each layer is:
    // - a Clifford-only layer with probability (1 - p.prob_t), or
    // - a single T gate on a random qubit with probability p.prob_t.
    // After reaching p.tcount, keep appending Cliffords until the next sample
    // would be a T gate, then stop (this gives a random suffix).
    if (p.tcount > 0 && p.prob_t <= 0.0) {
        throw std::runtime_error("prob_t must be > 0 when tcount > 0");
    }

    std::uniform_real_distribution<double> pick01(0.0, 1.0);
    const int cliff_max = p.allow_cnot ? 5 : 3;
    std::uniform_int_distribution<int> pick_cliff(0, cliff_max);

    auto rand_cliff_layer = [&]() -> std::pair<std::string, std::string> {
        switch (pick_cliff(rng)) {
            case 0: return {"H", "I"};
            case 1: return {"I", "H"};
            case 2: return {"S", "I"};
            case 3: return {"I", "S"};
            case 4: return {"C(2)", "X"};
            default: return {"X", "C(1)"};
        }
    };

    Circuit circ;
    circ.tdepth = p.tcount;
    int tcount = 0;

    auto push_layer = [&](const std::pair<std::string, std::string>& st) {
        circ.row0.push_back(st.first);
        circ.row1.push_back(st.second);
    };

    while (tcount <= p.tcount) {
        double eps = pick01(rng);
        if(tcount == p.tcount && eps < p.prob_t) break;
        if (2*eps < p.prob_t) {
            push_layer(std::pair{"T", "I"});
            ++tcount;
        } else if (eps < p.prob_t) {
            push_layer(std::pair{"I", "T"});
            ++tcount;
        } else {
            push_layer(rand_cliff_layer());
        }
    }

    return circ;
}

void write_rows(std::ostream& os, const Circuit& circ) {
    if (circ.row0.size() != circ.row1.size()) {
        throw std::runtime_error("Amy circuit rows must have equal length");
    }
    write_row(os, circ.row0);
    write_row(os, circ.row1);
}

void write_searches_block(std::ostream& os, const std::string& label, const Circuit& circ) {
    os << label << " 2\n";
    write_rows(os, circ);
}

} // namespace amy
