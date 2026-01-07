// Implementation moved from apps/main.cpp (no logic changes)
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for_each.h>
#include <atomic>
#include <optional>

#include "algo/Generate.hpp"
#include "config/Globals.hpp"
#include "so6/T_Operator.hpp"
#include "sys/memory.hpp"
#include "util/progress_tracker.hpp"

namespace algo {

// Helper: expand all T-neighbors for a single SO6 element S, inserting into 'next'.
// Skips repeating the last T used to reach S and respects an optional stop predicate.
// Updates local_counter once per T tried and records the first stop hit in winner_value.
static void add_new_neighbors_for(const SO6& S, LUT& lut, tbb::concurrent_unordered_set<SO6>& next, const std::function<bool(const SO6&)>& stop_pred,
    std::atomic<bool>& should_stop, std::atomic_flag& winner_claimed,
    SO6& winner_value)
{
    const uint8_t last_T = S.last_T;
    for (uint8_t T = 0; T < 15 && !should_stop.load(std::memory_order_relaxed); ++T) {
        if (T == last_T) continue;
        SO6 toInsert = T_OperatorRuntime(T) * S;
        // Deduplicate against all finalized layers, not just the immediate prior.
        if (lut.find(toInsert) == lut.end()) {
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

// Generic tbb::parallel_for_each wrapper with hidden progress bookkeeping.
// - total_elems: number of elements in the range (for throttling)
// - per_elem_work: nominal work credited per element (e.g., 15 attempted T's)
// - found_supplier(): returns the current "found"/set size to display as the second bar number
// - body(elem): performs work for a single element
template <class Iter, class FoundSupplier, class Body>
static inline void parallel_for_each_with_bar(Iter begin, Iter end,
                                              std::size_t total_elems,
                                              std::size_t per_elem_work,
                                              indicators::ProgressTracker* bars,
                                              FoundSupplier&& found_supplier,
                                              Body&& body) {
    std::atomic<size_t> global_counter{0};
    std::atomic_flag progress_lock = ATOMIC_FLAG_INIT;
    tbb::enumerable_thread_specific<size_t> local_counters;

    const std::size_t interval_size = std::max<std::size_t>(total_elems / 100u, 1u);

    tbb::parallel_for_each(begin, end, [&](const auto& elem) {
        body(elem);
        auto& local_counter = local_counters.local();
        ++local_counter;
        if (local_counter >= interval_size && !progress_lock.test_and_set(std::memory_order_acquire)) {
            if (bars) {
                auto prog = global_counter.fetch_add(local_counter * per_elem_work, std::memory_order_relaxed);
                bars->set_progress(prog, found_supplier());
            }
            local_counter = 0;
            progress_lock.clear(std::memory_order_release);
        }
    });
}

tbb::concurrent_unordered_set<SO6> get_next_T_count(LUT& gen_set, indicators::ProgressTracker* bars, const std::function<bool(const SO6&)>& stop_pred, SO6* stop_value_out) {
    auto& current = gen_set.current();
    tbb::concurrent_unordered_set<SO6> next;

    std::atomic<bool> should_stop{false};
    std::atomic_flag winner_claimed = ATOMIC_FLAG_INIT;
    SO6 winner_value; // set exactly once when stop_pred first returns true

    parallel_for_each_with_bar(current.begin(), current.end(), current.size(), 15, bars,
        [&]() { return next.size(); },
        [&](const SO6& S) {
            if (should_stop.load(std::memory_order_relaxed)) return;
            add_new_neighbors_for(S, gen_set, next, stop_pred, should_stop, winner_claimed, winner_value);
        });

    gen_set.push_back(std::move(next));
    if (stop_value_out && should_stop.load(std::memory_order_relaxed)) *stop_value_out = winner_value;
    return next;
}

LUT create_lookup_table (const SO6& root, const std::function<bool(const SO6&)>& stop_pred, SO6* stop_value_out) {
    // Hide cursor to reduce flicker during progress updates; signal handler restores it.
    indicators::show_console_cursor(false);
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

        // Avoid copying LUTs: pass by const reference when checking membership.
        auto pred = [&](const SO6& s, const LUT& lut){
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

namespace algo {

void extend_lookup_table_bf(LUT& gen_set, const std::function<bool(const SO6&)>& stop_pred, SO6* stop_value_out) {
    // Hide cursor for this extension phase; signal handler restores on interrupt.
    indicators::show_console_cursor(false);
    const auto& current = gen_set.current();

    const int brute_force_depth = std::max<int>(0, target_T_count - stored_depth_max);
    if (brute_force_depth == 0) return;

    std::atomic<bool> should_stop{false};
    std::atomic_flag winner_claimed = ATOMIC_FLAG_INIT;
    SO6 winner_value;

    // For this brute-force phase we treat "work" as iterating over leaves in
    // the current layer and, for each leaf, exploring all T-words of a given
    // length d, then repeating for d+1, etc. We drive the progress bars one
    // depth at a time so that they reflect the per-depth sweeps clearly.
    for (int depth = 1; depth <= brute_force_depth; ++depth) {
        if (should_stop.load(std::memory_order_relaxed)) break;
        std::atomic<std::size_t> processed{0};

        std::unique_ptr<indicators::ProgressTracker> bars =
            std::make_unique<indicators::ProgressTracker>(
                static_cast<int>(stored_depth_max + depth - 1),
                current.size(),
                current.size());

        parallel_for_each_with_bar(current.begin(), current.end(), current.size(), 1, bars.get(),
            [&]() { return processed.load(std::memory_order_relaxed); },
            [&](const SO6& S) {
                if (should_stop.load(std::memory_order_relaxed)) return;
                processed.fetch_add(1, std::memory_order_relaxed);
                const uint8_t last_T = S.last_T;

                // Enumerate all base-14 codes that map to admissible
                // T-sequences of length = depth (skipping the forbidden T
                // on each step, as in BFSExtensionRawRange).
                std::size_t count = 1;
                for (int i = 0; i < depth; ++i) count *= 14u;

                for (std::size_t code = 0; code < count && !should_stop.load(std::memory_order_relaxed); ++code) {
                    SO6 cur = S;
                    std::size_t x = code;
                    uint8_t forbid = last_T;
                    for (int pos = 0; pos < depth; ++pos) {
                        uint8_t d = static_cast<uint8_t>(x % 14u);
                        x /= 14u;
                        uint8_t t = static_cast<uint8_t>(d + (d >= forbid ? 1u : 0u));
                        cur = T_OperatorRuntime(t, false) * cur;
                        forbid = t;
                    }
                    if (stop_pred && stop_pred(cur)) {
                        should_stop.store(true, std::memory_order_relaxed);
                        if (!winner_claimed.test_and_set(std::memory_order_acq_rel)) {
                            winner_value = cur;
                        }
                        return;
                    }
                    (void)cur;
                }
            });

        // This brute-force phase does not insert into the LUT, but we still want
        // the tracker to finalize timing and close out the bars cleanly. Use the
        // number of processed leaves as the "found" count so the final snapshot
        // shows a non-zero numerator instead of "0 / N".
        bars->complete(processed.load(std::memory_order_relaxed));
    }

    if (stop_value_out && should_stop.load(std::memory_order_relaxed)) {
        *stop_value_out = winner_value;
    }
}

} // namespace algo
