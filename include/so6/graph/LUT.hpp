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
#include "ds/hash_containers.hpp"
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
using finalized_set = exact::hash::unordered_set<SO6, FinalizedHash32, std::equal_to<SO6>>;
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

    void finalize_current_set(indicators::ProgressBar* finalize_bar = nullptr) {
        size_t availableMemory = getAvailableMemory();
        size_t elementSize = sizeof(SO6);
        size_t maxElements = availableMemory / elementSize;
        if (maxElements == 0) throw std::runtime_error("Insufficient memory to store the lookup table");

        // Batch populate the finalized set using chunked range inserts
        const size_t total = finalSet.size();
        finalized_set robin_set;
        robin_set.reserve(total);

        size_t count = 0;
        if (finalize_bar) finalize_bar->set_option(indicators::option::MaxProgress{total});

        // Use a reusable chunk buffer to reduce per-insert overhead while keeping progress updates
        constexpr size_t CHUNK = 8192; // tuned for cache/bucket locality
        std::vector<SO6> chunk; chunk.reserve(std::min(CHUNK, total));
        for (const auto& v : finalSet) {
            chunk.push_back(v);
            if (chunk.size() == chunk.capacity()) {
                robin_set.insert(std::make_move_iterator(chunk.begin()), std::make_move_iterator(chunk.end()));
                count += chunk.size();
                if (finalize_bar) finalize_bar->set_progress(count);
                chunk.clear();
            }
        }
        if (!chunk.empty()) {
            robin_set.insert(std::make_move_iterator(chunk.begin()), std::make_move_iterator(chunk.end()));
            count += chunk.size();
            if (finalize_bar) finalize_bar->set_progress(count);
        }
        lookupTable.emplace_back(std::move(robin_set));

        // Release memory held by the concurrent working set
        working_set().swap(finalSet);
    }

    size_t size() const { return lookupTable.size()+1; }

    void push_back(const working_set& set) { finalSet.insert(set.begin(), set.end()); }

    const finalized_set& current() { return lookupTable.back(); }

    // Number of elements pending finalization into the LUT
    size_t pending_size() const { return finalSet.size(); }

    const finalized_set& prior() const {
        if(lookupTable.size() > 1 ) return lookupTable[lookupTable.size()-2];
        return empty_set;
    }

    auto begin() { return lookupTable.begin(); }
    auto end() { return lookupTable.end(); }

private:
    std::vector<finalized_set> lookupTable = {};
    working_set finalSet;
};

#endif // LUT_HPP
