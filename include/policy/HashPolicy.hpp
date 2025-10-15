// Centralized hash mixers and helpers (eliminates scattered ifdefs)
#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include "Z2.hpp"

namespace hashpolicy {

// Fixed mixer (formerly selected by EXACT_SYNTH_HASH_VARIANT=5)
static inline size_t mix64_variant(uint64_t x) {
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    x *= 0x2545F4914F6CDD1DULL;
    return static_cast<size_t>(x);
}

static inline size_t z_freq_hash(const Z2 z, const int i) {
    uint64_t seed = (static_cast<uint64_t>(std::hash<Z2>{}(std::abs(z))) << 3) | static_cast<uint64_t>(i & 0x7);
    return mix64_variant(seed);
}

// Removed z_freq_hash_variant and runtime variant mixer; profiling-only support

} // namespace hashpolicy
