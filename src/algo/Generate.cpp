// Implementation moved from apps/main.cpp (no logic changes)
#include <tbb/enumerable_thread_specific.h>
#include <tbb/parallel_for_each.h>
#include <atomic>
#include <chrono>
#include <optional>

#include "algo/Generate.hpp"
#include "config/Globals.hpp"
#include "so6/T_Operator.hpp"
#include "so6/TT_Operator.hpp"
#include "sys/memory.hpp"
#include "util/progress_tracker.hpp"

namespace algo {

// Helper: expand all T-neighbors for a single SO6 element S, inserting into 'next'.
// Skips repeating the last T used to reach S and respects an optional stop predicate.
// Updates local_counter once per T tried and records the first stop hit in winner_value.
static void add_new_neighbors_for(const SO6& S, LUT& lut, tbb::concurrent_unordered_set<SO6>& next, const std::function<bool(const SO6&)>& stop_pred,
    std::atomic<bool>& should_stop, std::atomic_flag& winner_claimed,
    SO6& winner_value, std::atomic<size_t>& inserted_count)
{
    const uint8_t last_T = S.last_T;
    for (uint8_t T = 0; T < 15 && !should_stop.load(std::memory_order_relaxed); ++T) {
        if (T == last_T) continue;
        SO6 toInsert = T_OperatorRuntime(T) * S;
        // Deduplicate against all finalized layers, not just the immediate prior.
        // NOTE: dedup here is best-effort under parallelism. The lazy canonicalization
        // (const_cast on first comparison) can race on a shared matrix and let a few
        // duplicates slip through (over-count only -- never under-count, since a positive
        // match means genuine equivalence). LUT::dedup() cleans these up single-threaded
        // afterward. See add_new_neighbors_for / LUT::dedup.
        if (lut.find(toInsert) != lut.end()) continue;
        if (!stop_pred) {
            // Hot path: no predicate, toInsert is dead after the insert.
            if (next.insert(std::move(toInsert)).second)
                inserted_count.fetch_add(1, std::memory_order_relaxed);
            continue;
        }
        auto ins = next.insert(toInsert);
        if (ins.second) {
            inserted_count.fetch_add(1, std::memory_order_relaxed);
            if (stop_pred(toInsert)) {
                should_stop.store(true, std::memory_order_relaxed);
                if (!winner_claimed.test_and_set(std::memory_order_acq_rel)) winner_value = toInsert;
            }
        }
    }
}

// Helper: expand all legal TT-neighbors for a single SO6 source S using the
// shared-first-T fan-out. For each first-T group, copy S once and apply the first T
// (value-only) to a shared intermediate; then for each legal second emit a candidate
// by copying the intermediate and applying ONLY the second-T butterfly. The first T is
// thus amortized across all endpoints in its group, so the per-endpoint cost is ~one
// SO6 copy + one T butterfly + one metadata finalization (see plan/benchmarks).
//
// Pruning is precomputed per parent TT move in tt::kLegalByPrevMove[last_TT]: it keeps
// only candidates whose 4-T composition with the parent is geodesic (genuinely +2 depth),
// grouped by first T. The loop just reads off the allowed moves -- no per-candidate
// legality checks, no heap allocation.
static void add_new_TT_neighbors_for(const SO6& S, LUT& lut, tbb::concurrent_unordered_set<SO6>& next,
    const std::function<bool(const SO6&)>& stop_pred,
    std::atomic<bool>& should_stop, std::atomic_flag& winner_claimed,
    SO6& winner_value, std::atomic<size_t>& inserted_count)
{
    // Pruning is keyed on the full parent TT move (last_TT), not just its last T, so the
    // table can reject candidates that don't net +2 depth via either of the parent's T's.
    const std::size_t prev = (S.last_TT == tt::kNoMove) ? tt::kRootPrev : S.last_TT;
    const tt::LegalGroups& legal = tt::kLegalByPrevMove[prev];

    for (uint8_t f = 0; f < 15; ++f) {
        const uint8_t cnt = legal.count[f];
        if (cnt == 0) continue;                         // group pruned away (includes f==L)
        if (should_stop.load(std::memory_order_relaxed)) return;

        SO6 inter = S;                                  // one copy per first-T group
        tt::apply_T_values_only_rt(f, inter);           // apply the shared first T once (values only)

        for (uint8_t k = 0; k < cnt; ++k) {
            if (should_stop.load(std::memory_order_relaxed)) return;
            const uint8_t mv = legal.moves[f][k];
            const uint8_t second_t = tt::kAlphabet[mv].second;

            SO6 cand = inter;                            // copy intermediate (stack, no alloc)
            tt::apply_T_values_only_rt(second_t, cand);  // apply ONLY the second-T butterfly
            cand.invalidate_derived_state();             // single metadata finalization
            cand.last_TT = mv;                           // the only TT history field

            // Deduplicate against all finalized layers (best-effort under parallelism;
            // LUT::dedup() cleans any race-induced over-counts single-threaded afterward,
            // exactly as the T path does).
            if (lut.find(cand) != lut.end()) continue;
            if (!stop_pred) {
                if (next.insert(std::move(cand)).second)
                    inserted_count.fetch_add(1, std::memory_order_relaxed);
                continue;
            }
            auto ins = next.insert(cand);
            if (ins.second) {
                inserted_count.fetch_add(1, std::memory_order_relaxed);
                if (stop_pred(cand)) {
                    should_stop.store(true, std::memory_order_relaxed);
                    if (!winner_claimed.test_and_set(std::memory_order_acq_rel)) winner_value = cand;
                }
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

tbb::concurrent_unordered_set<SO6> get_next_T_count(LUT& gen_set,
                                                    indicators::ProgressTracker* bars,
                                                    const std::function<bool(const SO6&)>& stop_pred,
                                                    SO6* stop_value_out,
                                                    const std::chrono::steady_clock::time_point* deadline) {
    auto& current = gen_set.current();
    tbb::concurrent_unordered_set<SO6> next;

    std::atomic<bool> should_stop{false};
    std::atomic_flag winner_claimed = ATOMIC_FLAG_INIT;
    SO6 winner_value; // set exactly once when stop_pred first returns true
    std::atomic<size_t> inserted{0}; // cheap O(1) frontier size for the progress bar

    const bool has_deadline = (deadline != nullptr);

    parallel_for_each_with_bar(current.begin(), current.end(), current.size(), 15, bars,
        [&]() { return inserted.load(std::memory_order_relaxed); },
        [&](const SO6& S) {
            if (should_stop.load(std::memory_order_relaxed)) return;
            if (has_deadline && std::chrono::steady_clock::now() >= *deadline) {
                should_stop.store(true, std::memory_order_relaxed);
                return;
            }
            add_new_neighbors_for(S, gen_set, next, stop_pred, should_stop, winner_claimed, winner_value, inserted);
        });

    gen_set.push_back(std::move(next));
    if (stop_value_out && should_stop.load(std::memory_order_relaxed)) *stop_value_out = winner_value;
    return next;
}

// TT sibling of get_next_T_count: expands the current finalized layer using fused TT
// moves (~151 endpoints per ordinary source vs ~14 for T). Reuses the same parallel
// wrapper, concurrent working set, dedup, finalize, stop-predicate, and progress.
tbb::concurrent_unordered_set<SO6> get_next_TT_count(LUT& gen_set,
                                                     indicators::ProgressTracker* bars,
                                                     const std::function<bool(const SO6&)>& stop_pred,
                                                     SO6* stop_value_out,
                                                     const std::chrono::steady_clock::time_point* deadline) {
    auto& current = gen_set.current();
    tbb::concurrent_unordered_set<SO6> next;

    std::atomic<bool> should_stop{false};
    std::atomic_flag winner_claimed = ATOMIC_FLAG_INIT;
    SO6 winner_value;
    std::atomic<size_t> inserted{0};

    const bool has_deadline = (deadline != nullptr);

    parallel_for_each_with_bar(current.begin(), current.end(), current.size(), 151, bars,
        [&]() { return inserted.load(std::memory_order_relaxed); },
        [&](const SO6& S) {
            if (should_stop.load(std::memory_order_relaxed)) return;
            if (has_deadline && std::chrono::steady_clock::now() >= *deadline) {
                should_stop.store(true, std::memory_order_relaxed);
                return;
            }
            add_new_TT_neighbors_for(S, gen_set, next, stop_pred, should_stop, winner_claimed, winner_value, inserted);
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

// TT sibling of create_lookup_table. stored_depth_max is in actual T-count units and is
// even (validated in Globals::configure), so this builds stored_depth_max/2 TT layers;
// TT layer i holds matrices at actual T-depth 2*(i+1). The progress label shows actual
// T-depth, not the layer index.
LUT create_lookup_table_TT(const SO6& root, const std::function<bool(const SO6&)>& stop_pred, SO6* stop_value_out) {
    indicators::show_console_cursor(false);
    LUT gen_set(root);
    const int tt_layers = stored_depth_max / 2;
    for (int tt_layer = 0; tt_layer < tt_layers; ++tt_layer) {
        std::atomic<bool> layer_hit{false};
        std::function<bool(const SO6&)> pred_wrapper = nullptr;
        if (stop_pred) {
            pred_wrapper = [&](const SO6& s){ bool r = stop_pred(s); if (r) layer_hit.store(true, std::memory_order_relaxed); return r; };
        }
        // ProgressTracker prints (arg+1); pass 2*tt_layer+1 so it shows the actual
        // T-depth being produced (2*(tt_layer+1)). per-source work ~= 151 endpoints.
        std::unique_ptr<indicators::ProgressTracker> bars = std::make_unique<indicators::ProgressTracker>(2 * tt_layer + 1, gen_set.current().size() * 151, gen_set.current().size() * 151);
        get_next_TT_count(gen_set, bars.get(), pred_wrapper, stop_value_out);
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

    const int kMaxDepth = std::max<int>(0, target_T_count - stored_depth_max);
    if (kMaxDepth == 0) return;

    std::atomic<bool> should_stop{false};
    std::atomic_flag winner_claimed = ATOMIC_FLAG_INIT;
    SO6 winner_value;

    // For this brute-force phase we treat "work" as iterating over leaves in
    // the current layer and, for each leaf, exploring all T-words of a given
    // length d, then repeating for d+1, etc. We drive the progress bars one
    // depth at a time so that they reflect the per-depth sweeps clearly.
    for (int depth = 0; depth < kMaxDepth; ++depth) {
        std::atomic<std::size_t> processed{0};

        // Label by ACTUAL T-depth: stored layers cover T-depth (size-1) in T mode and
        // 2*(size-1) in TT mode; the brute-force phase advances one T per depth either way.
        const int stored_t_depth =
            (generator_kind == GeneratorKind::TT ? 2 : 1) * (static_cast<int>(gen_set.size()) - 1);
        std::unique_ptr<indicators::ProgressTracker> bars =
            std::make_unique<indicators::ProgressTracker>(
                stored_t_depth + depth,
                current.size(),
                current.size());

        parallel_for_each_with_bar(current.begin(), current.end(), current.size(), 1, bars.get(),
            [&]() { return processed.load(std::memory_order_relaxed); },
            [&](const SO6& S) {
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
