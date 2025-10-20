/**
 * @file main.cpp
 * @brief Entry point for T-operator product generation and benchmarking.
 *
 * Builds lookup tables (LUT) of SO6 matrices up to a configured T-depth and
 * iterates across layers with parallel progress reporting.
 */

#include <tbb/task_group.h>
#include <tbb/parallel_for_each.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/global_control.h>
#include <util/progress_tracker.hpp>
#include <atomic>
#include <iomanip>
#include <fstream>
#include <cstdlib>
#include <csignal>
#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/graph/LUT.hpp" // Rooted SO6 BFS/LUT
#include "util/io_utils.hpp"
#include "algo/Generate.hpp"
#include <fstream>
#include <string>
#include <sstream>
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
    #if (EXACT_FREQ_NONE==1)
        return "none (scan/sort-6 on the fly)";
    #elif (EXACT_FREQ_COLS_ONLY==1)
        return "columns-only (rows on-the-fly)";
    #else
        return "rows+columns (stored maps)";
    #endif
    };
    auto hash_backend = [](){
    #if defined(EXACT_USE_ANKERL)
        return "ankerl::unordered_dense";
    #elif defined(EXACT_USE_BOOST)
        return "boost::unordered_*";
    #else
        return "std::unordered_*";
    #endif
    };

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
    print_bool("log_scaling", log_scaling);
    print_bool("plot_scaling", plot_scaling);
    print_u8("stored_depth_max", stored_depth_max);
    print_u8("target_T_count", target_T_count);
    std::cout << "======================================\n";

    // Limit oneTBB parallelism to the requested thread count
    // Keep this object alive for the duration of the computation.
    tbb::global_control gc(tbb::global_control::max_allowed_parallelism,
                           static_cast<std::size_t>(std::max<uint8_t>(1, THREADS)));

    std::vector<double> rss_by_layer_mb;                     // Capture memory per finalized layer
    std::vector<double> per_t_time_s;                        // Runtime by T (seconds)
    std::vector<size_t> per_t_mem_bytes;                     // Delta memory by T (bytes)
    LUT gen_set = algo::create_lookup_table(SO6::identity(), &rss_by_layer_mb, &per_t_time_s, &per_t_mem_bytes); // Build LUT and record metrics

    // No additional per-layer parallel loop here; create_lookup_table already built layers.
    indicators::show_console_cursor(true);

    // Optionally log scaling CSV and plot via gnuplot
    if (log_scaling || plot_scaling) {
        const char* csv_name = "scaling.csv";
        // Always (re)write CSV if either flag is set
        std::ofstream csv(csv_name, std::ios::out | std::ios::trunc);
        csv << "T,runtime_s,delta_mem_bytes\n";
        size_t rows = per_t_time_s.size();
        for (size_t i = 0; i < rows; ++i) {
            double tval = (i < per_t_time_s.size() ? per_t_time_s[i] : 0.0);
            size_t dmem = (i < per_t_mem_bytes.size() ? per_t_mem_bytes[i] : 0);
            csv << (i+1) << "," << std::fixed << std::setprecision(6) << tval << "," << dmem << "\n";
        }
        csv.close();

        if (plot_scaling) {
            const char* gp_name = "scaling.gnuplot";
            std::ofstream gp(gp_name, std::ios::out | std::ios::trunc);
            gp << "set datafile separator comma\n";
            gp << "set grid\n";
            gp << "set term pngcairo size 1200,800\n";
            gp << "set output 'scaling.png'\n";
            gp << "set xlabel 'T'\n";
            gp << "set ylabel 'Runtime (s)'\n";
            gp << "set y2label 'Delta Memory (MB)'\n";
            gp << "set ytics nomirror\n";
            gp << "set y2tics\n";
            // Log scales: skip zero/negative values so gnuplot doesn't error
            gp << "set logscale y\n";
            gp << "set logscale y2\n";
            // Use small epsilons so zeros still appear and the line remains connected on log axes
            gp << "eps_rt = 1e-9\n";
            gp << "eps_mb = 1.0/(1024.0*1024.0)\n"; // one byte in MB
            gp << "plot '" << csv_name << "' using 1:( $2>0 ? $2 : eps_rt ) axes x1y1 with linespoints title 'Runtime (s)',\\\n";
            gp << "     '" << csv_name << "' using 1:( ($3>0 ? ($3/1024.0/1024.0) : eps_mb) ) axes x1y2 with linespoints title 'Delta Mem (MB)'\n";
            gp.close();
        }
    }

    return 0;
}
