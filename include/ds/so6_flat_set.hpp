// Specialized flat set for SO6 with open addressing + robin-hood probing.
// Focused on memory locality and speed for finalized sets. Single-threaded.
#ifndef SO6_FLAT_SET_HPP
#define SO6_FLAT_SET_HPP

#include <cstddef>
#include <cstdint>
#include <vector>
#include <limits>
#include <utility>
#include <type_traits>
#include <iterator>
#include "so6/SO6.hpp"
#include "hash_types.hpp"

namespace so6ds {

class SO6FlatSet {
public:
    SO6FlatSet() { reserve(1024); }
    explicit SO6FlatSet(size_t n) { reserve(n); }

    void clear() {
        _size = 0;
        for (auto &b : _buckets) b.state = Empty;
    }

    void reserve(size_t n) {
        size_t cap = 1;
        while (cap < n * 2) cap <<= 1; // target load factor <= 0.5
        rehash(cap);
    }

    size_t size() const { return _size; }
    bool empty() const { return _size == 0; }

    // Insert value if not present
    void insert(const SO6 &v) {
        if ((_size + 1) * 10 > _buckets.size() * 8) { // load factor > 0.8
            rehash(_buckets.size() ? _buckets.size() * 2 : 1024);
        }
        size_t h = so6_hash(v);
        Bucket b{v, Occupied, h, 0};
        size_t idx = h & _mask;
        uint32_t dist = 0;
        for (;;) {
            Bucket &cur = _buckets[idx];
            if (cur.state == Empty) {
                _buckets[idx] = b;
                ++_size;
                return;
            }
            if (cur.state == Occupied && cur.hash == h && cur.value == v) {
                return; // already present
            }
            // robin-hood: steal if our probe distance is greater
            if (cur.state == Occupied && cur.probe_dist < dist) {
                std::swap(cur, b);
            }
            idx = (idx + 1) & _mask;
            ++dist;
            b.probe_dist = dist;
        }
    }

    const SO6 *find_ptr(const SO6 &v) const {
        if (_buckets.empty()) return nullptr;
        size_t h = so6_hash(v);
        size_t idx = h & _mask;
        uint32_t dist = 0;
        for (;;) {
            const Bucket &cur = _buckets[idx];
            if (cur.state == Empty) return nullptr; // not found
            if (cur.state == Occupied) {
                if (cur.hash == h && cur.value == v) return &cur.value;
                if (cur.probe_dist < dist) return nullptr; // not in table
            }
            idx = (idx + 1) & _mask;
            ++dist;
        }
    }

    bool contains(const SO6 &v) const { return find_ptr(v) != nullptr; }

    // Basic iteration over occupied buckets
    enum State : uint8_t { Empty = 0, Occupied = 1 };
    struct Bucket {
        SO6 value{};
        State state{Empty};
        hash_t hash{0};
        uint32_t probe_dist{0};
    };

    struct iterator {
        using self = iterator;
        using iterator_category = std::forward_iterator_tag;
        using value_type = SO6;
        using difference_type = std::ptrdiff_t;
        using pointer = const SO6*;
        using reference = const SO6&;
        const std::vector<Bucket> *ref;
        size_t i;
        void skip() {
            while (i < ref->size() && (*ref)[i].state != Occupied) ++i;
        }
        iterator(const std::vector<Bucket> *r, size_t idx) : ref(r), i(idx) { skip(); }
        reference operator*() const { return (*ref)[i].value; }
        pointer operator->() const { return &(*ref)[i].value; }
        self &operator++() { ++i; skip(); return *this; }
        bool operator==(const self &o) const { return i == o.i && ref == o.ref; }
        bool operator!=(const self &o) const { return i != o.i || ref != o.ref; }
    };

    iterator begin() const { return iterator{&_buckets, 0}; }
    iterator end() const { return iterator{&_buckets, _buckets.size()}; }

    // Public find returning iterator (defined after iterator struct above)
    iterator find(const SO6 &v) const {
        if (_buckets.empty()) return end();
        size_t h = so6_hash(v);
        size_t idx = h & _mask;
        uint32_t dist = 0;
        for (;;) {
            const Bucket &cur = _buckets[idx];
            if (cur.state == Empty) return end();
            if (cur.state == Occupied) {
                if (cur.hash == h && cur.value == v) return iterator{&_buckets, idx};
                if (cur.probe_dist < dist) return end();
            }
            idx = (idx + 1) & _mask;
            ++dist;
        }
    }

private:

    std::vector<Bucket> _buckets;
    size_t _mask{0};
    size_t _size{0};

    static inline hash_t so6_hash(const SO6 &s) {
        // combine s.hash and s.col_hash (both size_t); xor-shift-mix
        size_t h = s.hash ^ (s.col_hash + 0x9e3779b97f4a7c15ull + (s.hash << 6) + (s.hash >> 2));
        // final mix
        h ^= h >> 33; h *= 0xff51afd7ed558ccdull; h ^= h >> 33; h *= 0xc4ceb9fe1a85ec53ull; h ^= h >> 33;
        return static_cast<hash_t>(h);
    }

    void rehash(size_t new_cap) {
        if (new_cap < 8) new_cap = 8;
        size_t cap = 1; while (cap < new_cap) cap <<= 1;
        std::vector<Bucket> old;
        old.swap(_buckets);
        _buckets.assign(cap, Bucket{});
        _mask = cap - 1;
        size_t old_size = _size; _size = 0;
        if (!old.empty()) {
            for (auto &b : old) if (b.state == Occupied) insert(b.value);
            _size = old_size; // insert already counted; ensure invariant
        }
    }
};

} // namespace so6ds

#endif // SO6_FLAT_SET_HPP
