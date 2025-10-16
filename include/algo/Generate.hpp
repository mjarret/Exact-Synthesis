// Generation helpers extracted from main (no logic changes)
#pragma once

#include <tbb/concurrent_unordered_set.h>
#include <util/progress_tracker.hpp>

#include "so6/SO6.hpp"
#include "so6/LUT.hpp"

namespace algo {

// Build next T layer and push into LUT
tbb::concurrent_unordered_set<SO6> get_next_T_count(LUT& gen_set, indicators::ProgressTracker* bars = nullptr);

// Build the lookup table up to configured stored depth
LUT create_lookup_table (const SO6& root = SO6::identity());

} // namespace algo
