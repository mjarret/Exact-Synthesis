#include <benchmark/benchmark.h>

#include <random>
#include <vector>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "algo/Generate.hpp"

namespace {

// How deep to build the LUT for this benchmark.
// (Use a relatively large depth for realistic matrices.)
constexpr int kMaxDepth = 11;

// How many SO6 targets to use per benchmark run.
constexpr std::size_t kNumTargets = 64;

// Maximum number of elements to sample per depth layer (to keep runtime bounded).
constexpr std::size_t kMaxPerLayer = 256;

// For each depth we keep:
//  - a sample of LUT elements at that depth (for matrix-matrix multiplies)
//  - a sample of random T-sequences of that length (for repeated T-operator applies)
struct DepthLayerData {
    std::vector<SO6> elems;                   // sample of LUT elements at this depth
    std::vector<std::vector<uint8_t>> tseqs;  // sample of random T-sequences of length=depth
};

struct Fixture {
    std::vector<DepthLayerData> depths; // index by depth (1..maxDepth)
    std::vector<SO6> targets;
};

SO6 random_target(std::mt19937_64& rng) {
    constexpr int kMaxTargetLen = 20;
    std::uniform_int_distribution<int> len_dist(1, kMaxTargetLen);
    std::uniform_int_distribution<int> t_dist(0, 14);

    int len = len_dist(rng);
    SO6 cur = SO6::identity();
    for (int i = 0; i < len; ++i) {
        cur = T_OperatorRuntime(static_cast<uint8_t>(t_dist(rng))) * cur;
    }
    return cur;
}

Fixture build_fixture() {
    Fixture f;

    suppress_indicators = true;
    stored_depth_max = static_cast<uint8_t>(kMaxDepth);

    LUT lut = algo::create_lookup_table(SO6::identity(), nullptr, nullptr);

    const int available_depth = static_cast<int>(lut.size()) - 1;
    const int max_depth = std::min(kMaxDepth, available_depth);

    f.depths.resize(static_cast<std::size_t>(max_depth + 1)); // index 0 unused

    // RNG for sampling and for generating T-sequences.
    std::mt19937_64 rng(1337);
    std::uniform_int_distribution<int> t_dist(0, 14);

    int depth = 0;
    for (const auto& layer : lut.layers()) {
        if (depth == 0) { ++depth; continue; } // skip root layer
        if (depth > max_depth) break;

        DepthLayerData d;
        d.elems.reserve(std::min<std::size_t>(layer.size(), kMaxPerLayer));
        d.tseqs.reserve(std::min<std::size_t>(layer.size(), kMaxPerLayer));

        std::size_t count = 0;
        for (const auto& elem : layer) {
            if (count >= kMaxPerLayer) break;

            d.elems.push_back(elem);

            // Generate a random T-sequence of length = depth for this sample.
            std::vector<uint8_t> seq;
            seq.reserve(static_cast<std::size_t>(depth));
            for (int i = 0; i < depth; ++i) {
                seq.push_back(static_cast<uint8_t>(t_dist(rng)));
            }
            d.tseqs.push_back(std::move(seq));
            ++count;
        }

        f.depths[static_cast<std::size_t>(depth)] = std::move(d);
        ++depth;
    }

    // Build random targets (shared across all depths and both strategies).
    f.targets.reserve(kNumTargets);
    for (std::size_t i = 0; i < kNumTargets; ++i) {
        f.targets.push_back(random_target(rng));
    }

    return f;
}

const Fixture& get_fixture() {
    static Fixture f = build_fixture();
    return f;
}

} // namespace

// Benchmark: for a given depth d, multiply a target by a LUT element at depth d
// using full SO6 matrix multiplication. We time a single representative
// matrix-matrix multiply per iteration (and let Google Benchmark handle
// repetition), rather than sweeping the entire layer each time.
static void BM_Mul_LUT_Depth(benchmark::State& state, int depth) {
    const auto& fixture = get_fixture();
    if (depth <= 0 || static_cast<std::size_t>(depth) >= fixture.depths.size()) {
        state.SkipWithError("Depth out of range for BM_Mul_LUT_Depth");
        return;
    }

    const auto& layer = fixture.depths[static_cast<std::size_t>(depth)];
    const auto& targets = fixture.targets;
    if (layer.elems.empty() || targets.empty()) {
        state.SkipWithError("No elements or targets available for BM_Mul_LUT_Depth");
        return;
    }

    // Use a single representative pair (target, depth-d LUT element).
    const SO6& t = targets.front();
    const SO6& s = layer.elems.front();

    for (auto _ : state) {
        SO6 prod = t * s;
        benchmark::DoNotOptimize(prod);
    }
}

// Benchmark: for the same depth d, apply a length-d T sequence directly to
// the target (one T per step), instead of a matrix-matrix multiply. We time a
// single representative chain per iteration.
static void BM_Apply_T_Depth(benchmark::State& state, int depth) {
    const auto& fixture = get_fixture();
    if (depth <= 0 || static_cast<std::size_t>(depth) >= fixture.depths.size()) {
        state.SkipWithError("Depth out of range for BM_Apply_T_Depth");
        return;
    }

    const auto& layer = fixture.depths[static_cast<std::size_t>(depth)];
    const auto& targets = fixture.targets;
    if (layer.tseqs.empty() || targets.empty()) {
        state.SkipWithError("No elements or targets available for BM_Apply_T_Depth");
        return;
    }

    const auto& seq  = layer.tseqs.front();
    const SO6& base  = targets.front();

    for (auto _ : state) {
        SO6 cur = base;
        for (uint8_t t_idx : seq) {
            cur = T_OperatorRuntime(t_idx) * cur;
        }
        benchmark::DoNotOptimize(cur);
    }
}

// Register benchmarks for depths 1..kMaxDepth, alternating by depth so that
// the LUT multiply and T-operator application appear adjacent in the output.
constexpr double kMinSecondsPerDepth = 5.0;

#define REGISTER_DEPTH_PAIR(D, LABEL)                                                   \
    BENCHMARK_CAPTURE(BM_Mul_LUT_Depth, depth_##D, D)                                   \
        ->Name("depth/" LABEL "/mul_lut")                                              \
        ->MinTime(kMinSecondsPerDepth);                                                \
    BENCHMARK_CAPTURE(BM_Apply_T_Depth, depth_##D, D)                                   \
        ->Name("depth/" LABEL "/apply_T")                                              \
        ->MinTime(kMinSecondsPerDepth);

REGISTER_DEPTH_PAIR(1,  "01")
REGISTER_DEPTH_PAIR(2,  "02")
REGISTER_DEPTH_PAIR(3,  "03")
REGISTER_DEPTH_PAIR(4,  "04")
REGISTER_DEPTH_PAIR(5,  "05")
REGISTER_DEPTH_PAIR(6,  "06")
REGISTER_DEPTH_PAIR(7,  "07")
REGISTER_DEPTH_PAIR(8,  "08")
REGISTER_DEPTH_PAIR(9,  "09")
REGISTER_DEPTH_PAIR(10, "10")
REGISTER_DEPTH_PAIR(11, "11")

#undef REGISTER_DEPTH_PAIR

BENCHMARK_MAIN();
