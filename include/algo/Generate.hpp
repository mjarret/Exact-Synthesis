// Generation helpers extracted from main (no logic changes)
#pragma once

#include <tbb/concurrent_unordered_set.h>
#include <util/progress_tracker.hpp>
#include <functional>
#include <optional>

#include "so6/SO6.hpp"
#include "ds/LUT.hpp"

namespace algo {

// Build next T layer and push into LUT.
// Optional stop_pred is invoked after each successful insert into the next-layer working set.
// If it returns true for any candidate, generation stops early (best-effort) and returns the
// partially built next layer. Default is no-op (never stops).
tbb::concurrent_unordered_set<SO6> get_next_T_count(
    LUT& gen_set,
    indicators::ProgressTracker* bars = nullptr,
    const std::function<bool(const SO6&)>& stop_pred = nullptr,
    SO6* stop_value_out = nullptr);

// Build next TT layer (paired T-gate alphabet, 165 compounds) and push into LUT.
tbb::concurrent_unordered_set<SO6> get_next_TT_count(
    LUT& gen_set,
    indicators::ProgressTracker* bars = nullptr,
    const std::function<bool(const SO6&)>& stop_pred = nullptr,
    SO6* stop_value_out = nullptr);

// Build the lookup table using paired TT alphabet (each layer = 2 T-gates).
LUT create_lookup_table_TT(
    const SO6& root = SO6::identity(),
    const std::function<bool(const SO6&)>& stop_pred = nullptr,
    SO6* stop_value_out = nullptr);

// Build the lookup table up to configured stored depth.
// ProgressTracker handles timing and RSS metrics internally; no external vectors needed.
LUT create_lookup_table (
    const SO6& root = SO6::identity(),
    const std::function<bool(const SO6&)>& stop_pred = nullptr,
    SO6* stop_value_out = nullptr);

// Extend search beyond the stored LUT using a breadth-first brute-force sweep.
// The LUT depth is controlled by stored_depth_max, while target_T_count controls the
// brute-force depth beyond the LUT (target_T_count - stored_depth_max layers).
// Optional stop_pred is invoked after each candidate in the brute-force sweep; on
// the first match it stops early and writes the match to stop_value_out.
void extend_lookup_table_bf(
    LUT& gen_set,
    const std::function<bool(const SO6&)>& stop_pred = nullptr,
    SO6* stop_value_out = nullptr);

// Result of building two LUTs in lockstep until an intersection is found
struct DualLUTMatch {
    bool found{false};
    SO6 meet{};      // the intersecting element
    int side{-1};    // 0 = first, 1 = second (which side's insert hit the match)
};

// Build two LUTs in alternating layers. For each side, the stop predicate checks
// membership against the union of finalized layers from the opposite side.
// Returns immediately upon the first intersection.
std::optional<SO6> build_two_lookup_tables_until_match(LUT& first, LUT& second);

} // namespace algo
