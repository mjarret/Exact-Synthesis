#include "so6/SO6.hpp"
#include "util/utils.hpp"
#include "sort/sort6.hpp"
#include <algorithm>
#include <array>
#include "ds/Lehmer6.hpp"

using std::strong_ordering;
constexpr auto Equal = std::strong_ordering::equal;
constexpr auto Less = std::strong_ordering::less;
constexpr auto Greater = std::strong_ordering::greater;

void SO6::canonical_form() {
    // Get equivalence classes and put into a consistent form
    auto row_ecs = row_equivalence_classes();

    // Seed initial permutations directly from equivalence classes (no Row/Col writes)
    uint8_t row_init[6];
    uint8_t* ptr = row_init;
    std::vector<FrequencyKey> row_keys; row_keys.reserve(row_ecs.size());
    for (auto const& kv : row_ecs) row_keys.push_back(kv.first);
    std::sort(row_keys.begin(), row_keys.end());
    for (auto const& key : row_keys) {
        auto it = row_ecs.find(key);
        uint8_t tmp[6];
        const uint8_t k = it->second.size();
        it->second.to_array(tmp);
        for (uint8_t j = 0; j < k; ++j) *ptr++ = tmp[j];
    }

    uint8_t col_init[6];
    ptr = col_init;
    auto col_ecs = col_equivalence_classes();
    std::vector<FrequencyKey> col_keys; col_keys.reserve(col_ecs.size());
    for (auto const& kv : col_ecs) col_keys.push_back(kv.first);
    std::sort(col_keys.begin(), col_keys.end());
    for (auto const& key : col_keys) {
        auto it = col_ecs.find(key);
        uint8_t tmp[6];
        const uint8_t k = it->second.size();
        it->second.to_array(tmp);
        for (uint8_t j = 0; j < k; ++j) *ptr++ = tmp[j];
    }

    // Initialize Lehmer as source of truth
    {
        std::array<uint8_t, 6> ra{}; for (int ii = 0; ii < 6; ++ii) ra[ii] = row_init[ii];
        std::array<uint8_t, 6> ca{}; for (int ii = 0; ii < 6; ++ii) ca[ii] = col_init[ii];
        row_perm_lh_ = Lehmer6::from_perm(ra);
        col_perm_lh_ = Lehmer6::from_perm(ca);
    }

    uint8_t row_perm[6];
    uint8_t col_perm[6];
    
    do { 
        ptr = row_perm;
        for (auto const& key : row_keys) {
            auto it = row_ecs.find(key);
            uint8_t tmp[6];
            const uint8_t k = it->second.size();
            it->second.to_array(tmp);
            for (uint8_t j = 0; j < k; ++j) *ptr++ = tmp[j];
        }
        // Use row_perm directly for row-only comparator (avoid encode/decode)
        const uint8_t* row_a = row_perm;
        
        for(uint8_t k = 0; k < 32; ++k) {
            uint16_t sc = utils::POS; // utils::POS = 0b10
            for(int l = 1; l < 6; ++l) {
                if (k & (1 << (l-1))) {
                    sc = utils::set_mask_sign(sc, l, utils::NEG);
                } else {
                    sc = utils::set_mask_sign(sc, l, utils::POS);
                }
            }

            // Local iterator using row_a and fixed column index (no get_column, no Col perm)
            struct RowOnlyColIter {
                const SO6& s;
                const uint8_t* row; // size 6
                int col_idx;        // 0..5
                int i;              // 0..6
                Z2 operator*() const { return s.get_element(row[i], col_idx); }
                RowOnlyColIter& operator++() { ++i; return *this; }
                bool operator!=(const RowOnlyColIter& other) const { return i != other.i; }
            };

            auto comparator = [&](int i, int j) {
                RowOnlyColIter li{ *this, row_a, i, 0 };
                RowOnlyColIter le{ *this, row_a, i, 6 };
                RowOnlyColIter ri{ *this, row_a, j, 0 };
                RowOnlyColIter re{ *this, row_a, j, 6 };
                return Less == utils::lex_order(li, le, ri, re, sc, sc);
            };

            ptr = col_perm;
            for (auto const& key : col_keys) {
                auto itc = col_ecs.find(key);
                uint8_t tmp[6];
                const uint8_t k = itc->second.size();
                itc->second.to_array(tmp);
                struct PermView { using value_type=uint8_t; uint8_t* d; size_t n; size_t size() const { return n; } uint8_t& operator[](size_t i){ return d[i]; } const uint8_t& operator[](size_t i) const { return d[i]; } uint8_t* begin(){ return d; } uint8_t* end(){ return d+n; } };
                PermView pv{tmp, k};
                sort6::sorting_network_dispatch(pv, comparator);
                // re-encode and append
                itc->second = order6::Order6::from_array(tmp, k);
                for (uint8_t j = 0; j < k; ++j) *ptr++ = tmp[j];
            }

            // Compare using candidate arrays directly (current decoded inside is_better)
            if (is_better_permutation(row_perm, col_perm, sc)) {
                // Source of truth: assign Lehmer
                {
                    std::array<uint8_t, 6> ra{}; for (int ii = 0; ii < 6; ++ii) ra[ii] = row_perm[ii];
                    std::array<uint8_t, 6> ca{}; for (int ii = 0; ii < 6; ++ii) ca[ii] = col_perm[ii];
                    row_perm_lh_ = Lehmer6::from_perm(ra);
                    col_perm_lh_ = Lehmer6::from_perm(ca);
                }
                sign_convention = sc;
            }
        }
    }  while (get_next_equivalence_class(row_ecs));

}

