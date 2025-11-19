// Benchmark: DyadicSqrt2 vs native integers (32-bit) per-operation

#include <benchmark/benchmark.h>
#include <cstdint>
#include "DyadicSqrt2.hpp"

// Tune runtime per test
constexpr double kMinSecondsPerBench = 5.0;

// ---------------- DyadicSqrt2 operations ----------------

static void BM_Dyadic_AddAssign(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    DyadicSqrt2 b(3, 5, 1);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        DyadicSqrt2 y = b;
        for (int i = 0; i < 256; ++i) { x += y; benchmark::DoNotOptimize(x); }
    }
}

static void BM_Dyadic_SubAssign(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    DyadicSqrt2 b(3, 5, 1);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        DyadicSqrt2 y = b;
        for (int i = 0; i < 256; ++i) { x -= y; benchmark::DoNotOptimize(x); }
    }
}

static void BM_Dyadic_MulAssign(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    DyadicSqrt2 b(3, 5, 1);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        DyadicSqrt2 y = b;
        for (int i = 0; i < 256; ++i) { x *= y; benchmark::DoNotOptimize(x); }
    }
}

static void BM_Dyadic_ShiftLeft(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        for (int i = 0; i < 256; ++i) { x <<= 1; benchmark::DoNotOptimize(x); }
    }
}

static void BM_Dyadic_ShiftRight(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 8);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        for (int i = 0; i < 256; ++i) { x >>= 1; benchmark::DoNotOptimize(x); }
    }
}

// ---------------- Integer baselines (32-bit) ----------------

static void BM_Int32_AddAssign(benchmark::State& state) {
    int32_t a = 1234567, b = 89101112;
    for (auto _ : state) {
        int32_t x = a, y = b, acc = 0;
        for (int i = 0; i < 1024; ++i) { x += y; acc ^= x; }
        benchmark::DoNotOptimize(acc);
    }
}

static void BM_Int32_SubAssign(benchmark::State& state) {
    int32_t a = 1234567, b = 89101112;
    for (auto _ : state) {
        int32_t x = a, y = b, acc = 0;
        for (int i = 0; i < 1024; ++i) { x -= y; acc ^= x; }
        benchmark::DoNotOptimize(acc);
    }
}

static void BM_Int32_MulAssign(benchmark::State& state) {
    int32_t a = 1234, b = 5678;
    for (auto _ : state) {
        int32_t x = a, y = b, acc = 0;
        for (int i = 0; i < 1024; ++i) { x *= y; acc ^= x; }
        benchmark::DoNotOptimize(acc);
    }
}

static void BM_Int32_ShiftLeft(benchmark::State& state) {
    uint32_t a = 0x00FF00FFu;
    for (auto _ : state) {
        uint32_t x = a, acc = 0;
        for (int i = 0; i < 1024; ++i) { x <<= 1; acc ^= x; }
        benchmark::DoNotOptimize(acc);
    }
}

static void BM_Int32_ShiftRight(benchmark::State& state) {
    uint32_t a = 0xFF00FF00u;
    for (auto _ : state) {
        uint32_t x = a, acc = 0;
        for (int i = 0; i < 1024; ++i) { x >>= 1; acc ^= x; }
        benchmark::DoNotOptimize(acc);
    }
}

// ---------------- Register benches ----------------

BENCHMARK(BM_Dyadic_AddAssign)->Name("Dyadic/AddAssign")->MinTime(kMinSecondsPerBench);
BENCHMARK(BM_Int32_AddAssign)->Name("Int32/AddAssign")->MinTime(kMinSecondsPerBench);

BENCHMARK(BM_Dyadic_SubAssign)->Name("Dyadic/SubAssign")->MinTime(kMinSecondsPerBench);
BENCHMARK(BM_Int32_SubAssign)->Name("Int32/SubAssign")->MinTime(kMinSecondsPerBench);

BENCHMARK(BM_Dyadic_MulAssign)->Name("Dyadic/MulAssign")->MinTime(kMinSecondsPerBench);
BENCHMARK(BM_Int32_MulAssign)->Name("Int32/MulAssign")->MinTime(kMinSecondsPerBench);

BENCHMARK(BM_Dyadic_ShiftLeft)->Name("Dyadic/ShiftLeft")->MinTime(kMinSecondsPerBench);
BENCHMARK(BM_Int32_ShiftLeft)->Name("Int32/ShiftLeft")->MinTime(kMinSecondsPerBench);

BENCHMARK(BM_Dyadic_ShiftRight)->Name("Dyadic/ShiftRight")->MinTime(kMinSecondsPerBench);
BENCHMARK(BM_Int32_ShiftRight)->Name("Int32/ShiftRight")->MinTime(kMinSecondsPerBench);

BENCHMARK_MAIN();

