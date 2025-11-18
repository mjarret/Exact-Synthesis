// Benchmark: MITM search on precomputed SO6 targets.
//
// Targets (and their known actual depths) are generated offline by
// `mitm_seed_gen` into benchmarks/mitm_seeds.txt, then reused here as
// deterministic seeds so that benchmark work is dominated by MITM itself.

#include <benchmark/benchmark.h>

#include <array>
#include <cstdint>
#include <fstream>
#include <random>
#include <sstream>
#include <string>
#include <vector>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "ds/MITM.hpp"

namespace {

struct Seed {
    int id{0};
    int actual_depth{0};
    int dl{0};
    int dr{0};
    std::vector<uint8_t> walk; // T sequence used to build the target from identity
};

const std::vector<Seed>& load_seeds(const std::string& path = "benchmarks/mitm_seeds.txt") {
    static std::vector<Seed> seeds;
    static bool loaded = false;
    if (loaded) return seeds;
    loaded = true;

    std::ifstream in(path);
    if (!in) {
        std::cerr << "[mitm_bench] warning: could not open seed file '" << path << "'\n";
        return seeds;
    }

    std::string line;
    while (std::getline(in, line)) {
        if (line.empty()) continue;
        if (line[0] == '#') continue;
        std::istringstream iss(line);

        Seed s;
        int walk_len = 0;
        if (!(iss >> s.id >> s.actual_depth >> s.dl >> s.dr >> walk_len)) continue;
        if (walk_len <= 0) continue;

        s.walk.resize(static_cast<size_t>(walk_len));
        bool ok = true;
        for (int i = 0; i < walk_len; ++i) {
            int t = 0;
            if (!(iss >> t)) { ok = false; break; }
            if (t < 0 || t > 14) { ok = false; break; }
            s.walk[static_cast<size_t>(i)] = static_cast<uint8_t>(t);
        }
        if (!ok) continue;
        seeds.push_back(std::move(s));
    }
    return seeds;
}

SO6 target_from_seed(const Seed& s) {
    SO6 cur = SO6::identity();
    for (uint8_t t : s.walk) {
        cur = T_OperatorRuntime(t) * cur;
    }
    return cur;
}

std::uint64_t total_elements(const LUT& lut) {
    std::uint64_t total = 0;
    for (const auto& layer : lut.layers()) {
        total += static_cast<std::uint64_t>(layer.size());
    }
    return total;
}

} // namespace

// Args:
//   arg0 = search_depth  (per-side MITM depth limit; should be >= seed search depth)
static void BM_MITM_Seeds(benchmark::State& state) {
    const int search_depth = static_cast<int>(state.range(0));

    const auto& seeds = load_seeds();
    if (seeds.empty()) {
        state.SkipWithError("No MITM seeds loaded; run mitm_seed_gen first");
        return;
    }

    suppress_indicators = true; // avoid interactive progress bars in benchmarks
    stored_depth_max = static_cast<uint8_t>(search_depth);

    std::uint64_t successes = 0;
    std::uint64_t failures  = 0;
    std::uint64_t sum_actual_depth = 0;
    std::uint64_t sum_known_depth  = 0;
    std::uint64_t sum_dl = 0;
    std::uint64_t sum_dr = 0;
    std::uint64_t sum_walk_len = 0;
    std::uint64_t sum_left_nodes = 0;
    std::uint64_t sum_right_nodes = 0;

    std::size_t idx = 0;

    for (auto _ : state) {
        const Seed& s = seeds[idx];
        idx = (idx + 1) % seeds.size();

        SO6 target = target_from_seed(s);

        MITM mitm(SO6::identity(), target);
        auto meet_opt = generate_mitm_until_match(mitm);
        if (!meet_opt) {
            ++failures;
            continue;
        }

        const SO6& meet = *meet_opt;
        auto left_path_opt  = mitm.left().path_to(meet);
        auto right_path_opt = mitm.right().path_to(meet);
        if (!left_path_opt || !right_path_opt) {
            ++failures;
            continue;
        }

        const int dl = static_cast<int>(left_path_opt->size());
        const int dr = static_cast<int>(right_path_opt->size());
        const int actual_depth = dl + dr;

        sum_actual_depth += static_cast<std::uint64_t>(actual_depth);
        sum_known_depth  += static_cast<std::uint64_t>(s.actual_depth);
        sum_dl           += static_cast<std::uint64_t>(dl);
        sum_dr           += static_cast<std::uint64_t>(dr);
        sum_walk_len     += static_cast<std::uint64_t>(s.walk.size());
        sum_left_nodes   += total_elements(mitm.left());
        sum_right_nodes  += total_elements(mitm.right());
        ++successes;

        benchmark::DoNotOptimize(mitm.left());
        benchmark::DoNotOptimize(mitm.right());
        benchmark::DoNotOptimize(meet);
        benchmark::ClobberMemory();
    }

    const double total_runs = static_cast<double>(successes + failures);
    const double succ = successes ? static_cast<double>(successes) : 1.0;

    state.counters["runs"]          = total_runs;
    state.counters["seed_count"]    = static_cast<double>(seeds.size());
    state.counters["successes"]     = static_cast<double>(successes);
    state.counters["failures"]      = static_cast<double>(failures);
    state.counters["success_rate"]  = successes / (total_runs > 0.0 ? total_runs : 1.0);
    state.counters["avg_actual_d"]  = static_cast<double>(sum_actual_depth) / succ;
    state.counters["avg_known_d"]   = static_cast<double>(sum_known_depth) / succ;
    state.counters["avg_walk_len"]  = static_cast<double>(sum_walk_len) / succ;
    state.counters["avg_dl"]        = static_cast<double>(sum_dl) / succ;
    state.counters["avg_dr"]        = static_cast<double>(sum_dr) / succ;
    state.counters["avg_left_nodes"]  = static_cast<double>(sum_left_nodes) / succ;
    state.counters["avg_right_nodes"] = static_cast<double>(sum_right_nodes) / succ;
}

// A few representative search depths (per side).
BENCHMARK(BM_MITM_Seeds)
    ->Name("mitm/seeds")
    ->Arg(8)
    ->Arg(10)
    ->Arg(12);

BENCHMARK_MAIN();