FrequencyTable SO6::row_equivalence_classes() {
    std::map<FrequencyKey, std::vector<uint8_t>> tmp;
    for (int row = 0; row < 6; ++row) {
        FrequencyKey key;
        #if (EXACT_FREQ_COLS_ONLY == 0) && (EXACT_FREQ_NONE == 0)
        for (const auto& kv : row_frequency[row]) {
            if (key.size < key.entries.size()) key.entries[key.size++] = {kv.first, kv.second};
        }
        #else
        // Build frequencies by scanning matrix row
        FrequencyMap fm;
        for (int c = 0; c < 6; ++c) { fm[ std::abs(get_element(static_cast<uint8_t>(row), static_cast<uint8_t>(c)) ) ]++; }
        for (const auto& kv : fm) {
            if (key.size < key.entries.size()) key.entries[key.size++] = {kv.first, kv.second};
        }
        #endif
        const std::size_t n = std::min<std::size_t>(key.size, key.entries.size());
        std::sort(key.entries.begin(), key.entries.begin() + n,
                  [](auto const& a, auto const& b){ if (auto c = a.first <=> b.first; c != 0) return c < 0; return a.second < b.second; });
        tmp[key].push_back(static_cast<uint8_t>(row));
    }
    FrequencyTable ret;
    for (auto& [k, list] : tmp) {
        ret[k] = order6::Order6::from_array(list.data(), static_cast<uint8_t>(list.size()));
    }
    return ret;
}

FrequencyTable SO6::col_equivalence_classes() {
    std::map<FrequencyKey, std::vector<uint8_t>> tmp;
    for (int col = 0; col < 6; ++col) {
        FrequencyKey key;
        #if (EXACT_FREQ_NONE == 0)
        for (const auto& kv : col_frequency[col]) {
            if (key.size < key.entries.size()) key.entries[key.size++] = {kv.first, kv.second};
        }
        #else
        // Build frequencies by scanning matrix column
        FrequencyMap fm;
        for (int r = 0; r < 6; ++r) { fm[ std::abs(get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(col)) ) ]++; }
        for (const auto& kv : fm) {
            if (key.size < key.entries.size()) key.entries[key.size++] = {kv.first, kv.second};
        }
        #endif
        const std::size_t n = std::min<std::size_t>(key.size, key.entries.size());
        std::sort(key.entries.begin(), key.entries.begin() + n,
                  [](auto const& a, auto const& b){ if (auto c = a.first <=> b.first; c != 0) return c < 0; return a.second < b.second; });
        tmp[key].push_back(static_cast<uint8_t>(col));
    }
    FrequencyTable ret;
    for (auto& [k, list] : tmp) {
        ret[k] = order6::Order6::from_array(list.data(), static_cast<uint8_t>(list.size()));
    }
    return ret;
}

bool SO6::get_next_equivalence_class(FrequencyTable& ecs) {
    std::vector<FrequencyKey> keys; keys.reserve(ecs.size());
    for (auto const& kv : ecs) keys.push_back(kv.first);
    std::sort(keys.begin(), keys.end());
    for (auto const& k : keys) {
        auto it = ecs.find(k);
        if (it->second.next_permutation()) return true;
        it->second.reset();
    }
    return false;
}
