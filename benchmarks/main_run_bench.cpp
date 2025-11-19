#include <benchmark/benchmark.h>

#include <cstdlib>

// Benchmarks that repeatedly run main compiled with the Z2 backend vs the
// DyadicSqrt2 backend. Both binaries are expected in the CWD:
//   ./main_z2     (built with NUMERIC=z2)
//   ./main_dyadic (built with NUMERIC=dyadic)
//
// Each run uses "-t 11" so Globals::setParameters/configure are exercised in
// exactly the same way as a normal main invocation.

namespace {

constexpr const char* kCmdZ2     = "./main_z2 -t 11 --no-indicators 1 > /dev/null 2>&1";
constexpr const char* kCmdDyadic = "./main_dyadic -t 11 --no-indicators 1 > /dev/null 2>&1";

} // namespace

static void BM_Main_Z2_T11(benchmark::State& state) {
    for (auto _ : state) {
        int rc = std::system(kCmdZ2);
        benchmark::DoNotOptimize(rc);
    }
}

static void BM_Main_Dyadic_T11(benchmark::State& state) {
    for (auto _ : state) {
        int rc = std::system(kCmdDyadic);
        benchmark::DoNotOptimize(rc);
    }
}

constexpr double kMainMinSeconds = 10; // at least 100s

BENCHMARK(BM_Main_Z2_T11)
    ->Name("main/Z2_t11")
    ->MinTime(kMainMinSeconds);

BENCHMARK(BM_Main_Dyadic_T11)
    ->Name("main/DyadicSqrt2_t11")
    ->MinTime(kMainMinSeconds);

BENCHMARK_MAIN();
