#include <random>
#include <iostream>
#include <array>
#include <algorithm>
#include <cstring> // for std::strlen
#include <string>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "ds/MITM.hpp"
#include "ds/Lehmer6.hpp"

namespace {
} // namespace

struct Args {
    int depth{20};          // number of random T steps to build the second root
    int search_depth{10};   // BFS depth limit for each side
    uint64_t seed{0};
    int threads{0};
    std::string target_spec;
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s(argv[i]);
        auto val = [&](const char* key) -> const char* {
            size_t n = std::strlen(key);
            if (s.size() > n && s.compare(0, n, key) == 0 && s[n] == '=') return s.c_str() + n + 1;
            return nullptr;
        };
        if (s == "--help" || s == "-h") {
        std::cout << "Usage: mitm_match_tester [--depth=N] [--search-depth=N] [--seed=U64] [--threads=N] [--target=MAT]\n";
        std::exit(0);
    }
    if (auto* v = val("--depth"))   a.depth   = std::max(1, std::atoi(v));
    else if (auto* v = val("--search-depth")) a.search_depth = std::max(1, std::atoi(v));
    else if (auto* v = val("--seed")) a.seed  = std::strtoull(v, nullptr, 10);
    else if (auto* v = val("--threads")) a.threads = std::max(0, std::atoi(v));
    else if (auto* v = val("--target")) a.target_spec = v;
    }
return a;
}

SO6 random_target(std::mt19937_64& rng, int steps) {
    std::uniform_int_distribution<int> d(0, 14);
    SO6 cur = SO6::identity();
    for (int i = 0; i < steps; ++i) {
        cur = T_OperatorRuntime(static_cast<uint8_t>(d(rng))) * cur;
    }
    return cur;
}

SO6 random_perm_sign(const SO6& base, std::mt19937_64& rng) {
    std::array<uint8_t,6> row{0,1,2,3,4,5};
    std::array<uint8_t,6> col{0,1,2,3,4,5};
    std::shuffle(row.begin(), row.end(), rng);
    std::shuffle(col.begin(), col.end(), rng);
    uint8_t sign_mask = static_cast<uint8_t>(rng() & 0x3Fu);

    // Apply permutation/sign directly to the raw data
    SO6 out;
    for (int r = 0; r < 6; ++r) {
        uint8_t src_r = row[r];
        bool neg = ((sign_mask >> r) & 1u) != 0;
        for (int c = 0; c < 6; ++c) {
            uint8_t src_c = col[c];
            auto v = base.get_element(src_r, src_c);
            if (neg) v = -v;
            out.set_element(r, c, v);
        }
    }
    // Reset permutation/sign metadata to identity for this new raw layout
    out.last_T = base.last_T;
    out.set_sign_mask(0);
    out.set_row_perm_lh(Lehmer6()); // identity
    out.set_col_perm_lh(Lehmer6()); // identity
    out.recompute_hash();
    return out;
}

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);

    suppress_indicators = false; // show MITM progress
    stored_depth_max = static_cast<uint8_t>(args.search_depth);
    target_T_count = static_cast<uint8_t>(args.search_depth + 1);
    if (args.threads > 0) THREADS = static_cast<uint8_t>(args.threads);

    std::mt19937_64 rng(args.seed ? args.seed : std::random_device{}());

    SO6 target = SO6::identity();
    if (!args.target_spec.empty()) {
        target = SO6(args.target_spec);
        target.last_T = 15;
    } else {
        // Pick a random target by a short random walk of length = depth, then random perm/sign.
        target = random_target(rng, args.depth);
        target = random_perm_sign(target, rng);
    }

    MITM mitm(SO6::identity(), target);
    MITMMatchResult res = generate_mitm_match(mitm);

    if (!res.found) {
        std::cout << "[mitm_match_tester] no meet found within depth " << args.search_depth << "\n";
        return 1;
    }

    std::cout << "[mitm_match_tester] meet found\n";
    std::cout << "  dl = " << res.dl << ", dr = " << res.dr
              << " (path lengths left/right)\n";
    std::cout << "  left_path size  = " << res.left_path.size() << "\n";
    std::cout << "  right_path size = " << res.right_path.size() << "\n\n";

    std::cout << "Meet (canonical form from left side):\n";
    res.meet.print_with_perms(std::cout);
    std::cout << "\n\n";

    if (res.mapping_ok) {
        std::cout << "Row mapping (left -> right): ";
        for (int i = 0; i < 6; ++i) {
            std::cout << int(res.row_map[static_cast<size_t>(i)]) << (i == 5 ? '\n' : ' ');
        }
        std::cout << "Col mapping (left -> right): ";
        for (int i = 0; i < 6; ++i) {
            std::cout << int(res.col_map[static_cast<size_t>(i)]) << (i == 5 ? '\n' : ' ');
        }
        std::cout << "Sign mask (rows on left to flip): 0x"
                  << std::hex << int(res.sign_mask) << std::dec << "\n";
    } else {
        std::cout << "[mitm_match_tester] reconcile_matrices failed to produce a mapping\n";
    }

    return 0;
}
