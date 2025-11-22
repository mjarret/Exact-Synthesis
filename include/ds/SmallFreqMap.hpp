// Compact 6-slot frequency map specialized for SO6 row/col frequencies
#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <iterator>
#include <cstddef>
#include <algorithm>
#include "ds/Count6.hpp"
#include "DyadicSqrt2.hpp"

// Layout: structure-of-arrays to avoid per-entry padding.
// - keys[i] holds the Z2 key (packed to 24 bits)
// - counts[i] holds the frequency; 0 means empty slot
// Capacity is fixed at 6; iteration skips empty slots.
struct SmallFreqMap {
    // Keys packed to 24 bits each: 6 * 3 bytes = 18 bytes total
    std::array<std::array<uint8_t, 3>, 6> keys24{};
    // Packed counts for slots 0..4: 5 * 3 bits = 15 bits used in a 16-bit word.
    // Slot 5 is derived on demand: if key[5] != 0 => count=1; else remainder = 6 - sum(count[0..4]).
    uint16_t counts15{0};

    static constexpr uint32_t mask3 = 0x7u;
    static constexpr uint8_t capacity = 6;

    inline uint8_t get_count(uint8_t i) const {
        if (i < 5) [[likely]] {
            uint16_t shift = static_cast<uint16_t>(i) * 3u;
            return static_cast<uint8_t>((counts15 >> shift) & mask3);
        }
        // If sixth key present (non-zero), its count is 1; else derived remainder
        const auto &b = keys24[5];
        const uint32_t v = static_cast<uint32_t>(b[0]) | (static_cast<uint32_t>(b[1]) << 8) | (static_cast<uint32_t>(b[2]) << 16);
        if (v != 0u) return 1u;
        uint8_t s = 0;
        for (uint8_t k = 0; k < 5; ++k) s = static_cast<uint8_t>(s + get_count(k));
        return static_cast<uint8_t>(6u - s);
    }
    
    inline void set_count(uint8_t i, uint8_t v) {
        if (i == 5) return; // sixth is derived (or no-op)
        uint16_t shift = static_cast<uint16_t>(i) * 3u;
        counts15 = static_cast<uint16_t>(counts15 & static_cast<uint16_t>(~(mask3 << shift)));
        counts15 = static_cast<uint16_t>(counts15 | static_cast<uint16_t>((static_cast<uint16_t>(v & mask3) << shift)));
    }

    struct CountRef {
        SmallFreqMap* m;
        uint8_t i;
        operator uint8_t() const { return m->get_count(i); }
        CountRef& operator=(uint8_t v) { m->set_count(i, v); return *this; }
        CountRef& operator++() { m->set_count(i, static_cast<uint8_t>(m->get_count(i) + 1)); return *this; }
        CountRef& operator--() { m->set_count(i, static_cast<uint8_t>(m->get_count(i) - 1)); return *this; }
        CountRef operator++(int) { CountRef tmp{*this}; ++(*this); return tmp; }
        CountRef operator--(int) { CountRef tmp{*this}; --(*this); return tmp; }
    };


    struct EntryRef { DyadicSqrt2 first; CountRef second; };
    struct EntryConstRef { DyadicSqrt2 first; uint8_t second; };

    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = EntryRef;
        using reference = EntryRef;
        // no operator-> to avoid proxy lifetime issues
        using iterator_category = std::forward_iterator_tag;

        iterator() : m(nullptr), idx(capacity) {}
        iterator(SmallFreqMap* m_, uint8_t i) : m(m_), idx(i) { skip(); }

        reference operator*() { return EntryRef{ m->unpack_key(idx), m->count_ref(idx) }; }
        bool operator==(const iterator& o) const { return m == o.m && idx == o.idx; }
        bool operator!=(const iterator& o) const { return !(*this == o); }
        iterator& operator++() { advance(); return *this; }

        // expose index for erase
        uint8_t index() const { return idx; }

    private:
        void skip() {
            while (idx < capacity && m->get_count(idx) == 0) ++idx;
        }
        void advance() { if (idx < capacity) { ++idx; skip(); } }

