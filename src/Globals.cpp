#include "config/Globals.hpp"
#include <string>
#include <iostream>
#include <chrono>
#include <thread>
#include <cstdlib>
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <utility>
#include <type_traits>
#include <cmath>

// Lightweight CLI: use header-only cxxopts (same as sibling Schelling workspace)
#include <third_party/cxxopts.hpp>

// Threading and performance tracking
uint8_t THREADS; //store maximum number of threads here
std::chrono::high_resolution_clock::time_point tcount_init_time = std::chrono::high_resolution_clock::now();    // Initialize with current time
std::chrono::duration<double> timeelapsed = std::chrono::duration<double>::zero();                              // Initialize as zero

// Pattern handling and search settings     
std::string pattern_file = "";
std::string case_file = "";
std::string root_string ="";
// SO6 root = SO6::identity();

// Configuration and state variables
uint8_t target_T_count = 8;            
uint8_t stored_depth_max = 255;
uint8_t num_gen_sets = 1;
bool verbose = false;
bool cases_flag = false;
bool suppress_indicators = false;
bool log_scaling = false;
bool plot_scaling = false;

void Globals::setParameters(int argc, char *argv[]) {
    try {
        // defaults
        int tcount_param = 8;
        int stored_depth_param = 0;
        std::string threads_s = std::to_string(std::max(1u, std::thread::hardware_concurrency() - 1));
        bool verbose_flag = false;
        bool no_indicators_flag = false;
        std::string root_s;
        bool log_scaling_flag = false;
        bool plot_scaling_flag = false;

        cxxopts::Options desc("Exact-Synthesis", "Exact-Synthesis options");
        desc.add_options()
            ("h,help", "show help")
            ("t,tcount", "target T count", cxxopts::value<int>(tcount_param))
            ("s,stored_depth", "maximum stored depth", cxxopts::value<int>(stored_depth_param))
            ("f,pattern_file", "pattern file", cxxopts::value<std::string>(pattern_file))
            ("v,verbose", "enable verbosity", cxxopts::value<bool>(verbose_flag))
            ("n,threads", "number of threads (number or 'max')", cxxopts::value<std::string>(threads_s))
            ("r,root", "search tree root circuit string", cxxopts::value<std::string>(root_s))
            ("c,cases", "looking for specific cases (not used)", cxxopts::value<bool>(cases_flag))
            ("no-indicators", "suppress interactive progress indicators", cxxopts::value<bool>(no_indicators_flag))
            ("log-scaling", "write scaling CSV at end (scaling.csv)", cxxopts::value<bool>(log_scaling_flag))
            ("plot-scaling", "generate scaling plots with gnuplot (requires gnuplot)", cxxopts::value<bool>(plot_scaling_flag))
        ;

        auto result = desc.parse(argc, argv);
        if (result.count("help")) {
            std::cout << desc.help() << "\n";
            std::exit(EXIT_SUCCESS);
        }

        // commit parsed values
        target_T_count = static_cast<uint8_t>(std::max(1, tcount_param));
        stored_depth_max = static_cast<uint8_t>(std::max(0, stored_depth_param));
        num_gen_sets = stored_depth_max;
        // optional fields
        if (!root_s.empty()) root_string = root_s;

        // threads: numeric or "max"
        if (threads_s == "max") {
            THREADS = static_cast<uint8_t>(std::max(1u, std::thread::hardware_concurrency()));
        } else {
            try {
                int tn = std::stoi(threads_s);
                THREADS = static_cast<uint8_t>(tn);
            } catch (...) {
                THREADS = static_cast<uint8_t>(std::max(1u, std::thread::hardware_concurrency() - 1));
            }
        }

        // runtime flags
        suppress_indicators = no_indicators_flag;
        log_scaling = log_scaling_flag;
        plot_scaling = plot_scaling_flag;
        verbose = verbose_flag;

    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        std::exit(EXIT_FAILURE);
    }
}

// Configure run based on global parameters
void Globals::configure()
{
    if (stored_depth_max == 0 || stored_depth_max > target_T_count-1) stored_depth_max = target_T_count-1;
    if (stored_depth_max < std::ceil((float)target_T_count/2)) stored_depth_max = (uint8_t) std::ceil((float)target_T_count/2); 
    
    if (THREADS > std::thread::hardware_concurrency()) {
        THREADS = std::thread::hardware_concurrency();
    } else if(THREADS <= 0) {
        THREADS = 1;
    }

    // No direct stdout here; a consolidated configuration summary
    // is printed at program start in apps/main.cpp.
}
