// Local, header-only microbench to emulate OrderedPartition6 enumeration
// without depending on the bak-2 headers.
#include <benchmark/benchmark.h>
#include <vector>
#include <array>
#include <cstdint>
#include "ds/Lehmer6.hpp"
#include "ds/Order6.hpp"

struct OP6Local {
    uint16_t lehmer_index{0};
    uint8_t  divider_mask{0};
    static inline OP6Local pack(uint16_t l, uint8_t d) { return OP6Local{static_cast<uint16_t>(l & 0x03FFu), static_cast<uint8_t>(d & 0x1Fu)}; }
    inline std::array<uint8_t,6> to_array() const { return Lehmer6::decode(lehmer_index); }

    struct Permuter {
        uint8_t dm{0};
        std::vector<uint16_t> cycle; // Lehmer indices in successor order for this dm
        std::size_t pos{0};
        bool started{false};

        explicit Permuter(OP6Local start) : dm(start.divider_mask) {
            // Build successor cycle from identity permutation and divider mask
            // Compute block masks from identity base [0..5]
            uint8_t base[6] = {0,1,2,3,4,5};
            uint8_t block_masks[6]{}; uint8_t B = 0;
            uint8_t cur = static_cast<uint8_t>(1u << base[0]);
            for (int i = 0; i < 5; ++i) {
                const uint8_t bit = static_cast<uint8_t>(1u << base[i+1]);
                if (dm & (1u << i)) { block_masks[B++] = cur; cur = bit; }
                else cur = static_cast<uint8_t>(cur | bit);
            }
            block_masks[B++] = cur;

            // Mixed-radix enumeration over blocks using Order6 decode
            uint16_t counts[6]{}; uint16_t total = 1;
            for (uint8_t b = 0; b < B; ++b) { const uint8_t k = order6::Order6::popcnt(block_masks[b]); counts[b] = order6::FACT[k]; total = static_cast<uint16_t>(total * counts[b]); }
            cycle.reserve(total);
            for (uint16_t mr = 0; mr < total; ++mr) {
                uint16_t x = mr, rnk[6]{};
                for (uint8_t b = 0; b < B; ++b) { const uint16_t cnt = counts[b]; rnk[b] = static_cast<uint16_t>(x % cnt); x = static_cast<uint16_t>(x / cnt); }
                std::array<uint8_t,6> perm{}; uint8_t* w = perm.data();
                for (uint8_t b = 0; b < B; ++b) { order6::Order6 o{}; o.set_mask_rank(block_masks[b], rnk[b]); uint8_t tmp[6]; o.to_array(tmp); const uint8_t k = o.size(); for (uint8_t j = 0; j < k; ++j) *w++ = tmp[j]; }
                cycle.push_back(Lehmer6::encode(perm));
            }
        }

        void reset() { pos = 0; started = false; }
        uint16_t count() const { return static_cast<uint16_t>(cycle.size()); }
        bool next(OP6Local& out) {
            if (cycle.empty()) return false;
            if (started && pos == 0) return false;
            out.lehmer_index = cycle[pos];
            out.divider_mask = dm;
            started = true;
            pos = (pos + 1) % cycle.size();
            return true;
        }
    };
};

static uint8_t divmask_from_blocks(const std::vector<uint8_t>& blocks) {
    // Divider bit i splits between positions i and i+1; set at block ends except last
    uint8_t m = 0;
    uint8_t pos = 0;
    for (size_t i = 0; i + 1 < blocks.size(); ++i) {
        pos = static_cast<uint8_t>(pos + blocks[i]);
        m |= static_cast<uint8_t>(1u << (pos - 1));
    }
    return m;
}

static void bench_op6_blocks(benchmark::State& state, const std::vector<uint8_t>& blocks) {
    const uint16_t lidx = 0; // identity permutation
    const uint8_t dm = divmask_from_blocks(blocks);
    OP6Local start = OP6Local::pack(lidx, dm);
    OP6Local::Permuter perm(start);
    for (auto _ : state) {
        OP6Local cur = start;
        perm.reset();
        uint32_t cnt = 0;
        while (perm.next(cur)) {
            auto arr = cur.to_array();
            benchmark::DoNotOptimize(arr);
            ++cnt;
        }
        benchmark::DoNotOptimize(cnt);
    }
}

// Same set of partitions
static void BM_OP6_blocks_6(benchmark::State& s){ bench_op6_blocks(s, {6}); }
static void BM_OP6_blocks_51(benchmark::State& s){ bench_op6_blocks(s, {5,1}); }
static void BM_OP6_blocks_42(benchmark::State& s){ bench_op6_blocks(s, {4,2}); }
static void BM_OP6_blocks_411(benchmark::State& s){ bench_op6_blocks(s, {4,1,1}); }
static void BM_OP6_blocks_33(benchmark::State& s){ bench_op6_blocks(s, {3,3}); }
static void BM_OP6_blocks_321(benchmark::State& s){ bench_op6_blocks(s, {3,2,1}); }
static void BM_OP6_blocks_222(benchmark::State& s){ bench_op6_blocks(s, {2,2,2}); }
static void BM_OP6_blocks_21111(benchmark::State& s){ bench_op6_blocks(s, {2,1,1,1,1}); }
static void BM_OP6_blocks_111111(benchmark::State& s){ bench_op6_blocks(s, {1,1,1,1,1,1}); }

BENCHMARK(BM_OP6_blocks_6);
BENCHMARK(BM_OP6_blocks_51);
BENCHMARK(BM_OP6_blocks_42);
BENCHMARK(BM_OP6_blocks_411);
BENCHMARK(BM_OP6_blocks_33);
BENCHMARK(BM_OP6_blocks_321);
BENCHMARK(BM_OP6_blocks_222);
BENCHMARK(BM_OP6_blocks_21111);
BENCHMARK(BM_OP6_blocks_111111);

BENCHMARK_MAIN();
