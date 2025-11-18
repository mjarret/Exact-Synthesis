/**
 * @file main.cpp
 * @brief Entry point for T-operator product generation and benchmarking.
 *
 * Builds lookup tables (LUT) of SO6 matrices up to a configured T-depth and
 * iterates across layers with parallel progress reporting.
 */

#include <tbb/global_control.h>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <csignal>
#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "ds/LUT.hpp" // Rooted SO6 BFS/LUT
#include "util/io_utils.hpp"
#include "algo/Generate.hpp"
#include <string>
#include <thread>

namespace {
std::string read_cpu_model() {
    std::ifstream in("/proc/cpuinfo");
    std::string line;
    while (std::getline(in, line)) {
        if (line.rfind("model name", 0) == 0) {
            auto pos = line.find(':');
            if (pos != std::string::npos) {
                std::string v = line.substr(pos + 1);
                // trim leading spaces
                size_t i = v.find_first_not_of(" \t");
                return (i == std::string::npos) ? v : v.substr(i);
            }
        }
    }
    return std::string("unknown");
}

std::size_t read_mem_total_bytes() {
    std::ifstream in("/proc/meminfo");
    std::string k; std::size_t kb=0; std::string unit;
    while (in >> k >> kb >> unit) {
        if (k == "MemTotal:") {
            return kb * 1024ULL;
        }
        // skip rest of line
        std::string rest; std::getline(in, rest);
    }
    return 0;
}

} // namespace
#include <iostream>


// tbb::global_control c(tbb::global_control::max_allowed_parallelism, std::max(static_cast<unsigned int>(1), std::thread::hardware_concurrency()-1));

// Generation helpers moved to algo:: (no logic changes)


/**
 * @brief The main function of the program.
 *
 * This function is the entry point of the program. It initializes the necessary parameters,
 * reads pattern and case files, performs various operations on the data, and outputs the results.
 *
 * @param argc The number of command-line arguments.
 * @param argv An array of command-line arguments.
 * @return The exit status of the program.
 */
int main(int argc, char **argv)
{
    const size_t available_memory = getAvailableMemory();
    
    // Register signal handlers
    signal(SIGINT, io_utils::signal_handler);             // Handle Ctrl+C
    signal(SIGTERM, io_utils::signal_handler);            // Handle termination signal

    auto program_init_time = io_utils::now();             // Begin timekeeping

    Globals::setParameters(argc, argv);         // Initialize parameters to command line argument
    Globals::configure();                       // Configure the globals to remove inconsistencies

    // --- Configuration summary ---
    auto print_bool = [](const char* k, bool v){ std::cout << "  " << std::left << std::setw(22) << k << ": " << (v?"yes":"no") << "\n"; };
    auto print_u8   = [](const char* k, uint8_t v){ std::cout << "  " << std::left << std::setw(22) << k << ": " << unsigned(v) << "\n"; };
    auto freq_policy = [](){
        // Frequency policy simplified: always compute on the fly.
        return "none (scan/sort-6 on the fly)";
    };
    auto hash_backend = [](){ return kHashBackendName; };

    std::cout << "=== Exact-Synthesis Configuration ===\n";
    // Build/runtime
    print_u8("threads", THREADS);
    std::cout << "  " << std::left << std::setw(22) << "hash backend" << ": " << hash_backend() << "\n";
    std::cout << "  " << std::left << std::setw(22) << "frequency policy" << ": " << freq_policy() << "\n";
    // No GPU accelerator integrated (CPU-only build)
    // CPU/GPU details
    std::cout << "  " << std::left << std::setw(22) << "cpu" << ": " << read_cpu_model() << "\n";
    std::cout << "  " << std::left << std::setw(22) << "hw concurrency" << ": " << std::thread::hardware_concurrency() << "\n";
    // Memory
    auto mem_total = read_mem_total_bytes();
    std::cout << "  " << std::left << std::setw(22) << "ram total" << ": " << std::fixed << std::setprecision(2)
              << (double(mem_total)/ (1024.0*1024.0*1024.0)) << " GiB\n";
    std::cout << "  " << std::left << std::setw(22) << "ram available" << ": "
              << (double(available_memory)/ (1024.0*1024.0*1024.0)) << " GiB\n";
    // Problem size / storage
    std::cout << "  " << std::left << std::setw(22) << "sizeof(SO6)" << ": " << SO6::size_bytes() << " bytes\n";
    std::cout << "  Z2 layout: numerator=" << bits_for_numerator
              << " (int=" << bits_for_int_c << ", sqrt2=" << bits_for_sqrt2_c << ")"
              << ", denom_exp=" << bits_for_denom_exp << "\n";
    // GPU/OpenCL info omitted in CPU-only build
    // Inputs/flags
    print_bool("suppress_indicators", suppress_indicators);
    print_bool("verbose", verbose);
    print_u8("stored_depth_max", stored_depth_max);
    print_u8("target_T_count", target_T_count);
    std::cout << "======================================\n";

    // Limit oneTBB parallelism to the requested thread count
    // Keep this object alive for the duration of the computation.
    tbb::global_control gc(tbb::global_control::max_allowed_parallelism,
                           static_cast<std::size_t>(std::max<uint8_t>(1, THREADS)));

    LUT gen_set = algo::create_lookup_table(SO6::identity(), nullptr, nullptr); // Build LUT; ProgressTracker handles metrics

    // ---- Test-drive DFS extension iterator over the last layer ----
    // Continue "generating" (without insertion) by enumerating all T-chains
    // of length 1..dfs_depth from each leaf, in parallel, with progress bars.
    {
        // Heuristic depth: if target_T_count > stored_depth_max, extend by the remainder; else 1.
        const int dfs_depth = std::max<int>(1, static_cast<int>(target_T_count) - static_cast<int>(stored_depth_max));
        const std::size_t leaf_count = gen_set.current().size();

        // Predict total extensions = leaves * sum_{k=1..d} 14^k  (no immediate repeats)
        auto sum14 = [&](int d)->std::size_t {
            std::size_t acc = 0, pow = 14;
            for (int k = 1; k <= d; ++k) { acc += pow; if (k+1 <= d) pow *= 14; }
            return acc;
        };
        const std::size_t predicted_total = leaf_count * sum14(dfs_depth);

        // Progress tracker labeled DFS; use predicted_total as both work and matrix counters
        std::unique_ptr<indicators::ProgressTracker> dfs_bar =
            std::make_unique<indicators::ProgressTracker>(static_cast<int>(stored_depth_max), predicted_total, predicted_total, "DFS");

        std::atomic<std::size_t> processed{0};
        const std::size_t update_every = std::max<std::size_t>(predicted_total / 200, 1024);

        auto range = gen_set.dfs_extensions(dfs_depth);

        tbb::parallel_for_each(range.begin(), range.end(), [&](const SO6& /*state*/){
            // No-op body for now; just count states to exercise the iterator in parallel
            std::size_t n = processed.fetch_add(1, std::memory_order_relaxed) + 1;
            if (!suppress_indicators && (n % update_every == 0)) {
                dfs_bar->set_progress(std::min(n, predicted_total), n);
            }
        });

        // Final update + completion snapshot
        dfs_bar->set_progress(std::min<std::size_t>(processed, predicted_total), processed);
        dfs_bar->complete(processed);
    }

    indicators::show_console_cursor(true);

    // Metrics (time/memory) are now handled and displayed by ProgressTracker; CSV/plot generation removed.

    return 0;
}
