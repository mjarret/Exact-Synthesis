#include "so6/SO6.hpp"
#include "util/utils.hpp"
#include "sort/sort6.hpp"
#include <algorithm>

using std::strong_ordering;
constexpr auto Equal = std::strong_ordering::equal;
constexpr auto Less = std::strong_ordering::less;
constexpr auto Greater = std::strong_ordering::greater;

void SO6::canonical_form() {
    // Get equivalence classes and put into a consistent form
    auto row_ecs = row_equivalence_classes();

    uint8_t* ptr = Row;
    for (const auto& [row_freq_map, rows] : row_ecs) {
        ptr = std::copy(rows.begin(), rows.end(), ptr);
    }

    ptr = Col;
    auto col_ecs = col_equivalence_classes();
    for (const auto& [col_freq_map, cols] : col_ecs) {
        ptr = std::copy(cols.begin(), cols.end(), ptr);
    }

    uint8_t row_perm[6];
    uint8_t col_perm[6];
    
    do { 
        ptr = row_perm;
        for (const auto&[key, group] : row_ecs) {
            ptr = std::copy(group.begin(), group.end(), ptr);
        }
        
        for(uint8_t k = 0; k < 32; ++k) {
            uint16_t sc = utils::POS; // utils::POS = 0b10
            for(int l = 1; l < 6; ++l) {
                if (k & (1 << (l-1))) {
                    sc = utils::set_mask_sign(sc, l, utils::NEG);
                } else {
                    sc = utils::set_mask_sign(sc, l, utils::POS);
                }
            }

            auto comparator = [&](int i, int j) {
                auto left = get_column(i, row_perm);
                auto right = get_column(j, row_perm);
                return Less == utils::lex_order(left, right, sc, sc);
            };

            ptr = col_perm;
            for (auto &[key, col_class] : col_ecs) {
                sort6::sorting_network_dispatch(col_class, comparator);
                ptr = std::copy(col_class.begin(), col_class.end(), ptr);
            }

            if (is_better_permutation(row_perm, col_perm, sc)) {
                std::copy(row_perm, row_perm + 6, Row);
                std::copy(col_perm, col_perm + 6, Col);
                sign_convention = sc;
            }
        }
    }  while (get_next_equivalence_class(row_ecs));
}

FrequencyTable SO6::row_equivalence_classes() {
    FrequencyTable ret;
    for (int row = 0; row < 6; ++row) {
        FrequencyKey key;
        // Copy entries into a small sortable array
        for (const auto& kv : row_frequency[row]) {
            if (key.size < key.entries.size()) {
                key.entries[key.size++] = {kv.first, kv.second};
            }
        }
        const std::size_t n = std::min<std::size_t>(key.size, key.entries.size());
        std::sort(key.entries.begin(), key.entries.begin() + n,
                  [](auto const& a, auto const& b){ if (auto c = a.first <=> b.first; c != 0) return c < 0; return a.second < b.second; });
        ret[key].push_back(row);
    }
    return ret;
}

FrequencyTable SO6::col_equivalence_classes() {
    FrequencyTable ret;
    for (int col = 0; col < 6; ++col) {
        FrequencyKey key;
        for (const auto& kv : col_frequency[col]) {
            if (key.size < key.entries.size()) {
                key.entries[key.size++] = {kv.first, kv.second};
            }
        }
        const std::size_t n = std::min<std::size_t>(key.size, key.entries.size());
        std::sort(key.entries.begin(), key.entries.begin() + n,
                  [](auto const& a, auto const& b){ if (auto c = a.first <=> b.first; c != 0) return c < 0; return a.second < b.second; });
        ret[key].push_back(col);
    }
    return ret;
}

bool SO6::get_next_equivalence_class(FrequencyTable& row_equivalence_classes) {
    bool more_permutations = false;
    for (auto&[key,group] : row_equivalence_classes) {
        if (std::next_permutation(group.begin(), group.end())) {
            more_permutations = true;
            break;
        } else {
            std::sort(group.begin(), group.end());
        }
    }

    return more_permutations;
}
