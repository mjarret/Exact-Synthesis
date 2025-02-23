// 
//  * T Operator Product Generation Main File
//  * @file main.cpp
//  * @author Michael Jarret
//  * @author Andrew Glaudell
//  * @author Sam Mendelson
//  * @author Mingzhen Tian
//  * @version 11/22/24
//  

#include <chrono>
#include <unordered_set>
#include <tbb/concurrent_unordered_set.h>
#include <tbb/concurrent_queue.h>
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for_each.h>
#include <tbb/parallel_for.h>
#include <tbb/parallel_for_each.h>
#include <tbb/global_control.h>
#include <tbb/concurrent_unordered_set.h>
#include <progress_tracker.hpp>
#include <atomic>
#include <csignal>
#include "./Globals.hpp"
#include "./SO6.hpp"
#include <io_utils.hpp>
#include <utils.hpp>

tbb::global_control c(tbb::global_control::max_allowed_parallelism, std::max(static_cast<unsigned int>(1), std::thread::hardware_concurrency()-1));
tbb::concurrent_queue<std::string> output_queue; // Thread-safe queue for output

/**
 * @brief Store specific cosets T_0{curr} based on the current T count and free multiply depth.
 * This method saves a subset of the SO6 objects to the generating set, which are used in later iterations.
 * 
 * @param curr_T_count The current T count in the main computation loop.
 * @param free_multiply_depth The depth until which free multiplication is performed.
 * @param num_generating_sets The total number of generating sets.
 * @param current The current set of SO6 objects.
 * @param generating_set Reference to an array of vectors of SO6 objects to store the generated sets.
 */
std::vector<SO6> storeCosets(int curr_T_count, tbb::concurrent_unordered_set<SO6>& current)
{
    std::vector<SO6> generating_set(current.begin(), current.end());
    tbb::concurrent_vector<SO6> filtered_set;
    // Filter elements using a parallel_for loop
    tbb::parallel_for(size_t(0), generating_set.size(), [&](size_t i) {
        if (generating_set[i].last_T != 0) {
            filtered_set.push_back(generating_set[i].left_multiply_by_T(0)); // Thread-safe addition
        }
    });

    return std::vector<SO6>(filtered_set.begin(), filtered_set.end());
}

std::vector<SO6> convert_set_to_vector(const tbb::concurrent_unordered_set<SO6>& set) {
    std::vector<SO6> vec(set.size());

    tbb::parallel_for(tbb::blocked_range<size_t>(0, set.size()),
        [&](const tbb::blocked_range<size_t>& r) {
            auto it = set.begin();
            std::advance(it, r.begin());
            std::move(it, std::next(it, r.size()), vec.begin() + r.begin());  
        }
    );

    return vec;
}


tbb::concurrent_unordered_set<SO6> get_next_T_count(const tbb::concurrent_unordered_set<SO6>& prior, const tbb::concurrent_unordered_set<SO6>& current, indicators::ProgressTracker& bars) {
    
    // Initialize the counters
    std::atomic<size_t> global_counter{0};
    std::atomic_flag progress_lock = ATOMIC_FLAG_INIT;

    size_t interval_size = (current.size() * 15)/100;
    tbb::enumerable_thread_specific<size_t> local_counters;

    tbb::concurrent_unordered_set<SO6> next;
    tbb::parallel_for_each(current.begin(), current.end(), [&](const SO6& S) {
            auto& local_counter = local_counters.local();

            uint8_t last_T = S.last_T;
            for (int T = 0; T < last_T; ++T, ++local_counter)
            {           
                SO6 toInsert = S.left_multiply_by_T(T);
                if(prior.find(toInsert) == prior.end()) next.insert(toInsert);
            }

            for (int T = last_T + 1; T < 15; ++T, ++local_counter)
            {
                SO6 toInsert = S.left_multiply_by_T(T);
                if(prior.find(toInsert) == prior.end()) next.insert(toInsert);
            }

            if (local_counter >= interval_size && !progress_lock.test_and_set(std::memory_order_acquire)) { 
                bars.set_progress(global_counter.fetch_add(local_counter, std::memory_order_relaxed), next.size());
                local_counter = 0;
                progress_lock.clear(std::memory_order_release); // Allow other threads to enter
            }
        }
    );

    return next;
}

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
    indicators::show_console_cursor(false);
    io_utils::initialize_progress_tracker();

    // Register signal handlers
    signal(SIGINT, io_utils::signal_handler);             // Handle Ctrl+C
    signal(SIGTERM, io_utils::signal_handler);            // Handle termination signal

    auto program_init_time = io_utils::now();             // Begin timekeeping

    Globals::setParameters(argc, argv);         // Initialize parameters to command line argument
    Globals::configure();                       // Configure the globals to remove inconsistencies

    std::vector<tbb::concurrent_unordered_set<SO6>> generating_set = {{}, {SO6::identity()}};

    for (int curr_T_count = 0; curr_T_count < stored_depth_max; ++curr_T_count)
    {        
        indicators::ProgressTracker bars(curr_T_count, generating_set[curr_T_count + 1].size() * 15, generating_set[curr_T_count + 1].size() * 15);    
        generating_set.push_back(get_next_T_count(generating_set[curr_T_count], generating_set[curr_T_count + 1], bars));
        bars.complete(generating_set[curr_T_count + 2].size());
    }

    // std::vector<SO6> to_compute(generating_set[stored_depth_max + 1].begin(), current.end());
    // to_compute.shrink_to_fit();
    const size_t set_size = generating_set[stored_depth_max+1].size();
    size_t interval_size = 1 + set_size / 100;
    std::atomic_flag progress_lock = ATOMIC_FLAG_INIT;

    for (int curr_T_count = stored_depth_max; curr_T_count < target_T_count; ++curr_T_count)
    {
        // Initialize the counters
        std::atomic<size_t> counter{0};   

        size_t tmp = generating_set[2+curr_T_count - stored_depth_max].size();
        indicators::ProgressTracker bars(curr_T_count, set_size, set_size*tmp);    

        tbb::enumerable_thread_specific<size_t> local_counters(0);
        tbb::parallel_for_each(generating_set[stored_depth_max + 1].begin(), generating_set[stored_depth_max + 1].end(), [&](const SO6& S) {

            // Perform computation
            if (curr_T_count == stored_depth_max) {
                for (const SO6 &G : generating_set[2])  {
                    SO6 N = S.left_multiply_by_T(0);
                }
            } else {
                for (const SO6 &G : (generating_set[2 + curr_T_count - stored_depth_max])) {
                    SO6 N = G * S;
                }
            }

            if (++local_counters.local() >= interval_size && !progress_lock.test_and_set(std::memory_order_acquire)) { 
                bars.set_progress(counter.fetch_add(local_counters.local(), std::memory_order_relaxed), set_size*tmp);
                local_counters.local() = 0;
                progress_lock.clear(std::memory_order_release); // Allow other threads to enter
            }
        });

        tbb::this_task_arena::isolate([&] { bars.complete(set_size*tmp); });
    }

    io_utils::progress_tracker.print_progress();
    indicators::show_console_cursor(true);
    return 0;
}
