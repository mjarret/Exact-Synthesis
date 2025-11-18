// Benchmark: MITM runtime by discovered optimal length.
//
// Generates random targets by applying a random sequence of up to 18 T
// operators to the identity. For each target we first solve it once with MITM
// to determine the optimal decomposition length (dl + dr) and bucket the
// target by that length. A separate Google Benchmark is registered per
// populated length bucket and runs MITM over targets in that bucket
// round-robin, measuring time to reach a meeting point. Indicators are
// suppressed and no console output is produced.

#include <benchmark/benchmark.h>

#include <optional>
#include <random>
#include <string>
#include <vector>

#include <tbb/concurrent_unordered_set.h>

#include "config/Globals.hpp"
#include "ds/MITM.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"

namespace {

constexpr int kMaxTargetLen = 18;
constexpr int kMaxPerLength = 10; // targets to keep per discovered length bucket
constexpr int kMaxAttempts = 400; // cap warmup attempts to prevent hangs

SO6 random_target(std::mt19937_64& rng) {
    std::uniform_int_distribution<int> len_dist(1, kMaxTargetLen);
    std::uniform_int_distribution<int> t_dist(0, 14);

    int len = len_dist(rng);
    SO6 cur = SO6::identity();
    for (int i = 0; i < len; ++i) {
        cur = T_OperatorRuntime(static_cast<uint8_t>(t_dist(rng))) * cur;
    }
    return cur;
}

// Returns optimal length found by MITM for target (dl + dr), or nullopt on failure.
std::optional<int> mitm_length_for_target(const SO6& target, int depth_limit, SO6* meet_out = nullptr) {
    stored_depth_max = static_cast<uint8_t>(depth_limit);
    suppress_indicators = true;

    MITM mitm(SO6::identity(), target);
    auto meet_opt = generate_mitm_until_match(mitm);
    if (!meet_opt) return std::nullopt;

    const SO6& meet = *meet_opt;
    SO6 lm = *mitm.left().find(meet);
    SO6 rm = *mitm.right().find(meet);
    auto left_path  = mitm.left().path_to(lm);
    auto right_path = mitm.right().path_to(rm);
    if (!left_path || !right_path) {
        std::exit(0);
        return std::nullopt;
    }

    if (meet_out) *meet_out = meet;
    return static_cast<int>(left_path->size() + right_path->size());
}

struct Bucket {
    int length{0};
    std::vector<SO6> targets;
};

std::vector<Bucket> build_buckets() {
    suppress_indicators = true;

    std::vector<tbb::concurrent_unordered_set<SO6>> buckets(static_cast<size_t>(kMaxTargetLen + 1));

    std::mt19937_64 rng(1337);
    int attempts = 0;
    int inserted = 0;
    auto bucket_full = [&](int len)->bool{
        return buckets[static_cast<size_t>(len)].size() >= static_cast<size_t>(kMaxPerLength);
    };

    while (attempts < kMaxAttempts && inserted < kMaxPerLength * kMaxTargetLen) {
        ++attempts;
        SO6 target = random_target(rng);
        auto len_opt = mitm_length_for_target(target, kMaxTargetLen);
        if (!len_opt) continue;
        int len = *len_opt;
        if (len <= 0 || len > kMaxTargetLen) continue;
        if (bucket_full(len)) continue;
        buckets[static_cast<size_t>(len)].insert(target);
        ++inserted;
    }

    std::vector<Bucket> out;
    out.reserve(static_cast<size_t>(kMaxTargetLen));
    for (int len = 1; len <= kMaxTargetLen; ++len) {
        auto& set = buckets[static_cast<size_t>(len)];
        if (set.empty()) continue;
        Bucket b;
        b.length = len;
        b.targets.reserve(set.size());
        for (const auto& t : set) b.targets.push_back(t);
        out.push_back(std::move(b));
    }
    return out;
}

void register_benchmarks(const std::vector<Bucket>& buckets) {
    for (const auto& bucket : buckets) {
        const std::string name = "mitm/depth_" + std::to_string(bucket.length);
        benchmark::RegisterBenchmark(name, [bucket](benchmark::State& state) {
            suppress_indicators = true;
            stored_depth_max = static_cast<uint8_t>(kMaxTargetLen);

            std::size_t idx = 0;
            std::uint64_t successes = 0;
            std::uint64_t failures = 0;
            std::uint64_t depth_sum = 0;

            for (auto _ : state) {
                const SO6& target = bucket.targets[idx];
                idx = (idx + 1) % bucket.targets.size();

                MITM mitm(SO6::identity(), target);
                auto meet_opt = generate_mitm_until_match(mitm);
                benchmark::DoNotOptimize(meet_opt);
            }

            const double runs = static_cast<double>(successes + failures);
            const double succ = successes ? static_cast<double>(successes) : 1.0;
            state.counters["runs"] = runs;
            state.counters["successes"] = static_cast<double>(successes);
            state.counters["failures"] = static_cast<double>(failures);
            state.counters["success_rate"] = successes / (runs > 0.0 ? runs : 1.0);
            state.counters["bucket_len"] = static_cast<double>(bucket.length);
            state.counters["bucket_size"] = static_cast<double>(bucket.targets.size());
            state.counters["expected_depth"] = static_cast<double>(bucket.length);
            state.counters["avg_depth_measured"] = successes ? static_cast<double>(depth_sum) / succ : 0.0;
        })->Unit(benchmark::kMillisecond);
    }
}

} // namespace

int main(int argc, char** argv) {
    suppress_indicators = true;
    auto buckets = build_buckets();

    benchmark::Initialize(&argc, argv);
    register_benchmarks(buckets);
    if (benchmark::ReportUnrecognizedArguments(argc, argv)) return 1;
    benchmark::RunSpecifiedBenchmarks();
    return 0;
}
