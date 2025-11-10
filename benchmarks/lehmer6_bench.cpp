// Google Benchmark suite comparing current Lehmer6 vs bak-2 Lehmer6
#include <benchmark/benchmark.h>
#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>

// Include current Lehmer6 and alias it
#include "ds/Lehmer6.hpp"
using CurrLehmer6 = Lehmer6;

// Include bak-2 Lehmer6 with macro renaming to avoid symbol clash
#define Lehmer6 Lehmer6_bak2
#include "../Exact-Synthesis-bak-2/include/ds/Lehmer6.hpp"
#undef Lehmer6
using Bak2Lehmer6 = Lehmer6_bak2;

namespace {

// Precompute all 720 permutations in lex order once (shared input for encode)
static std::array<std::array<uint8_t, 6>, 720> make_all_perms() {
    std::array<std::array<uint8_t, 6>, 720> out{};
    std::array<uint8_t, 6> p{0,1,2,3,4,5};
    size_t r = 0;
    do { out[r++] = p; } while (std::next_permutation(p.begin(), p.end()));
    return out;
}
static const auto kAllPerms = make_all_perms();

// Warm-up helpers to ensure LUTs/tables are initialized before timing
inline void warmup_curr() {
    CurrLehmer6 p = CurrLehmer6::from_index(0);
    benchmark::DoNotOptimize(p[0]);
}
inline void warmup_bak2() {
    Bak2Lehmer6 p = Bak2Lehmer6::from_index(0);
    benchmark::DoNotOptimize(p[0]);
}

// ----- encode -----
static void BM_encode_curr(benchmark::State& st) {
    warmup_curr();
    for (auto _ : st) {
        uint32_t acc = 0;
        for (const auto& a : kAllPerms) {
            acc += CurrLehmer6::encode(std::span<const uint8_t, 6>(a));
        }
        benchmark::DoNotOptimize(acc);
    }
}

static void BM_encode_bak2(benchmark::State& st) {
    warmup_bak2();
    for (auto _ : st) {
        uint32_t acc = 0;
        for (const auto& a : kAllPerms) {
            acc += Bak2Lehmer6::encode(std::span<const uint8_t, 6>(a));
        }
        benchmark::DoNotOptimize(acc);
    }
}

// ----- decode -----
template <typename L>
static void BM_decode_generic(benchmark::State& st) {
    for (auto _ : st) {
        uint32_t acc = 0;
        for (uint16_t r = 0; r < 720; ++r) {
            auto a = L::decode(r);
            acc += a[0];
        }
        benchmark::DoNotOptimize(acc);
    }
}

static void BM_decode_curr(benchmark::State& st) { warmup_curr(); BM_decode_generic<CurrLehmer6>(st); }
static void BM_decode_bak2(benchmark::State& st) { warmup_bak2(); BM_decode_generic<Bak2Lehmer6>(st); }

// ----- operator[] -----
template <typename L>
static void BM_index_op_generic(benchmark::State& st) {
    for (auto _ : st) {
        uint32_t acc = 0;
        for (uint16_t r = 0; r < 720; ++r) {
            L p = L::from_index(r);
            // sum all positions to force multiple operator[] calls
            for (int i = 0; i < 6; ++i) acc += p[static_cast<size_t>(i)];
        }
        benchmark::DoNotOptimize(acc);
    }
}
static void BM_index_op_curr(benchmark::State& st) { warmup_curr(); BM_index_op_generic<CurrLehmer6>(st); }
static void BM_index_op_bak2(benchmark::State& st) { warmup_bak2(); BM_index_op_generic<Bak2Lehmer6>(st); }

// ----- to_array -----
template <typename L>
static void BM_to_array_generic(benchmark::State& st) {
    for (auto _ : st) {
        uint32_t acc = 0;
        for (uint16_t r = 0; r < 720; ++r) {
            L p = L::from_index(r);
            auto a = p.to_array();
            acc += a[0];
        }
        benchmark::DoNotOptimize(acc);
    }
}
static void BM_to_array_curr(benchmark::State& st) { warmup_curr(); BM_to_array_generic<CurrLehmer6>(st); }
static void BM_to_array_bak2(benchmark::State& st) { warmup_bak2(); BM_to_array_generic<Bak2Lehmer6>(st); }

// ----- next_permutation -----
template <typename L>
static void BM_next_perm_generic(benchmark::State& st) {
    for (auto _ : st) {
        L p = L::from_index(0);
        uint32_t count = 0;
        for (int i = 0; i < 720; ++i) { count += p.next_permutation(); }
        benchmark::DoNotOptimize(count);
    }
}
static void BM_next_perm_curr(benchmark::State& st) { warmup_curr(); BM_next_perm_generic<CurrLehmer6>(st); }
static void BM_next_perm_bak2(benchmark::State& st) { warmup_bak2(); BM_next_perm_generic<Bak2Lehmer6>(st); }

// ----- prev_permutation -----
template <typename L>
static void BM_prev_perm_generic(benchmark::State& st) {
    for (auto _ : st) {
        L p = L::from_index(719);
        uint32_t count = 0;
        for (int i = 0; i < 720; ++i) { count += p.prev_permutation(); }
        benchmark::DoNotOptimize(count);
    }
}
static void BM_prev_perm_curr(benchmark::State& st) { warmup_curr(); BM_prev_perm_generic<CurrLehmer6>(st); }
static void BM_prev_perm_bak2(benchmark::State& st) { warmup_bak2(); BM_prev_perm_generic<Bak2Lehmer6>(st); }

// ----- apply -----
template <typename L>
static void BM_apply_generic(benchmark::State& st) {
    for (auto _ : st) {
        uint32_t acc = 0;
        for (uint16_t r = 0; r < 720; ++r) {
            L p = L::from_index(r);
            uint32_t arr[6] = {0,1,2,3,4,5};
            p.apply(arr);
            acc += arr[0];
        }
        benchmark::DoNotOptimize(acc);
    }
}
static void BM_apply_curr(benchmark::State& st) { warmup_curr(); BM_apply_generic<CurrLehmer6>(st); }
static void BM_apply_bak2(benchmark::State& st) { warmup_bak2(); BM_apply_generic<Bak2Lehmer6>(st); }

} // namespace

// Register benchmarks
BENCHMARK(BM_encode_curr)->Name("encode/current");
BENCHMARK(BM_encode_bak2)->Name("encode/bak2");

BENCHMARK(BM_decode_curr)->Name("decode/current");
BENCHMARK(BM_decode_bak2)->Name("decode/bak2");

BENCHMARK(BM_index_op_curr)->Name("index_op/current");
BENCHMARK(BM_index_op_bak2)->Name("index_op/bak2");

BENCHMARK(BM_to_array_curr)->Name("to_array/current");
BENCHMARK(BM_to_array_bak2)->Name("to_array/bak2");

BENCHMARK(BM_next_perm_curr)->Name("next_perm/current");
BENCHMARK(BM_next_perm_bak2)->Name("next_perm/bak2");

BENCHMARK(BM_prev_perm_curr)->Name("prev_perm/current");
BENCHMARK(BM_prev_perm_bak2)->Name("prev_perm/bak2");

BENCHMARK(BM_apply_curr)->Name("apply/current");
BENCHMARK(BM_apply_bak2)->Name("apply/bak2");

BENCHMARK_MAIN();
