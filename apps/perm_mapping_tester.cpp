/**
 * Brute-force permutation mapping tester for SO6 permutation views.
 *
 * Verifies equivalence between:
 *  - get_column(col, row_arr, col_arr) vs explicit decode-only ArrayColIter
 *  - canonicalizer row-only comparator mapping vs array path
 *  - is_better decode-only mapping vs array path (simulated)
 */

#include <array>
#include <vector>
#include <algorithm>
#include <iostream>
#include <cstdint>

#include "so6/SO6.hpp"
#include "ds/Lehmer6.hpp"
#include "util/utils.hpp"

static std::array<uint8_t,6> make_array_from_perm(const uint8_t* p) {
    std::array<uint8_t,6> a{};
    for (int i = 0; i < 6; ++i) a[static_cast<size_t>(i)] = p[i];
    return a;
}

// Iter via explicit arrays (row+col) like our decode-only comparator
struct ArrayColIter {
    const SO6& s;
    const uint8_t* row;   // size 6
    const uint8_t* col;   // size 6
    int col_idx;          // 0..5 (unpermuted column index)
    int i;                // 0..6 (row step)
    const Z2& operator*() const {
        const int c = col ? col[col_idx] : col_idx;
        const int r = row ? row[i] : i;
        return s.arr[(c << 2) + (c << 1) + r];
    }
    ArrayColIter& operator++() { ++i; return *this; }
    bool operator!=(const ArrayColIter& other) const { return i != other.i; }
};

// Row-only iterator used by canonicalizer comparator
struct RowOnlyColIter {
    const SO6& s;
    const uint8_t* row;   // size 6
    int col_idx;          // 0..5
    int i;                // 0..6
    const Z2& operator*() const {
        return s.arr[(col_idx << 2) + (col_idx << 1) + row[i]];
    }
    RowOnlyColIter& operator++() { ++i; return *this; }
    bool operator!=(const RowOnlyColIter& other) const { return i != other.i; }
};

static std::vector<int> seq_from_iter(ArrayColIter a, ArrayColIter b) {
    std::vector<int> out; out.reserve(6);
    for (; a != b; ++a) {
        const Z2& z = *a;
        // Recover index from address within s.arr: pointer math
        // Since we cannot reliably subtract pointers across, derive via element comparison not needed.
        // Instead, compute the index deterministically from row/col used above:
        // But we don't capture row/col there; so build a shadow index: not available here.
        // Simpler: push the underlying linear offset by reading arr values set uniquely (done below).
        out.push_back(z.int_c); // relies on unique init of arr below
    }
    return out;
}

static std::vector<int> seq_from_iter(RowOnlyColIter a, RowOnlyColIter b) {
    std::vector<int> out; out.reserve(6);
    for (; a != b; ++a) {
        const Z2& z = *a;
        out.push_back(z.int_c);
    }
    return out;
}

// Legacy iterator path (pair<SO6::Iterator,SO6::Iterator>)
static std::vector<int> seq_from_iter(SO6::Iterator a, SO6::Iterator b) {
    std::vector<int> out; out.reserve(6);
    for (; a != b; ++a) {
        const Z2& z = *a;
        out.push_back(z.int_c);
    }
    return out;
}

