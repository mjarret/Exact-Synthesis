/**
 * @file io_utils.hpp
 * @brief Console progress, signal handling, and data I/O helpers.
 */
#ifndef IO_UTILS_HPP
#define IO_UTILS_HPP

#include <iostream>
#include <fstream>
#include <filesystem>
#include <iomanip>
#include <sstream>
#include <thread>
#include <atomic>
#include <csignal>
#include <chrono>
#include <tbb/concurrent_queue.h>
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include "util/progress_tracker.hpp"

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

    inline std::ofstream prepare_T_count_io(const int t) {
        std::filesystem::path dir = "./data";
        if (!std::filesystem::exists(dir)) {
            std::filesystem::create_directory(dir);
        }
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
