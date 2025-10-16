// Implementation moved from apps/main.cpp (no logic changes)
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for_each.h>
#include <atomic>

#include "algo/Generate.hpp"
#include "config/Globals.hpp"

namespace algo {

tbb::concurrent_unordered_set<SO6> get_next_T_count(LUT& gen_set, indicators::ProgressTracker* bars) {
    auto& current = gen_set.current();
    auto& prior = gen_set.prior();
    tbb::concurrent_unordered_set<SO6> next;

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

LUT create_lookup_table (const SO6& root) {
    LUT gen_set(root);
    std::unique_ptr<indicators::ProgressTracker> bars;
    for (int curr_T_count = 0; curr_T_count < stored_depth_max; ++curr_T_count) {
        bars = std::make_unique<indicators::ProgressTracker>(curr_T_count, gen_set.current().size() * 15, gen_set.current().size() * 15);
        get_next_T_count(gen_set, bars.get());
        gen_set.finalize_current_set();
        bars->complete(gen_set.current().size());
    }
    return gen_set;
}

} // namespace algo
