// Centralized frequency-related data structures used by SO6
#pragma once

#include <array>
#include <utility>
#include <cstdint>
// <compare> not needed when comparing raw packed values

#include "Z2.hpp"
#include "ds/SmallFreqMap.hpp"
#include "ds/Order6.hpp"
#include "ds/hash_containers.hpp"

// Primary frequency map type used for row/column tallies
using FrequencyMap = SmallFreqMap;

// Key describing a multiset of (Z2 -> count) pairs with fixed max capacity 6.
// Entries are kept in a compact array and compared lexicographically by (Z2, count).
struct FrequencyKey {
    std::array<std::pair<Z2, uint8_t>, 6> entries{};
    uint8_t size = 0;
    bool operator<(FrequencyKey const& other) const {
        if (size != other.size) return size < other.size;
        for (uint8_t i = 0; i < size; ++i) {
            auto a = entries[i].first.data;
            auto b = other.entries[i].first.data;
            if (a != b) return a < b; // raw compare on packed Z2
            if (entries[i].second != other.entries[i].second) return entries[i].second < other.entries[i].second;
        }
        return false;
    }
    // Optional helper: sum of counts across entries (<= 6)
    uint8_t total() const noexcept {
        uint8_t s = 0;
        for (uint8_t i = 0; i < size; ++i) s = static_cast<uint8_t>(s + entries[i].second);
        return s;
    }
};

struct FrequencyKeyEq {
    bool operator()(FrequencyKey const& a, FrequencyKey const& b) const noexcept {
        if (a.size != b.size) return false;
        for (uint8_t i = 0; i < a.size; ++i) {
            if (a.entries[i].second != b.entries[i].second) return false;
            if (a.entries[i].first.data != b.entries[i].first.data) return false;
        }
        return true;
    }
};

struct FrequencyKeyHash {
    size_t operator()(FrequencyKey const& k) const noexcept {
        uint64_t h = k.size;
        for (uint8_t i = 0; i < k.size; ++i) {
            uint64_t v = static_cast<uint64_t>(k.entries[i].first.data);
            uint64_t c = static_cast<uint64_t>(k.entries[i].second);
            v ^= (v << 13) | (v >> 51);
            h ^= v + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2);
            h ^= (c + 0x9e3779b97f4a7c15ULL + (h << 6) + (h >> 2));
        }
        return static_cast<size_t>(h);
    }
};

// Maps a frequency-signature key to a compact order encoding of the indices
using FrequencyTable = exact::hash::unordered_map<FrequencyKey, order6::Order6, FrequencyKeyHash, FrequencyKeyEq>;