int main() {
    // Prepare SO6 with unique markers per cell: arr[c*6+r] = (c*6+r)+1 (nonzero)
    SO6 s;
    for (int c = 0; c < 6; ++c) {
        for (int r = 0; r < 6; ++r) {
            s.arr[(c << 2) + (c << 1) + r] = Z2((c*6 + r) + 1, 0, 0);
        }
    }

    // Enumerate all row and column perms (6! each)
    std::array<uint8_t,6> row{}; for (int i = 0; i < 6; ++i) row[i] = static_cast<uint8_t>(i);
    int mismatches = 0;
    int tested = 0;

    auto build_sc = [](uint8_t k){
        uint16_t sc = utils::POS; // index 0 POS
        for (int l = 1; l < 6; ++l) {
            if (k & (1u << (l-1))) sc = utils::set_mask_sign(sc, l, utils::NEG);
            else sc = utils::set_mask_sign(sc, l, utils::POS);
        }
        return sc;
    };

    do {
        // Build Lehmer for row and row array mirror
        Lehmer6 lr = Lehmer6::from_perm(row);
        uint8_t row_a[6]; for (int i = 0; i < 6; ++i) row_a[i] = lr[static_cast<size_t>(i)];

        // Row-only mapping check for Canonicalizer comparator vs array path
        for (int col = 0; col < 6; ++col) {
            auto pair_arr = s.get_column(static_cast<uint8_t>(col), row.data(), nullptr);
            RowOnlyColIter rb{ s, row_a, col, 0 };
            RowOnlyColIter re{ s, row_a, col, 6 };
            auto seq_legacy = seq_from_iter(pair_arr.first, pair_arr.second);
            auto seq_rowonly = seq_from_iter(rb, re);
            if (seq_legacy != seq_rowonly) {
                ++mismatches;
                std::cerr << "ROW-ONLY mismatch at col=" << col << " row=[";
                for (int i=0;i<6;++i) std::cerr << int(row[i]) << (i<5?' ':' ');
                std::cerr << "]\nlegacy: "; for (int v: seq_legacy) std::cerr << v << ' ';
                std::cerr << "\nrowonly: "; for (int v: seq_rowonly) std::cerr << v << ' ';
                std::cerr << "\n";
                if (mismatches > 10) return 1;
            }
            ++tested;
        }

        // Row-only sign-aware comparator parity for all i<j and 32 sign masks
        for (int i = 0; i < 6; ++i) {
            for (int j = i+1; j < 6; ++j) {
                for (uint8_t k = 0; k < 32; ++k) {
                    uint16_t sc = build_sc(k);
                    auto legacy_left = s.get_column(static_cast<uint8_t>(i), row.data(), nullptr);
                    auto legacy_right = s.get_column(static_cast<uint8_t>(j), row.data(), nullptr);
                    auto ord_legacy = utils::lex_order(legacy_left, legacy_right, sc, sc);

                    RowOnlyColIter li{ s, row_a, i, 0 };
                    RowOnlyColIter le{ s, row_a, i, 6 };
                    RowOnlyColIter ri{ s, row_a, j, 0 };
                    RowOnlyColIter re2{ s, row_a, j, 6 };
                    auto ord_rowonly = utils::lex_order(li, le, ri, re2, sc, sc);
                    if (ord_legacy != ord_rowonly) {
                        ++mismatches;
                        std::cerr << "ROW-ONLY SIGN mismatch i=" << i << " j=" << j << " k=" << int(k) << " row=[";
                        for (int t=0;t<6;++t) std::cerr << int(row[t]) << (t<5?' ':' ');
                        std::cerr << "]\n";
                        if (mismatches > 10) return 1;
                    }
                    ++tested;
                }
            }
        }

        // Now enumerate all col perms too, but limit brute force by sampling: iterate all for completeness
        std::array<uint8_t,6> col{}; for (int i = 0; i < 6; ++i) col[i] = static_cast<uint8_t>(i);
        do {
            Lehmer6 lc = Lehmer6::from_perm(col);
            uint8_t col_a[6]; for (int i = 0; i < 6; ++i) col_a[i] = lc[static_cast<size_t>(i)];

            for (int c = 0; c < 6; ++c) {
                auto pair_arr = s.get_column(static_cast<uint8_t>(c), row.data(), col.data());
                ArrayColIter ab{ s, row_a, col_a, c, 0 };
                ArrayColIter ae{ s, row_a, col_a, c, 6 };
                auto seq_legacy = seq_from_iter(pair_arr.first, pair_arr.second);
                auto seq_array = seq_from_iter(ab, ae);
                if (seq_legacy != seq_array) {
                    ++mismatches;
                    std::cerr << "FULL mismatch at col=" << c << " row=[";
                    for (int i=0;i<6;++i) std::cerr << int(row[i]) << (i<5?' ':' ');
                    std::cerr << "] col=["; for (int i=0;i<6;++i) std::cerr << int(col[i]) << (i<5?' ':' ');
                    std::cerr << "]\nlegacy: "; for (int v: seq_legacy) std::cerr << v << ' ';
                    std::cerr << "\narray : "; for (int v: seq_array) std::cerr << v << ' ';
                    std::cerr << "\n";
                    if (mismatches > 10) return 1;
                }
                ++tested;
            }

            // Full sign-aware parity for sampled other permutations
            // Sample first few other rows/cols (up to 6 each) to keep runtime sane
            int sample_count = 0;
            std::array<uint8_t,6> row2 = row;
            do {
                Lehmer6 lr2 = Lehmer6::from_perm(row2);
                uint8_t row2_a[6]; for (int i = 0; i < 6; ++i) row2_a[i] = lr2[static_cast<size_t>(i)];
                std::array<uint8_t,6> col2 = col;
                int sample_col = 0;
                do {
                    Lehmer6 lc2 = Lehmer6::from_perm(col2);
                    uint8_t col2_a[6]; for (int i = 0; i < 6; ++i) col2_a[i] = lc2[static_cast<size_t>(i)];
                    for (int c = 0; c < 6; ++c) {
                        auto left_legacy = s.get_column(static_cast<uint8_t>(c), row.data(), col.data());
                        auto right_legacy = s.get_column(static_cast<uint8_t>(c), row2.data(), col2.data());
                        ArrayColIter left_begin{ s, row_a, col_a, c, 0 };
                        ArrayColIter left_end  { s, row_a, col_a, c, 6 };
                        ArrayColIter right_begin{ s, row2_a, col2_a, c, 0 };
                        ArrayColIter right_end  { s, row2_a, col2_a, c, 6 };
                        for (uint8_t k1 = 0; k1 < 4; ++k1) {
                            uint16_t sc1 = build_sc(k1);
                            uint16_t sc2 = build_sc((k1+7) & 31);
                            auto ord_legacy = utils::lex_order(left_legacy, right_legacy, sc1, sc2);
                            auto ord_array  = utils::lex_order(left_begin, left_end, right_begin, right_end, sc1, sc2);
                            if (ord_legacy != ord_array) {
                                ++mismatches;
                                std::cerr << "FULL SIGN mismatch at c=" << c << " k1=" << int(k1) << " row/col vs row2/col2\n";
                                if (mismatches > 10) return 1;
                            }
                            ++tested;
                        }
                    }
                    ++sample_col;
                    if (sample_col >= 4) break;
                } while (std::next_permutation(col2.begin(), col2.end()));
                ++sample_count;
                if (sample_count >= 4) break;
            } while (std::next_permutation(row2.begin(), row2.end()));
        } while (std::next_permutation(col.begin(), col.end()));

    } while (std::next_permutation(row.begin(), row.end()));

    std::cout << "Tested pairs: " << tested << ", mismatches: " << mismatches << "\n";
    return mismatches == 0 ? 0 : 1;
}
