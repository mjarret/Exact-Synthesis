// Benchmark: MITM search on random SO6 targets, built on-the-fly.
//
// What this file does (high level):
//   * Builds a small, fixed set of random "seeds" per search depth. Each seed
//     is just a short T-word applied to the identity to obtain a target SO6.
//   * For each seed, runs either:
//       - the pure MITM/LUT strategy (`generate_mitm_until_match`), or
//       - the hybrid strategy (LUT up to a threshold, then brute-force T-chains
//         from the smaller frontier into the opposite LUT).
//   * Google Benchmark is only measuring the MITM search itself; seed creation
//     and random number work happen once per depth and are not in the hot loop.
//
// There is no separate seed generator binary and no seed file on disk; both
// benchmark variants share the exact same in-memory seeds for a fair comparison.

#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <random>
#include <string>
#include <vector>
#include <optional>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "ds/MITM.hpp"
#include "algo/Generate.hpp"

namespace {

// A single random T-walk used to define a target.
struct Seed {
    int walk_len{0};
    // T sequence used to build the target from identity. Each entry is a
    // T-operator index in [0, 15], with the property that no two consecutive
    // operators are the same (we enforce the "forbid last_T" rule).
    std::vector<uint8_t> walk;
};

SO6 target_from_seed(const Seed& s) {
    SO6 cur = SO6::identity();
    for (uint8_t t : s.walk) {
        cur = T_OperatorRuntime(t) * cur;
    }
    return cur;
}

std::uint64_t total_elements(const LUT& lut) {
    std::uint64_t total = 0;
    for (const auto& layer : lut.layers()) {
        total += static_cast<std::uint64_t>(layer.size());
    }
    return total;
}

// Precomputed seed pools per search depth so that:
//  - Seed generation cost is not benchmarked (done once per depth), and
//  - Both strategies (pure and hybrid) share the exact same targets.
struct DepthSeedPool {
    int depth{0};
    std::vector<Seed> seeds;
};

constexpr int kNumSeedsPerDepth = 64;

const std::vector<Seed>& seeds_for_depth(int search_depth) {
    static std::vector<DepthSeedPool> pools;
    // Find existing pool for this depth, if any.
    for (const auto& p : pools) {
        if (p.depth == search_depth) return p.seeds;
    }

    // Build a new pool for this depth.
    DepthSeedPool pool;
    pool.depth = search_depth;
    pool.seeds.reserve(kNumSeedsPerDepth);

    static std::mt19937_64 rng(1337);
    // Walk lengths are between 2 and search_depth so that the target is
    // reachable but still non-trivial.
    std::uniform_int_distribution<int> walk_len_dist(2, std::max(2, search_depth));
    std::uniform_int_distribution<int> t_digit_dist(0, 13); // base-14, skip via "forbid" rule

    for (int i = 0; i < kNumSeedsPerDepth; ++i) {
        Seed s;
        s.walk_len = walk_len_dist(rng);
        s.walk.resize(static_cast<std::size_t>(s.walk_len));

        uint8_t last_t = 15; // sentinel "no forbidden T" for first step
        for (int j = 0; j < s.walk_len; ++j) {
            int d = t_digit_dist(rng); // base-14 digit in [0, 13]
            uint8_t t = static_cast<uint8_t>(d + (d >= last_t ? 1u : 0u));
            s.walk[static_cast<std::size_t>(j)] = t;
            last_t = t;
        }
        pool.seeds.push_back(std::move(s));
    }

    pools.push_back(std::move(pool));
    return pools.back().seeds;
}

// Hybrid MITM strategy:
//  1) Alternate LUT expansions as in generate_mitm_until_match up to
//     switch_depth per side (or until a hit).
//  2) Once both sides have at least switch_depth finalized layers, freeze
//     the LUTs and run a brute T-word search from the side with fewer leaves,
//     checking membership against the opposing LUT on-the-fly.
std::optional<SO6> generate_mitm_hybrid(MITM& mitm, int switch_depth) {
    LUT& left  = mitm.left();
    LUT& right = mitm.right();
    SO6 meet{};

    // Phase 1: normal alternating expansions up to switch_depth per side.
    for (int depth = 0; depth < 2 * switch_depth; ++depth) {
        bool expand_left = left.current().size() <= right.current().size();

        bool hit = false;
        LUT& active  = expand_left ? left  : right;
        LUT& passive = expand_left ? right : left;

        auto pred = [&](const SO6& s) {
            bool r = (passive.back().find(s) != passive.back().end());
            if (r) hit = true;
            return r;
        };

        algo::get_next_T_count(active, nullptr, pred, &meet);
        active.finalize_current_set(nullptr);

        if (hit) return meet;
        if (mitm.depth_left() >= switch_depth && mitm.depth_right() >= switch_depth) break;
    }

    // Phase 2: brute T-word search from the smaller side against the static
    // opposing LUT. We only need to search in one direction.
    LUT& small       = (left.current().size() <= right.current().size()) ? left : right;
    const LUT& other = (&small == &left) ? right : left;

    // Remaining depth budget per side is limited by stored_depth_max.
    const int max_remaining = static_cast<int>(stored_depth_max) - std::max(mitm.depth_left(), mitm.depth_right());
    if (max_remaining <= 0) return std::nullopt;

    // For each leaf in the smaller frontier, explore all admissible T-words
    // of length 1..max_remaining and check for membership in the other LUT.
    const auto& leaves = small.current();
    for (const auto& leaf : leaves) {
        const uint8_t last_T = leaf.last_T;

        for (int depth = 1; depth <= max_remaining; ++depth) {
            std::size_t count = 1;
            for (int i = 0; i < depth; ++i) count *= 14u;

            for (std::size_t code = 0; code < count; ++code) {
                SO6 cur = leaf;
                std::size_t x = code;
                uint8_t forbid = last_T;
                for (int pos = 0; pos < depth; ++pos) {
                    uint8_t d = static_cast<uint8_t>(x % 14u);
                    x /= 14u;
                    uint8_t t = static_cast<uint8_t>(d + (d >= forbid ? 1u : 0u));
                    cur = T_OperatorRuntime(t, false) * cur;
                    forbid = t;
                }
                if (other.find(cur) != other.end()) {
                    return cur;
                }
            }
        }
    }

    return std::nullopt;
}

} // namespace

