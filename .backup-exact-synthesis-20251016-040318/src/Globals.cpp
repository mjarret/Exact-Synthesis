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
bool cases_flag = false;

void Globals::setParameters(int argc, char *argv[]) {
    try {
        // defaults
        int tcount_param = 8;
        int stored_depth_param = 0;
        std::string threads_s = std::to_string(std::max(1u, std::thread::hardware_concurrency() - 1));
        bool verbose_flag = false;
        std::string root_s;

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

        // unused global currently, but keep behavior
        (void)verbose_flag; // suppress unused warning if not used elsewhere

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

    // Output configuration
    std::cout << "[Config] Generating up to T=" << (int) target_T_count << ".\n";
    std::cout << "[Config] Storing at most T=" << (int) stored_depth_max << " in memory.\n";
    std::cout << "[Config] Running on " << (int) THREADS << " threads.\n";
    if (!pattern_file.empty()) {
        std::cout << "[Config] Searching for patterns in file " << pattern_file << "\n";
    } else {
        std::cout << "[Config] No pattern file.\n";
    }

    if (!case_file.empty()) {
        std::cout << "[Config] Cases file " << case_file << "\n";
        cases_flag=true;
    } 

    if (root_string.empty()) {
        // root = SO6::identity();
        std::cout << "[Config] No root specified. Using identity.\n";
    } else {
        // root = SO6::reconstruct_from_circuit_string(root_string);
        std::cout << "[Config] Root specified: " << root_string << "\n";
    }

    if (cases_flag) {
        std::cout << "[Config] Looking for specific cases.\n";
    } else {
        std::cout << "[Config] Looking for all cases.\n";
    }
}
