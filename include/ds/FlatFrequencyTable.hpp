// Flat, stack-friendly map from uint16_t signature to order6::Order6
// Provides just the API surface used in canonicalization:
// - size(), reserve(no-op)
// - operator[](key) -> Value& (creates entry if missing)
// - find(key) -> iterator (pointer-like); begin()/end() for range-for
// - Entry has .first (key) and .second (value)
#pragma once

#include <cstdint>
#include <cstddef>
#include <array>
#include <unordered_map>
#include <mutex>
#include <vector>
#include <cstring>
#include <algorithm>
#include "ds/Order6.hpp"

namespace ds {

class FlatFrequencyTable {
public:
    struct Entry {
        uint16_t first{0};         // signature key
        order6::Order6 second{};   // Order within the equivalence block
    };

    using iterator = Entry*;
    using const_iterator = const Entry*;

    FlatFrequencyTable() = default;

    // Capacity / size
    void reserve(std::size_t) {} // no-op; fixed max = 6
    std::size_t size() const { return n_; }
    bool empty() const { return n_ == 0; }
    void clear() { n_ = 0; }

    // Iteration
    iterator begin() { return entries_; }
    iterator end() { return entries_ + n_; }
    const_iterator begin() const { return entries_; }
    const_iterator end() const { return entries_ + n_; }
    const_iterator cbegin() const { return entries_; }
    const_iterator cend() const { return entries_ + n_; }

    // Lookup
    iterator find(uint16_t key) {
        for (std::size_t i = 0; i < n_; ++i) if (entries_[i].first == key) return entries_ + i;
        return end();
    }
    const_iterator find(uint16_t key) const {
        for (std::size_t i = 0; i < n_; ++i) if (entries_[i].first == key) return entries_ + i;
        return end();
    }

    // Insert / access (creates entry if missing)
    order6::Order6& operator[](uint16_t key) {
        for (std::size_t i = 0; i < n_; ++i) {
            if (entries_[i].first == key) return entries_[i].second;
        }
        // create new
        if (n_ < 6) {
            entries_[n_].first = key;
            entries_[n_].second = order6::Order6{};
            return entries_[n_++].second;
        }
        // Should never happen; fallback to last slot
        return entries_[5].second;
    }

    // Build/attach cached mixed‑radix advance LUT once per block-size pattern
    void prepare_advance_lut() {
        if (lut_ != nullptr) return;
        const uint8_t nb = static_cast<uint8_t>(n_);
        // key: number of blocks in low 8 bits, then each block size in next bytes
        uint64_t key = nb;
        for (uint8_t i = 0; i < nb; ++i) {
            key |= (static_cast<uint64_t>(entries_[i].second.size()) & 0xFFull) << (8 * (i + 1));
        }
        auto &cache = lut_cache();
        {
            std::lock_guard<std::mutex> lk(lut_cache_mutex());
            auto it = cache.find(key);
            if (it == cache.end()) {
                LUT lut; // default len = 0
                // compute factorial per block and total states
                uint16_t facts[6]{};
                uint32_t total = 1;
                for (uint8_t i = 0; i < nb; ++i) {
                    const uint8_t k = entries_[i].second.size();
                    const uint16_t f = order6::FACT[k];
                    facts[i] = f;
                    total *= f;
                }
                if (total > 1u) {
                    lut.len = static_cast<uint16_t>(total - 1u);
                    uint16_t ranks[6]{};
                    for (uint16_t step = 0; step < lut.len; ++step) {
                        uint8_t adv = 0;
                        while (adv < nb && static_cast<uint32_t>(ranks[adv]) + 1u == facts[adv]) ++adv;
                        lut.adv_idx[step] = adv;
                        for (uint8_t j = 0; j < adv; ++j) ranks[j] = 0;
                        ++ranks[adv];
                    }
                }
                auto ins = cache.emplace(key, std::move(lut));
                it = ins.first;
            }
            lut_ = &it->second;
        }
        lut_pos_ = 0;
    }

    // Advance using the cached LUT; false on full wrap (and resets ranks)
    bool next_via_lut() {
        if (lut_ == nullptr) prepare_advance_lut();
        if (lut_->len == 0) return false;
        if (lut_pos_ >= lut_->len) {
            for (std::size_t i = 0; i < n_; ++i) entries_[i].second.reset();
            lut_pos_ = 0;
            return false;
        }
        const uint8_t adv = lut_->adv_idx[lut_pos_++];
        for (uint8_t j = 0; j < adv; ++j) entries_[j].second.reset();
        (void)entries_[adv].second.next_permutation();
        return true;
    }

    // (OP6-style full-cycle caching removed)

private:
    Entry entries_[6]{};
    std::size_t n_{0};
    struct LUT { std::array<uint8_t, 720> adv_idx{}; uint16_t len{0}; };
    static inline std::unordered_map<uint64_t, LUT>& lut_cache() {
        static std::unordered_map<uint64_t, LUT> cache; return cache;
    }
    static inline std::mutex& lut_cache_mutex() {
        static std::mutex mtx; return mtx;
    }
    const LUT* lut_{nullptr};
    uint16_t lut_pos_{0};
    // (cycle_* fields removed)
};

} // namespace ds
