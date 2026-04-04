/**
 * @file LUT.hpp
 * @brief Rooted SO6 graph exploration (layered BFS by T-depth).
 */
#ifndef LUT_HPP
#define LUT_HPP

#include <vector>
#include <tbb/concurrent_unordered_set.h>
#include <ankerl/unordered_dense.h>
#include <optional>
#include <functional>
#include <iostream>
#include <iterator>
#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "sys/memory.hpp"
#include "util/progress_tracker.hpp"

using working_set = tbb::concurrent_unordered_set<SO6>;

// Container-local hasher for finalized_set: widen to 32 bits using existing 16-bit fields
// This changes only bucket placement inside the robin_hood set; it does not affect std::hash<SO6>
struct FinalizedHash32 {
    uint16_t operator()(const SO6& s) const noexcept {
        return s.primary_hash();
    }
};

// Backend-selectable finalized set
using finalized_set = ankerl::unordered_dense::set<SO6, FinalizedHash32, std::equal_to<SO6>>;
static const finalized_set empty_set;



/**
 * @brief Accumulates finalized SO6 sets layer-by-layer with memory-aware finalization.
 */
class LUT {

public:

    // Forward declaration so methods can return ElementIterator.
    class ElementIterator;

    LUT(const SO6& root = SO6::identity()) {
        // Reserve a reasonable number of layers up front to avoid small
        // reallocations as we grow; stored_depth_max is a global upper bound.
        extern uint8_t stored_depth_max;
        lookupTable.reserve(static_cast<size_t>(stored_depth_max) + 1u);
        lookupTable.push_back(finalized_set{});
        lookupTable.back().insert(root);
        all_raw_hashes_.insert(root.raw_hash());
    };

    void finalize_current_set(indicators::ProgressTracker* tracker = nullptr) {
        size_t availableMemory = getAvailableMemory();
        size_t elementSize = sizeof(SO6);
        size_t maxElements = availableMemory / elementSize;
        if (maxElements == 0) throw std::runtime_error("Insufficient memory to store the lookup table");

        // Batch populate the finalized set using chunked range inserts
        const size_t total = finalSet.size();
        finalized_set finalized_layer;
        finalized_layer.reserve(total);

        size_t count = 0;
        if (tracker) tracker->on_finalize_started(total);
        indicators::ProgressBar* finalize_bar = tracker ? tracker->get_finalize_bar() : nullptr;

        // Use a reusable chunk buffer to reduce per-insert overhead while keeping progress updates
        constexpr size_t CHUNK = 8192; // tuned for cache/bucket locality
        std::vector<SO6> chunk; chunk.reserve(std::min(CHUNK, total));
        for (const auto& v : finalSet) {
            chunk.push_back(v);
            if (chunk.size() == chunk.capacity()) {
                finalized_layer.insert(std::make_move_iterator(chunk.begin()), std::make_move_iterator(chunk.end()));
                count += chunk.size();
                if (finalize_bar) finalize_bar->set_progress(count);
                chunk.clear();
            }
        }
        if (!chunk.empty()) {
            finalized_layer.insert(std::make_move_iterator(chunk.begin()), std::make_move_iterator(chunk.end()));
            count += chunk.size();
            if (finalize_bar) finalize_bar->set_progress(count);
        }
        lookupTable.emplace_back(std::move(finalized_layer));

        // Populate raw hash set from newly finalized layer
        for (const auto& s : lookupTable.back()) {
            all_raw_hashes_.insert(s.raw_hash());
        }

        // Release memory held by the concurrent working set
        working_set().swap(finalSet);

        if (tracker) {
            tracker->on_finalize_finished();
            // Complete the tracker with the finalized layer size
            tracker->complete(lookupTable.back().size());
        }
    }

    size_t size() const { return lookupTable.size(); }

    void push_back(const working_set& set) { finalSet.insert(set.begin(), set.end()); }

    const finalized_set& current() { return lookupTable.back(); }
    const finalized_set& current() const { return lookupTable.back(); }

    // Number of elements pending finalization into the LUT
    size_t pending_size() const { return finalSet.size(); }

    const finalized_set& prior() const {
        if(lookupTable.size() > 1 ) return lookupTable[lookupTable.size()-2];
        return empty_set;
    }

    // Legacy layer iterators (explicit) over the raw lookupTable.
    auto layers_begin() { return lookupTable.begin(); }
    auto layers_end()   { return lookupTable.end(); }
    auto layers_begin() const { return lookupTable.begin(); }
    auto layers_end()   const { return lookupTable.end(); }

    // Lookup an element across all finalized layers and return a flat iterator
    // into the LUT, compatible with begin()/end() and elements().
    ElementIterator find(const SO6& s) const;
    ElementIterator find(const SO6& s) {
        return static_cast<const LUT*>(this)->find(s);
    }

    // Lookup restricted to a single layer by index using the underlying
    // finalized_set iterators.
    auto find_in_layer(const SO6& s, size_t layer_idx) {
        auto& layer = lookupTable[layer_idx];
        return layer.find(s);
    }

    auto find_in_layer(const SO6& s, size_t layer_idx) const {
        const auto& layer = lookupTable[layer_idx];
        return layer.find(s);
    }

    auto &back() const { return lookupTable.back(); }

