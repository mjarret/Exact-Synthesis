#include "so6/SO6.hpp"
#include "util/utils.hpp"

#include <array>
#include <cstdint>
#include <iomanip>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <queue>
#include <unordered_set>
#include <vector>

namespace {

using MoveSeq = std::vector<uint8_t>;

std::string format_DyadicSqrt2(const DyadicSqrt2& z) {
    std::ostringstream oss;
    oss << '(' << static_cast<int>(z.int_c)
        << ',' << static_cast<int>(z.sqrt2_c)
        << ';' << static_cast<int>(z.denom_exp) << ')';
    return oss.str();
}

void dump_state(const std::string& label, const SO6& s) {
    auto rows = s.row_perm_lh().to_array();
    auto cols = s.col_perm_lh().to_array();

    std::cout << label << "\n";
    std::cout << "  sign mask : 0x" << std::hex << static_cast<int>(s.sign_mask())
              << std::dec << "\n";
    std::cout << "  hash      : " << s.primary_hash() << "\n";
    std::cout << "  col_hash  : " << s.column_hash() << "\n";
    std::cout << "  row_perm  :";
    for (auto v : rows) std::cout << ' ' << static_cast<int>(v);
    std::cout << "\n  col_perm  :";
    for (auto v : cols) std::cout << ' ' << static_cast<int>(v);
    std::cout << "\n  canonical matrix (rows x cols):\n";
    for (int r = 0; r < 6; ++r) {
        std::cout << "    ";
        for (int c = 0; c < 6; ++c) {
            std::cout << std::setw(12)
                      << format_DyadicSqrt2(s.get_element(rows[static_cast<size_t>(r)],
                                                 cols[static_cast<size_t>(c)]));
        }
        std::cout << '\n';
    }
}

enum class UpdateVariant { SubtractThenNeg, DirectAssign };

template <UpdateVariant Mode>
SO6 left_multiply_by_T_variant(SO6 S, uint8_t idx) {
    static constexpr std::array<std::pair<int, int>, 15> pairs{{
        {0, 1}, {0, 2}, {0, 3}, {0, 4}, {0, 5},
        {1, 2}, {1, 3}, {1, 4}, {1, 5},
        {2, 3}, {2, 4}, {2, 5},
        {3, 4}, {3, 5},
        {4, 5}
    }};

    if (idx >= pairs.size()) {
        throw std::out_of_range("T index out of range");
    }

    const auto [row1, row2] = pairs[idx];

    S.hash = static_cast<uint16_t>(S.hash
        - SO6::row_frequency_signature(S, row1)
        - SO6::row_frequency_signature(S, row2));

    for (int col = 0; col < 6; ++col) {
        uint16_t col_freq = SO6::col_frequency_signature(S, col);
        uint16_t col_sig = static_cast<uint16_t>(col_freq ^ (col_freq >> 1));
        S.hash = static_cast<uint16_t>(S.hash - col_sig);
        S.col_hash = static_cast<uint16_t>(S.col_hash - col_sig);

        DyadicSqrt2 a = S.get_element(row1, static_cast<uint8_t>(col));
        DyadicSqrt2 b = S.get_element(row2, static_cast<uint8_t>(col));
        const DyadicSqrt2 a_old = a;
        const DyadicSqrt2 b_old = b;

        a += b_old;
        if constexpr (Mode == UpdateVariant::SubtractThenNeg) {
            b -= a_old;
            b = -b;
        } else {
            DyadicSqrt2 tmp = a_old;
            tmp -= b_old;
            b = tmp;
        }
        a.denom_exp += (a.int_c != 0);
        b.denom_exp += (b.int_c != 0);

        S.set_element(row1, static_cast<uint8_t>(col), a);
        S.set_element(row2, static_cast<uint8_t>(col), b);

        col_freq = SO6::col_frequency_signature(S, col);
        col_sig = static_cast<uint16_t>(col_freq ^ (col_freq >> 1));
        S.hash = static_cast<uint16_t>(S.hash + col_sig);
        S.col_hash = static_cast<uint16_t>(S.col_hash + col_sig);
    }

    S.canonical_form();
    S.hash = static_cast<uint16_t>(S.hash
        + SO6::row_frequency_signature(S, row1)
        + SO6::row_frequency_signature(S, row2));
    S.last_T = idx;
    return S;
}

std::string path_to_string(const MoveSeq& path) {
    if (path.empty()) return "{}";
    std::ostringstream oss;
    oss << '{';
    for (size_t i = 0; i < path.size(); ++i) {
        if (i) oss << ' ';
        oss << static_cast<int>(path[i]);
    }
    oss << '}';
    return oss.str();
}

} // namespace

int main() {
    const SO6 root = SO6::identity();

    struct Node {
        SO6 state;
        MoveSeq path;
    };

    std::queue<Node> frontier;
    frontier.push(Node{root, {}});

    std::unordered_set<SO6> visited;
    visited.insert(root);

    constexpr int depth_cap = 7;

    while (!frontier.empty()) {
        Node cur = frontier.front();
        frontier.pop();

        const int depth = static_cast<int>(cur.path.size());
        const uint8_t last_T = cur.state.last_T;

        for (uint8_t t = 0; t < 15; ++t) {
            if (t == last_T) continue;

            SO6 subtract_then_neg = left_multiply_by_T_variant<UpdateVariant::SubtractThenNeg>(cur.state, t);
            SO6 direct_assign     = left_multiply_by_T_variant<UpdateVariant::DirectAssign>(cur.state, t);

            if (!(subtract_then_neg == direct_assign)) {
                MoveSeq witness = cur.path;
                witness.push_back(t);
                std::cout << "Mismatch detected after sequence " << path_to_string(witness) << "\n";
                dump_state("subtract_then_neg result", subtract_then_neg);
                dump_state("direct_assign result", direct_assign);
                return EXIT_FAILURE;
            }

            if (depth < depth_cap) {
                if (visited.insert(direct_assign).second) {
                    MoveSeq next_path = cur.path;
                    next_path.push_back(t);
                    frontier.push(Node{direct_assign, std::move(next_path)});
                }
            }
        }
    }

    std::cout << "No mismatch detected up to depth " << depth_cap << '\n';
    return EXIT_SUCCESS;
}
