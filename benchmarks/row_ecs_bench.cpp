// Benchmark: row equivalence classes construction from 6 uint16_t signatures.
// Compare: current (SignatureMaskMap) vs bak-2 OrderedPartition6::sort_and_partition_6.

#include <benchmark/benchmark.h>
#include <array>
#include <vector>
#include <cstdint>
#include <algorithm>
#include "./include/ds/SignatureMaskMap.hpp"

// Bring bak-2 classes with unique names
#define Lehmer6 Lehmer6_bak2
#include "../Exact-Synthesis-bak-2/include/ds/Lehmer6.hpp"
#undef Lehmer6
using Bak2Lehmer6 = Lehmer6_bak2;

#define Lehmer6 Bak2Lehmer6
#define OrderedPartition6 OrderedPartition6_bak2
#include "../Exact-Synthesis-bak-2/include/ds/OrderedPartition6.hpp"
#undef OrderedPartition6
#undef Lehmer6
using Bak2OrderedPartition6 = OrderedPartition6_bak2;

namespace {

static inline uint32_t mix(uint32_t x) {
    x ^= x >> 17; x *= 0x85EBCA6B; x ^= x >> 13; x *= 0xC2B2AE35; x ^= x >> 16; return x;
}

static std::vector<std::array<uint16_t,6>> make_samples(size_t n) {
    std::vector<std::array<uint16_t,6>> v; v.reserve(n);
    uint32_t x = 1;
    for (size_t i = 0; i < n; ++i) {
        std::array<uint16_t,6> a{};
        for (int r = 0; r < 6; ++r) {
            x = mix(x + 0x9E3779B1u + static_cast<uint32_t>(r) + static_cast<uint32_t>(i<<4));
            a[static_cast<size_t>(r)] = static_cast<uint16_t>(x & 0x7); // 8 buckets to create dups
        }
        v.push_back(a);
    }
    return v;
}
static auto kSamples = make_samples(1024);

static void BM_row_ecs_current(benchmark::State& st) {
    for (auto _ : st) {
        uint64_t acc = 0;
        for (const auto& sig : kSamples) {
            ds::SignatureMaskMap m;
            for (uint8_t r = 0; r < 6; ++r) m[sig[r]] |= r;
            // Accumulate only size (construction result), no extra traversal
            acc += m.size();
        }
        benchmark::DoNotOptimize(acc);
    }
}

// Current path + sort unique keys (to mirror gather_sorted_keys)
static void BM_row_ecs_current_with_sort(benchmark::State& st) {
    for (auto _ : st) {
        uint64_t acc = 0;
        for (const auto& sig : kSamples) {
            ds::SignatureMaskMap m;
            for (uint8_t r = 0; r < 6; ++r) m[sig[r]] |= r;
            // extract unique keys and insertion sort them (n <= 6)
            uint16_t keys[6];
            uint8_t n = static_cast<uint8_t>(m.size());
            for (uint8_t i = 0; i < n; ++i) keys[i] = m.data()[i].sig;
            for (uint8_t i = 1; i < n; ++i) {
                uint16_t cur = keys[i];
                int j = i - 1;
                while (j >= 0 && keys[j] > cur) { keys[j + 1] = keys[j]; --j; }
                keys[j + 1] = cur;
            }
            acc += n;
        }
        benchmark::DoNotOptimize(acc);
    }
}

static void BM_row_ecs_bak2_sort_and_partition(benchmark::State& st) {
    for (auto _ : st) {
        uint64_t acc = 0;
        for (const auto& sig : kSamples) {
            auto op = Bak2OrderedPartition6::sort_and_partition_6(sig);
            // Accumulate only the compact representation (construction result)
            acc += op.packed;
        }
        benchmark::DoNotOptimize(acc);
    }
}

} // namespace

BENCHMARK(BM_row_ecs_current)->Name("row_ecs/current");
BENCHMARK(BM_row_ecs_current_with_sort)->Name("row_ecs/current_with_sort");
BENCHMARK(BM_row_ecs_bak2_sort_and_partition)->Name("row_ecs/bak2_sort_and_partition");

BENCHMARK_MAIN();