// Args:
//   arg0 = search_depth  (per-side MITM depth limit; should be >= seed search depth)
static void BM_MITM_Seeds(benchmark::State& state) {
    const int search_depth = static_cast<int>(state.range(0));

    suppress_indicators = true; // avoid interactive progress bars in benchmarks
    stored_depth_max = static_cast<uint8_t>(search_depth);

    // Precompute a shared seed pool for this depth (used by both strategies).
    const auto& seeds = seeds_for_depth(search_depth);

    std::uint64_t successes = 0;
    std::uint64_t failures  = 0;
    std::uint64_t sum_actual_depth = 0;
    std::uint64_t sum_dl = 0;
    std::uint64_t sum_dr = 0;
    std::uint64_t sum_walk_len = 0;
    std::uint64_t sum_left_nodes = 0;
    std::uint64_t sum_right_nodes = 0;

    for (auto _ : state) {
        // Process the entire seed pool every benchmark iteration so that
        // each iteration does identical work and changing benchmark_min_time
        // only affects the number of repetitions, not the mix of seeds.
        for (const Seed& s : seeds) {
            SO6 target = target_from_seed(s);

            MITM mitm(SO6::identity(), target);
            auto meet_opt = generate_mitm_until_match(mitm);
            if (!meet_opt) {
                ++failures;
                continue;
            }

            const SO6& meet = *meet_opt;
            auto left_path_opt  = mitm.left().path_to(meet);
            auto right_path_opt = mitm.right().path_to(meet);
            if (!left_path_opt || !right_path_opt) {
                ++failures;
                continue;
            }

            const int dl = static_cast<int>(left_path_opt->size());
            const int dr = static_cast<int>(right_path_opt->size());
            const int actual_depth = dl + dr;

            sum_actual_depth += static_cast<std::uint64_t>(actual_depth);
            sum_dl           += static_cast<std::uint64_t>(dl);
            sum_dr           += static_cast<std::uint64_t>(dr);
            sum_walk_len     += static_cast<std::uint64_t>(s.walk.size());
            sum_left_nodes   += total_elements(mitm.left());
            sum_right_nodes  += total_elements(mitm.right());
            ++successes;

            benchmark::DoNotOptimize(mitm.left());
            benchmark::DoNotOptimize(mitm.right());
            benchmark::DoNotOptimize(meet);
            benchmark::ClobberMemory();
        }
    }

    const double total_runs = static_cast<double>(successes + failures);
    const double succ = successes ? static_cast<double>(successes) : 1.0;

    state.counters["runs"]            = total_runs;
    state.counters["successes"]       = static_cast<double>(successes);
    state.counters["failures"]        = static_cast<double>(failures);
    state.counters["success_rate"]    = successes / (total_runs > 0.0 ? total_runs : 1.0);
    state.counters["avg_actual_d"]    = static_cast<double>(sum_actual_depth) / succ;
    state.counters["avg_walk_len"]    = static_cast<double>(sum_walk_len) / succ;
    state.counters["avg_dl"]          = static_cast<double>(sum_dl) / succ;
    state.counters["avg_dr"]          = static_cast<double>(sum_dr) / succ;
    state.counters["avg_left_nodes"]  = static_cast<double>(sum_left_nodes) / succ;
    state.counters["avg_right_nodes"] = static_cast<double>(sum_right_nodes) / succ;
}