        SmallFreqMap* m;
        uint8_t idx;
        friend struct SmallFreqMap;
    };

    class const_iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = EntryConstRef;
        using reference = EntryConstRef;
        // no operator-> to avoid proxy lifetime issues
        using iterator_category = std::forward_iterator_tag;

        const_iterator() : m(nullptr), idx(capacity) {}
        const_iterator(const SmallFreqMap* m_, uint8_t i) : m(m_), idx(i) { skip(); }
        const_iterator(const iterator& it) : m(it.m), idx(it.idx) { skip(); }

        reference operator*() { return EntryConstRef{ m->unpack_key(idx), m->get_count(idx) }; }
        bool operator==(const const_iterator& o) const { return m == o.m && idx == o.idx; }
        bool operator!=(const const_iterator& o) const { return !(*this == o); }
        const_iterator& operator++() { advance(); return *this; }

    private:
        void skip() {
            while (idx < capacity && m->get_count(idx) == 0) ++idx;
        }
        void advance() { if (idx < capacity) { ++idx; skip(); } }

        const SmallFreqMap* m;
        uint8_t idx;
        friend struct SmallFreqMap;
    };

    iterator begin() { return iterator(this, 0); }
    iterator end() { return iterator(this, capacity); }
    const_iterator begin() const { return const_iterator(this, 0); }
    const_iterator end() const { return const_iterator(this, capacity); }

    iterator find(const DyadicSqrt2& k) {
        for (uint8_t i = 0; i < capacity; ++i) {
            if (get_count(i) != 0 && unpack_key(i) == k) return iterator(this, i);
        }
        return end();
    }
    const_iterator find(const DyadicSqrt2& k) const {
        for (uint8_t i = 0; i < capacity; ++i) {
            if (get_count(i) != 0 && unpack_key(i) == k) return const_iterator(this, i);
        }
        return end();
    }

    CountRef operator[](const DyadicSqrt2& k) {
        // Zero key is implicit remainder; treat as no-op
        if (k.data == 0) return CountRef{this, capacity};
        // Existing explicit slots 0..4
        for (uint8_t i = 0; i < 5; ++i) {
            if (get_count(i) != 0 && unpack_key(i) == k) return CountRef{this, i};
        }
        // Sixth distinct key present and matches: move to explicit if room
        const uint32_t v5 = static_cast<uint32_t>(keys24[5][0]) | (static_cast<uint32_t>(keys24[5][1]) << 8) | (static_cast<uint32_t>(keys24[5][2]) << 16);
        if (v5 != 0u && unpack_key(5) == k) {
            for (uint8_t i = 0; i < 5; ++i) {
                if (get_count(i) == 0) {
                    pack_key(i, k);
                    set_count(i, 1);
                    keys24[5] = {0,0,0};
                    return CountRef{this, i};
                }
            }
            // No explicit room; leave derived at count 1
            return CountRef{this, capacity};
        }
        // Insert new explicit slot if available
        for (uint8_t i = 0; i < 5; ++i) {
            if (get_count(i) == 0) { pack_key(i, k); set_count(i, 0); return CountRef{this, i}; }
        }
        // Otherwise set as sixth distinct key (implicit count=1) and return no-op
        pack_key(5, k);
        return CountRef{this, capacity};
    }

    // Convenience helpers (inline, zero-cost abstraction)
    void increment(const DyadicSqrt2& k) { (*this)[k]++; }
    void decrement(const DyadicSqrt2& k) {
        if (k.data == 0) return; // implicit remainder
        auto it = find(k);
        if (it != end()) {
            if ((*it).second == 1) erase(it);
            else --(*it).second;
        }
    }

    void erase(iterator it) {
        uint8_t i = it.index();
        if (i < 5) {
            set_count(i, 0);
            compact();
        } else if (i == 5) {
            keys24[5] = {0,0,0};
        }
    }

    void reserve(size_t) {}

    // --- Count6 interop (cold path helpers) ---
    inline Count6 as_count6() const noexcept {
        std::array<uint8_t, 6> c{};
        for (uint8_t i = 0; i < capacity; ++i) c[i] = get_count(i);
        return Count6::from_counts(c);
    }

    inline void set_counts_from(const Count6& enc) noexcept {
        auto c = enc.to_counts();
        for (uint8_t i = 0; i < 5; ++i) set_count(i, c[i]);
    }

