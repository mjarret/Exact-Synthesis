/**
 * Hash recomputation tester
 *
 * 1) Builds a LUT rooted at identity up to a small stored depth.
 * 2) Samples many elements from the finalized layers.
 * 3) Verifies that calling recompute_hash() reproduces the stored hash fields.
 */

#include <iostream>
#include <vector>
#include <algorithm>
#include <random>
#include <cstdint>
#include <string>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/graph/LUT.hpp"
#include "algo/Generate.hpp"

namespace {

struct Args {
    int stored_depth = 10;   // how many layers to build (finalized)
    int samples = 100000;     // how many elements to test (max)
    int threads = 0;        // 0 = leave default, else override
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s(argv[i]);
        auto eq = s.find('=');
        auto val_of = [&](const std::string& key) -> const char* {
            if (s.rfind(key + "=", 0) == 0) return s.c_str() + key.size() + 1;
            return nullptr;
        };
        if (s == "-h" || s == "--help") {
            std::cout << "Usage: hash_recompute_tester [--stored-depth=N] [--samples=N] [--threads=N]\n";
            std::exit(0);
        }
        if (const char* v = val_of("--stored-depth")) { a.stored_depth = std::max(0, std::atoi(v)); continue; }
        if (const char* v = val_of("--samples"))      { a.samples      = std::max(1, std::atoi(v)); continue; }
        if (const char* v = val_of("--threads"))      { a.threads      = std::max(0, std::atoi(v)); continue; }
    }
    return a;
}

} // namespace

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);

    // Configure globals for a small, deterministic test run
    suppress_indicators = true;                // silence progress bars
    stored_depth_max = static_cast<uint8_t>(args.stored_depth);
    target_T_count = static_cast<uint8_t>(std::max(args.stored_depth + 1, 1));
    if (args.threads > 0) THREADS = static_cast<uint8_t>(args.threads);

    // Build LUT rooted at identity
    LUT lut = algo::create_lookup_table(SO6::identity());

    // Gather elements from all finalized layers
    std::vector<SO6> pool;
    for (const auto& layer : lut) {
        pool.insert(pool.end(), layer.begin(), layer.end());
        if (static_cast<int>(pool.size()) >= args.samples) break; // fast path if early layers suffice
    }
    if (pool.empty()) {
        std::cerr << "No elements found in LUT (stored_depth_max=" << int(stored_depth_max) << ")\n";
        return 1;
    }

    // Shuffle and pick up to samples elements
    std::mt19937_64 rng(std::random_device{}());
    std::shuffle(pool.begin(), pool.end(), rng);
    if (static_cast<int>(pool.size()) > args.samples) pool.resize(static_cast<size_t>(args.samples));

    // Verify recompute_hash leaves signatures unchanged
    size_t tested = 0;
    size_t mismatches = 0;
    for (const SO6& s : pool) {
        SO6 copy = s;
        const uint16_t h0 = s.hash;
        const uint16_t ch0 = s.col_hash;
        copy.recompute_hash();
        if (copy.hash != h0 || copy.col_hash != ch0) {
            ++mismatches;
            std::cerr << "Mismatch after recompute: hash(" << h0 << "," << ch0
                      << ") -> (" << copy.hash << "," << copy.col_hash << ")\n";
            // continue to report more; keep it simple
        }
        ++tested;
    }

    std::cout << "Tested " << tested << " elements; mismatches=" << mismatches << "\n";
    return mismatches == 0 ? 0 : 2;
}
