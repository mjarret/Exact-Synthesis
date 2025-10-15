// Out-of-class definition of SO6::Iterator
#pragma once

#include <cstdint>
#include <cstddef>
#include <iterator>
#include "Z2.hpp"

class SO6::Iterator {
    public:
        Iterator(const SO6& so6, int index, const uint8_t* Row = nullptr, const uint8_t* Col = nullptr)
            : so6_(so6), index_(index), Row_(Row), Col_(Col) {}


        // Dereference operator to get the current element based on Row and Col permutation
        const Z2& operator*() const {
            int row_index = (Row_ != nullptr) ? Row_[index_ % 6] : index_ % 6;
            int col_index = (Col_ != nullptr) ? Col_[index_ / 6] : index_ / 6;
            return so6_.arr[(col_index << 2) + (col_index << 1) + row_index];
        }

        // Increment
        Iterator& operator++() { ++index_; return *this; }

        // Comparison
        bool operator!=(const Iterator& other) const { return index_ != other.index_; }
        bool operator==(const Iterator& other) const { return index_ == other.index_; }

    private:
        const SO6& so6_;
        int index_;
        const uint8_t *Row_;
        const uint8_t *Col_;
};
