#include <benchmark/benchmark.h>

#include "Z2.hpp"
#include "DyadicSqrt2.hpp"

// Per-operation and mixed microbenchmarks to compare Z2 vs DyadicSqrt2.

// ---- Per-operation benches: isolate a single arithmetic op in the inner loop ----

static void BM_Z2_AddAssign(benchmark::State& state) {
    Z2 a(1, 0, 0);
    Z2 b(3, 5, 1);
    for (auto _ : state) {
        Z2 x = a;
        Z2 y = b;
        for (int i = 0; i < 256; ++i) {
            x += y;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Dyadic_AddAssign(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    DyadicSqrt2 b(3, 5, 1);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        DyadicSqrt2 y = b;
        for (int i = 0; i < 256; ++i) {
            x += y;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Z2_SubAssign(benchmark::State& state) {
    Z2 a(1, 0, 0);
    Z2 b(3, 5, 1);
    for (auto _ : state) {
        Z2 x = a;
        Z2 y = b;
        for (int i = 0; i < 256; ++i) {
            x -= y;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Dyadic_SubAssign(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    DyadicSqrt2 b(3, 5, 1);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        DyadicSqrt2 y = b;
        for (int i = 0; i < 256; ++i) {
            x -= y;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Z2_MulAssign(benchmark::State& state) {
    Z2 a(1, 0, 0);
    Z2 b(3, 5, 1);
    for (auto _ : state) {
        Z2 x = a;
        Z2 y = b;
        for (int i = 0; i < 256; ++i) {
            x *= y;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Dyadic_MulAssign(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    DyadicSqrt2 b(3, 5, 1);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        DyadicSqrt2 y = b;
        for (int i = 0; i < 256; ++i) {
            x *= y;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Z2_ShiftLeft(benchmark::State& state) {
    Z2 a(1, 0, 0);
    for (auto _ : state) {
        Z2 x = a;
        for (int i = 0; i < 256; ++i) {
            x <<= 1;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Dyadic_ShiftLeft(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        for (int i = 0; i < 256; ++i) {
            x <<= 1;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Z2_ShiftRight(benchmark::State& state) {
    Z2 a(1, 0, 4); // give it some exponent headroom
    for (auto _ : state) {
        Z2 x = a;
        for (int i = 0; i < 256; ++i) {
            x >>= 1;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Dyadic_ShiftRight(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 4);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        for (int i = 0; i < 256; ++i) {
            x >>= 1;
            benchmark::DoNotOptimize(x);
        }
    }
}

// ---- Additional benches for pieces used in the Mixed test ----

static void BM_Z2_Reduce(benchmark::State& state) {
    Z2 a(1, 0, 4);
    for (auto _ : state) {
        Z2 x = a;
        for (int i = 0; i < 256; ++i) {
            x.reduce();
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Dyadic_Reduce(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 4);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        for (int i = 0; i < 256; ++i) {
            x.reduce();
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Z2_Abs(benchmark::State& state) {
    Z2 a(1, 0, 0);
    for (auto _ : state) {
        Z2 x = a;
        for (int i = 0; i < 256; ++i) {
            Z2 ax = std::abs(x);
            benchmark::DoNotOptimize(ax);
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Dyadic_Abs(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        for (int i = 0; i < 256; ++i) {
            DyadicSqrt2 ax = std::abs(x);
            benchmark::DoNotOptimize(ax);
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Z2_Negate(benchmark::State& state) {
    Z2 a(1, 0, 0);
    for (auto _ : state) {
        Z2 x = a;
        for (int i = 0; i < 256; ++i) {
            x = -x;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Dyadic_Negate(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        for (int i = 0; i < 256; ++i) {
            x = -x;
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Z2_Compare(benchmark::State& state) {
    Z2 a(1, 0, 0);
    Z2 b(3, 5, 1);
    for (auto _ : state) {
        Z2 x = a;
        Z2 y = b;
        for (int i = 0; i < 256; ++i) {
            bool eq = (x == y);
#if __cpp_impl_three_way_comparison
            auto ord = (x <=> y);
            bool lt = (ord == std::strong_ordering::less);
#else
            bool lt = (x < y);
#endif
            benchmark::DoNotOptimize(eq);
            benchmark::DoNotOptimize(lt);
            benchmark::DoNotOptimize(x);
            benchmark::DoNotOptimize(y);
        }
    }
}

static void BM_Dyadic_Compare(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    DyadicSqrt2 b(3, 5, 1);
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        DyadicSqrt2 y = b;
        for (int i = 0; i < 256; ++i) {
            bool eq = (x == y);
#if __cpp_impl_three_way_comparison
            auto ord = (x <=> y);
            bool lt = (ord == std::strong_ordering::less);
#else
            bool lt = (x < y);
#endif
            benchmark::DoNotOptimize(eq);
            benchmark::DoNotOptimize(lt);
            benchmark::DoNotOptimize(x);
            benchmark::DoNotOptimize(y);
        }
    }
}

static void BM_Z2_Hash(benchmark::State& state) {
    Z2 a(1, 0, 0);
    std::hash<Z2> hasher;
    for (auto _ : state) {
        Z2 x = a;
        std::size_t acc = 0;
        for (int i = 0; i < 256; ++i) {
            acc ^= hasher(x);
            x += a;
            benchmark::DoNotOptimize(acc);
            benchmark::DoNotOptimize(x);
        }
    }
}

static void BM_Dyadic_Hash(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    std::hash<DyadicSqrt2> hasher;
    for (auto _ : state) {
        DyadicSqrt2 x = a;
        std::size_t acc = 0;
        for (int i = 0; i < 256; ++i) {
            acc ^= hasher(x);
            x += a;
            benchmark::DoNotOptimize(acc);
            benchmark::DoNotOptimize(x);
        }
    }
}
// ---- Mixed microbenchmarks: full arithmetic surface in one loop ----

static void BM_Z2_Mixed(benchmark::State& state) {
    Z2 a(1, 0, 0);
    Z2 b(3, 5, 1);
    std::hash<Z2> hasher;

    for (auto _ : state) {
        Z2 x = a;
        Z2 y = b;
        std::size_t h_acc = 0;

        for (int i = 0; i < 256; ++i) {
            // basic arithmetic
            x += y;
            x -= y;
            x *= y;

            // shifts
            x <<= 1;
            x >>= 1;

            // reduce and abs
            x.reduce();
            Z2 ax = std::abs(x);

            // unary negation
            Z2 neg = -x;

            // comparisons
            bool eq = (x == y);
    #if __cpp_impl_three_way_comparison
            auto ord = (x <=> y);
            bool lt = (ord == std::strong_ordering::less);
    #else
            bool lt = (x < y);
    #endif

            // hash
            h_acc ^= hasher(x);

            benchmark::DoNotOptimize(x);
            benchmark::DoNotOptimize(y);
            benchmark::DoNotOptimize(ax);
            benchmark::DoNotOptimize(neg);
            benchmark::DoNotOptimize(eq);
            benchmark::DoNotOptimize(lt);
        }
        benchmark::DoNotOptimize(h_acc);
    }
}

static void BM_Dyadic_Mixed(benchmark::State& state) {
    DyadicSqrt2 a(1, 0, 0);
    DyadicSqrt2 b(3, 5, 1);
    std::hash<DyadicSqrt2> hasher;

    for (auto _ : state) {
        DyadicSqrt2 x = a;
        DyadicSqrt2 y = b;
        std::size_t h_acc = 0;

        for (int i = 0; i < 256; ++i) {
            // basic arithmetic
            x += y;
            x -= y;
            x *= y;

            // shifts
            x <<= 1;
            x >>= 1;

            // reduce and abs
            x.reduce();
            DyadicSqrt2 ax = std::abs(x);

            // unary negation
            DyadicSqrt2 neg = -x;

            // comparisons
            bool eq = (x == y);
    #if __cpp_impl_three_way_comparison
            auto ord = (x <=> y);
            bool lt = (ord == std::strong_ordering::less);
    #else
            bool lt = (x < y);
    #endif

            // hash
            h_acc ^= hasher(x);

            benchmark::DoNotOptimize(x);
            benchmark::DoNotOptimize(y);
            benchmark::DoNotOptimize(ax);
            benchmark::DoNotOptimize(neg);
            benchmark::DoNotOptimize(eq);
            benchmark::DoNotOptimize(lt);
        }
        benchmark::DoNotOptimize(h_acc);
    }
}

constexpr double kMinSecondsPerBench = 5.0;

// BENCHMARK(BM_Z2_AddAssign)->Name("Z2/AddAssign")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Dyadic_AddAssign)->Name("DyadicSqrt2/AddAssign")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Z2_SubAssign)->Name("Z2/SubAssign")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Dyadic_SubAssign)->Name("DyadicSqrt2/SubAssign")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Z2_MulAssign)->Name("Z2/MulAssign")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Dyadic_MulAssign)->Name("DyadicSqrt2/MulAssign")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Z2_ShiftLeft)->Name("Z2/ShiftLeft")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Dyadic_ShiftLeft)->Name("DyadicSqrt2/ShiftLeft")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Z2_ShiftRight)->Name("Z2/ShiftRight")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Dyadic_ShiftRight)->Name("DyadicSqrt2/ShiftRight")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Z2_Reduce)->Name("Z2/Reduce")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Dyadic_Reduce)->Name("DyadicSqrt2/Reduce")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Z2_Abs)->Name("Z2/Abs")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Dyadic_Abs)->Name("DyadicSqrt2/Abs")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Z2_Negate)->Name("Z2/Negate")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Dyadic_Negate)->Name("DyadicSqrt2/Negate")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Z2_Compare)->Name("Z2/Compare")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Dyadic_Compare)->Name("DyadicSqrt2/Compare")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Z2_Hash)->Name("Z2/Hash")->MinTime(kMinSecondsPerBench);
// BENCHMARK(BM_Dyadic_Hash)->Name("DyadicSqrt2/Hash")->MinTime(kMinSecondsPerBench);
BENCHMARK(BM_Z2_Mixed)->Name("Z2/Mixed")->MinTime(kMinSecondsPerBench);
BENCHMARK(BM_Dyadic_Mixed)->Name("DyadicSqrt2/Mixed")->MinTime(kMinSecondsPerBench);

BENCHMARK_MAIN();