private:
    inline DyadicSqrt2 unpack_key(uint8_t i) const {
        const auto &b = keys24[i];
        uint32_t v = static_cast<uint32_t>(b[0])
                   | (static_cast<uint32_t>(b[1]) << 8)
                   | (static_cast<uint32_t>(b[2]) << 16);
        return DyadicSqrt2(v);
    }
    inline void pack_key(uint8_t i, const DyadicSqrt2& k) {
        const uint32_t v = static_cast<uint32_t>(k.data) & 0xFFFFFFu;
        keys24[i][0] = static_cast<uint8_t>(v & 0xFFu);
        keys24[i][1] = static_cast<uint8_t>((v >> 8) & 0xFFu);
        keys24[i][2] = static_cast<uint8_t>((v >> 16) & 0xFFu);
    }
    CountRef count_ref(uint8_t i) { return CountRef{this, i}; }

    void compact() {
        // Compact explicit slots 0..4 (non-zero counts forward) and align keys; optionally pull sixth into explicit.
        std::array<std::array<uint8_t,3>,6> newk{};
        uint16_t newc = 0;
        uint8_t dst = 0;
        for (uint8_t i = 0; i < 5; ++i) {
            uint8_t c = static_cast<uint8_t>((counts15 >> (i*3)) & mask3);
            if (c) {
                newk[dst] = keys24[i];
                newc |= static_cast<uint16_t>((c & mask3) << (dst*3));
                ++dst;
            }
        }
        // If room remains and a sixth distinct key exists, move it into explicit set with count=1
        const uint32_t v5 = static_cast<uint32_t>(keys24[5][0]) | (static_cast<uint32_t>(keys24[5][1]) << 8) | (static_cast<uint32_t>(keys24[5][2]) << 16);
        bool moved6 = false;
        if (dst < 5 && v5 != 0u) {
            newk[dst] = keys24[5];
            newc |= static_cast<uint16_t>((1u & mask3) << (dst*3));
            ++dst;
            moved6 = true;
        }
        for (uint8_t i = 0; i < 5; ++i) keys24[i] = newk[i];
        counts15 = newc;
        if (moved6) keys24[5] = {0,0,0};
    }

public:
    // --- Convenience introspection helpers ---
    // Sum of all counts (<= 6 by construction)
    inline uint8_t total() const noexcept {
        uint8_t s = 0;
        for (uint8_t i = 0; i < capacity; ++i) s = static_cast<uint8_t>(s + get_count(i));
        return s;
    }
    // Number of active (non-zero) slots (<= 6)
    inline uint8_t active_slots() const noexcept {
        uint8_t n = 0;
        for (uint8_t i = 0; i < capacity; ++i) n = static_cast<uint8_t>(n + (get_count(i) != 0));
        return n;
    }

    // Equality as a multiset of (Z2 -> count). Ignores slot positions.
    inline bool operator==(const SmallFreqMap& o) const noexcept {
        // Fast path: pointer equality
        if (this == &o) return true;
        // Fast path: totals must match
        if (total() != o.total()) return false;

        // Collect entries
        std::array<EntryConstRef, capacity> a{}; uint8_t na = 0;
        std::array<EntryConstRef, capacity> b{}; uint8_t nb = 0;
        for (uint8_t i = 0; i < capacity; ++i) {
            uint8_t c = get_count(i);
            if (c) a[na++] = EntryConstRef{ unpack_key(i), c };
            c = o.get_count(i);
            if (c) b[nb++] = EntryConstRef{ o.unpack_key(i), c };
        }
        if (na != nb) return false;

        auto less_pair = [](const EntryConstRef& x, const EntryConstRef& y){
            if (auto c = x.first <=> y.first; c != 0) return c < 0; return x.second < y.second;
        };
        std::sort(a.begin(), a.begin() + na, less_pair);
        std::sort(b.begin(), b.begin() + nb, less_pair);
        for (uint8_t i = 0; i < na; ++i) {
            if (!(a[i].first == b[i].first)) return false;
            if (a[i].second != b[i].second) return false;
        }
        return true;
    }
    inline bool operator!=(const SmallFreqMap& o) const noexcept { return !(*this == o); }
};
