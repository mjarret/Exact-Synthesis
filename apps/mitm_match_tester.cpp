/**
 * MITM correctness tester for build_two_lookup_tables_until_match
 *
 * Steps:
 *  - Build a LUT to depth D from identity.
 *  - Pick a random element from the last finalized layer (depth D).
 *  - Run the alternating meet-in-the-middle expansion between identity and that element.
 *  - Verify that the meeting point depths from each root sum to D (shortest path length).
 */

#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cstring>
#include <sstream>
#include <indicators/progress_bar.hpp>
#include <cstdint>
#include <string>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "ds/LUT.hpp"
#include "algo/Generate.hpp"

namespace {

struct Args {
    int depth = 4;    // build initial LUT to this depth from identity
    int trials = 5; // how many random back-layer choices to test
    uint64_t seed = 0; // RNG seed; 0 -> random_device
    int threads = 0;   // optional thread override
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
            std::cout << "Usage: mitm_match_tester [--depth=N] [--trials=N] [--seed=U64] [--threads=N]\n";
            std::exit(0);
        }
        if (auto* v = val("--depth"))     a.depth   = std::max(1, std::atoi(v));
        else if (auto* v = val("--trials"))   a.trials  = std::max(1, std::atoi(v));
        else if (auto* v = val("--seed"))     a.seed    = std::strtoull(v, nullptr, 10);
        else if (auto* v = val("--threads"))  a.threads = std::max(0, std::atoi(v));
    }
    return a;
}

// Find the minimal layer index within a LUT where x appears; returns -1 if not found.
int layer_of(LUT& lut, const SO6& x) {
    int layer = 0;
    for (auto it = lut.begin(); it != lut.end(); ++it, ++layer) {
        const finalized_set& fs = *it;
        if (fs.find(x) != fs.end()) return layer;
    }
    return -1;
}

// Pick a uniformly random element from the last finalized layer of the LUT
SO6 pick_random_back(const LUT& lut, std::mt19937_64& rng) {
    const finalized_set& back = lut.current();
    if (back.empty()) throw std::runtime_error("empty back layer");
    std::uniform_int_distribution<size_t> d(0, back.size() - 1);
    size_t idx = d(rng);
    size_t k = 0;
    for (const auto& x : back) { if (k++ == idx) return x; }
    return SO6::identity();
}

// One trial: choose random back-layer target, run MITM, compute depths
struct TrialResult { bool found; int d; int dl; int dr; int side; };

TrialResult run_trial(const LUT& ref, std::mt19937_64& rng) {
    // Choose a target uniformly from the back layer
    SO6 target = pick_random_back(ref, rng);

    // Compute minimal depth from identity in the reference LUT
    LUT ref_copy = ref; // layer_of expects non-const; LUT iterators are non-const accessors
    const int d = layer_of(ref_copy, target);

    // Replicate the current-layer-only MITM to capture which side triggers
    LUT left(SO6::identity());
    LUT right(target);
    for (int depth = 0; depth < stored_depth_max; ++depth) {
        // Expand left against right's current layer
        bool hit_first = false;
        SO6 intersection;
        auto pred_first = [&](const SO6& s){ bool r = (right.back().find(s) != right.back().end()); if (r) hit_first = true; return r; };
        algo::get_next_T_count(left, nullptr, pred_first, &intersection); // depth went to depth+1 for left
        left.finalize_current_set(nullptr);
        if (hit_first) {
            int dl = layer_of(left, intersection);
            int dr = layer_of(right, intersection);
            std::cout << "hit at depth " << depth + 1 << " from left side\n";
            return {true, d, dl, dr, 0};
        }

        // Expand right against left's current layer
        bool hit_second = false;
        auto pred_second = [&](const SO6& s){ bool r = (left.back().find(s) != left.back().end()); if (r) hit_second = true; return r; };
        // Expand the right side using the standard T-moves
        algo::get_next_T_count(right, nullptr, pred_second, &intersection);
        right.finalize_current_set(nullptr);
        if (hit_second) {
            int dl = layer_of(left, intersection);
            int dr = layer_of(right, intersection);
            std::cout << "hit at depth " << depth + 1 << " from right side\n";
            return {true, d, dl, dr, 1};
        }
    }
    return {false, d, -1, -1, -1};
}

} // namespace

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);

    // Configure runtime
    suppress_indicators = true;
    stored_depth_max = static_cast<uint8_t>(args.depth);
    target_T_count = static_cast<uint8_t>(args.depth + 1);
    if (args.threads > 0) THREADS = static_cast<uint8_t>(args.threads);

    // Build a reference LUT from identity to depth D
    std::cout << "Building reference LUT to depth " << args.depth << "...\n";
    LUT ref = algo::create_lookup_table(SO6::identity());
    std::cout << "Reference LUT built with " << ref.size() << " layers.\n";

    std::mt19937_64 rng(args.seed ? args.seed : std::random_device{}());

    size_t trials = static_cast<size_t>(args.trials);
    size_t failures = 0;
    size_t ok_count = 0;
    size_t equal_halves = 0; // dl==dr occurrences
    size_t side0 = 0, side1 = 0; // which return site triggered

    for (size_t t = 0; t < trials; ++t) {
        std::cout << "Trial " << (t + 1) << "/" << trials << ": ";
        auto tr = run_trial(ref, rng);
        if (!tr.found || tr.d < 0 || tr.d > args.depth) {
            ++failures;
            continue;
        }
        bool ok = (tr.dl + tr.dr == tr.d);
        ok_count += ok;
        if (tr.dl == tr.dr) ++equal_halves;
        if (tr.side == 0) ++side0; else if (tr.side == 1) ++side1;
        std::cout << "d=" << tr.d << ", dl=" << tr.dl << ", dr=" << tr.dr << ", side=" << tr.side
                  << " => " << (ok ? "OK" : "FAIL") << "\n";
    }

    std::cout << "Trials: " << trials
              << ", ok: " << ok_count
              << ", failures: " << failures
              << ", equal dl==dr: " << equal_halves
              << ", side0: " << side0
              << ", side1: " << side1
              << "\n";
    return failures == 0 ? 0 : 6;
}
