#ifndef LUT_HPP
#define LUT_HPP

#include <vector>
#include <tbb/concurrent_unordered_set.h>
#include "./Z2.hpp"
#include "./SO6.hpp"

using t_count_data = tbb::concurrent_unordered_set<SO6>;

struct LUT {
public:
    size_t start = 0;

    LUT(size_t size = 0) : lookupTable(size) {}

    LUT(const std::initializer_list<std::vector<SO6>>& init_list) {
        lookupTable.reserve(init_list.size());
        for (const auto& vec : init_list) {
            lookupTable.emplace_back(vec.begin(), vec.end());
        }
        start = lookupTable.size();
    }

    void insert(size_t index, const SO6& value) {
        if (index < lookupTable.size()) {
            lookupTable[index].insert(value);
        }
    }

    bool contains(size_t index, const SO6& value) const {
        if (index < lookupTable.size()) {
            return lookupTable[index].find(value) != lookupTable[index].end();
        }
        return false;
    }

    std::optional<SO6> lookup(size_t index, const SO6 value) const {
        if (index < lookupTable.size()) {
            auto it = lookupTable[index].find(value);
            if (it != lookupTable[index].end()) {
                return *it;
            }
        }
        return std::nullopt;
    }

    std::string circuit_string(size_t index, SO6 s) const {
        if(index == start) return "F";
        SO6 next = lookup(index-1, s.left_multiply_by_T(s.last_T)).value();
        return std::to_string(s.last_T) + " " + circuit_string(index-1, next);
    }

    size_t size() const {
        return lookupTable.size();
    }
    
    t_count_data& operator[](size_t index) {
        return lookupTable[index+start];
    }

    t_count_data& at(size_t index) {
        return lookupTable[index];
    }

    void push_back(const t_count_data& set) {
        lookupTable.push_back(set);
    }

    const t_count_data& current() {
        return lookupTable.back();
    }

    const t_count_data& prior() const {
        return lookupTable[lookupTable.size() -2];
    }

    auto begin() {
        return lookupTable.begin() + start;
    }

    auto end() {
        return lookupTable.end();
    }

    Z2 get_maximum() {
        Z2 r(0,0,0);
        for(auto set : lookupTable) {
            for (auto s : set) {
                for(auto z : s.arr) {
                    if(std::abs(r).int_c < std::abs(z).int_c) {
                        r.int_c = z.int_c;
                    }
                    if(std::abs(r).sqrt2_c < std::abs(z).sqrt2_c) {
                        r.sqrt2_c = z.sqrt2_c;
                    }
                    if(std::abs(r).denom_exp < std::abs(z).denom_exp) {
                        r.denom_exp = z.denom_exp;
                    }
                }
            }
        }
        return r;
    }

private:
    std::vector<t_count_data> lookupTable;
};

#endif // LUT_HPP