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

// Build next T layer using transpose T^T moves and push into LUT (used for reverse/dual search).
// API mirrors get_next_T_count.

// Build the lookup table up to configured stored depth.
// ProgressTracker handles timing and RSS metrics internally; no external vectors needed.
LUT create_lookup_table (
    const SO6& root = SO6::identity(),
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
