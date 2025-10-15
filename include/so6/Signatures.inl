// Out-of-class inline defs for SO6 signature helpers (no logic changes)
#pragma once

inline size_t SO6::frequency_hash(const FrequencyMap& f) {
    size_t hash = 0;
    for (const auto& kv : f) {
        size_t h = z_freq_hash(kv.first, kv.second);
        hash += h + h * h;
    }
    hash ^= hash >> 3;
    hash ^= hash >> 1;
    return hash;
}
