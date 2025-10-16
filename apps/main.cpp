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
#include <util/progress_tracker.hpp>
#include <atomic>
#include <csignal>
#include "config/Globals.hpp"
#include "so6/SO6.hpp"
#include "so6/LUT.hpp" // Ensure this header file defines the LUT class
#include "util/io_utils.hpp"
#include "algo/Generate.hpp"


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

    LUT gen_set = algo::create_lookup_table();               // Initialize the generating set with the identity element as root

    const size_t set_size = gen_set.current().size();
    size_t interval_size = 1 + set_size / 100;
    std::atomic_flag progress_lock = ATOMIC_FLAG_INIT;
    tbb::task_group tg;


    int curr_T_count = stored_depth_max;
    for (auto gs : gen_set) {
        if(curr_T_count > target_T_count) break;

        // Initialize the counters
        std::atomic<size_t> counter{0};
        std::atomic<size_t> found{0};   
        tbb::enumerable_thread_specific<size_t> local_counters(0);
        tbb::enumerable_thread_specific<size_t> local_found(0);

        indicators::ProgressTracker bars(curr_T_count++, set_size, set_size*gs.size());
        
        // auto back_search = gen_set[curr_T_count - stored_depth_max - 1];

        tg.run_and_wait([&] {
            tbb::parallel_for_each(gen_set.current().begin(), gen_set.current().end(), [&](const SO6& S) {
                // Perform computation
                if (curr_T_count == stored_depth_max) 
                    for (const SO6 &G : gs) for (int i = 0; i < 15; i++) {
                        volatile SO6 N = S.left_multiply_by_T(i);
                        // if(!gen_set.contains(N)) local_found.local()++;
                        // local_found.local()++;
                    }
                else 
                    for (const SO6 &G : gs) {
                        volatile SO6 N = G * S;
                        // if(!gen_set.contains(N)) local_found.local()++;
                    }

                if (++local_counters.local() >= interval_size && !progress_lock.test_and_set(std::memory_order_acquire)) { 
                    counter.fetch_add(local_counters.local(), std::memory_order_relaxed);
                    found.fetch_add(local_found.local(), std::memory_order_relaxed);
                    local_counters.local() = 0;
                    local_found.local() = 0;
                    bars.set_progress(counter.load(std::memory_order_relaxed), found.load(std::memory_order_relaxed));
                    // bar.set_progress(counter.load(std::memory_order_relaxed));
                    progress_lock.clear(std::memory_order_release); // Allow other threads to enter
                }
            });
        });
        bars.complete(0);
    }
    
    indicators::show_console_cursor(true);

    return 0;
}
