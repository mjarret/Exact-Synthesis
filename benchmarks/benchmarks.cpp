/**
 * \file benchmarks.cpp
 * \brief Benchmarks comparing raw integer addition vs. project Z2 addition.
 *
 * Benchmarks
 * - RawAdd: baseline uint16_t additions
 * - Z2Add:  project Z2::operator+= (baseline approach)
 *
 * Build
 * - cd benchmarks && make
 *
 * Run
 * - BENCH_THREADS=1 ./benchmarks.out --rel_error=0.02 --delta=0.001
 * - Optional dataset: --dataset=8192
 * - Any Google Benchmark flags may be passed
 *
 * Notes
 * - Repetitions derived from Hoeffding-style bounds (rel_error, delta)
 * - Per-iteration setup excluded via PauseTiming/ResumeTiming
 */
#include <vector>
#include <random>
#include <benchmark/benchmark.h>
#include <cstdint>
#include <thread>
#include <cstdlib>   // getenv, atoi, atof
#include <cstring>   // strncmp
#include <cmath>     // log, ceil
#include "Z2.hpp"

// === Error-bound driven repetitions (Hoeffding-style) ===
// If X in [0,1], Hoeffding: P(|X̄ - E[X]| >= eps) <= 2 exp(-2 n eps^2)
// n >= (1/(2 eps^2)) * ln(2/delta)
static double g_rel_error = 0.05;   // default relative error eps
static double g_delta     = 0.005;  // default failure prob delta
static int    g_dataset   = 8192;   // default problem size argument

inline int CalculateHoeffdingRepetitions(double rel_error, double delta) {
    if (rel_error <= 0.0) rel_error = 1e-6;
    if (delta <= 0.0) delta = 1e-9;
    const double n = (std::log(2.0 / delta)) / (2.0 * rel_error * rel_error);
    const int reps = static_cast<int>(std::ceil(n));
    return std::max(3, reps);
}

// Generate random Z2 values by seeding the underlying 32-bit storage
std::vector<Z2> generateRandomZ2(size_t count) {
    std::vector<Z2> data;
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, -1); // Full 32-bit range
    for (size_t i = 0 ; i < count; i++) {data.push_back(Z2(dist(rng)));}
    return data;
}


// Z2 addition benchmark: Baseline (uses Z2::operator+=)
// The inner loop performs 1000 dependent adds to stay in L1 and exercise carry/normalize paths.
static void Benchmark_Z2Add(benchmark::State& state) {
    size_t dataSize = state.range(0);
    Z2 left(static_cast<uint32_t>(0));
    Z2 right(static_cast<uint32_t>(0));
    auto xValues = generateRandomZ2(dataSize);

    for (auto _ : state) {
        state.PauseTiming(); // Pause timing for setup
            right = xValues[state.iterations() % xValues.size()];
        state.ResumeTiming(); // Resume timing for the actual addition
        #pragma unroll
        for(size_t i = 0; i < 1000; ++i)
            benchmark::DoNotOptimize(left += right);
    }
}

//

// Base-√2 variants

// Removed Hoeffding/Chernoff helpers: rely on Google Benchmark's
// native MinTime and Repetitions to achieve statistical stability.

// Aggressive configuration for longer runtime/precision
// Set BENCH_THREADS env var (e.g., BENCH_THREADS=1) to override thread count.
void BenchmarkPrime(benchmark::internal::Benchmark* b) {
    int mt = static_cast<int>(std::thread::hardware_concurrency()) - 2;
    if (const char* env = std::getenv("BENCH_THREADS")) {
        int v = std::atoi(env);
        if (v > 0) mt = v;
    }
    if (mt < 1) mt = 1;
    b->Threads(mt)
     ->ArgName("dataset")
     ->Arg(g_dataset)
     ->ReportAggregatesOnly(true);
}

// Default configuration: a balance between runtime and stability
// Set BENCH_THREADS env var (e.g., BENCH_THREADS=1) to override thread count.
void BenchmarkConfig(benchmark::internal::Benchmark* b) {
    int mt = static_cast<int>(std::thread::hardware_concurrency()) - 2;
    if (const char* env = std::getenv("BENCH_THREADS")) {
        int v = std::atoi(env);
        if (v > 0) mt = v;
    }
    if (mt < 1) mt = 1;
    b->Threads(mt)
     ->ArgName("dataset")
     ->Arg(g_dataset)
     ->ReportAggregatesOnly(true);
}


// Local helper for raw uint16_t benchmark
std::vector<uint16_t> generateRandomUint16(size_t count) {
    std::vector<uint16_t> data(count);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint16_t> dist(0, 65535);
    for (auto &val : data) {
        val = dist(rng);
    }
    return data;
}

// Baseline: add a random 8-bit value into a 16-bit accumulator.
// Measures core add throughput with minimal structure/branching.
static void Benchmark_RawAdd(benchmark::State& state) {
    size_t dataSize = state.range(0);
    auto yValues = generateRandomUint16(dataSize);   
    uint_fast16_t left = 0;
    for (auto _ : state) {
        state.PauseTiming(); // Pause timing for setup
        uint_fast8_t right = yValues[state.iterations() % yValues.size()];
        state.ResumeTiming(); // Resume timing for the actual addition
        #pragma unroll 
        for (size_t i = 0; i < 1000; ++i) {
            benchmark::DoNotOptimize(left += right);
        }
    }
}

//

// Registered benchmarks and default configuration
BENCHMARK(Benchmark_RawAdd)->Apply(BenchmarkConfig);
BENCHMARK(Benchmark_Z2Add)->Apply(BenchmarkConfig);
// Custom main to parse theorist-friendly error bounds from CLI
int main(int argc, char** argv) {
    std::vector<char*> forward_args;
    std::vector<std::string> owned_args; // keep dynamically created flags alive
    forward_args.reserve(argc + 1);
    forward_args.push_back(argv[0]);

    for (int i = 1; i < argc; ++i) {
        const char* s = argv[i];
        if (std::strncmp(s, "--rel_error=", 12) == 0) {
            g_rel_error = std::atof(s + 12);
            continue;
        }
        if (std::strncmp(s, "--delta=", 8) == 0) {
            g_delta = std::atof(s + 8);
            continue;
        }
        if (std::strncmp(s, "--dataset=", 10) == 0) {
            g_dataset = std::atoi(s + 10);
            if (g_dataset <= 0) g_dataset = 8192;
            continue;
        }
        // Pass all other args through to Google Benchmark
        forward_args.push_back(argv[i]);
    }

    // Inject derived repetitions for Google Benchmark unless user supplied their own
    bool user_set_reps = false;
    for (char* a : forward_args) {
        if (!a) continue;
        if (std::strncmp(a, "--benchmark_repetitions=", 24) == 0) { user_set_reps = true; break; }
    }
    if (!user_set_reps) {
        const int reps = CalculateHoeffdingRepetitions(g_rel_error, g_delta);
        owned_args.emplace_back(std::string("--benchmark_repetitions=") + std::to_string(reps));
        forward_args.insert(forward_args.begin() + 1, const_cast<char*>(owned_args.back().c_str()));
    }

    int new_argc = static_cast<int>(forward_args.size());
    forward_args.push_back(nullptr);
    benchmark::Initialize(&new_argc, forward_args.data());
    if (benchmark::ReportUnrecognizedArguments(new_argc, forward_args.data())) return 1;
    return benchmark::RunSpecifiedBenchmarks();
}
