// Benchmark: column comparison — current method vs raw lexicographic

#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <random>
#include <vector>

#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "util/utils.hpp"

namespace {

SO6 random_target(std::mt19937_64& rng, int steps) {
    std::uniform_int_distribution<int> d(0, 14);
    SO6 cur = SO6::identity();
    for (int i = 0; i < steps; ++i) cur = T_OperatorRuntime(static_cast<uint8_t>(d(rng))) * cur;
    return cur;
}

// Low-level 24-bit loader (column-major: 3 bytes per entry)
static inline __attribute__((always_inline)) uint32_t load24(const uint8_t* base_col, uint8_t row) {
    const uint8_t* p = base_col + static_cast<int>(row) * 3;
    return static_cast<uint32_t>(p[0])
         | (static_cast<uint32_t>(p[1]) << 8)
         | (static_cast<uint32_t>(p[2]) << 16);
}

// 144-bit packed column key used by packed benchmarks
struct Packed144 { unsigned __int128 hi; uint16_t tail; };

// Current column-compare logic (byte-level fast path mirroring cmp_col_fast)
std::strong_ordering cmp_col_current_public(
    const SO6& s,
    const uint8_t* rowL, int cL,
    const uint8_t* rowR, int cR,
    uint16_t first_sign_mask, uint16_t second_sign_mask)
{
    using std::strong_ordering;
    constexpr auto Equal = std::strong_ordering::equal;
    constexpr auto Less = std::strong_ordering::less;
    constexpr auto Greater = std::strong_ordering::greater;

    const uint8_t* baseL = s.arr24_ + static_cast<int>(cL) * 18;
    const uint8_t* baseR = s.arr24_ + static_cast<int>(cR) * 18;

    int i = 0;
    strong_ordering comp1 = Equal;
    strong_ordering comp2 = Equal;

    // Phase 1: determine orientation by first non-zero (int_c byte)
    for (; i < 6; ++i) {
        const uint32_t Ld = load24(baseL, rowL[i]);
        const uint32_t Rd = load24(baseR, rowR[i]);
        const int8_t Lic = static_cast<int8_t>(Ld & 0xFF);
        const int8_t Ric = static_cast<int8_t>(Rd & 0xFF);
        comp1 = (Lic < 0) ? Less : (Lic > 0 ? Greater : Equal);
        comp2 = (Ric < 0) ? Less : (Ric > 0 ? Greater : Equal);
        if (comp1 == Equal && comp2 == Equal) continue;
        if (comp1 == Equal) return Greater;
        if (comp2 == Equal) return Less;
        const uint8_t fsm = static_cast<uint8_t>((first_sign_mask  >> i) & utils::BITS);
        const uint8_t ssm = static_cast<uint8_t>((second_sign_mask >> i) & utils::BITS);
        if ((comp1 == Less) ^ (fsm == utils::NEG)) first_sign_mask  ^= 0x3Fu;
        if ((comp2 == Less) ^ (ssm == utils::NEG)) second_sign_mask ^= 0x3Fu;
        break;
    }

    // Phase 2: lex compare with sign masks applied to the 16-bit magnitude
    for (; i < 6; ++i) {
        const uint32_t Ld = load24(baseL, rowL[i]);
        const uint32_t Rd = load24(baseR, rowR[i]);
        const uint16_t Lnum = static_cast<uint16_t>(Ld & 0xFFFFu);
        const uint16_t Rnum = static_cast<uint16_t>(Rd & 0xFFFFu);
        const bool first_is_neg  = (((first_sign_mask  >> i) & utils::BITS) == utils::NEG);
        const bool second_is_neg = (((second_sign_mask >> i) & utils::BITS) == utils::NEG);

        const uint16_t Lnum_eff = first_is_neg  ? static_cast<uint16_t>((Lnum != 0) ? (256u - Lnum) : 0u) : Lnum;
        const uint16_t Rnum_eff = second_is_neg ? static_cast<uint16_t>((Rnum != 0) ? (256u - Rnum) : 0u) : Rnum;
        const uint32_t Leff = (Ld & 0xFF0000u) | Lnum_eff;
        const uint32_t Reff = (Rd & 0xFF0000u) | Rnum_eff;

        const uint32_t Lval = (static_cast<uint8_t>(Lnum_eff & 0xFFu) != 0) ? Leff : 0u;
        const uint32_t Rval = (static_cast<uint8_t>(Rnum_eff & 0xFFu) != 0) ? Reff : 0u;

        if (Rval == Lval) continue;
        if (static_cast<int8_t>(Lnum & 0xFFu) == 0) return Greater;
        if (static_cast<int8_t>(Rnum & 0xFFu) == 0) return Less;
        return (Rval < Lval) ? Less : Greater;
    }
    return Equal;
}

// Raw lexicographic column compare: no orientation, no sign flips (byte-level)
std::strong_ordering cmp_col_raw_public(
    const SO6& s,
    const uint8_t* row, int cL,
    int cR)
{
    const uint8_t* baseL = s.arr24_ + static_cast<int>(cL) * 18;
    const uint8_t* baseR = s.arr24_ + static_cast<int>(cR) * 18;
    for (int i = 0; i < 6; ++i) {
        const uint32_t Ld = load24(baseL, row[i]);
        const uint32_t Rd = load24(baseR, row[i]);
        const uint32_t Lval = (static_cast<int8_t>(Ld & 0xFF) == 0) ? 0u : Ld;
        const uint32_t Rval = (static_cast<int8_t>(Rd & 0xFF) == 0) ? 0u : Rd;
        if (Rval == Lval) continue;
        return (Rval < Lval) ? std::strong_ordering::less : std::strong_ordering::greater;
    }
    return std::strong_ordering::equal;
}

struct Workload {
    SO6 s;
    std::array<uint8_t,6> row{}; // identity
    std::array<std::pair<int,int>, 6> col_pairs{}; // simple fixed pairs
};

Workload make_work(std::mt19937_64& rng) {
    Workload w{};
    w.s = random_target(rng, 16);
    for (int i = 0; i < 6; ++i) w.row[static_cast<size_t>(i)] = static_cast<uint8_t>(i);
    // pairs (0,1), (1,2), ... (5,0)
    for (int i = 0; i < 6; ++i) {
        int a = i;
        int b = (i + 1) % 6;
        w.col_pairs[static_cast<size_t>(i)] = {a, b};
    }
    return w;
}

} // namespace

