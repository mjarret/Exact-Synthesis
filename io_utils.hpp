#ifndef IO_UTILS_HPP
#define IO_UTILS_HPP

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>
#include <csignal>
#include <mutex>
#include <chrono>
#include <tbb/concurrent_queue.h>
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include "include/progress_tracker.hpp"

// Global variables for I/O handling
namespace io_utils {
    inline std::atomic<bool> done(false);
    inline tbb::concurrent_queue<std::string> output_queue;
    inline indicators::DynamicProgress<indicators::ProgressBar> progress_tracker;

    // Progress Tracker Functions
    inline void initialize_progress_tracker() {
        progress_tracker.set_option(indicators::option::HideBarWhenComplete{false});
    }

    inline size_t add_job_to_tracker(int t_count) {
        std::ostringstream ss;
        ss << std::left << std::setw(7) << std::min(t_count + 1, 9999) << std::setfill(' ');
        std::string formatted_count = ss.str();
        auto terminal_width = indicators::terminal_size().second;
        auto bar = std::make_unique<indicators::ProgressBar>(
                        indicators::option::BarWidth{terminal_width - 80},
                        indicators::option::ForegroundColor{indicators::Color::green},
                        indicators::option::ShowElapsedTime{true},
                        indicators::option::ShowRemainingTime{true},
                        indicators::option::PrefixText{"T= " + formatted_count},
                        indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}});

        return progress_tracker.push_back(std::move(bar));
    }

    inline size_t add_matrix_tracker() {
        std::ostringstream ss;
        ss << std::setw(20) << std::setfill(' ') << " Finding Matrices: ";
        auto terminal_width = indicators::terminal_size().second;    
        auto bar = std::make_unique<indicators::ProgressBar>(
                    indicators::option::BarWidth{terminal_width - 70},
                    indicators::option::ForegroundColor{indicators::Color::red},
                    indicators::option::PrefixText{ss.str()},
                    indicators::option::FontStyles{
                std::vector<indicators::FontStyle>{indicators::FontStyle::bold}});

        return progress_tracker.push_back(std::move(bar));
    }

    inline size_t add_io_tracker() {
        std::ostringstream ss;
        ss << std::setw(20) << std::setfill(' ') << " I/O Progress: ";
        auto bar = std::make_unique<indicators::ProgressBar>(
                    indicators::option::BarWidth{40},
                    indicators::option::ForegroundColor{indicators::Color::blue},
                    indicators::option::PrefixText{ss.str()},
                    indicators::option::FontStyles{
                std::vector<indicators::FontStyle>{indicators::FontStyle::bold}});

        return progress_tracker.push_back(std::move(bar));
    }

    // Threaded Output Handling
    inline void io_thread_function(std::ofstream& output_file) {
        std::string data;
        while (!done || !output_queue.empty()) {
            if (output_queue.try_pop(data)) {
                output_file << data << std::endl;
            }
        }
    }

    // Signal Handling
    inline void signal_handler(int signum) {
        indicators::kill_signal_received.store(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(100)); 
        std::cout << "Interrupt signal (" << signum << ") received. Exiting..." << std::endl;
        indicators::show_console_cursor(true);
        std::exit(signum);
    }

    // // File I/O Handling
    // inline void read_pattern_file(const std::string& pattern_file_path) {
    //     if(pattern_file_path.empty()) return;

    //     std::cout << "[Read] Reading patterns from " << pattern_file_path << std::endl;
    //     std::ifstream patternFile(pattern_file_path);

    //     if (!patternFile.is_open()) {
    //         std::cerr << "Failed to open pattern file: " << pattern_file_path << std::endl;
    //         return;
    //     }

    //     std::string line;
    //     while (std::getline(patternFile, line)) {
    //         pattern currentPattern(line);
    //         int case_num = currentPattern.case_num();
    //         if(case_num == 0) continue;
    //         insert_all_permutations(currentPattern);
    //     }
    //     patternFile.close();

    //     pattern identityPattern = pattern::identity();
    //     pattern_set.unsafe_erase(identityPattern);
    //     pattern_set.unsafe_erase(identityPattern.pattern_mod());
    //     std::cout << "[Finished] Loaded " << pattern_set.size() << " non-identity patterns." << std::endl;
    // }

    inline std::ofstream prepare_T_count_io(const int t, uint8_t &stored_depth_max, uint8_t &target_T_count) {
        std::string file_string = "./data/" + std::to_string(t) + ".dat";
        std::ofstream of(file_string, std::ios::out | std::ios::trunc);
        return of;
    }

    inline void finish_io(const uint& matrices_found, const bool b, std::ofstream& of) {
        of.close();
    }

    // Time Handling
    inline std::chrono::_V2::high_resolution_clock::time_point now() {
        return std::chrono::_V2::high_resolution_clock::now();
    }

    inline std::string time_since(std::chrono::_V2::high_resolution_clock::time_point &s) {
        std::chrono::duration<double> duration = now() - s;
        int64_t time = std::chrono::duration_cast<std::chrono::milliseconds>(duration).count();
        if (time < 1000) return std::to_string(time).append("ms");
        if (time < 60000) return std::to_string((float)time / 1000).substr(0, 5).append("s");
        if (time < 3600000) return std::to_string((float)time / 60000).substr(0, 5).append("min");
        if (time < 86400000) return std::to_string((float)time / 3600000).substr(0, 5).append("hr");
        return std::to_string((float)time / 86400000).substr(0, 5).append("days");
    }
} // namespace io_utils

#endif // IO_UTILS_HPP
