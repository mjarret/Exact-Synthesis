// Benchmarks for the TT (double-T) generator.
//
// Per-endpoint transform cost is the headline number: the shared-first-T fan-out should
// cost close to a single T transform-and-copy per emitted endpoint (target ~1.10-1.20x),
// and one TT endpoint must be materially cheaper than two sequential public T applies.
// Full-LUT builds compare T vs TT at EQUAL actual T-depth (TT emits ~151 endpoints/source
// vs ~14 for T, so only equal-T-depth / per-endpoint comparisons are meaningful).
//
// Build: make tt_operator_bench   (links the TT core; -DEXACT_DISABLE_INDICATORS)

#include <benchmark/benchmark.h>

#include <cstdint>
#include <random>
#include <thread>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "so6/TT_Operator.hpp"
#include "so6/TT_Alphabet.hpp"
#include "algo/Generate.hpp"
#include "ds/LUT.hpp"

namespace {

SO6 random_so6(uint64_t seed, int steps) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> d(0, 14);
    SO6 cur = SO6::identity();
    for (int i = 0; i < steps; ++i)
        cur = T_OperatorRuntime(static_cast<uint8_t>(d(rng))) * cur;
    cur.last_T = 15;
    cur.last_TT = 255;
    return cur;
}

// A representative source. last_TT=255 => the TT fan-out sees the full 165 legal moves;
// a typical interior source sees 151. We benchmark from a fixed source for stability.
const SO6 g_src = random_so6(0xABCDEFu, 12);

constexpr uint8_t kSampleMoveOverlap = 5;   // (1,2): overlaps -> chained butterflies
constexpr uint8_t kSampleMoveDisjoint = [] {
    for (uint8_t mv = 0; mv < tt::kNumMoves; ++mv)
        if (tt::kAlphabet[mv].disjoint) return mv;
    return uint8_t{0};
}();

// 1. One public T apply: copy + butterfly + canonical_reset + last_T.
void BM_T_apply(benchmark::State& s) {
    for (auto _ : s) {
        SO6 r = T_OperatorRuntime(3) * g_src;
        benchmark::DoNotOptimize(r.arr_.data());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_T_apply);

// 2. One standalone fused TT apply: copy + two butterflies (one column pass) + canon.
void BM_TT_fused_apply(benchmark::State& s) {
    const uint8_t mv = kSampleMoveOverlap;
    for (auto _ : s) {
        SO6 r = TT_OperatorRuntime(mv) * g_src;
        benchmark::DoNotOptimize(r.arr_.data());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_TT_fused_apply);

// 3. Two sequential PUBLIC T applies (the naive baseline the TT path must beat):
//    two copies + two canonical resets.
void BM_TT_two_public_T(benchmark::State& s) {
    const uint8_t f = tt::kAlphabet[kSampleMoveOverlap].first;
    const uint8_t sec = tt::kAlphabet[kSampleMoveOverlap].second;
    for (auto _ : s) {
        SO6 r = T_OperatorRuntime(sec) * (T_OperatorRuntime(f) * g_src);
        benchmark::DoNotOptimize(r.arr_.data());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_TT_two_public_T);

// 4. Per-endpoint transform+copy for the T generator (no find/insert): copy + butterfly +
//    metadata finalize, over all 15 neighbours. items/s -> per-endpoint cost.
void BM_T_endpoint_transform(benchmark::State& s) {
    int64_t emitted = 0;
    for (auto _ : s) {
        for (uint8_t t = 0; t < 15; ++t) {
            SO6 cand = g_src;
            tt::apply_T_values_only_rt(t, cand);
            cand.invalidate_derived_state();
            cand.last_T = t;
            benchmark::DoNotOptimize(cand.arr_.data());
        }
        benchmark::ClobberMemory();
        emitted += 15;
    }
    s.SetItemsProcessed(emitted);
}
BENCHMARK(BM_T_endpoint_transform);

// 5. Per-endpoint transform+copy for the shared-first-T TT fan-out (no find/insert): the
//    first T is applied once per group, then one second-T butterfly per endpoint. items/s
//    -> per-endpoint cost; compare to BM_T_endpoint_transform.
void BM_TT_endpoint_transform(benchmark::State& s) {
    const std::size_t prev = (g_src.last_TT == tt::kNoMove) ? tt::kRootPrev : g_src.last_TT;
    const tt::LegalGroups& legal = tt::kLegalByPrevMove[prev];
    int64_t emitted = 0;
    for (auto _ : s) {
        int64_t this_iter = 0;
        for (uint8_t f = 0; f < 15; ++f) {
            const uint8_t cnt = legal.count[f];
            if (!cnt) continue;
            SO6 inter = g_src;
            tt::apply_T_values_only_rt(f, inter);   // shared first T, amortized over the group
            for (uint8_t k = 0; k < cnt; ++k) {
                const uint8_t mv = legal.moves[f][k];
                SO6 cand = inter;
                tt::apply_T_values_only_rt(tt::kAlphabet[mv].second, cand);
                cand.invalidate_derived_state();
                cand.last_TT = mv;
                benchmark::DoNotOptimize(cand.arr_.data());
                ++this_iter;
            }
        }
        benchmark::ClobberMemory();
        emitted += this_iter;
    }
    s.SetItemsProcessed(emitted);
}
BENCHMARK(BM_TT_endpoint_transform);

// 6. Full LUT construction at equal actual T-depth (build + dedup). arg = T-depth.
void BM_build_T(benchmark::State& s) {
    const int depth = static_cast<int>(s.range(0));
    for (auto _ : s) {
        s.PauseTiming();
        THREADS = static_cast<uint8_t>(std::max(1u, std::thread::hardware_concurrency()));
        stored_depth_max = static_cast<uint8_t>(depth);
        target_T_count = static_cast<uint8_t>(depth);
        suppress_indicators = true; verbose = false;
        s.ResumeTiming();
        LUT lut = algo::create_lookup_table(SO6::identity(), nullptr, nullptr);
        lut.dedup();
        benchmark::DoNotOptimize(lut.size());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_build_T)->Arg(8)->Unit(benchmark::kMillisecond);

void BM_build_TT(benchmark::State& s) {
    const int depth = static_cast<int>(s.range(0));   // actual T-depth (even)
    for (auto _ : s) {
        s.PauseTiming();
        THREADS = static_cast<uint8_t>(std::max(1u, std::thread::hardware_concurrency()));
        stored_depth_max = static_cast<uint8_t>(depth);
        target_T_count = static_cast<uint8_t>(depth);
        suppress_indicators = true; verbose = false;
        s.ResumeTiming();
        LUT lut = algo::create_lookup_table_TT(SO6::identity(), nullptr, nullptr);
        lut.dedup();
        benchmark::DoNotOptimize(lut.size());
        benchmark::ClobberMemory();
    }
}
BENCHMARK(BM_build_TT)->Arg(8)->Unit(benchmark::kMillisecond);

} // namespace

BENCHMARK_MAIN();
