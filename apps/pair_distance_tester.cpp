/**
 * Random pair distance tester using MITM
 *
 * - Builds a LUT of depth D rooted at identity (shortest paths from I).
 * - Samples many random vertex pairs from that LUT.
 * - For each pair (u, v), runs build_two_lookup_tables_until_match(u, v)
 *   and records the meet depths dl, dr; verifies the sum as a valid distance.
 */

#include <iostream>
#include <vector>
#include <random>
#include <algorithm>
#include <cstdint>
#include <string>
#include <cstring>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "ds/LUT.hpp"
#include "algo/Generate.hpp"

namespace {

struct Args {
    int depth = 10;     // LUT depth from identity
    int pairs = 10;   // how many random pairs to test
    uint64_t seed = 0;  // RNG seed (0 => random_device)
    int threads = 0;    // optional override
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
            std::cout << "Usage: pair_distance_tester [--depth=N] [--pairs=N] [--seed=U64] [--threads=N]\n";
            std::exit(0);
        }
        if (auto* v = val("--depth"))   a.depth   = std::max(1, std::atoi(v));
        else if (auto* v = val("--pairs"))   a.pairs   = std::max(1, std::atoi(v));
        else if (auto* v = val("--seed"))    a.seed    = std::strtoull(v, nullptr, 10);
        else if (auto* v = val("--threads")) a.threads = std::max(0, std::atoi(v));
    }
    return a;
}

int layer_of(LUT& lut, const SO6& x) {
    int layer = 0;
    for (auto it = lut.begin(); it != lut.end(); ++it, ++layer) {
        const finalized_set& fs = *it;
        if (fs.find(x) != fs.end()) return layer;
    }
    return -1;
}

} // namespace

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);

    // Configure globals
    suppress_indicators = true;
    stored_depth_max = static_cast<uint8_t>(args.depth);
    target_T_count = static_cast<uint8_t>(args.depth + 1);
    if (args.threads > 0) THREADS = static_cast<uint8_t>(args.threads);

    // Build reference LUT from identity
    LUT ref = algo::create_lookup_table(SO6::identity());

    std::cout << "LUT built with " << ref.size() << " layers up to depth " << static_cast<int>(stored_depth_max) << "\n";

    std::mt19937_64 rng(args.seed ? args.seed : std::random_device{}());

    auto pick_random = [&](std::mt19937_64& rr) -> SO6 {
        size_t total = 0;
        for (const auto& layer : ref) total += layer.size();
        if (total < 2) throw std::runtime_error("Insufficient vertices in LUT");
        std::uniform_int_distribution<size_t> d(0, total - 1);
        size_t idx = d(rr);
        for (const auto& layer : ref) {
            if (idx < layer.size()) {
                size_t k = 0;
                for (const auto& x : layer) { if (k++ == idx) return x; }
            } else {
                idx -= layer.size();
            }
        }
        return SO6::identity();
    };

    size_t tested = 0, failures = 0;
    uint64_t sum_dist = 0;
    uint64_t max_dist = 0;

    for (int t = 0; t < args.pairs; ++t) {
        SO6 v = pick_random(rng);
        LUT left(SO6::identity());
        LUT right(v);

        // Compute distance via MITM from identity to v
        auto match = algo::build_two_lookup_tables_until_match(left, right);
        if (!match) {
            ++failures;
            std::cerr << "No meet found for sampled v within depth=" << args.depth << "\n";
            continue;
        }

        const SO6& meet = *match;
        int dl = layer_of(left, meet);
        int dr = layer_of(right, meet);
        if (dl < 0 || dr < 0) {
            ++failures;
            std::cerr << "Meet not present in one of the LUTs (dl=" << dl << ", dr=" << dr << ")\n";
            continue;
        }
        // Reference shortest distance from identity to v using the prebuilt LUT
        int dref = layer_of(ref, v);
        if (dref < 0) {
            ++failures;
            std::cerr << "Target not found in reference LUT\n";
            continue;
        }

        uint64_t dpair = static_cast<uint64_t>(dl + dr);
        if (dpair != static_cast<uint64_t>(dref)) {
            ++failures;
            std::cerr << "Distance mismatch: dl+dr=" << dpair << " vs dref=" << dref << "\n";
            continue;
        }
        sum_dist += dpair;
        if (dpair > max_dist) max_dist = dpair;
        ++tested;
    }

    std::cout << "Pairs tested: " << tested << "/" << args.pairs << ", failures=" << failures << "\n";
    if (tested > 0) {
        std::cout << "Average distance (dl+dr): " << (double(sum_dist) / double(tested)) << "\n";
        std::cout << "Max observed distance: " << max_dist << "\n";
    }
    return failures == 0 ? 0 : 2;
}
