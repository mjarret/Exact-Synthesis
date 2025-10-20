// Generation helpers extracted from main (no logic changes)
#pragma once

#include <tbb/concurrent_unordered_set.h>
#include <util/progress_tracker.hpp>

#include "so6/SO6.hpp"
#include "so6/graph/LUT.hpp"

namespace algo {

// Build next T layer and push into LUT
tbb::concurrent_unordered_set<SO6> get_next_T_count(LUT& gen_set, indicators::ProgressTracker* bars = nullptr);

// Build the lookup table up to configured stored depth.
// If rss_by_layer_mb is non-null, appends process RSS (in MB) after each layer finalization.
// If per_t_time_s is non-null, appends runtime (seconds) for each T.
// If per_t_mem_bytes is non-null, appends delta memory bytes used for each T.
LUT create_lookup_table (
    const SO6& root = SO6::identity(),
    std::vector<double>* rss_by_layer_mb = nullptr,
    std::vector<double>* per_t_time_s = nullptr,
    std::vector<size_t>* per_t_mem_bytes = nullptr);

} // namespace algo
