// Implementation moved from apps/main.cpp (no logic changes)
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for_each.h>
#include <atomic>
#include <optional>

#include "algo/Generate.hpp"
#include "config/Globals.hpp"
#include "so6/T_Operator.hpp"
#include "sys/memory.hpp"

namespace algo {

// Helper: expand all T-neighbors for a single SO6 element S, inserting into 'next'.
// Skips repeating the last T used to reach S and respects an optional stop predicate.
// Updates local_counter once per T tried and records the first stop hit in winner_value.
static void add_new_neighbors_for(const SO6& S, const finalized_set& prior, tbb::concurrent_unordered_set<SO6>& next, const std::function<bool(const SO6&)>& stop_pred,
    std::atomic<bool>& should_stop, std::atomic_flag& winner_claimed,
    SO6& winner_value)
{
    const uint8_t last_T = S.last_T;
    for (uint8_t T = 0; T < 15 && !should_stop.load(std::memory_order_relaxed); ++T) {
        if (T == last_T) continue;
        SO6 toInsert = T_OperatorRuntime(T) * S;
        if (prior.find(toInsert) == prior.end()) {
            auto ins = next.insert(toInsert);
            if (stop_pred && ins.second && stop_pred(toInsert)) {
                should_stop.store(true, std::memory_order_relaxed);
                if (!winner_claimed.test_and_set(std::memory_order_acq_rel)) winner_value = toInsert;
            }
        }
    }
}

} // namespace algo

namespace algo {

tbb::concurrent_unordered_set<SO6> get_next_T_count(LUT& gen_set, indicators::ProgressTracker* bars, const std::function<bool(const SO6&)>& stop_pred, SO6* stop_value_out) {
    auto& current = gen_set.current();
    auto& prior = gen_set.prior();
    tbb::concurrent_unordered_set<SO6> next;

    std::atomic<size_t> global_counter{0};
    std::atomic_flag progress_lock = ATOMIC_FLAG_INIT;
    std::atomic<bool> should_stop{false};
    std::atomic_flag winner_claimed = ATOMIC_FLAG_INIT;
    SO6 winner_value; // set exactly once when stop_pred first returns true

    size_t interval_size = current.size()/100;
    tbb::enumerable_thread_specific<size_t> local_counters;

    tbb::parallel_for_each(current.begin(), current.end(), [&](const SO6& S) {
            if (should_stop.load(std::memory_order_relaxed)) return;
            auto& local_counter = local_counters.local();

            add_new_neighbors_for(S, prior, next, stop_pred, should_stop, winner_claimed, winner_value);
            ++local_counter; 

            if (local_counter >= interval_size && !progress_lock.test_and_set(std::memory_order_acquire)) {
                if (bars) {
                    bars->set_progress(global_counter.fetch_add(local_counter*15, std::memory_order_relaxed), next.size());
                }
                local_counter = 0;
                progress_lock.clear(std::memory_order_release);
            }
        }
    );

    gen_set.push_back(std::move(next));
    if (stop_value_out && should_stop.load(std::memory_order_relaxed)) *stop_value_out = winner_value;
    return next;
}

LUT create_lookup_table (const SO6& root, const std::function<bool(const SO6&)>& stop_pred, SO6* stop_value_out) {
    LUT gen_set(root);
    for (int curr_T_count = 0; curr_T_count < stored_depth_max; ++curr_T_count) {
        std::atomic<bool> layer_hit{false};
        std::function<bool(const SO6&)> pred_wrapper = nullptr;
        
        if (stop_pred) {
            pred_wrapper = [&](const SO6& s){ bool r = stop_pred(s); if (r) layer_hit.store(true, std::memory_order_relaxed); return r; };
        }

        // Always construct a ProgressTracker; it will suppress itself if indicators are disabled
        std::unique_ptr<indicators::ProgressTracker> bars = std::make_unique<indicators::ProgressTracker>(curr_T_count, gen_set.current().size() * 15, gen_set.current().size() * 15);

        get_next_T_count(gen_set, bars.get(), pred_wrapper, stop_value_out);

        // Finalize with progress bar (ProgressTracker manages RSS/time internally)
        gen_set.finalize_current_set(bars.get());
        if (stop_pred && layer_hit.load(std::memory_order_relaxed)) break;
    }
    return gen_set;
}

std::optional<SO6> build_two_lookup_tables_until_match(LUT& first, LUT& second) {
    SO6 meet_first;
    // We alternate expansions up to stored_depth_max layers each (or until a hit)
    for (int depth = 0; depth < stored_depth_max; ++depth) {
        // Expand first side against second_union
        std::atomic<bool> connected{false};
        
        auto pred = [&](const SO6& s, LUT lut){ 
            bool r = (lut.back().find(s) != lut.back().end()); 
            if (r) connected.store(true, std::memory_order_relaxed); 
            return r; 
        };
        auto pred_first  = [&](const SO6& s){ return pred(s, second); };
        auto pred_second = [&](const SO6& s){ return pred(s, first);  };

        get_next_T_count(first, nullptr, pred_first, &meet_first);
        first.finalize_current_set(nullptr);
        if (connected.load(std::memory_order_relaxed))  return meet_first;        

        get_next_T_count(second, nullptr, pred_second, &meet_first);
        second.finalize_current_set(nullptr);
        if (connected.load(std::memory_order_relaxed))  return meet_first;

    }
    return std::nullopt; // not found within configured depth
}

} // namespace algo
