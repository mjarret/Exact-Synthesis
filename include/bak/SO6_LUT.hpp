#ifndef SO6_LUT_HPP
#define SO6_LUT_HPP

#include "absl/container/flat_hash_set.h"
#include <iostream>
#include <tbb/concurrent_set.h>
#include <SO6.hpp>

using hash_t = decltype(std::hash<SO6>{}(std::declval<SO6>()));

class SO6_LUT {
public:

    SO6_LUT() = default;

    // Add a tbb::concurrent_set to the lookup table
    void addSet(const tbb::concurrent_set<SO6>& set) {
        for (const auto& element : set) {
            lookupTable.insert(element);
        }
    }

    const SO6* operator[](const SO6& key) {
        auto it = lookupTable.find(key);
        return (it != lookupTable.end()) ? &(*it) : nullptr;
    }

private:
    // The underlying lookup table using absl::flat_hash_map
    absl::flat_hash_set<SO6> lookupTable;
};

#endif // SO6_LUT_HPP