/**
 * @file LUT.hpp
 * @brief Layered lookup table (LUT) of SO6 sets by T-depth.
 */
#ifndef LUT_HPP
#define LUT_HPP

#include <vector>
#include <tbb/concurrent_unordered_set.h>
#include <robin_hood.h>
#include "so6/SO6.hpp"


using working_set = tbb::concurrent_unordered_set<SO6>;
using finalized_set = robin_hood::unordered_flat_set<SO6>;
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
        // Tune memory: higher load factor and pre-reserve expected size
        // lookupTable.back().max_load_factor(0.9f);
        // lookupTable.back().reserve(finalSet.size());
        // lookupTable.back().max_load_factor(2.0f);
        lookupTable.back().reserve(finalSet.size());

        size_t count = 0;
        for (const auto& v : finalSet) {
            lookupTable.back().insert(v);
            if (finalize_bar) finalize_bar->set_progress(++count);
        }
        working_set().swap(finalSet);

        // Removed optional hash and signature sweeps (profiling-only code)
    }

    // insert/contains helpers removed (unused)

    // find by layer removed (unused)

    // contains_in_prior removed (unused)

    size_t size() const {
        return lookupTable.size()+1; // 1 for final set
    }

    void push_back(const working_set& set) {
        finalSet.insert(set.begin(), set.end());
    }

    const finalized_set& current() {
        return lookupTable.back();
    }

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
