// FrequencyPolicy.hpp
// Compile-time switch for frequency storage strategy
#pragma once

// EXACT_FREQ_COLS_ONLY = 0 -> keep both row and col frequency maps (baseline)
// EXACT_FREQ_COLS_ONLY = 1 -> keep only column maps; build row keys/signatures on the fly
#ifndef EXACT_FREQ_COLS_ONLY
#define EXACT_FREQ_COLS_ONLY 0
#endif

// EXACT_FREQ_NONE = 1 -> remove both row and col maps; build both sides on the fly
#ifndef EXACT_FREQ_NONE
#define EXACT_FREQ_NONE 1
#endif

#if (EXACT_FREQ_NONE) && (EXACT_FREQ_COLS_ONLY)
#error "EXACT_FREQ_NONE and EXACT_FREQ_COLS_ONLY are mutually exclusive"
#endif
