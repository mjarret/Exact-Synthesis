#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <chrono>
#include <string>

// Threading and performance tracking
extern uint8_t THREADS;
extern std::chrono::high_resolution_clock::time_point tcount_init_time;
extern std::chrono::duration<double> timeelapsed;

// Pattern handling and search settings
extern std::string pattern_file;
extern std::string case_file;
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

class Globals {
    public:
        static void setParameters(int argc, char *argv[]);
        static void configure();
};
#endif // GLOBALS_HPP