    // -------- Flat element iteration (root-first, layer order) --------
    class ElementIterator {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = SO6;
        using difference_type = std::ptrdiff_t;
        using pointer = const SO6*;
        using reference = const SO6&;

        ElementIterator(const std::vector<finalized_set>* layers, size_t layer_idx)
            : layers_(layers), layer_idx_(layer_idx)
        {
            if (layer_idx_ < layers_->size()) {
                it_ = (*layers_)[layer_idx_].begin();
                advance_to_valid();
            }
        }

        // Construct an iterator pointing at a specific element within a layer.
        ElementIterator(const std::vector<finalized_set>* layers,
                        size_t layer_idx,
                        finalized_set::const_iterator it)
            : layers_(layers), layer_idx_(layer_idx), it_(it) {}

        reference operator*() const { return *it_; }
        pointer operator->() const { return &(*it_); }

        ElementIterator& operator++() {
            ++it_;
            advance_to_valid();
            return *this;
        }

        bool operator==(const ElementIterator& other) const {
            if (ended() && other.ended()) return true;
            return layers_ == other.layers_ && layer_idx_ == other.layer_idx_ && it_ == other.it_;
        }
        bool operator!=(const ElementIterator& other) const { return !(*this == other); }

    private:
        bool ended() const { return layer_idx_ >= layers_->size(); }

        void advance_to_valid() {
            while (layer_idx_ < layers_->size() && it_ == (*layers_)[layer_idx_].end()) {
                ++layer_idx_;
                if (layer_idx_ < layers_->size()) it_ = (*layers_)[layer_idx_].begin();
            }
        }

        const std::vector<finalized_set>* layers_;
        size_t layer_idx_;
        finalized_set::const_iterator it_;
    };

    struct ElementRange {
        const std::vector<finalized_set>* layers;
        ElementIterator begin() const { return ElementIterator(layers, 0); }
        ElementIterator end() const { return ElementIterator(layers, layers->size()); }
    };

    // Range for layers (when you want layer-wise loops explicitly)
    struct LayerRange {
        const std::vector<finalized_set>* layers;
        auto begin() const { return layers->begin(); }
        auto end() const { return layers->end(); }
    };

    // Iterate all elements in BFS layer order (root first).
    ElementRange elements() const { return ElementRange{&lookupTable}; }

    // Range over layers if needed.
    LayerRange layers() const { return LayerRange{&lookupTable}; }

    // Default iteration yields elements.
    ElementIterator begin() const { return ElementIterator(&lookupTable, 0); }
    ElementIterator end() const { return ElementIterator(&lookupTable, lookupTable.size()); }


    /**
     * @brief Recover the T-sequence from the root to a target element already stored in the LUT.
     *
     * Uses the stored last_T on each node to search the prior layer for a parent p such that
     * T(last_T) * p == current. Returns empty optional if the element is not present or no
     * consistent predecessor chain is found.
     */
    std::optional<std::vector<uint8_t>> path_to(const SO6& target) const {
        // Locate the layer containing target, could be done recursively by terminating at root
        // but might be less efficient due to repeated searches.
        int layer_idx = -1;
        int idx = 0;
        for (const auto& layer : lookupTable) {
            if (layer.find(target) != layer.end()) { layer_idx = idx; break; }
            ++idx;
        }

        if (layer_idx <= 0) {
            if (layer_idx == 0) return std::vector<uint8_t>{}; // root
            return std::nullopt; // not found
        }

        SO6 current = target;
        std::vector<uint8_t> path; // will be reversed at the end

        for (int l = layer_idx; l > 0; --l) {
            uint8_t t = current.last_T;
            path.push_back(t);
            const auto& prev = lookupTable[static_cast<size_t>(l - 1)];
            // Find a parent p in the previous layer such that T(t) * p == current.
            // We do a linear scan here; this is only used for path reconstruction
            // in diagnostics/benchmarks, not in the main search.
            bool found_parent = false;
            for (const auto& cand : prev) {
                if (T_OperatorRuntime(t) * cand == current) {
                    current = cand;
                    found_parent = true;
                    break;
                }
            }
            if (!found_parent) {
                return std::nullopt;
            }
        }

        std::reverse(path.begin(), path.end());
        return path;
    }

    SO6 root() const {
        return *lookupTable.front().begin();
    }

    /// Check if a raw hash has been seen in any finalized layer.
    bool raw_hash_seen(uint64_t rh) const {
        return all_raw_hashes_.count(rh) > 0;
    }

    /// Access the raw hash set (for pre-populating layer-local sets).
    const tbb::concurrent_unordered_set<uint64_t>& raw_hashes() const {
        return all_raw_hashes_;
    }

private:
    std::vector<finalized_set> lookupTable = {};
    working_set finalSet;
    tbb::concurrent_unordered_set<uint64_t> all_raw_hashes_;
};

// Implementation of flat find() declared earlier.
inline LUT::ElementIterator LUT::find(const SO6& s) const {
    size_t layer_idx = 0;
    for (const auto& layer : lookupTable) {
        auto it = layer.find(s);
        if (it != layer.end()) {
            return ElementIterator(&lookupTable, layer_idx, it);
        }
        ++layer_idx;
    }
    return end();
}

#endif // LUT_HPP
