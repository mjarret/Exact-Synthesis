/**
 * @file set_bench.cpp
 * @brief Benchmarks comparing std::unordered_set<SO6> vs custom SO6FlatSet.
 */

#include <benchmark/benchmark.h>
#include <random>
#include <vector>
#include <unordered_set>
#include "so6/SO6.hpp"
#include "ds/so6_flat_set.hpp"

static std::vector<SO6> make_dataset(size_t n) {
    std::vector<SO6> v; v.reserve(n);
    SO6 cur = SO6::identity();
    for (size_t i = 0; i < n; ++i) {
        uint8_t t = static_cast<uint8_t>(i % 15);
        cur = cur.left_multiply_by_T(t);
        v.push_back(cur);
    }
    return v;
}

static void BM_UnorderedSet_Insert(benchmark::State &st) {
    size_t N = st.range(0);
    auto data = make_dataset(N);
    for (auto _ : st) {
        st.PauseTiming();
        std::unordered_set<SO6> us; us.reserve(N);
        st.ResumeTiming();
        for (auto &x : data) us.insert(x);
        benchmark::DoNotOptimize(us.size());
    }
}

static void BM_FlatSet_Insert(benchmark::State &st) {
    size_t N = st.range(0);
    auto data = make_dataset(N);
    for (auto _ : st) {
        st.PauseTiming();
        so6ds::SO6FlatSet fs; fs.reserve(N);
        st.ResumeTiming();
        for (auto &x : data) fs.insert(x);
        benchmark::DoNotOptimize(fs.size());
    }
}

BENCHMARK(BM_UnorderedSet_Insert)->Arg(1024)->Arg(4096)->Arg(16384);
BENCHMARK(BM_FlatSet_Insert)->Arg(1024)->Arg(4096)->Arg(16384);

BENCHMARK_MAIN();