static void BM_ColCompare_Current(benchmark::State& state) {
    std::mt19937_64 rng(12345);
    auto work = make_work(rng);
    uint16_t sL = 0, sR = 0; // no extra sign mask; orientation still applied
    uint64_t acc = 0;
    for (auto _ : state) {
        for (const auto& [a,b] : work.col_pairs) {
            auto ord = cmp_col_current_public(work.s, work.row.data(), a, work.row.data(), b, sL, sR);
            acc += (ord == std::strong_ordering::less);
        }
    }
    benchmark::DoNotOptimize(acc);
}

static void BM_ColCompare_Raw(benchmark::State& state) {
    std::mt19937_64 rng(12345);
    auto work = make_work(rng);
    uint64_t acc = 0;
    for (auto _ : state) {
        for (const auto& [a,b] : work.col_pairs) {
            auto ord = cmp_col_raw_public(work.s, work.row.data(), a, b);
            acc += (ord == std::strong_ordering::less);
        }
    }
    benchmark::DoNotOptimize(acc);
}

// Specialized current comparator for identity row order and zero external masks
static inline __attribute__((always_inline)) std::strong_ordering
cmp_col_current_id0(const SO6& s, int cL, int cR) {
    using std::strong_ordering; constexpr auto Equal = strong_ordering::equal;
    constexpr auto Less = strong_ordering::less; constexpr auto Greater = strong_ordering::greater;
    const uint8_t* baseL = s.arr24_ + static_cast<int>(cL) * 18;
    const uint8_t* baseR = s.arr24_ + static_cast<int>(cR) * 18;

    int i = 0; bool invL = false, invR = false;
    for (; i < 6; ++i) {
        const uint32_t Ld = load24(baseL, static_cast<uint8_t>(i));
        const uint32_t Rd = load24(baseR, static_cast<uint8_t>(i));
        const int8_t Lic = static_cast<int8_t>(Ld & 0xFF);
        const int8_t Ric = static_cast<int8_t>(Rd & 0xFF);
        if (Lic == 0 && Ric == 0) continue;
        if (Lic == 0) return Greater;
        if (Ric == 0) return Less;
        invL = (Lic < 0); invR = (Ric < 0);
        break;
    }
    for (; i < 6; ++i) {
        const uint32_t Ld = load24(baseL, static_cast<uint8_t>(i));
        const uint32_t Rd = load24(baseR, static_cast<uint8_t>(i));
        const uint16_t Lnum = static_cast<uint16_t>(Ld & 0xFFFFu);
        const uint16_t Rnum = static_cast<uint16_t>(Rd & 0xFFFFu);
        const uint16_t Lnum_eff = invL ? static_cast<uint16_t>((Lnum != 0) ? (256u - Lnum) : 0u) : Lnum;
        const uint16_t Rnum_eff = invR ? static_cast<uint16_t>((Rnum != 0) ? (256u - Rnum) : 0u) : Rnum;
        const uint32_t Leff = (Ld & 0xFF0000u) | Lnum_eff;
        const uint32_t Reff = (Rd & 0xFF0000u) | Rnum_eff;
        const uint32_t Lval = (static_cast<uint8_t>(Lnum_eff & 0xFFu) != 0) ? Leff : 0u;
        const uint32_t Rval = (static_cast<uint8_t>(Rnum_eff & 0xFFu) != 0) ? Reff : 0u;
        if (Rval == Lval) continue;
        if (static_cast<int8_t>(Lnum & 0xFFu) == 0) return Greater;
        if (static_cast<int8_t>(Rnum & 0xFFu) == 0) return Less;
        return (Rval < Lval) ? Less : Greater;
    }
    return Equal;
}

