#include <benchmark/benchmark.h>
#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "algo/Generate.hpp"

static void BM_BuildLUT_T8(benchmark::State& state) {
  // Configure globals for a small, representative run
  target_T_count = 8;            // t = 8
  stored_depth_max = 0;          // let configure normalize to [ceil(t/2), t-1]
  verbose = false;
  suppress_indicators = true;    // also compiled with EXACT_DISABLE_INDICATORS in targets
  Globals::configure();

  for (auto _ : state) {
    // Build LUT from identity with no early stop predicate
    auto lut = algo::create_lookup_table(SO6::identity(), nullptr, nullptr);
    benchmark::DoNotOptimize(lut.size());
    benchmark::ClobberMemory();
  }
}

BENCHMARK(BM_BuildLUT_T8);
BENCHMARK_MAIN();

