/**
 * LUT history tester:
 *  - Build a LUT to a requested T-depth from identity.
 *  - Brute-force over every element in the LUT.
 *  - Recover each T-path via LUT::path_to and re-apply it to verify the matrix matches.
 *  - Emits a lightweight progress indicator while verifying.
 */

#include <iostream>
#include <random>
#include <vector>
#include <optional>
#include <algorithm>
#include <cstring>
#include <unordered_set>
#include <atomic>
#include <mutex>
#include <tbb/parallel_for_each.h>
#include <tbb/blocked_range.h>
#include <tbb/global_control.h>
#include <memory>

#include "config/Globals.hpp"
#include "ds/LUT.hpp"
#include "algo/Generate.hpp"
#include "so6/T_Operator.hpp"

namespace {

struct Args {
    int depth = 4;
    uint64_t seed = 0; // unused now (kept for compatibility)
    int threads = 0;
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
            std::cout << "Usage: lut_history_tester [--depth=N] [--seed=U64] [--threads=N]\n";
            std::exit(0);
        }
        if (auto* v = val("--depth"))   a.depth   = std::max(1, std::atoi(v));
        else if (auto* v = val("--seed"))    a.seed    = std::strtoull(v, nullptr, 10);
        else if (auto* v = val("--threads")) a.threads = std::max(0, std::atoi(v));
    }
    return a;
}

SO6 rebuild_from_path(const SO6& root, const std::vector<uint8_t>& path) {
    SO6 cur = root;
    for (uint8_t t : path) cur = T_OperatorRuntime(t) * cur;
    return cur;
}

} // namespace

SO6 random_root(std::mt19937_64& rng, int steps = 7) {
    std::uniform_int_distribution<int> d(0, 14);
    SO6 cur = SO6::identity();
    for (int i = 0; i < steps; ++i) {
        cur = T_OperatorRuntime(static_cast<uint8_t>(d(rng))) * cur;
    }
    return cur;
}

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);

    suppress_indicators = true;
    stored_depth_max = static_cast<uint8_t>(args.depth);
    target_T_count = static_cast<uint8_t>(args.depth + 1);
    if (args.threads > 0) THREADS = static_cast<uint8_t>(args.threads);

    std::mt19937_64 rng(args.seed ? args.seed : std::random_device{}());

    auto run_suite = [&](const SO6& root, const char* label){
        std::cout << "Building LUT to depth " << args.depth << " (root=" << label << ")...\n";
        LUT lut = algo::create_lookup_table(root);
        const SO6 lut_root = lut.root();

        // Count total elements for progress.
        size_t total = 0;
        for (auto it = lut.layers_begin(); it != lut.layers_end(); ++it) total += it->size();
        if (total == 0) { std::cerr << "[error] LUT is empty (root=" << label << ")\n"; return 1; }

        std::atomic<size_t> ok{0}, failures{0}, processed{0};
        std::atomic<int> logged{0};
        const int log_limit = 5;
        std::mutex log_mu;

        const size_t progress_interval = std::max<size_t>(1, total / 100); // ~1% updates
        std::atomic<size_t> next_progress{progress_interval};

        // Limit TBB worker count if requested.
        std::unique_ptr<tbb::global_control> ctrl;
        if (args.threads > 0) {
            ctrl = std::make_unique<tbb::global_control>(
                tbb::global_control::max_allowed_parallelism,
                static_cast<size_t>(args.threads));
        }

        tbb::parallel_for_each(lut.begin(), lut.end(), [&](const SO6& s) {
            auto path = lut.path_to(s); // expected to succeed; treat null as fatal
            if (!path) {
                failures.fetch_add(1, std::memory_order_relaxed);
                if (logged.fetch_add(1, std::memory_order_relaxed) < log_limit) {
                    std::lock_guard<std::mutex> lk(log_mu);
                    std::cerr << "[fatal] no path (hash=" << s.hash << ", col_hash=" << s.col_hash << ")\n";
                }
            } else {
                SO6 rebuilt = rebuild_from_path(lut_root, *path);
                if (rebuilt == s) {
                    ok.fetch_add(1, std::memory_order_relaxed);
                } else {
                    failures.fetch_add(1, std::memory_order_relaxed);
                    if (logged.fetch_add(1, std::memory_order_relaxed) < log_limit) {
                        std::lock_guard<std::mutex> lk(log_mu);
                        std::cerr << "[mismatch] path_len=" << path->size()
                                  << " last_T=" << static_cast<int>(s.last_T)
                                  << " hash=" << s.hash << " col_hash=" << s.col_hash << "\n";
                    }
                }
            }

            size_t done = processed.fetch_add(1, std::memory_order_relaxed) + 1;
            if (done >= next_progress.load(std::memory_order_relaxed)) {
                std::lock_guard<std::mutex> lk(log_mu);
                double pct = (static_cast<double>(done) / static_cast<double>(total)) * 100.0;
                std::cout << "\rVerifying " << done << "/" << total << " (" << static_cast<int>(pct) << "%)..." << std::flush;
                next_progress.store(std::min(total, done + progress_interval), std::memory_order_relaxed);
            }
        });

        std::cout << "\rVerifying " << total << "/" << total << " (100%)... done\n";
        std::cout << "Checked: " << total << ", ok: " << ok.load() << ", failures: " << failures.load() << "\n";
        return failures.load() == 0 ? 0 : 7;
    };

    int rc1 = run_suite(SO6::identity(), "identity");
    int rc2 = run_suite(random_root(rng), "random");
    return (rc1 == 0 && rc2 == 0) ? 0 : 7;
}
