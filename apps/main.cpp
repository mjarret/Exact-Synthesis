/**
 * @file main.cpp
 * @brief Entry point for T-operator product generation and benchmarking.
 *
 * Builds lookup tables (LUT) of SO6 matrices up to a configured T-depth and
 * iterates across layers with parallel progress reporting.
 */

#include <tbb/global_control.h>
#include <tbb/parallel_for_each.h>
#include <iomanip>
#include <fstream>
#include <exception>
#include <cstdlib>
#include <csignal>
#include <chrono>
#include <random>
#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "ds/LUT.hpp" // Rooted SO6 BFS/LUT
#include "util/io_utils.hpp"
#include "util/lut_export.hpp"
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

SO6 random_root(uint64_t seed, int steps, uint64_t& out_seed) {
    std::mt19937_64 rng(seed);
    std::uniform_int_distribution<int> d(0, 14);
    SO6 cur = SO6::identity();
    for (int i = 0; i < steps; ++i) {
        cur = T_OperatorRuntime(static_cast<uint8_t>(d(rng))) * cur;
    }
    cur.last_T = 15; // ensure root has no forbidden previous T
    out_seed = seed;
    return cur;
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

    const std::string root_spec = Globals::root_spec();
    std::string root_label;
    SO6 root = SO6::identity();
    uint64_t root_seed = 0;
    constexpr int kRandomRootSteps = 64;

    if (root_spec.empty() || root_spec == "identity") {
        root_label = "identity";
    } else if (root_spec == "random") {
        std::random_device rd;
        root = random_root(rd(), kRandomRootSteps, root_seed);
        root_label = "random (steps=" + std::to_string(kRandomRootSteps)
                     + ", seed=" + std::to_string(root_seed) + ")";
    } else if (!root_spec.empty() && root_spec.front() == '{') {
        root = SO6(root_spec);
        root.last_T = 15;
        root_label = "custom (matrix)";
    } else {
        std::cerr << "[warn] unsupported --root value '" << root_spec
                  << "'; using identity\n";
        root_label = "identity (unsupported: " + root_spec + ")";
    }

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
    std::cout << "  DyadicSqrt2 layout: numerator=" << sizeof(DyadicSqrt2().kBitsForNumerator)
              << " (int=" << sizeof(DyadicSqrt2().kBitsForIntC) << ", sqrt2=" << sizeof(DyadicSqrt2().kBitsForSqrt2C) << ")"
              << ", denom_exp=" << sizeof(DyadicSqrt2().kBitsForDenomExp) << "\n";
    // GPU/OpenCL info omitted in CPU-only build
    // Inputs/flags
    std::cout << "  " << std::left << std::setw(22) << "root" << ": " << root_label << "\n";
    print_bool("suppress_indicators", suppress_indicators);
    print_bool("use_tt (paired)", use_tt);
    print_bool("verbose", verbose);
    print_u8("stored_depth_max", stored_depth_max);
    print_u8("target_T_count", target_T_count);
    std::cout << "======================================\n";

    // Limit oneTBB parallelism to the requested thread count
    // Keep this object alive for the duration of the computation.
    tbb::global_control gc(tbb::global_control::max_allowed_parallelism,
                           static_cast<std::size_t>(std::max<uint8_t>(1, THREADS)));

    LUT gen_set = use_tt
        ? algo::create_lookup_table_TT(root, nullptr, nullptr)
        : algo::create_lookup_table(root, nullptr, nullptr);
    if (!use_tt) algo::extend_lookup_table_bf(gen_set); // BF extension only for single-T mode

    try {
        auto export_summary = lut_export::write_lut_database(gen_set);
        double mib = static_cast<double>(export_summary.bytes_written) / (1024.0 * 1024.0);
        std::cout << "LUT export wrote " << export_summary.records << " SO6 entries to "
                  << export_summary.path << " (" << std::fixed << std::setprecision(2)
                  << mib << " MiB across " << export_summary.bucket_count << " buckets)\n";
    } catch (const std::exception& ex) {
        std::cerr << "LUT export failed: " << ex.what() << "\n";
    }

    indicators::show_console_cursor(true);

    // Metrics (time/memory) are now handled and displayed by ProgressTracker; CSV/plot generation removed.

    return 0;
}
