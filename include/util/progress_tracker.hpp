#ifndef PROGRESS_TRACKER_HPP
#define PROGRESS_TRACKER_HPP

#include <iostream>
#include <iomanip>
#include <sstream>
#include <chrono>
#include <fstream>
#include <unistd.h>
#include <atomic>
#include <algorithm>
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>

namespace indicators {

    inline indicators::DynamicProgress<indicators::ProgressBar> progress_bars;
    inline std::atomic<bool> kill_signal_received{false}; // Global flag to track if bars are killed

    /**
     * @brief Handles progress bars for tracking computation.
     */
    class ProgressTracker {
    private:
        size_t current_tracker;
        size_t matrix_counter;
        std::chrono::_V2::high_resolution_clock::time_point start_time;

        // Read resident set size (RSS) in bytes from /proc/self/statm (Linux)
        static inline size_t process_rss_bytes() {
            long resident_pages = 0;
            std::ifstream statm("/proc/self/statm");
            if (statm.good()) {
                long size_pages = 0; // unused
                statm >> size_pages >> resident_pages;
            }
            long page_size = sysconf(_SC_PAGESIZE);
            if (resident_pages <= 0 || page_size <= 0) return 0;
            return static_cast<size_t>(resident_pages) * static_cast<size_t>(page_size);
        }

        // Private method to add a job tracker
        size_t add_job_to_tracker(int t_count, size_t total_work = 100) {
            std::ostringstream ss;
            ss << std::left << std::setw(7) << std::min(t_count + 1, 9999) << std::setfill(' ');
            std::string formatted_count = ss.str();
            auto terminal_width = indicators::terminal_size().second;
            auto bar = std::make_unique<indicators::ProgressBar>(
                indicators::option::BarWidth{terminal_width - 80},
                indicators::option::ForegroundColor{indicators::Color::green},
                indicators::option::ShowElapsedTime{true},
                indicators::option::ShowRemainingTime{true},
                indicators::option::ShowPercentage{true},
                indicators::option::MaxProgress{total_work},
                indicators::option::PrefixText{"T= " + formatted_count},
                indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}
            );

            return progress_bars.push_back(std::move(bar));
        }

        // Private method to add a matrix tracker
        size_t add_matrix_tracker(size_t matrix_counter = 15) {
            std::ostringstream ss;
            ss << std::setw(20) << std::setfill(' ') << " Finding Matrices: ";
            auto terminal_width = indicators::terminal_size().second;
            auto bar = std::make_unique<indicators::ProgressBar>(
                indicators::option::BarWidth{terminal_width - 90},
                indicators::option::ForegroundColor{indicators::Color::red},
                indicators::option::PrefixText{ss.str()},
                indicators::option::MaxProgress(matrix_counter),
                indicators::option::FontStyles{
                std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}
            );

            return progress_bars.push_back(std::move(bar));
        }

    public:
        /**
         * @brief Constructor that creates bars and starts the timer.
         * @param current_t_count The T count for job tracking.
         */
        size_t total_work = 100;

        ProgressTracker(int current_t_count, size_t total_work_ = 100, size_t matrix_counter_ = 15) {
            total_work = total_work_;
            matrix_counter = matrix_counter_;
            current_tracker = add_job_to_tracker(current_t_count, total_work);
            matrix_counter = add_matrix_tracker(matrix_counter);
            start_time = std::chrono::high_resolution_clock::now();
        }

        /**
         * @brief Completes the progress bars and finalizes them.
         * @param result_size The size of the final computed result.
         */
        void complete(size_t result_size) {
            if(kill_signal_received.load()) return;

            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                std::chrono::high_resolution_clock::now() - start_time
            ).count();

            std::ostringstream oss;
            oss << std::setw(10) << std::setfill(' ') << " Time: " 
                << std::scientific << std::setprecision(2) << duration / 1000.0 << "s";

            progress_bars[current_tracker].set_option(indicators::option::PostfixText{oss.str()});
            progress_bars[current_tracker].set_progress(total_work);
            // Also show memory usage alongside discovered count
            {
                size_t rss = process_rss_bytes();
                double rss_mb = rss / (1024.0 * 1024.0);
                std::ostringstream m;
                m << result_size << " | RSS: " << std::fixed << std::setprecision(1) << rss_mb << " MB";
                progress_bars[matrix_counter].set_option(indicators::option::PostfixText{m.str()});
            }
            progress_bars[matrix_counter];
        }

        inline void set_progress(size_t progress, size_t set_size) {
            if(kill_signal_received) return;
            // Update discovered count and current process RSS
            size_t rss = process_rss_bytes();
            double rss_mb = rss / (1024.0 * 1024.0);
            std::ostringstream m;
            m << set_size << " | RSS: " << std::fixed << std::setprecision(1) << rss_mb << " MB";
            progress_bars[matrix_counter].set_option(indicators::option::PostfixText{m.str()});
            progress_bars[matrix_counter].set_progress(progress);
            progress_bars[current_tracker].set_progress(progress);
        }

        /**
         * @brief Accessors for bar indices.
         */
        size_t get_current_tracker() const { return current_tracker; }
        size_t get_matrix_counter() const { return matrix_counter; }

    };

} // namespace progress_tracker

#endif // PROGRESS_TRACKER_HPP
