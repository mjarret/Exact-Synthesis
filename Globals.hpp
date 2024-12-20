// Globals.hpp

#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <chrono>
#include <omp.h>
#include <tbb/concurrent_set.h>
#include "pattern.hpp" // Assuming this is your custom class
#include "SO6.hpp"     // Assuming this is your custom class

// Threading and performance tracking
extern uint8_t THREADS;
extern omp_lock_t omp_lock;
extern std::chrono::high_resolution_clock::time_point tcount_init_time;
extern std::chrono::duration<double> timeelapsed;

// Pattern handling and search settings
extern tbb::concurrent_set<pattern> pattern_set;
extern std::string pattern_file;
extern std::string case_file;
extern SO6 root;
extern std::string root_string;

// Configuration and state variables
extern uint8_t target_T_count;
extern uint8_t stored_depth_max;
extern uint8_t num_gen_sets;
extern bool saveResults;
extern bool verbose;
extern bool transpose_multiply;
extern bool explicit_search_mode;
extern bool cases_flag;

// Counters
extern int counter_zero;
extern int counter_odd;
extern int counter_even;

class Globals {
    public:
        static void setParameters(int argc, char *argv[]);
        static void configure();
};
#endif // GLOBALS_HPP
