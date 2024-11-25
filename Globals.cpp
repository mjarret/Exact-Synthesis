#include "Globals.hpp"
#include "utils.hpp"
#include <thread> 
#include <boost/program_options.hpp>
#include <tbb/concurrent_set.h>
#include <omp.h>

namespace po = boost::program_options;

// Threading and performance tracking
uint8_t THREADS; //store maximum number of threads here
omp_lock_t omp_lock;
std::chrono::high_resolution_clock::time_point tcount_init_time = std::chrono::high_resolution_clock::now(); // Initialize with current time
std::chrono::duration<double> timeelapsed = std::chrono::duration<double>::zero(); // Initialize as zero

// Pattern handling and search settings
tbb::concurrent_set<pattern> pattern_set;         
std::vector<pattern> cases;
std::string pattern_file = "";
std::string case_file = "";
std::string root_string ="";
SO6 root = SO6::identity();

// Configuration and state variables
uint8_t target_T_count = 8;            
uint8_t stored_depth_max = 255;
uint8_t num_gen_sets = 1;
bool cases_flag = false;

// // Counters
int counter_zero = 0;
int counter_odd = 0;
int counter_even = 0;

void Globals::setParameters(int argc, char *argv[]) {
    try {
        // Define options
        po::options_description desc("Allowed options");
        int tcount_param;
        int stored_depth_param;
        int threads_param;

        desc.add_options()
            ("help,h", "produce help message")
            ("tcount,t", po::value<int>(&tcount_param)->default_value(8), "target T count")
            ("stored_depth,s", po::value<int>(&stored_depth_param)->default_value(0), "maximum stored depth")
            ("pattern_file,f", po::value<std::string>(&pattern_file), "pattern file")
            ("verbose,v", po::bool_switch(), "enable verbosity")
            ("threads,n", po::value<std::string>()->default_value(std::to_string(std::thread::hardware_concurrency()-1)), "number of threads")
            ("root,r", po::value<std::string>(), "set the root of the search tree by specifying a circuit.")
            ("cases,c", po::bool_switch(&cases_flag), "flag to tell code whether we are looking for specific cases (not used).");
        po::variables_map vm;
        po::store(po::parse_command_line(argc, argv, desc), vm);
        po::notify(vm);

        target_T_count = (uint8_t) (std::max(1,tcount_param));
        stored_depth_max = (uint8_t) stored_depth_param;
        num_gen_sets = utils::num_generating_sets(target_T_count, stored_depth_max);
   
        if (vm.count("help")) {
            std::cout << desc << "\n";
            std::exit(EXIT_SUCCESS);
        }

        if (vm.count("threads")) {
            if(vm["threads"].as<std::string>() == "max") {
                THREADS = std::thread::hardware_concurrency();
            } else {
                THREADS = (uint8_t) std::stoi(vm["threads"].as<std::string>());
            }
        }


    } catch (std::exception& e) {
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
        // REIMPLEMENT ME
        // root = SO6::reconstruct_from_circuit_string(root_string);
        std::cout << "[Config] Root specified: " << root_string << "\n";
    }

    if (cases_flag) {
        std::cout << "[Config] Looking for specific cases.\n";
    } else {
        std::cout << "[Config] Looking for all cases.\n";
    }

    // if (verbose) {
    //     std::cout << "[Config] Verbose mode enabled.\n";
    // }

    // if (transpose_multiply) {
    //     std::cout << "[Config] Transpose multiply enabled.\n";
    // }

    // if (explicit_search_mode) {
    //     std::cout << "[Config] Explicit search mode enabled.\n";
    // }

    // if (saveResults) {
    //     std::cout << "[Config] Saving results to file.\n";
    // }
    // SO6 case_representative = SO6::reconstruct_from_circuit_string("5 2 0 6 12 1 4 13 12 4 2 9 0");
}


