#include <benchmark/benchmark.h>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "ds/LUT.hpp"
#include "algo/Generate.hpp"

namespace {

// Build a LUT once up to depth 10 and pick a single leaf.
struct SweepFixture {
    LUT lut;
    SO6 leaf;

    SweepFixture() {
        // Configure globals: we want depth 10 stored.
        target_T_count = 20;          // anything >= 2*depth; configure() will normalize, but we bypass it here.
        stored_depth_max = 10;
        verbose = false;
        suppress_indicators = true;

        lut = algo::create_lookup_table(SO6::identity(), nullptr, nullptr);

        // Choose a single leaf from the deepest stored layer if available,
        // otherwise fall back to identity so the benchmark is still valid.
        if (!lut.back().empty()) {
            leaf = *lut.back().begin();
        } else {
            leaf = SO6::identity();
        }
    }
};

} // namespace

static void BM_LUT_Sweep_RightMultiplyLeaf(benchmark::State& state) {
    static SweepFixture fixture; // constructed once, reused across iterations

    std::uint64_t checksum = 0;
    for (auto _ : state) {
        checksum = 0;
        for (const auto& m : fixture.lut.elements()) {
            SO6 prod = m * fixture.leaf;
            benchmark::DoNotOptimize(prod);
            checksum ^= prod.hash;
        }
        benchmark::ClobberMemory();
    }

    state.counters["matrices"] = static_cast<double>(
        std::distance(fixture.lut.elements().begin(), fixture.lut.elements().end()));
    state.counters["checksum"] = static_cast<double>(checksum);
}

BENCHMARK(BM_LUT_Sweep_RightMultiplyLeaf)->Unit(benchmark::kMillisecond);
BENCHMARK_MAIN();

