/**
 * @file bfs_cpu_bench.cpp
 * @brief CPU (TBB) BFS LUT benchmark with per-layer timing — mirrors the
 *        output format of the CUDA `bfs_gpu --depth N` binary so the two can
 *        be diffed side by side.
 *
 * Usage:
 *   ./bfs_cpu_bench --depth 10
 *   ./bfs_cpu_bench --depth 10 -n max     # use all cores (default)
 */

#include <tbb/global_control.h>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <thread>

#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "ds/LUT.hpp"
#include "algo/Generate.hpp"

int main(int argc, char** argv) {
    int depth = 10;
    std::string threads_spec = "max";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if      (a == "--depth" && i + 1 < argc) depth = std::atoi(argv[++i]);
        else if (a == "-n"      && i + 1 < argc) threads_spec = argv[++i];
        else if (a == "-h" || a == "--help") {
            printf("Usage: %s --depth <N> [-n <threads|max>]\n", argv[0]);
            return 0;
        }
    }

    // Configure TBB parallelism.
    unsigned hw = std::max(1u, std::thread::hardware_concurrency());
    unsigned nthreads = hw;
    if (threads_spec != "max") {
        try { nthreads = (unsigned)std::max(1, std::stoi(threads_spec)); }
        catch (...) { nthreads = hw; }
    }
    tbb::global_control ctl(tbb::global_control::max_allowed_parallelism, nthreads);

    // Suppress interactive progress bars (we do our own reporting).
    suppress_indicators = true;
    // Target T-count = depth + 1 (algorithm layers are 0..stored_depth_max).
    target_T_count   = static_cast<uint8_t>(depth + 1);
    stored_depth_max = static_cast<uint8_t>(depth);

    printf("Device: CPU (TBB, %u threads)\n", nthreads);
    printf("Depth: %d\n", depth);

    // Build LUT layer-by-layer with explicit timing.
    LUT gen(SO6::identity());
    printf("  Layer 0: 1 matrices\n");

    for (int d = 1; d <= depth; ++d) {
        auto t0 = std::chrono::high_resolution_clock::now();
        algo::get_next_T_count(gen, nullptr, nullptr, nullptr);
        gen.finalize_current_set(nullptr);
        auto t1 = std::chrono::high_resolution_clock::now();

        double sec = std::chrono::duration<double>(t1 - t0).count();
        size_t layer_n = 0;
        int layer_idx = 0;
        for (const auto& layer : gen.layers()) {
            if (layer_idx == d) { layer_n = layer.size(); break; }
            ++layer_idx;
        }
        printf("  Layer %d: %zu matrices  (%.3f s)\n", d, layer_n, sec);
        fflush(stdout);
        if (layer_n == 0) break;
    }
    return 0;
}
