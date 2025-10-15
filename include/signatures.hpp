// Experimental signature functions for SO6 fast-reject studies
#pragma once

#include <cstddef>
#include <cstdint>
#include "SO6.hpp"

namespace signatures {

// Variant 0: current combined row/col frequency hash (baseline)
inline size_t sig_rowcol_baseline(const SO6& s) {
    size_t row_sig = 0, col_sig = 0;
    for (int r = 0; r < 6; ++r) row_sig += SO6::frequency_hash(s.row_frequency[r]);
    for (int c = 0; c < 6; ++c) {
        size_t cf = SO6::frequency_hash(s.col_frequency[c]);
        col_sig += (cf ^ (cf >> 1));
    }
    // Same combine used elsewhere
    return row_sig ^ (col_sig + 0x9e3779b97f4a7c15ULL + (row_sig<<6) + (row_sig>>2));
}

// Variant 1: row-weighted sum + column-weighted sum, different mixing
inline size_t sig_rowcol_weighted(const SO6& s) {
    uint64_t row = 0, col = 0;
    for (int r = 0; r < 6; ++r) {
        uint64_t acc = 0;
        for (const auto& kv : s.row_frequency[r]) {
            acc += SO6::z_freq_hash(kv.first, kv.second) * 0x9e3779b185ebca87ULL;
        }
        row += acc * (0x632BE59BD9B4E019ULL + static_cast<uint64_t>(r));
    }
    for (int c = 0; c < 6; ++c) {
        uint64_t acc = 0;
        for (const auto& kv : s.col_frequency[c]) {
            acc += SO6::z_freq_hash(kv.first, kv.second) * 0x94D049BB133111EBULL;
        }
        col += acc * (0xBF58476D1CE4E5B9ULL + static_cast<uint64_t>(c));
    }
    row ^= row >> 29; col ^= col >> 31;
    return static_cast<size_t>(row ^ (col + 0x9e3779b97f4a7c15ULL + (row<<6) + (row>>2)));
}

// Variant 2: 64-bit bloom-ish OR over hashed keys for rows+cols
inline size_t sig_bloom64(const SO6& s) {
    uint64_t bloom_r = 0, bloom_c = 0;
    auto add = [](uint64_t& b, size_t h){ b |= (1ULL << (h & 63)); b |= (1ULL << ((h >> 6) & 63)); };
    for (int r = 0; r < 6; ++r) for (const auto& kv : s.row_frequency[r]) add(bloom_r, SO6::z_freq_hash(kv.first, kv.second));
    for (int c = 0; c < 6; ++c) for (const auto& kv : s.col_frequency[c]) add(bloom_c, SO6::z_freq_hash(kv.first, kv.second));
    uint64_t x = bloom_r ^ (bloom_c + 0x9e3779b97f4a7c15ULL + (bloom_r<<6) + (bloom_r>>2));
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
    return static_cast<size_t>(x);
}

// Variant 3: per-row zero-count bitpack + per-col zero-count bitpack, combined
inline size_t sig_zero_counts(const SO6& s) {
    uint32_t rz = 0, cz = 0; // 6 rows/cols * 3 bits each = 18 bits per mask
    for (int r = 0; r < 6; ++r) {
        uint8_t zc = 0;
        for (const auto& kv : s.row_frequency[r]) if (kv.first.data == 0) zc = kv.second; // count zeros
        rz |= (static_cast<uint32_t>(zc & 7) << (r * 3));
    }
    for (int c = 0; c < 6; ++c) {
        uint8_t zc = 0;
        for (const auto& kv : s.col_frequency[c]) if (kv.first.data == 0) zc = kv.second;
        cz |= (static_cast<uint32_t>(zc & 7) << (c * 3));
    }
    uint64_t x = (static_cast<uint64_t>(rz) << 21) ^ static_cast<uint64_t>(cz);
    x ^= x >> 29; x *= 0x9ddfea08eb382d69ULL; x ^= x >> 28;
    return static_cast<size_t>(x);
}

// Variant 4: array-driven signature without frequency maps: 64-bit bloom over 36 abs(Z2)
inline size_t sig_arr_bloom64(const SO6& s) {
    uint64_t bloom = 0;
    auto add = [](uint64_t& b, size_t h){ b |= (1ULL << (h & 63)); b |= (1ULL << ((h >> 7) & 63)); };
    for (int i = 0; i < 36; ++i) add(bloom, std::hash<Z2>{}(std::abs(s.arr[i])));
    uint64_t x = bloom;
    x ^= x >> 33; x *= 0xff51afd7ed558ccdULL; x ^= x >> 33; x *= 0xc4ceb9fe1a85ec53ULL; x ^= x >> 33;
    return static_cast<size_t>(x);
}

inline size_t compute(int variant, const SO6& s) {
    switch (variant) {
        case 0: return sig_rowcol_baseline(s);
        case 1: return sig_rowcol_weighted(s);
        case 2: return sig_bloom64(s);
        case 3: return sig_zero_counts(s);
        case 4: return sig_arr_bloom64(s);
        default: return sig_rowcol_baseline(s);
    }
}

} // namespace signatures

