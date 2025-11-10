#ifndef PROGRESS_TRACKER_HPP
#define PROGRESS_TRACKER_HPP

#include <atomic>
#include <chrono>
#include <cstddef>

#ifdef EXACT_DISABLE_INDICATORS

// Compile-time elimination: provide no-op stubs when indicators are disabled.
namespace indicators {
    inline std::atomic<bool> kill_signal_received{false};
    inline void show_console_cursor(bool) {}
    class ProgressBar {
    public:
        void set_progress(std::size_t) {}
        template <typename T>
        void set_option(const T&) {}
    };
    class ProgressTracker {
    public:
        size_t total_work = 100;
        ProgressTracker(int, size_t, size_t) {}
        void complete(size_t) {}
        inline void set_progress(size_t, size_t) {}
        inline void on_finalize_started(size_t) {}
        inline void on_finalize_finished() {}
        inline indicators::ProgressBar* get_finalize_bar() { return nullptr; }
        inline indicators::ProgressBar* get_io_bar() { return nullptr; }
    };
} // namespace indicators

#else

#include <iomanip>
#include <sstream>
#include <algorithm>
#include <indicators/dynamic_progress.hpp>
#include <indicators/progress_bar.hpp>
#include "sys/memory.hpp"
#include "config/Globals.hpp"

namespace indicators {

    inline std::atomic<bool> kill_signal_received{false}; // Global flag to track if bars are killed

    // Simple state carried between layers for predictions
    inline size_t g_prev_matrices_found{0};
    inline double g_prev_memory_slope_bpm{0.0}; // bytes per matrix from previous layer

    /**
     * @brief Minimal progress tracker with two sub-bars and simple predictions.
     */
    class ProgressTracker {
    private:
        indicators::DynamicProgress<indicators::ProgressBar> bars_{}; // per‑T progress manager
        size_t main_idx;
        size_t find_idx;
        size_t finalize_idx;

        size_t total_work_local{100};
        std::chrono::high_resolution_clock::time_point start_time;

        size_t predicted_mats{0};
        size_t finalize_count{0};
        size_t rss_at_begin{0};
        int current_t_{0};

        bool enabled_{true};

        static inline size_t rss_bytes() { return getProcessRSSBytes(); }

        // v1-style main bar
        size_t add_job_to_tracker(int t_count, size_t total_work) {
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
            return bars_.push_back(std::move(bar));
        }

        // Finding Matrices sub-bar
        size_t add_find_tracker(size_t max_counter) {
            std::ostringstream ss;
            ss << std::setw(20) << std::setfill(' ') << " Finding Matrices: ";
            auto terminal_width = indicators::terminal_size().second;
            auto bar = std::make_unique<indicators::ProgressBar>(
                indicators::option::BarWidth{terminal_width - 90},
                indicators::option::ForegroundColor{indicators::Color::red},
                indicators::option::PrefixText{ss.str()},
                indicators::option::MaxProgress{std::max<size_t>(max_counter, 1)},
                indicators::option::FontStyles{
                    std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}
            );
            return bars_.push_back(std::move(bar));
        }

        // Finalizing LUT sub-bar
        size_t add_finalize_tracker(size_t max_counter) {
            std::ostringstream ss;
            ss << std::setw(20) << std::setfill(' ') << " Finalizing LUT: ";
            auto terminal_width = indicators::terminal_size().second;
            auto bar = std::make_unique<indicators::ProgressBar>(
                indicators::option::BarWidth{terminal_width - 90},
                indicators::option::ForegroundColor{indicators::Color::yellow},
                indicators::option::PrefixText{ss.str()},
                indicators::option::MaxProgress{std::max<size_t>(max_counter, 1)},
                indicators::option::FontStyles{
                    std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}
            );
            return bars_.push_back(std::move(bar));
        }

        static inline double expected_bytes(size_t predicted, double slope_bpm) {
            if (predicted == 0 || slope_bpm <= 0.0) return 0.0;
            return slope_bpm * static_cast<double>(predicted);
        }

