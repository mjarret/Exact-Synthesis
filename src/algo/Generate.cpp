// Implementation moved from apps/main.cpp (no logic changes)
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for_each.h>
#include <atomic>

#include "algo/Generate.hpp"
#include "config/Globals.hpp"
#include "sys/memory.hpp"

namespace algo {

tbb::concurrent_unordered_set<SO6> get_next_T_count(LUT& gen_set, indicators::ProgressTracker* bars) {
    auto& current = gen_set.current();
    auto& prior = gen_set.prior();
    tbb::concurrent_unordered_set<SO6> next;

    // Accelerator path removed; CPU parallel path only

    std::atomic<size_t> global_counter{0};
    std::atomic_flag progress_lock = ATOMIC_FLAG_INIT;

    size_t interval_size = (current.size() * 15)/100;
    tbb::enumerable_thread_specific<size_t> local_counters;

    tbb::parallel_for_each(current.begin(), current.end(), [&](const SO6& S) {
            auto& local_counter = local_counters.local();

            uint8_t last_T = S.last_T;
            for (size_t T = 0; T < last_T; ++T, ++local_counter) {
                SO6 toInsert = S.left_multiply_by_T(static_cast<uint8_t>(T));
                if(prior.find(toInsert) == prior.end())  next.insert(toInsert);
            }

            for (int T = last_T + 1; T < 15; ++T, ++local_counter) {
                SO6 toInsert = S.left_multiply_by_T(static_cast<uint8_t>(T));
                if(prior.find(toInsert) == prior.end()) next.insert(toInsert);
            }

            if (local_counter >= interval_size && !progress_lock.test_and_set(std::memory_order_acquire)) {
                if (bars) {
                    bars->set_progress(global_counter.fetch_add(local_counter, std::memory_order_relaxed), next.size());
                }
                local_counter = 0;
                progress_lock.clear(std::memory_order_release);
            }
        }
    );

    gen_set.push_back(std::move(next));
    return next;
}

LUT create_lookup_table (const SO6& root, std::vector<double>* rss_by_layer_mb, std::vector<double>* per_t_time_s, std::vector<size_t>* per_t_mem_bytes) {
    LUT gen_set(root);
    std::unique_ptr<indicators::ProgressTracker> bars;
    // Capture the baseline RSS before any T processing begins
    size_t rss_baseline0 = getProcessRSSBytes();
    for (int curr_T_count = 0; curr_T_count < stored_depth_max; ++curr_T_count) {
        auto t_start = std::chrono::high_resolution_clock::now();
        size_t rss_before = getProcessRSSBytes();
        if (!suppress_indicators) {
            bars = std::make_unique<indicators::ProgressTracker>(curr_T_count, gen_set.current().size() * 15, gen_set.current().size() * 15);
            get_next_T_count(gen_set, bars.get());
        } else {
            bars.reset();
            get_next_T_count(gen_set, nullptr);
        }
        if (!suppress_indicators && bars) {
            size_t expected = gen_set.pending_size();
            bars->on_finalize_started(expected);
            gen_set.finalize_current_set(bars->get_finalize_bar());
            bars->on_finalize_finished();
        } else {
            gen_set.finalize_current_set(nullptr);
        }
        if (!suppress_indicators && bars) {
            bars->complete(gen_set.current().size());
        }

        // Capture per-T metrics
        auto t_end = std::chrono::high_resolution_clock::now();
        double t_secs = std::chrono::duration_cast<std::chrono::microseconds>(t_end - t_start).count() / 1e6;
        size_t rss_after = getProcessRSSBytes();
        size_t delta_bytes = 0;
        if (curr_T_count == 0) {
            // For T=1, set delta to the initial baseline so the intercept is captured explicitly
            delta_bytes = rss_baseline0;
        } else {
            delta_bytes = (rss_after > rss_before) ? (rss_after - rss_before) : 0;
        }

        if (rss_by_layer_mb) {
            size_t rss = getProcessRSSBytes();
            double mb = static_cast<double>(rss) / (1024.0 * 1024.0);
            rss_by_layer_mb->push_back(mb);
        }
        if (per_t_time_s) per_t_time_s->push_back(t_secs);
        if (per_t_mem_bytes) per_t_mem_bytes->push_back(delta_bytes);
    }
    return gen_set;
}

} // namespace algo
