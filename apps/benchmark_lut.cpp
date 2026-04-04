/**
 * @file benchmark_lut.cpp
 * @brief Compare single-T (15 alphabet) vs paired-TT (165 alphabet) LUT construction.
 *
 * Reports per-layer sizes, growth rates, build time, and canonical_form() call counts.
 *
 * Usage:
 *   ./benchmark_lut.out                                          # identity, depth 6 vs 3
 *   ./benchmark_lut.out --depth_t 6 --depth_tt 3 --seeds 100    # 100 random seeds
 *   ./benchmark_lut.out --random_steps 32 --seeds 10            # deeper random roots
 */

#include <iostream>
#include <iomanip>
#include <chrono>
#include <cstdint>
#include <cmath>
#include <random>
#include <string>
#include <vector>
#include <numeric>
#include <algorithm>
#include <atomic>

#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "so6/TT_Operator.hpp"
#include "ds/LUT.hpp"
#include "algo/Generate.hpp"
#include "config/Globals.hpp"

extern std::atomic<uint64_t> g_canonical_form_calls;

struct BenchConfig {
    int depth_t  = 6;
    int depth_tt = 3;
    int seeds = 100;
    int random_steps = 16;
};

static BenchConfig parse_args(int argc, char** argv) {
    BenchConfig cfg;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "--depth_t" && i + 1 < argc)       cfg.depth_t = std::stoi(argv[++i]);
        else if (arg == "--depth_tt" && i + 1 < argc)  cfg.depth_tt = std::stoi(argv[++i]);
        else if (arg == "--seeds" && i + 1 < argc)     cfg.seeds = std::stoi(argv[++i]);
        else if (arg == "--random_steps" && i + 1 < argc) cfg.random_steps = std::stoi(argv[++i]);
    }
    return cfg;
}

static SO6 make_random_root(uint64_t seed, int steps) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> d(0, 14);
    SO6 cur = SO6::identity();
    int last = -1;
    for (int i = 0; i < steps; ++i) {
        int t = d(rng);
        while (t == last) t = d(rng);
        cur = T_OperatorRuntime(static_cast<uint8_t>(t)) * cur;
        last = t;
    }
    cur.last_T = 15;
    return cur;
}

struct RunResult {
    std::vector<size_t> layer_sizes;
    double elapsed_sec;
    uint64_t canonical_calls;
};

static RunResult run_single_T(const SO6& root, int depth) {
    stored_depth_max = static_cast<uint8_t>(depth);
    target_T_count = static_cast<uint8_t>(depth + 1);
    suppress_indicators = true;

    g_canonical_form_calls.store(0, std::memory_order_relaxed);
    auto t0 = std::chrono::high_resolution_clock::now();
    LUT lut = algo::create_lookup_table(root);
    auto t1 = std::chrono::high_resolution_clock::now();

    std::vector<size_t> sizes;
    for (const auto& layer : lut.layers()) sizes.push_back(layer.size());

    return {sizes, std::chrono::duration<double>(t1 - t0).count(),
            g_canonical_form_calls.load(std::memory_order_relaxed)};
}

static RunResult run_TT(const SO6& root, int depth) {
    stored_depth_max = static_cast<uint8_t>(depth);
    target_T_count = static_cast<uint8_t>(depth * 2 + 1);
    suppress_indicators = true;

    g_canonical_form_calls.store(0, std::memory_order_relaxed);
    auto t0 = std::chrono::high_resolution_clock::now();
    LUT lut = algo::create_lookup_table_TT(root);
    auto t1 = std::chrono::high_resolution_clock::now();

    std::vector<size_t> sizes;
    for (const auto& layer : lut.layers()) sizes.push_back(layer.size());

    return {sizes, std::chrono::duration<double>(t1 - t0).count(),
            g_canonical_form_calls.load(std::memory_order_relaxed)};
}

static double median(std::vector<double>& v) {
    std::sort(v.begin(), v.end());
    size_t n = v.size();
    return (n % 2 == 0) ? (v[n/2 - 1] + v[n/2]) / 2.0 : v[n/2];
}

static double mean(const std::vector<double>& v) {
    return std::accumulate(v.begin(), v.end(), 0.0) / v.size();
}

