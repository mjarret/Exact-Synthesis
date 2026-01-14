#include <benchmark/benchmark.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <mutex>
#include <random>
#include <set>
#include <thread>
#include <vector>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "algo/Generate.hpp"

namespace {

constexpr int kRandomRootSteps = 64;
constexpr benchmark::IterationCount kMinBatchIterations = 10;
constexpr double kMinSecondsPerBench = 2.0;

bool g_log_layers = false;
std::mutex g_log_mu;
std::set<int> g_logged_identity;
std::set<int> g_logged_random;

SO6 random_root(std::mt19937_64& rng, int steps) {
  std::uniform_int_distribution<int> d(0, 14);
  SO6 cur = SO6::identity();
  for (int i = 0; i < steps; ++i) {
    cur = T_OperatorRuntime(static_cast<uint8_t>(d(rng))) * cur;
  }
  return cur;
}

void configure_globals(int tcount) {
  target_T_count = tcount;
  stored_depth_max = 0;          // let configure normalize to [ceil(t/2), t-1]
  THREADS = std::thread::hardware_concurrency();
  verbose = false;
  suppress_indicators = true;    // also compiled with EXACT_DISABLE_INDICATORS in targets
  Globals::configure();
}

struct BenchArgs {
  int identity_max = 13;
  int random_max = 9;
  bool log_layers = false;
};

bool parse_positive_int(const char* s, int& out) {
  if (!s || *s == '\0') return false;
  char* end = nullptr;
  long v = std::strtol(s, &end, 10);
  if (!end || *end != '\0') return false;
  out = static_cast<int>(std::max<long>(1, v));
  return true;
}

BenchArgs parse_bench_args(int& argc, char** argv) {
  BenchArgs args;
  int write = 1;
  for (int i = 1; i < argc; ++i) {
    const char* a = argv[i];
    if (std::strcmp(a, "--log-layers") == 0) {
      args.log_layers = true;
      continue;
    }
    if (std::strncmp(a, "--identity-max=", 15) == 0) {
      parse_positive_int(a + 15, args.identity_max);
      continue;
    }
    if (std::strcmp(a, "--identity-max") == 0 && i + 1 < argc) {
      parse_positive_int(argv[++i], args.identity_max);
      continue;
    }
    if (std::strncmp(a, "--random-max=", 13) == 0) {
      parse_positive_int(a + 13, args.random_max);
      continue;
    }
    if (std::strcmp(a, "--random-max") == 0 && i + 1 < argc) {
      parse_positive_int(argv[++i], args.random_max);
      continue;
    }
    argv[write++] = argv[i];
  }
  argc = write;
  return args;
}

void log_lut_summary(const LUT& lut, int tcount, const char* label, std::set<int>& logged) {
  if (!g_log_layers) return;
  std::lock_guard<std::mutex> lk(g_log_mu);
  if (!logged.insert(tcount).second) return;

  std::size_t total = 0;
  std::vector<std::size_t> layers;
  for (const auto& layer : lut.layers()) {
    layers.push_back(layer.size());
    total += layer.size();
  }
  const std::size_t depth = layers.empty() ? 0 : (layers.size() - 1);

  std::cout << "[lut_build] root=" << label
            << " t=" << tcount
            << " depth=" << depth
            << " total=" << total
            << " layers=[";
  for (std::size_t i = 0; i < layers.size(); ++i) {
    if (i != 0) std::cout << ",";
    std::cout << layers[i];
  }
  std::cout << "]\n";
}

} // namespace

static void BM_BuildLUT_Identity(benchmark::State& state) {
  const int tcount = static_cast<int>(state.range(0));
  configure_globals(tcount);

  while (state.KeepRunningBatch(kMinBatchIterations)) {
    for (benchmark::IterationCount i = 0; i < kMinBatchIterations; ++i) {
      // Build LUT from identity with no early stop predicate
      auto lut = algo::create_lookup_table(SO6::identity(), nullptr, nullptr);
      if (g_log_layers) {
        state.PauseTiming();
        log_lut_summary(lut, tcount, "identity", g_logged_identity);
        state.ResumeTiming();
      }
      benchmark::DoNotOptimize(lut.size());
      benchmark::ClobberMemory();
    }
  }
}

static void BM_BuildLUT_RandomRoot(benchmark::State& state) {
  const int tcount = static_cast<int>(state.range(0));
  configure_globals(tcount);

  std::mt19937_64 rng(1337);
  SO6 root = random_root(rng, kRandomRootSteps);
  root.last_T = 15; // ensure the root does not skip one T-move

  while (state.KeepRunningBatch(kMinBatchIterations)) {
    for (benchmark::IterationCount i = 0; i < kMinBatchIterations; ++i) {
      // Build LUT from a long random T-walk root with no early stop predicate
      auto lut = algo::create_lookup_table(root, nullptr, nullptr);
      if (g_log_layers) {
        state.PauseTiming();
        log_lut_summary(lut, tcount, "random", g_logged_random);
        state.ResumeTiming();
      }
      benchmark::DoNotOptimize(lut.size());
      benchmark::ClobberMemory();
    }
  }
}

int main(int argc, char** argv) {
  BenchArgs args = parse_bench_args(argc, argv);
  g_log_layers = args.log_layers;

  benchmark::Initialize(&argc, argv);

  for (int t = 1; t <= args.identity_max; ++t) {
    benchmark::RegisterBenchmark("lut_build/identity", &BM_BuildLUT_Identity)
        ->Arg(t)
        ->MinTime(kMinSecondsPerBench)
        ->Unit(benchmark::kMillisecond);
  }
  for (int t = 1; t <= args.random_max; ++t) {
    benchmark::RegisterBenchmark("lut_build/random_root", &BM_BuildLUT_RandomRoot)
        ->Arg(t)
        ->MinTime(kMinSecondsPerBench)
        ->Unit(benchmark::kMillisecond);
  }

  benchmark::RunSpecifiedBenchmarks();
  benchmark::Shutdown();
  return 0;
}
