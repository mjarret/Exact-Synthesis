/**
 * @file mitm_seed_gen.cpp
 * @brief Offline generator for MITM benchmark seeds.
 *
 * Generates many random SO6 targets by T-walks, then runs the MITM matcher
 * to determine the *actual* distance (dl + dr) from identity. Results are
 * written to a simple text file which the benchmark can later consume.
 *
 * Each output line has the form:
 *
 *   id actual_depth dl dr walk_len t0 t1 ... t_{walk_len-1}
 *
 * where t_i are the T indices (0..14) used to build the target from identity.
 */

#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "ds/MITM.hpp"

namespace {

struct Args {
    int num_seeds{1000};
    int min_walk{2};
    int max_walk{8};
    int search_depth{12};
    std::uint64_t seed{0};
    std::string out_path{"benchmarks/mitm_seeds.txt"};
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
            std::cout
                << "Usage: mitm_seed_gen [--num=N] [--min-walk=N] [--max-walk=N]\n"
                << "                      [--search-depth=N] [--seed=U64] [--out=PATH]\n\n"
                << "Generates MITM benchmark seeds as lines of:\n"
                << "  id actual_depth dl dr walk_len t0 ... t_{walk_len-1}\n";
            std::exit(0);
        }
        if (auto* v = val("--num"))           a.num_seeds    = std::max(1, std::atoi(v));
        else if (auto* v = val("--min-walk")) a.min_walk     = std::max(1, std::atoi(v));
        else if (auto* v = val("--max-walk")) a.max_walk     = std::max(a.min_walk, std::atoi(v));
        else if (auto* v = val("--search-depth")) a.search_depth = std::max(1, std::atoi(v));
        else if (auto* v = val("--seed"))     a.seed         = std::strtoull(v, nullptr, 10);
        else if (auto* v = val("--out"))      a.out_path     = v;
    }
    return a;
}

SO6 target_from_walk(std::mt19937_64& rng, int walk_len, std::vector<uint8_t>& out_seq) {
    std::uniform_int_distribution<int> t_dist(0, 14);
    out_seq.resize(static_cast<size_t>(walk_len));

    SO6 cur = SO6::identity();
    for (int i = 0; i < walk_len; ++i) {
        uint8_t t = static_cast<uint8_t>(t_dist(rng));
        out_seq[static_cast<size_t>(i)] = t;
        cur = T_OperatorRuntime(t) * cur;
    }
    return cur;
}

} // namespace

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);

    std::cerr << "[mitm_seed_gen] starting\n";
    std::cerr.flush();

    suppress_indicators = true; // no progress bars for offline generation
    stored_depth_max = static_cast<uint8_t>(args.search_depth);

    std::cerr << "[mitm_seed_gen] search_depth=" << args.search_depth
              << " num_seeds=" << args.num_seeds
              << " min_walk=" << args.min_walk
              << " max_walk=" << args.max_walk << "\n";
    std::cerr.flush();

    std::mt19937_64 rng(args.seed ? args.seed : 0x123456789abcdef0ULL);
    std::uniform_int_distribution<int> walk_dist(args.min_walk, args.max_walk);

    std::ofstream out(args.out_path);
    if (!out) {
        std::cerr << "Error: could not open output file '" << args.out_path << "' for write\n";
        return 1;
    }

    std::cerr << "[mitm_seed_gen] opened output '" << args.out_path << "'\n";
    std::cerr.flush();

    out << "# MITM benchmark seeds\n";
    out << "# format: id actual_depth dl dr walk_len t0 ... t_{walk_len-1}\n";

    int generated = 0;
    int attempts = 0;
    std::vector<uint8_t> walk;

    while (generated < args.num_seeds) {
        ++attempts;
        int walk_len = walk_dist(rng);
        SO6 target = target_from_walk(rng, walk_len, walk);

        MITM mitm(SO6::identity(), target);
        auto meet_opt = generate_mitm_until_match(mitm);
        if (!meet_opt) continue;

        const SO6& meet = *meet_opt;
        auto left_path_opt  = mitm.left().path_to(meet);
        auto right_path_opt = mitm.right().path_to(meet);
        if (!left_path_opt || !right_path_opt) continue;

        int dl = static_cast<int>(left_path_opt->size());
        int dr = static_cast<int>(right_path_opt->size());
        int actual_depth = dl + dr;

        out << generated << ' '
            << actual_depth << ' '
            << dl << ' '
            << dr << ' '
            << walk_len;
        for (int i = 0; i < walk_len; ++i) {
            out << ' ' << static_cast<int>(walk[static_cast<size_t>(i)]);
        }
        out << '\n';
        ++generated;
    }

    std::cerr << "Generated " << generated << " seeds after " << attempts << " attempts\n";
    std::cerr << "Output written to: " << args.out_path << "\n";
    return 0;
}
