// Centralized hash mixers and helpers (eliminates scattered ifdefs)
#pragma once

#include <cstdint>
#include <cstddef>
#include <functional>
#include "Z2.hpp"

namespace hashpolicy {

#ifndef EXACT_SYNTH_HASH_VARIANT
#define EXACT_SYNTH_HASH_VARIANT 5
#endif

static inline size_t mix64_variant(uint64_t x) {
#if EXACT_SYNTH_HASH_VARIANT == 0
    x = (x ^ (x >> 16)) * 0x45d9f3bULL;
    x = (x ^ (x >> 16)) * 0x45d9f3bULL;
    x ^= (x >> 16);
    return static_cast<size_t>(x);
#elif EXACT_SYNTH_HASH_VARIANT == 1
    x += 0x9e3779b97f4a7c15ULL;
    x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL;
    x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL;
    x ^= (x >> 31);
    return static_cast<size_t>(x);
#elif EXACT_SYNTH_HASH_VARIANT == 2
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
    return static_cast<size_t>(x);
#elif EXACT_SYNTH_HASH_VARIANT == 3
    x ^= x >> 32; x *= 0xd6e8feb86659fd93ULL; x ^= x >> 32; x *= 0xd6e8feb86659fd93ULL; x ^= x >> 32;
    return static_cast<size_t>(x);
#elif EXACT_SYNTH_HASH_VARIANT == 4
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 29; x *= 0x9ddfea08eb382d69ULL; x ^= x >> 32;
    return static_cast<size_t>(x);
#elif EXACT_SYNTH_HASH_VARIANT == 5
    x ^= x >> 12; x ^= x << 25; x ^= x >> 27; x *= 0x2545F4914F6CDD1DULL; return static_cast<size_t>(x);
#else
    x ^= x >> 31; x *= 0x7fb5d329728ea185ULL; x ^= x >> 27; x *= 0x81dadef4bc2dd44dULL; x ^= x >> 33; return static_cast<size_t>(x);
#endif
}

static inline uint64_t mix64_variant_rt(uint64_t x, int v) {
    switch (v) {
        case 0: x = (x ^ (x >> 16)) * 0x45d9f3bULL; x = (x ^ (x >> 16)) * 0x45d9f3bULL; x ^= (x >> 16); return x;
        case 1: x += 0x9e3779b97f4a7c15ULL; x = (x ^ (x >> 30)) * 0xbf58476d1ce4e5b9ULL; x = (x ^ (x >> 27)) * 0x94d049bb133111ebULL; x ^= (x >> 31); return x;
        case 2: x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33; return x;
        case 3: x ^= x >> 32; x *= 0xd6e8feb86659fd93ULL; x ^= x >> 32; x *= 0xd6e8feb86659fd93ULL; x ^= x >> 32; return x;
        case 4: x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 29; x *= 0x9ddfea08eb382d69ULL; x ^= x >> 32; return x;
        case 5: x ^= x >> 12; x ^= x << 25; x ^= x >> 27; x *= 0x2545F4914F6CDD1DULL; return x;
        default: x ^= x >> 31; x *= 0x7fb5d329728ea185ULL; x ^= x >> 27; x *= 0x81dadef4bc2dd44dULL; x ^= x >> 33; return x;
    }
}

static inline size_t z_freq_hash(const Z2 z, const int i) {
    uint64_t seed = (static_cast<uint64_t>(std::hash<Z2>{}(std::abs(z))) << 3) | static_cast<uint64_t>(i & 0x7);
    return mix64_variant(seed);
}

static inline size_t z_freq_hash_variant(const Z2 z, const int i, int variant) {
    uint64_t seed = (static_cast<uint64_t>(std::hash<Z2>{}(std::abs(z))) << 3) | static_cast<uint64_t>(i & 0x7);
    return static_cast<size_t>(mix64_variant_rt(seed, variant));
}

} // namespace hashpolicy