        static inline std::string fmt_bytes(double bytes) {
            const double KB = 1024.0, MB = KB * 1024.0, GB = MB * 1024.0, TB = GB * 1024.0;
            double div = 1.0; const char* unit = "B";
            if (bytes >= TB) { div = TB; unit = "TB"; }
            else if (bytes >= GB) { div = GB; unit = "GB"; }
            else if (bytes >= MB) { div = MB; unit = "MB"; }
            else if (bytes >= KB) { div = KB; unit = "kB"; }
            std::ostringstream s; s << std::fixed << std::setprecision(3) << (bytes / div) << ' ' << unit; return s.str();
        }

    public:
        size_t total_work = 100;

        ProgressTracker(int current_t_count, size_t total_work_ = 100, size_t matrix_counter_ = 15) {
            total_work = total_work_;
            total_work_local = total_work_;
            start_time = std::chrono::high_resolution_clock::now();
            current_t_ = current_t_count;

            enabled_ = !suppress_indicators;

            if (!enabled_) {
                // Capture baselines for consistency, but don't set up bars
                rss_at_begin = rss_bytes();
                predicted_mats = 0;
                finalize_count = 0;
                return;
            }

            // Only show bars for the current T; hide completed bars to avoid re-printing
            bars_.set_option(indicators::option::HideBarWhenComplete{true});

            main_idx = add_job_to_tracker(current_t_count, total_work);

            predicted_mats = g_prev_matrices_found ? g_prev_matrices_found * 8 : 0;
            find_idx = add_find_tracker(predicted_mats ? predicted_mats : matrix_counter_);
            finalize_idx = add_finalize_tracker(matrix_counter_);

            // Initialize sub-bars
            {
                std::ostringstream m; m << 0 << "/" << (predicted_mats ? predicted_mats : 0);
                bars_[find_idx].set_option(indicators::option::PostfixText{m.str()});
                bars_[find_idx].set_progress(0);
            }
            {
                double exp_b = expected_bytes(predicted_mats, g_prev_memory_slope_bpm);
                std::ostringstream m; m << fmt_bytes(0.0) << "/" << (exp_b > 0.0 ? fmt_bytes(exp_b) : std::string("-- B"));
                bars_[finalize_idx].set_option(indicators::option::PostfixText{m.str()});
                bars_[finalize_idx].set_progress(0);
            }

            // Baseline RSS before this T-count begins
            rss_at_begin = rss_bytes();
        }

        void complete(size_t result_size) {
            if (!enabled_ || kill_signal_received.load()) return;
            // v1-style time postfix on the main bar
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::high_resolution_clock::now() - start_time).count();
            std::ostringstream oss;
            oss << std::setw(10) << std::setfill(' ') << " Time: " << std::scientific << std::setprecision(2) << duration / 1000.0 << "s";
            bars_[main_idx].set_option(indicators::option::PostfixText{oss.str()});
            bars_[main_idx].set_progress(total_work_local);

            // Mark finding bar complete so it is hidden for the next T
            {
                std::ostringstream m; m << result_size << "/" << (predicted_mats ? predicted_mats : 0);
                bars_[find_idx].set_option(indicators::option::PostfixText{m.str()});
                // Force completion to keep DynamicProgress from redrawing prior bars
                size_t find_max = std::max<std::size_t>(result_size, 1);
                bars_[find_idx].set_option(indicators::option::MaxProgress{find_max});
                bars_[find_idx].set_progress(find_max);
            }

            // Ensure the dynamic manager clears any live bar lines before we print snapshots
            bars_.print_progress();

