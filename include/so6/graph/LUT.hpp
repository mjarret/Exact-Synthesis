/**
 * @file LUT.hpp
 * @brief Rooted SO6 graph exploration (layered BFS by T-depth).
 */
#ifndef LUT_HPP
#define LUT_HPP

#include <vector>
#include <tbb/concurrent_unordered_set.h>
#include <robin_hood.h>
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

// Conservative max load factor (50%) to limit probe chains
using finalized_set = robin_hood::unordered_flat_set<SO6, FinalizedHash32, std::equal_to<SO6>, 50>;
static const finalized_set empty_set;

#include "sys/memory.hpp"

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
        if(maxElements == 0)
            throw std::runtime_error("Insufficient memory to store the lookup table");

        finalized_set robin_set;
        lookupTable.push_back(std::move(robin_set));
        // Reserve based on expected element count
        lookupTable.back().reserve(finalSet.size());

        size_t count = 0;
        for (const auto& v : finalSet) {
            lookupTable.back().insert(v);
            if (finalize_bar) finalize_bar->set_progress(++count);
        }
        working_set().swap(finalSet);
    }

    size_t size() const { return lookupTable.size()+1; }

    void push_back(const working_set& set) { finalSet.insert(set.begin(), set.end()); }

    const finalized_set& current() { return lookupTable.back(); }

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

