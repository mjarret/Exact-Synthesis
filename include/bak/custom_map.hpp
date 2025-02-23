#ifndef CUSTOM_MAP_HPP
#define CUSTOM_MAP_HPP

#include <array>
#include <optional>
#include <cstdint>
#include <stdexcept> // for std::out_of_range
#include "../Z2.hpp"


namespace CustomContainers {

class CustomMap {
public:
    // Constructor
    CustomMap() : size(0) {}

    // Insert a Z2 key with a value (int)
    void insert(const Z2& key, int value) {
        for (size_t i = 0; i < size; ++i) {
            if (data[i].key == key) {
                data[i].value += value; // Increment frequency if key exists
                return;
            }
        }
        data[size] = {key, value}; // Add new key-value pair
        ++size;
    }

    // Get the value associated with a Z2 key
    std::optional<int> get(const Z2& key) const {
        for (size_t i = 0; i < size; ++i) {
            if (data[i].key == key) {
                return data[i].value;
            }
        }
        return std::nullopt; // Key not found
    }

    // Check if a key exists
    bool contains(const Z2& key) const {
        for (size_t i = 0; i < size; ++i) {
            if (data[i].key == key) {
                return true;
            }
        }
        return false;
    }

    // Get the number of elements in the map
    size_t get_size() const {
        return size;
    }

    // Clear the map
    void clear() {
        size = 0;
    }

private:
    struct KeyValuePair {
        Z2 key;
        int value;
    };

    std::array<KeyValuePair, 6> data; // Fixed-size array to store up to 6 elements
    size_t size; // Current number of elements in the map
};

} // namespace CustomContainers

#endif // CUSTOM_MAP_HPP