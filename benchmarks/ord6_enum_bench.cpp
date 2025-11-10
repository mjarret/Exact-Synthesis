#include <benchmark/benchmark.h>
#include <vector>
#include <array>
#include <cstdint>
#include "ds/FlatFrequencyTable.hpp"
#include "ds/Order6.hpp"

using ds::FlatFrequencyTable;
using order6::Order6;

static FlatFrequencyTable build_table_from_blocks(const std::vector<uint8_t>& blocks) {
    FlatFrequencyTable t;
    uint8_t base = 0;
    for (size_t i = 0; i < blocks.size(); ++i) {
        uint8_t len = blocks[i];
        uint8_t mask = 0;
        for (uint8_t j = 0; j < len; ++j) mask |= static_cast<uint8_t>(1u << (base + j));
        base = static_cast<uint8_t>(base + len);
        Order6 o{}; o.set_mask_rank(mask, 0);
        t[static_cast<uint16_t>(100 + i)] = o; // arbitrary key
    }
    t.prepare_advance_lut();
    return t;
}

static void materialize(const FlatFrequencyTable& t, uint8_t out[6]) {
    uint8_t* w = out;
    uint8_t tmp[6];
    for (auto it = t.begin(); it != t.end(); ++it) {
        auto& o = it->second;
        const uint8_t k = o.size();
        o.to_array(tmp);
        for (uint8_t j = 0; j < k; ++j) *w++ = tmp[j];
    }
}

static void bench_order6_blocks(benchmark::State& state, const std::vector<uint8_t>& blocks) {
    auto t = build_table_from_blocks(blocks);
    for (auto _ : state) {
        uint32_t cnt = 0;
        uint8_t buf[6];
        while (true) {
            materialize(t, buf);
            benchmark::DoNotOptimize(buf);
            ++cnt;
            if (!t.next_via_lut()) break;
        }
        benchmark::DoNotOptimize(cnt);
    }
}

// Register a handful of representative partitions of 6
static void BM_Order6_blocks_6(benchmark::State& s){ bench_order6_blocks(s, {6}); }
static void BM_Order6_blocks_51(benchmark::State& s){ bench_order6_blocks(s, {5,1}); }
static void BM_Order6_blocks_42(benchmark::State& s){ bench_order6_blocks(s, {4,2}); }
static void BM_Order6_blocks_411(benchmark::State& s){ bench_order6_blocks(s, {4,1,1}); }
static void BM_Order6_blocks_33(benchmark::State& s){ bench_order6_blocks(s, {3,3}); }
static void BM_Order6_blocks_321(benchmark::State& s){ bench_order6_blocks(s, {3,2,1}); }
static void BM_Order6_blocks_222(benchmark::State& s){ bench_order6_blocks(s, {2,2,2}); }
static void BM_Order6_blocks_21111(benchmark::State& s){ bench_order6_blocks(s, {2,1,1,1,1}); }
static void BM_Order6_blocks_111111(benchmark::State& s){ bench_order6_blocks(s, {1,1,1,1,1,1}); }

BENCHMARK(BM_Order6_blocks_6);
BENCHMARK(BM_Order6_blocks_51);
BENCHMARK(BM_Order6_blocks_42);
BENCHMARK(BM_Order6_blocks_411);
BENCHMARK(BM_Order6_blocks_33);
BENCHMARK(BM_Order6_blocks_321);
BENCHMARK(BM_Order6_blocks_222);
BENCHMARK(BM_Order6_blocks_21111);
BENCHMARK(BM_Order6_blocks_111111);

BENCHMARK_MAIN();

