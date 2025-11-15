/**
 * @file LUT.hpp
 * @brief Rooted SO6 graph exploration (layered BFS by T-depth).
 */
#ifndef LUT_HPP
#define LUT_HPP

#include <vector>
#include <iomanip>
#include <sstream>
#include <tbb/concurrent_unordered_set.h>
#include <ankerl/unordered_dense.h>
#include "so6/SO6.hpp"

using working_set = tbb::concurrent_unordered_set<SO6>;

// Container-local hasher for finalized_set: widen to 32 bits using existing 16-bit fields
// This changes only bucket placement inside the robin_hood set; it does not affect std::hash<SO6>
struct FinalizedHash32 {
    size_t operator()(const SO6& s) const noexcept {
        uint32_t h = (static_cast<uint32_t>(s.col_hash) << 16) | static_cast<uint32_t>(s.hash);
        return static_cast<size_t>(h);
    }
};

// Backend-selectable finalized set
using finalized_set = ankerl::unordered_dense::set<SO6, FinalizedHash32, std::equal_to<SO6>>;
static const finalized_set empty_set;

#include "sys/memory.hpp"
#include "util/progress_tracker.hpp"

/**
 * @brief Accumulates finalized SO6 sets layer-by-layer with memory-aware finalization.
 */
class LUT {

public:

    LUT(const SO6& root = SO6::identity()) {
        lookupTable.push_back(finalized_set{});
        lookupTable.back().insert(root);
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

    auto begin() { return lookupTable.begin(); }
    auto end() { return lookupTable.end(); }

    auto find(const SO6& s) {
        for (auto& layer : lookupTable) {
            auto it = layer.find(s);
            if (it != layer.end()) return it;
        }
        return lookupTable.back().end();
    }

    auto find_in_layer(const SO6& s, size_t layer_idx) {
        auto& layer = lookupTable[layer_idx];
        return layer.find(s);
    }

    auto &back() const { return lookupTable.back(); }

private:
    std::vector<finalized_set> lookupTable = {};
    working_set finalSet;
};

#endif // LUT_HPP
