#include <benchmark/benchmark.h>

#include <cstdint>
#include <random>
#include <vector>

#include "algo/Generate.hpp"
#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "sys/memory.hpp"

namespace {

constexpr uint8_t kTargetTCount = 8;
constexpr int kRandomRootLength = 64;
constexpr std::size_t kNumRandomRoots = 16;

SO6 make_long_random_root(std::mt19937_64& rng, int length) {
  std::uniform_int_distribution<int> t_dist(0, 14);
  SO6 cur = SO6::identity();
  for (int i = 0; i < length; ++i) {
    cur = T_OperatorRuntime(static_cast<uint8_t>(t_dist(rng))) * cur;
  }
  return cur;
}

const std::vector<SO6>& random_roots() {
  static std::vector<SO6> roots = [] {
    std::vector<SO6> out;
    out.reserve(kNumRandomRoots);
    std::mt19937_64 rng(1337);
    for (std::size_t i = 0; i < kNumRandomRoots; ++i) {
      out.push_back(make_long_random_root(rng, kRandomRootLength));
    }
    return out;
  }();
  return roots;
}

void configure_globals() {
  target_T_count = kTargetTCount;
  stored_depth_max = 0;
  verbose = false;
  suppress_indicators = true;
  Globals::configure();
}

void record_lut_metrics(benchmark::State& state,
                        std::uint64_t total_layers,
                        std::uint64_t total_lut_bytes) {
  const double iterations = static_cast<double>(state.iterations());
  const double denom = iterations > 0.0 ? iterations : 1.0;
  state.counters["lut_layers_avg"] = total_layers / denom;
  state.counters["lut_rss_bytes"] = total_lut_bytes / denom;
}

} // namespace

static void BM_BuildLUT_IdentityRoot(benchmark::State& state) {
  configure_globals();
  std::uint64_t total_layers = 0;
  std::uint64_t total_lut_bytes = 0;

  for (auto _ : state) {
    state.PauseTiming();
    const size_t rss_before = getProcessRSSBytes();
    state.ResumeTiming();

    auto lut = algo::create_lookup_table(SO6::identity(), nullptr, nullptr);

    state.PauseTiming();
    const size_t rss_after = getProcessRSSBytes();
    state.ResumeTiming();

    total_layers += lut.size();
    if (rss_after > rss_before) {
      total_lut_bytes += static_cast<std::uint64_t>(rss_after - rss_before);
    }

    benchmark::DoNotOptimize(lut.size());
    benchmark::ClobberMemory();
  }

  record_lut_metrics(state, total_layers, total_lut_bytes);
}

static void BM_BuildLUT_RandomLongRoot(benchmark::State& state) {
  configure_globals();
  const auto& roots = random_roots();
  std::size_t root_index = 0;
  std::uint64_t total_layers = 0;
  std::uint64_t total_lut_bytes = 0;

  for (auto _ : state) {
    const SO6& root = roots[root_index++ % roots.size()];

    state.PauseTiming();
    const size_t rss_before = getProcessRSSBytes();
    state.ResumeTiming();

    auto lut = algo::create_lookup_table(root, nullptr, nullptr);

    state.PauseTiming();
    const size_t rss_after = getProcessRSSBytes();
    state.ResumeTiming();

    total_layers += lut.size();
    if (rss_after > rss_before) {
      total_lut_bytes += static_cast<std::uint64_t>(rss_after - rss_before);
    }

    benchmark::DoNotOptimize(lut.size());
    benchmark::ClobberMemory();
  }

  record_lut_metrics(state, total_layers, total_lut_bytes);
}

BENCHMARK(BM_BuildLUT_IdentityRoot);
BENCHMARK(BM_BuildLUT_RandomLongRoot);
BENCHMARK_MAIN();