int main(int argc, char** argv) {
    auto cfg = parse_args(argc, argv);

    std::cout << "=== LUT Benchmark ===" << std::endl;
    std::cout << "Seeds: " << cfg.seeds
              << "  random_steps: " << cfg.random_steps << std::endl;
    std::cout << "Single-T depth: " << cfg.depth_t
              << "  TT depth: " << cfg.depth_tt << std::endl;
    std::cout << std::endl;

    // --- Micro-benchmark: apply cost ---
    {
        SO6 root = SO6::identity();
        constexpr int N_ITERS = 100000;

        // Single T: apply one T gate N times
        auto t0 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N_ITERS; ++i) {
            SO6 tmp = T_OperatorRuntime(static_cast<uint8_t>(i % 15)) * root;
            (void)tmp;
        }
        auto t1 = std::chrono::high_resolution_clock::now();
        double t_ns = std::chrono::duration<double, std::nano>(t1 - t0).count() / N_ITERS;

        // TT: apply one TT compound N times
        auto t2 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N_ITERS; ++i) {
            SO6 tmp = TT_OperatorRuntime(static_cast<uint8_t>(i % 165)) * root;
            (void)tmp;
        }
        auto t3 = std::chrono::high_resolution_clock::now();
        double tt_ns = std::chrono::duration<double, std::nano>(t3 - t2).count() / N_ITERS;

        // canonical_form: measure standalone cost
        // First apply a T to get a non-identity matrix with sentinel canonical
        SO6 test_mat = T_OperatorRuntime(0) * root;
        auto t4 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N_ITERS; ++i) {
            SO6 tmp = test_mat;  // copy (canonical is sentinel after T apply)
            tmp.canonical_reset();
            tmp.row_perm_lh();   // triggers canonical_form()
        }
        auto t5 = std::chrono::high_resolution_clock::now();
        double canon_ns = std::chrono::duration<double, std::nano>(t5 - t4).count() / N_ITERS;

        // hash recompute
        auto t6 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N_ITERS; ++i) {
            SO6 tmp = test_mat;
            tmp.set_element(0, 0, tmp.get_element(0, 0)); // invalidate hash
            (void)tmp.primary_hash();  // triggers recompute_hash()
        }
        auto t7 = std::chrono::high_resolution_clock::now();
        double hash_ns = std::chrono::duration<double, std::nano>(t7 - t6).count() / N_ITERS;

        // SO6 copy
        auto t8 = std::chrono::high_resolution_clock::now();
        for (int i = 0; i < N_ITERS; ++i) {
            SO6 tmp = root;
            (void)tmp;
        }
        auto t9 = std::chrono::high_resolution_clock::now();
        double copy_ns = std::chrono::duration<double, std::nano>(t9 - t8).count() / N_ITERS;

        std::cout << "Micro-benchmark (" << N_ITERS << " iterations):" << std::endl;
        std::cout << "  SO6 copy:       " << std::fixed << std::setprecision(0) << copy_ns << " ns" << std::endl;
        std::cout << "  T  apply:       " << t_ns << " ns  (includes copy + canonical_reset)" << std::endl;
        std::cout << "  TT apply:       " << tt_ns << " ns  (fused, includes copy + canonical_reset)" << std::endl;
        std::cout << "  canonical_form: " << canon_ns << " ns" << std::endl;
        std::cout << "  recompute_hash: " << hash_ns << " ns" << std::endl;
        std::cout << std::endl;
        std::cout << "  TT/T apply:     " << std::setprecision(2) << tt_ns / t_ns << "x" << std::endl;
        std::cout << "  canon/T_apply:  " << std::setprecision(1) << canon_ns / t_ns << "x" << std::endl;
        std::cout << "  canon/TT_apply: " << canon_ns / tt_ns << "x" << std::endl;
        std::cout << std::endl;

        // Estimate time breakdown for BFS per frontier matrix
        double t_apply_total  = 15.0 * t_ns;
        double tt_apply_total = 165.0 * tt_ns;
        double t_canon_total  = 15.0 * canon_ns;   // upper bound: every candidate triggers canon
        double tt_canon_total = 165.0 * canon_ns;
        std::cout << "  Estimated per-frontier-matrix cost:" << std::endl;
        std::cout << "    T:  apply=" << std::setprecision(0) << t_apply_total << "ns"
                  << "  canon(max)=" << t_canon_total << "ns"
                  << "  ratio=" << std::setprecision(1) << t_canon_total / t_apply_total << "x" << std::endl;
        std::cout << "    TT: apply=" << std::setprecision(0) << tt_apply_total << "ns"
                  << "  canon(max)=" << tt_canon_total << "ns"
                  << "  ratio=" << std::setprecision(1) << tt_canon_total / tt_apply_total << "x" << std::endl;
        std::cout << std::endl;
    }

    // --- Identity root first ---
    {
        SO6 root = SO6::identity();
        auto r_t  = run_single_T(root, cfg.depth_t);
        auto r_tt = run_TT(root, cfg.depth_tt);

        std::cout << "Identity root:" << std::endl;
        std::cout << "  T  layers: ";
        for (auto s : r_t.layer_sizes) std::cout << s << " ";
        std::cout << " time=" << std::fixed << std::setprecision(2) << r_t.elapsed_sec
                  << "s  canon=" << r_t.canonical_calls << std::endl;
        std::cout << "  TT layers: ";
        for (auto s : r_tt.layer_sizes) std::cout << s << " ";
        std::cout << " time=" << r_tt.elapsed_sec
                  << "s  canon=" << r_tt.canonical_calls << std::endl;

        // T growth rates
        std::cout << "  T  growth: ";
        for (size_t i = 1; i < r_t.layer_sizes.size(); ++i) {
            double g = r_t.layer_sizes[i-1] > 0
                ? double(r_t.layer_sizes[i]) / r_t.layer_sizes[i-1] : 0;
            std::cout << std::fixed << std::setprecision(1) << g << "x ";
        }
        std::cout << std::endl;
        // TT growth rates
        std::cout << "  TT growth: ";
        for (size_t i = 1; i < r_tt.layer_sizes.size(); ++i) {
            double g = r_tt.layer_sizes[i-1] > 0
                ? double(r_tt.layer_sizes[i]) / r_tt.layer_sizes[i-1] : 0;
            std::cout << std::fixed << std::setprecision(1) << g << "x ";
        }
        std::cout << std::endl << std::endl;
    }

    // --- Random roots ---
    std::vector<double> t_times, tt_times, time_ratios, canon_ratios;
    // Per-layer growth rates for T and TT
    std::vector<std::vector<double>> t_growths(cfg.depth_t);
    std::vector<std::vector<double>> tt_growths(cfg.depth_tt);

    std::cout << std::setw(6) << "seed"
              << std::setw(10) << "T_time"
              << std::setw(10) << "TT_time"
              << std::setw(10) << "ratio"
              << std::setw(12) << "T_canon"
              << std::setw(12) << "TT_canon"
              << std::setw(10) << "c_ratio"
              << std::setw(12) << "T_last"
              << std::setw(12) << "TT_last"
              << std::endl;

    for (int seed = 0; seed < cfg.seeds; ++seed) {
        SO6 root = make_random_root(static_cast<uint64_t>(seed), cfg.random_steps);
        auto r_t  = run_single_T(root, cfg.depth_t);
        auto r_tt = run_TT(root, cfg.depth_tt);

        double tr = r_tt.elapsed_sec / r_t.elapsed_sec;
        double cr = double(r_tt.canonical_calls) / double(r_t.canonical_calls);
        t_times.push_back(r_t.elapsed_sec);
        tt_times.push_back(r_tt.elapsed_sec);
        time_ratios.push_back(tr);
        canon_ratios.push_back(cr);

        // Collect growth rates
        for (int d = 1; d <= cfg.depth_t && d < (int)r_t.layer_sizes.size(); ++d) {
            if (r_t.layer_sizes[d-1] > 0)
                t_growths[d-1].push_back(double(r_t.layer_sizes[d]) / r_t.layer_sizes[d-1]);
        }
        for (int d = 1; d <= cfg.depth_tt && d < (int)r_tt.layer_sizes.size(); ++d) {
            if (r_tt.layer_sizes[d-1] > 0)
                tt_growths[d-1].push_back(double(r_tt.layer_sizes[d]) / r_tt.layer_sizes[d-1]);
        }

        size_t t_last  = r_t.layer_sizes.empty()  ? 0 : r_t.layer_sizes.back();
        size_t tt_last = r_tt.layer_sizes.empty() ? 0 : r_tt.layer_sizes.back();

        std::cout << std::setw(6) << seed
                  << std::setw(10) << std::fixed << std::setprecision(2) << r_t.elapsed_sec
                  << std::setw(10) << r_tt.elapsed_sec
                  << std::setw(10) << tr
                  << std::setw(12) << r_t.canonical_calls
                  << std::setw(12) << r_tt.canonical_calls
                  << std::setw(10) << std::setprecision(2) << cr
                  << std::setw(12) << t_last
                  << std::setw(12) << tt_last
                  << std::endl;
    }

    // --- Summary ---
    std::cout << std::endl << "=== SUMMARY (" << cfg.seeds << " random seeds) ===" << std::endl;

    std::cout << "Time:  T median=" << std::fixed << std::setprecision(2) << median(t_times)
              << "s  TT median=" << median(tt_times) << "s" << std::endl;
    std::cout << "       T mean=" << mean(t_times)
              << "s  TT mean=" << mean(tt_times) << "s" << std::endl;
    std::cout << "Ratio: median=" << median(time_ratios)
              << "x  mean=" << mean(time_ratios) << "x" << std::endl;
    std::cout << "Canon: median=" << std::setprecision(2) << median(canon_ratios)
              << "x  mean=" << mean(canon_ratios) << "x" << std::endl;

    std::cout << std::endl << "Growth rates (median across seeds):" << std::endl;
    for (int d = 0; d < cfg.depth_t; ++d) {
        if (!t_growths[d].empty())
            std::cout << "  T  layer " << (d+1) << ": " << std::setprecision(2) << median(t_growths[d]) << "x" << std::endl;
    }
    for (int d = 0; d < cfg.depth_tt; ++d) {
        if (!tt_growths[d].empty())
            std::cout << "  TT layer " << (d+1) << ": " << std::setprecision(2) << median(tt_growths[d]) << "x" << std::endl;
    }

    return 0;
}
