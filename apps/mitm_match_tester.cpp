#include <random>
#include <iostream>
#include <array>
#include <algorithm>
#include <cstring> // for std::strlen
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <stdexcept>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "ds/MITM.hpp"
#include "ds/Lehmer6.hpp"

namespace {
    static std::string slurp(const std::string& path) {
        std::ifstream in(path);
        if (!in) {
            throw std::runtime_error("failed to read target file: " + path);
        }
        std::ostringstream ss;
        ss << in.rdbuf();
        return ss.str();
    }

    static bool is_blank_line(const std::string& line) {
        for (char c : line) {
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v') {
                return false;
            }
        }
        return true;
    }

    static std::vector<std::string> split_target_specs(const std::string& text) {
        std::vector<std::string> specs;
        std::istringstream in(text);
        std::string line;
        std::string current;
        while (std::getline(in, line)) {
            if (is_blank_line(line)) {
                if (!current.empty()) {
                    specs.push_back(current);
                    current.clear();
                }
                continue;
            }
            if (!current.empty()) current.push_back('\n');
            current += line;
        }
        if (!current.empty()) specs.push_back(current);
        return specs;
    }

    static bool has_non_ws(const std::string& text) {
        for (char c : text) {
            if (c != ' ' && c != '\t' && c != '\n' && c != '\r' && c != '\f' && c != '\v') {
                return true;
            }
        }
        return false;
    }
} // namespace

struct Args {
    int depth{20};          // number of random T steps to build the second root
    int search_depth{10};   // BFS depth limit for each side
    uint64_t seed{0};
    int threads{0};
    bool no_indicators{false};
    bool mitm_bf{false};
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
            std::cout << "Usage: mitm_match_tester [--depth=N] [--search-depth=N] [--seed=U64] "
                         "[--threads=N] [--target=MAT|@FILE] [--no-indicators] [--mitm-bf]\n";
            std::cout << "  @FILE may contain multiple targets separated by blank lines.\n";
            std::exit(0);
        }
        if (auto* v = val("--depth")) a.depth = std::max(1, std::atoi(v));
        else if (auto* v = val("--search-depth")) a.search_depth = std::max(1, std::atoi(v));
        else if (auto* v = val("--seed")) a.seed = std::strtoull(v, nullptr, 10);
        else if (auto* v = val("--threads")) a.threads = std::max(0, std::atoi(v));
        else if (auto* v = val("--target")) a.target_spec = v;
        else if (s == "--no-indicators") a.no_indicators = true;
        else if (s == "--mitm-bf") a.mitm_bf = true;
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

    suppress_indicators = args.no_indicators;
    stored_depth_max = static_cast<uint8_t>(args.search_depth);
    target_T_count = static_cast<uint8_t>(args.search_depth + 1);
    if (args.threads > 0) THREADS = static_cast<uint8_t>(args.threads);
    mitm_bf_extension = args.mitm_bf;

    std::mt19937_64 rng(args.seed ? args.seed : std::random_device{}());

    std::vector<std::string> target_specs;
    if (!args.target_spec.empty()) {
        std::string spec = args.target_spec;
        if (spec[0] == '@') {
            try {
                spec = slurp(spec.substr(1));
            } catch (const std::exception& e) {
                std::cerr << "[mitm_match_tester] " << e.what() << "\n";
                return 2;
            }
        }
        if (!has_non_ws(spec)) {
            std::cerr << "[mitm_match_tester] empty target spec\n";
            return 2;
        }
        target_specs = split_target_specs(spec);
        if (target_specs.empty()) {
            std::cerr << "[mitm_match_tester] no non-empty targets parsed\n";
            return 2;
        }
    }

    const int total_targets = target_specs.empty() ? 1 : static_cast<int>(target_specs.size());
    int failures = 0;

    for (int idx = 0; idx < total_targets; ++idx) {
        SO6 target = SO6::identity();
        if (!target_specs.empty()) {
            target = SO6(target_specs[static_cast<size_t>(idx)]);
            target.last_T = 15;
        } else {
            // Pick a random target by a short random walk of length = depth, then random perm/sign.
            target = random_target(rng, args.depth);
            target = random_perm_sign(target, rng);
        }

        if (!target_specs.empty()) {
            std::cout << "[mitm_match_tester] target " << (idx + 1) << "/" << total_targets << "\n";
        }
        std::cout << "[mitm_match_tester] target matrix:\n";
        target.print_raw(std::cout);
        std::cout << "\n\n";
        std::cout << "[mitm_match_tester] left root (identity):\n";
        SO6::identity().print_raw(std::cout);
        std::cout << "\n\n";
        std::cout << "[mitm_match_tester] right root (target):\n";
        target.print_raw(std::cout);
        std::cout << "\n\n";

        MITM mitm(SO6::identity(), target);
        MITMMatchResult res = generate_mitm_match(mitm);

        if (!res.found) {
            std::cout << "[mitm_match_tester] no meet found within depth " << args.search_depth << "\n";
            ++failures;
            continue;
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
            std::cout << "Sign mask (cols on left to flip): 0x"
                      << std::hex << int(res.col_sign_mask) << std::dec << "\n";
        } else {
            std::cout << "[mitm_match_tester] reconcile_matrices failed to produce a mapping\n";
        }
    }

    return failures == 0 ? 0 : 1;
}