static void BM_ColCompare_Current_Id0(benchmark::State& state) {
    std::mt19937_64 rng(12345);
    auto work = make_work(rng);
    uint64_t acc = 0;
    for (auto _ : state) {
        for (const auto& [a,b] : work.col_pairs) {
            auto ord = cmp_col_current_id0(work.s, a, b);
            acc += (ord == std::strong_ordering::less);
        }
    }
    benchmark::DoNotOptimize(acc);
}
// -------- Packed variants: build 144-bit keys then compare once --------

// Pack helpers (declared above): load24, Packed144, pack_col_raw, pack_col_current

static void BM_ColCompare_CurrentPacked(benchmark::State& state) {
    std::mt19937_64 rng(12345);
    auto work = make_work(rng);
    uint16_t sL = 0, sR = 0; // no external sign mask; orientation handled per column
    uint64_t acc = 0;
    for (auto _ : state) {
        for (const auto& [a,b] : work.col_pairs) {
            // Pack both columns independently, then compare R vs L
            // Reuse pack_col_current logic by inlining here to avoid extra function overhead
            auto pack_current = [&](int c, uint16_t mask){
                const uint8_t* base = work.s.arr24_ + static_cast<int>(c) * 18;
                // Orientation
                for (int i = 0; i < 6; ++i) {
                    const uint32_t d = load24(base, work.row[i]);
                    const int8_t ic = static_cast<int8_t>(d & 0xFF);
                    if (ic == 0) continue;
                    const uint8_t bit = static_cast<uint8_t>((mask >> i) & utils::BITS);
                    if ( (ic < 0) ^ (bit == utils::NEG) ) mask ^= 0x3Fu;
                    break;
                }
                // Pack
                unsigned __int128 acc128 = 0;
                for (int i = 0; i < 5; ++i) {
                    const uint32_t d = load24(base, work.row[i]);
                    const uint16_t num = static_cast<uint16_t>(d & 0xFFFFu);
                    const bool neg = (((mask >> i) & utils::BITS) == utils::NEG);
                    const uint16_t num_eff = neg ? static_cast<uint16_t>((num != 0) ? (256u - num) : 0u) : num;
                    const uint32_t eff = (static_cast<uint8_t>(num_eff & 0xFFu) != 0)
                                       ? ((d & 0xFF0000u) | num_eff)
                                       : 0u;
                    acc128 = (acc128 << 24) | static_cast<unsigned __int128>(eff);
                }
                const uint32_t d5 = load24(base, work.row[5]);
                const uint16_t num5 = static_cast<uint16_t>(d5 & 0xFFFFu);
                const bool neg5 = (((mask >> 5) & utils::BITS) == utils::NEG);
                const uint16_t num5_eff = neg5 ? static_cast<uint16_t>((num5 != 0) ? (256u - num5) : 0u) : num5;
                const uint32_t eff5 = (static_cast<uint8_t>(num5_eff & 0xFFu) != 0)
                                    ? ((d5 & 0xFF0000u) | num5_eff)
                                    : 0u;
                acc128 = (acc128 << 8) | static_cast<unsigned __int128>(eff5 & 0xFFu);
                return Packed144{acc128, static_cast<uint16_t>(eff5 >> 8)};
            };

            Packed144 L = pack_current(a, sL);
            Packed144 R = pack_current(b, sR);
            if (R.hi != L.hi) acc += (R.hi < L.hi);
            else              acc += (R.tail < L.tail);
        }
    }
    benchmark::DoNotOptimize(acc);
}