// Hybrid strategy benchmark: same seeds, but use generate_mitm_hybrid with a
// switch depth (here chosen as roughly half the per-side search depth).
static void BM_MITM_Seeds_Hybrid(benchmark::State& state) {
    const int search_depth  = static_cast<int>(state.range(0));
    const int switch_depth  = std::max(1, search_depth /2);

    suppress_indicators = true;
    stored_depth_max = static_cast<uint8_t>(search_depth);

    const auto& seeds = seeds_for_depth(search_depth);

    std::uint64_t successes = 0;
    std::uint64_t failures  = 0;
    std::uint64_t sum_actual_depth = 0;
    std::uint64_t sum_dl = 0;
    std::uint64_t sum_dr = 0;
    std::uint64_t sum_walk_len = 0;
    std::uint64_t sum_left_nodes = 0;
    std::uint64_t sum_right_nodes = 0;

    for (auto _ : state) {
        for (const Seed& s : seeds) {
            SO6 target = target_from_seed(s);

            MITM mitm(SO6::identity(), target);
            auto meet_opt = generate_mitm_hybrid(mitm, switch_depth);
            if (!meet_opt) {
                ++failures;
                continue;
            }

            const SO6& meet = *meet_opt;
            auto left_path_opt  = mitm.left().path_to(meet);
            auto right_path_opt = mitm.right().path_to(meet);
            if (!left_path_opt || !right_path_opt) {
                ++failures;
                continue;
            }

            const int dl = static_cast<int>(left_path_opt->size());
            const int dr = static_cast<int>(right_path_opt->size());
            const int actual_depth = dl + dr;

            sum_actual_depth += static_cast<std::uint64_t>(actual_depth);
            sum_dl           += static_cast<std::uint64_t>(dl);
            sum_dr           += static_cast<std::uint64_t>(dr);
            sum_walk_len     += static_cast<std::uint64_t>(s.walk.size());
            sum_left_nodes   += total_elements(mitm.left());
            sum_right_nodes  += total_elements(mitm.right());
            ++successes;

            benchmark::DoNotOptimize(mitm.left());
            benchmark::DoNotOptimize(mitm.right());
            benchmark::DoNotOptimize(meet);
            benchmark::ClobberMemory();
        }
    }

    const double total_runs = static_cast<double>(successes + failures);
    const double succ = successes ? static_cast<double>(successes) : 1.0;

    state.counters["runs"]            = total_runs;
    state.counters["successes"]       = static_cast<double>(successes);
    state.counters["failures"]        = static_cast<double>(failures);
    state.counters["success_rate"]    = successes / (total_runs > 0.0 ? total_runs : 1.0);
    state.counters["avg_actual_d"]    = static_cast<double>(sum_actual_depth) / succ;
    state.counters["avg_walk_len"]    = static_cast<double>(sum_walk_len) / succ;
    state.counters["avg_dl"]          = static_cast<double>(sum_dl) / succ;
    state.counters["avg_dr"]          = static_cast<double>(sum_dr) / succ;
    state.counters["avg_left_nodes"]  = static_cast<double>(sum_left_nodes) / succ;
    state.counters["avg_right_nodes"] = static_cast<double>(sum_right_nodes) / succ;
}

// A few representative search depths (per side).
BENCHMARK(BM_MITM_Seeds)
    ->Name("mitm/seeds")
    ->Arg(8)
    ->Arg(10)
    ->Arg(12)
    ->Arg(14);

BENCHMARK(BM_MITM_Seeds_Hybrid)
    ->Name("mitm/seeds_hybrid")
    ->Arg(8)
    ->Arg(10)
    ->Arg(12)
    ->Arg(14);

BENCHMARK_MAIN();
