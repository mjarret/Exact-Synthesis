// Small, stack-friendly map: signature -> 6-bit index mask.
// Designed for n=6 row/column grouping without dynamic allocation.
#pragma once

#include <cstdint>
#include <cstddef>

namespace ds {

struct SignatureMaskMap {
    static constexpr std::size_t kMaxEntries = 6;

    struct Entry {
        uint16_t sig{0};      // FrequencySignature (typically uint16_t)
        uint8_t mask{0};      // bit r set iff index r belongs to this signature
    };

    // Current number of occupied entries
    std::size_t size() const { return used_; }
    bool empty() const { return used_ == 0; }
    void clear() { used_ = 0; }

    // Add index 'idx' to the bucket for 'sig'.
    void add(uint16_t sig, uint8_t idx) {
        // Try to find existing signature
        for (std::size_t i = 0; i < used_; ++i) {
            if (entries_[i].sig == sig) {
                entries_[i].mask |= 1u << idx;
                return;
            }
        }
        // New entry
        entries_[used_].sig = sig;
        entries_[used_].mask = static_cast<uint8_t>(1u << idx);
        ++used_;
    }

    // Simple in-place insertion sort by 'sig' (n <= 6)
    void sort_by_signature() {
        for (std::size_t i = 1; i < used_; ++i) {
            Entry key = entries_[i];
            std::size_t j = i;
            while (j > 0 && key.sig < entries_[j - 1].sig) {
                entries_[j] = entries_[j - 1];
                --j;
            }
            entries_[j] = key;
        }
    }

    const Entry* data() const { return entries_; }
    Entry* data() { return entries_; }
    const Entry& operator[](std::size_t i) const { return entries_[i]; }
    Entry& operator[](std::size_t i) { return entries_[i]; }

    // Proxy to allow syntax: m[sig] |= idx; or m[sig] += idx;
    struct MaskRef {
        Entry* e;
        MaskRef& operator|=(uint8_t idx) {
            e->mask = static_cast<uint8_t>(e->mask | (1u << idx));
            return *this;
        }
        MaskRef& operator+=(uint8_t idx) { return (*this |= idx); }
        MaskRef& operator=(uint8_t mask) { e->mask = static_cast<uint8_t>(mask & 0x3Fu); return *this; }
        operator uint8_t() const { return e->mask; }
    };

    // Find or create entry by signature and return mask proxy
    MaskRef operator[](uint16_t sig) {
        for (std::size_t i = 0; i < used_; ++i) {
            if (entries_[i].sig == sig) return MaskRef{ &entries_[i] };
        }
        entries_[used_].sig = sig;
        entries_[used_].mask = 0;
        return MaskRef{ &entries_[used_++] };
    }

    // Extract indices contained in mask into 'out' (ascending), returns count.
    static inline uint8_t extract_indices(uint8_t mask, uint8_t out[6]) {
        uint8_t n = 0;
        while (mask) {
            uint8_t lsb = static_cast<uint8_t>(mask & -mask); // isolate lowest set bit
            uint8_t idx = 0;
            // map lsb (power of two) to index 0..5
            switch (lsb) {
                case 1u: idx = 0; break; case 2u: idx = 1; break; case 4u: idx = 2; break;
                case 8u: idx = 3; break; case 16u: idx = 4; break; case 32u: idx = 5; break;
                default: idx = 0; break; // unreachable for 6-bit masks
            }
            out[n++] = idx;
            mask = static_cast<uint8_t>(mask & static_cast<uint8_t>(mask - 1));
        }
        return n;
    }

private:
    Entry entries_[kMaxEntries];
    std::size_t used_{0};
};

} // namespace ds