static void BM_ColCompare_RawPacked(benchmark::State& state) {
    std::mt19937_64 rng(12345);
    auto work = make_work(rng);
    uint64_t acc = 0;
    for (auto _ : state) {
        for (const auto& [a,b] : work.col_pairs) {
            const uint8_t* baseL = work.s.arr24_ + static_cast<int>(a) * 18;
            const uint8_t* baseR = work.s.arr24_ + static_cast<int>(b) * 18;
            unsigned __int128 accL = 0, accR = 0;
            for (int i = 0; i < 5; ++i) {
                const uint32_t dL = load24(baseL, work.row[i]);
                const uint32_t dR = load24(baseR, work.row[i]);
                const uint32_t vL = (static_cast<int8_t>(dL & 0xFF) == 0) ? 0u : dL;
                const uint32_t vR = (static_cast<int8_t>(dR & 0xFF) == 0) ? 0u : dR;
                accL = (accL << 24) | static_cast<unsigned __int128>(vL);
                accR = (accR << 24) | static_cast<unsigned __int128>(vR);
            }
            const uint32_t dL5 = load24(baseL, work.row[5]);
            const uint32_t dR5 = load24(baseR, work.row[5]);
            const uint32_t vL5 = (static_cast<int8_t>(dL5 & 0xFF) == 0) ? 0u : dL5;
            const uint32_t vR5 = (static_cast<int8_t>(dR5 & 0xFF) == 0) ? 0u : dR5;
            accL = (accL << 8) | static_cast<unsigned __int128>(vL5 & 0xFFu);
            accR = (accR << 8) | static_cast<unsigned __int128>(vR5 & 0xFFu);
            const uint16_t tailL = static_cast<uint16_t>(vL5 >> 8);
            const uint16_t tailR = static_cast<uint16_t>(vR5 >> 8);
            if (accR != accL) acc += (accR < accL);
            else              acc += (tailR < tailL);
        }
    }
    benchmark::DoNotOptimize(acc);
}

BENCHMARK(BM_ColCompare_Current)->Name("col_compare/current");
BENCHMARK(BM_ColCompare_Raw)->Name("col_compare/raw");
BENCHMARK(BM_ColCompare_Current_Id0)->Name("col_compare/current_id0");
BENCHMARK(BM_ColCompare_CurrentPacked)->Name("col_compare/current_packed");
BENCHMARK(BM_ColCompare_RawPacked)->Name("col_compare/raw_packed");

BENCHMARK_MAIN();
