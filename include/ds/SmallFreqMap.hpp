// Compact 6-slot frequency map specialized for SO6 row/col frequencies
#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include "Z2.hpp"

// Layout: structure-of-arrays to avoid per-entry padding.
// - keys[i] holds the Z2 key
// - counts[i] holds the frequency; 0 means empty slot
// Capacity is fixed at 6; iteration skips empty slots.
struct SmallFreqMap {
    // Keys stored directly; this is already 6*32 bits tightly packed.
    std::array<Z2, 6> keys{};
    // Packed counts: 6 slots * 3 bits = 18 bits used.
    // Layout: slot i is at bits [i*3 .. i*3+2]. Value 0 => empty.
    uint32_t packed_counts{0};

    static constexpr uint32_t mask3 = 0x7u;
    static constexpr uint8_t capacity = 6;

    inline uint8_t get_count(uint8_t i) const {
        return static_cast<uint8_t>((packed_counts >> (i * 3)) & mask3);
    }
    inline void set_count(uint8_t i, uint8_t v) {
        uint32_t shift = static_cast<uint32_t>(i) * 3u;
        packed_counts &= ~(mask3 << shift);
        packed_counts |= (static_cast<uint32_t>(v & mask3) << shift);
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


    struct EntryRef { Z2 first; CountRef second; };
    struct EntryConstRef { Z2 first; uint8_t second; };

    class iterator {
    public:
        using difference_type = std::ptrdiff_t;
        using value_type = EntryRef;
        using reference = EntryRef;
        // no operator-> to avoid proxy lifetime issues
        using iterator_category = std::forward_iterator_tag;

        iterator() : m(nullptr), idx(capacity) {}
        iterator(SmallFreqMap* m_, uint8_t i) : m(m_), idx(i) { skip(); }

        reference operator*() { return EntryRef{ m->keys[idx], m->count_ref(idx) }; }
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

        reference operator*() { return EntryConstRef{ m->keys[idx], m->get_count(idx) }; }
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

    iterator find(const Z2& k) {
        for (uint8_t i = 0; i < capacity; ++i) {
            if (get_count(i) != 0 && keys[i] == k) return iterator(this, i);
        }
        return end();
    }
    const_iterator find(const Z2& k) const {
        for (uint8_t i = 0; i < capacity; ++i) {
            if (get_count(i) != 0 && keys[i] == k) return const_iterator(this, i);
        }
        return end();
    }

    CountRef operator[](const Z2& k) {
        for (uint8_t i = 0; i < capacity; ++i) {
            if (get_count(i) != 0 && keys[i] == k) return CountRef{this, i};
        }
        for (uint8_t i = 0; i < capacity; ++i) {
            if (get_count(i) == 0) { keys[i] = k; set_count(i, 0); return CountRef{this, i}; }
        }
        __builtin_unreachable();
    }

    void erase(iterator it) {
        uint8_t i = it.index();
        if (i < capacity) set_count(i, 0);
    }

    void reserve(size_t) {}

private:
    CountRef count_ref(uint8_t i) { return CountRef{this, i}; }
};
