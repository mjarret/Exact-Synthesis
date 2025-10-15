/**
 * @file hash_bench.cpp
 * @brief Microbench to compare hash mixer variants for frequency hashing.
 */

#include <benchmark/benchmark.h>
#include <vector>
#include <random>
#include "SO6.hpp"

static std::vector<SO6> make_chain(size_t n) {
    std::vector<SO6> v; v.reserve(n);
    SO6 cur = SO6::identity();
    v.push_back(cur);
    for (size_t i = 1; i < n; ++i) {
        cur = cur.left_multiply_by_T(static_cast<uint8_t>(i % 15));
        v.push_back(cur);
    }
    return v;
}

static void BM_FreqHash_Rows(benchmark::State& st) {
    auto v = make_chain(st.range(0));
    size_t sink = 0;
    for (auto _ : st) {
        for (auto &s : v) {
            for (int r = 0; r < 6; ++r) sink += SO6::frequency_hash(s.row_frequency[r]);
        }
    }
    benchmark::DoNotOptimize(sink);
}

static void BM_FreqHash_Cols(benchmark::State& st) {
    auto v = make_chain(st.range(0));
    size_t sink = 0;
    for (auto _ : st) {
        for (auto &s : v) {
            for (int c = 0; c < 6; ++c) sink += SO6::frequency_hash(s.col_frequency[c]);
        }
    }
    benchmark::DoNotOptimize(sink);
}

BENCHMARK(BM_FreqHash_Rows)->Arg(256)->Arg(1024);
BENCHMARK(BM_FreqHash_Cols)->Arg(256)->Arg(1024);

BENCHMARK_MAIN();

