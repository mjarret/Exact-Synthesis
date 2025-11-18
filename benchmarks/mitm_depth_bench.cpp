// Benchmark: MITM runtime vs target T-count using a near-optimal split.
//
// For a given target walk length (T-count), we generate a random target by
// applying that many T operators to the identity, then run a meet-in-the-middle
// search with per-side depth split ~= ceil(T/2). We record success rate and
// basic search stats. This is intended to complement mitm_bench.cpp which uses
// precomputed seeds; here we use fresh random targets to probe scaling.

#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <random>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "ds/MITM.hpp"

namespace {

SO6 random_target(std::mt19937_64& rng, int steps) {
    std::uniform_int_distribution<int> d(0, 14);
    SO6 cur = SO6::identity();
    for (int i = 0; i < steps; ++i) {
        cur = T_OperatorRuntime(static_cast<uint8_t>(d(rng))) * cur;
    }
    return cur;
}

// Sum elements across all layers (for a rough node count)
std::uint64_t total_elements(const LUT& lut) {
    std::uint64_t total = 0;
    for (const auto& layer : lut.layers()) total += static_cast<std::uint64_t>(layer.size());
    return total;
}

} // namespace

// Args:
//   arg0 = target T-count (walk length)
//   arg1 = RNG seed (optional; default 12345)
static void BM_MITM_DepthHeuristic(benchmark::State& state) {
    const int target_T = static_cast<int>(state.range(0));
    const std::uint64_t seed = (state.max_iterations == 0) ? 12345ull : 12345ull; // fixed for determinism

    // Heuristic per-side depth: ceil(target_T / 2)
    const int per_side = (target_T + 1) / 2;

    std::mt19937_64 rng(seed);
    suppress_indicators = true;  // no progress bars in benchmarks

    std::uint64_t successes = 0;
    std::uint64_t failures  = 0;
    std::uint64_t sum_left_nodes = 0;
    std::uint64_t sum_right_nodes = 0;

    for (auto _ : state) {
        stored_depth_max = static_cast<uint8_t>(per_side);

        SO6 target = random_target(rng, target_T);
        MITM mitm(SO6::identity(), target);
        auto meet_opt = generate_mitm_until_match(mitm);
        if (!meet_opt) {
            ++failures;
            continue;
        }

        ++successes;
        sum_left_nodes  += total_elements(mitm.left());
        sum_right_nodes += total_elements(mitm.right());

        benchmark::DoNotOptimize(mitm.left());
        benchmark::DoNotOptimize(mitm.right());
        benchmark::ClobberMemory();
    }

    const double total_runs = static_cast<double>(successes + failures);
    const double succ = successes ? static_cast<double>(successes) : 1.0;

    state.counters["runs"]          = total_runs;
    state.counters["successes"]     = static_cast<double>(successes);
    state.counters["failures"]      = static_cast<double>(failures);
    state.counters["success_rate"]  = successes / (total_runs > 0.0 ? total_runs : 1.0);
    state.counters["avg_left_nodes"]  = static_cast<double>(sum_left_nodes) / succ;
    state.counters["avg_right_nodes"] = static_cast<double>(sum_right_nodes) / succ;
}

// Representative T-counts. Adjust as desired.
BENCHMARK(BM_MITM_DepthHeuristic)
    ->Name("mitm/depth_heuristic")
    ->Arg(8)
    ->Arg(10)
    ->Arg(12)
    ->Arg(14)
    ->Arg(16);

BENCHMARK_MAIN();