            // Permanently print a static snapshot of the three bars so the screen scrolls naturally.
            auto termw = indicators::terminal_size().second;
            // Main snapshot (suppress built-in elapsed/remaining to avoid 00:00s)<00:00s)
            {
                std::ostringstream sp;
                sp << std::left << std::setw(7) << std::min(current_t_ + 1, 9999) << std::setfill(' ');
                std::string formatted = sp.str();
                indicators::ProgressBar line(
                    indicators::option::BarWidth{termw - 80},
                    indicators::option::ForegroundColor{indicators::Color::green},
                    indicators::option::ShowPercentage{true},
                    indicators::option::MaxProgress{total_work_local},
                    indicators::option::PrefixText{"T= " + formatted}
                );
                line.set_option(indicators::option::PostfixText{oss.str()});
                line.set_progress(total_work_local); // prints a newline via print_progress()
            }
            // Finding snapshot
            {
                std::ostringstream pfx; pfx << std::setw(20) << std::setfill(' ') << " Finding Matrices: ";
                indicators::ProgressBar line(
                    indicators::option::BarWidth{termw - 90},
                    indicators::option::ForegroundColor{indicators::Color::red},
                    indicators::option::MaxProgress{std::max<std::size_t>(result_size, 1)},
                    indicators::option::PrefixText{pfx.str()}
                );
                std::ostringstream fm; fm << result_size << "/" << (predicted_mats ? predicted_mats : 0);
                line.set_option(indicators::option::PostfixText{fm.str()});
                line.set_progress(std::max<std::size_t>(result_size, 1));
            }
            // Finalize snapshot
            {
                std::ostringstream pfx; pfx << std::setw(20) << std::setfill(' ') << " Finalizing LUT: ";
                indicators::ProgressBar line(
                    indicators::option::BarWidth{termw - 90},
                    indicators::option::ForegroundColor{indicators::Color::yellow},
                    indicators::option::MaxProgress{std::max<std::size_t>(finalize_count, 1)},
                    indicators::option::PrefixText{pfx.str()}
                );
                size_t rss_end = rss_bytes();
                double used = (rss_end > rss_at_begin) ? static_cast<double>(rss_end - rss_at_begin) : 0.0;
                double exp_b = expected_bytes(predicted_mats, g_prev_memory_slope_bpm);
                std::ostringstream pm; pm << fmt_bytes(used) << "/" << (exp_b > 0.0 ? fmt_bytes(exp_b) : std::string("-- B"));
                line.set_option(indicators::option::PostfixText{pm.str()});
                line.set_progress(std::max<std::size_t>(finalize_count, 1));
            }
        }

        inline void set_progress(size_t progress, size_t set_size) {
            if (!enabled_ || kill_signal_received.load()) return;
            // Update finding bar progress and postfix
            std::ostringstream m; m << set_size << "/" << (predicted_mats ? predicted_mats : 0);
            bars_[find_idx].set_option(indicators::option::PostfixText{m.str()});
            bars_[find_idx].set_progress(std::min(set_size, predicted_mats ? predicted_mats : set_size));
            // Update main bar progress
            bars_[main_idx].set_progress(progress);
        }

        // Sub-bar controls for finalization
        inline void on_finalize_started(size_t found_count) {
            if (!enabled_) return;
            finalize_count = found_count;
            if (finalize_count > 0) bars_[finalize_idx].set_option(indicators::option::MaxProgress{finalize_count});
            double exp_b = expected_bytes(predicted_mats, g_prev_memory_slope_bpm);
            std::ostringstream m; m << fmt_bytes(0.0) << "/" << (exp_b > 0.0 ? fmt_bytes(exp_b) : std::string("-- B"));
            bars_[finalize_idx].set_option(indicators::option::PostfixText{m.str()});
            bars_[finalize_idx].set_progress(0);
        }

        inline void on_finalize_finished() {
            if (!enabled_) return;
            size_t rss_end = rss_bytes();
            double used = (rss_end > rss_at_begin) ? static_cast<double>(rss_end - rss_at_begin) : 0.0;
            double exp_b = expected_bytes(predicted_mats, g_prev_memory_slope_bpm);
            std::ostringstream m; m << fmt_bytes(used) << "/" << (exp_b > 0.0 ? fmt_bytes(exp_b) : std::string("-- B"));
            bars_[finalize_idx].set_option(indicators::option::PostfixText{m.str()});
            bars_[finalize_idx].set_progress(finalize_count);

            // Update global predictors for the next layer
            if (finalize_count > 0) {
                g_prev_matrices_found = finalize_count;
                g_prev_memory_slope_bpm = used / static_cast<double>(finalize_count);
            }
        }

        inline indicators::ProgressBar* get_finalize_bar() { return enabled_ ? &bars_[finalize_idx] : nullptr; }
        inline indicators::ProgressBar* get_io_bar() { return nullptr; }

        // Accessors for compatibility (not used externally in current code except tests)
        size_t get_current_tracker() const { return main_idx; }
        size_t get_matrix_counter() const { return find_idx; }
    };

} // namespace indicators

#endif // EXACT_DISABLE_INDICATORS

#endif // PROGRESS_TRACKER_HPP
