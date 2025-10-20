#include <stdexcept>
#include "so6/SO6.hpp"
#include "config/Globals.hpp"
#include "util/utils.hpp"
#include "sort/sort6.hpp"

// Alias the values of std::strong_ordering for cleaner code
constexpr auto Equal = std::strong_ordering::equal;
constexpr auto Less = std::strong_ordering::less;
constexpr auto Greater = std::strong_ordering::greater;

/**
 * Basic constructor. Initializes Zero matrix.
 *
 */
SO6::SO6()
{
    // Packed buffer already zero-initialized via in-class initializer in header for arr24_.
}

// SO6Lite conversions removed in this build

const SO6& SO6::identity() {
    static const SO6 I = []() {
        SO6 temp;
        for (int k = 0; k < 6; k++) {
            // reserve() was a no-op for SmallFreqMap; removed for micro-optimization
            temp.set_element(static_cast<uint8_t>(k), static_cast<uint8_t>(k), Z2(1, 0, 0));
            #if (EXACT_FREQ_COLS_ONLY == 0) && (EXACT_FREQ_NONE == 0)
            temp.row_frequency[k][Z2(1, 0, 0)] = 1;
            temp.row_frequency[k][Z2(0, 0, 0)] = 5;
            #endif
            #if (EXACT_FREQ_NONE == 0)
            temp.col_frequency[k][Z2(1, 0, 0)] = 1;
            temp.col_frequency[k][Z2(0, 0, 0)] = 5;
            #endif
        }
        temp.canonical_form();
        temp.last_T = 15;
        temp.hash = prime;
        temp.col_hash = prime;
        return temp;
    }();
    return I;
}

/**
 * Overloads the * operator with matrix multiplication for SO6 objects
 * @param other reference to SO6 to be multiplied with (*this)
 * @return matrix multiplication of (*this) and other
 */
SO6 SO6::operator*(const SO6 &other) const
{
    SO6 prod;

    for (int row = 0; row < 6; ++row)
    {
        for (int k = 0; k < 6; ++k)
        {
            const Z2 left_element = get_element(static_cast<uint8_t>(row), static_cast<uint8_t>(k));
            if (left_element.int_c == 0) continue;
            for (int col = 0; col < 6; ++col)
            {
                Z2 right_element = other.get_element(static_cast<uint8_t>(k), static_cast<uint8_t>(col));
                if (right_element.int_c == 0) continue;
                Z2 cur = prod.get_element(static_cast<uint8_t>(row), static_cast<uint8_t>(col));
                cur += (left_element * right_element);
                prod.set_element(static_cast<uint8_t>(row), static_cast<uint8_t>(col), cur);
            }
        }
    }
    return prod;
}

SO6 SO6::left_multiply_by_T(const uint8_t i) const
{
    SO6 prod = *this;
    switch (i) {
        case 0: return left_multiply_by_T<0>(prod);
        case 1: return left_multiply_by_T<1>(prod);
        case 2: return left_multiply_by_T<2>(prod);
        case 3: return left_multiply_by_T<3>(prod);
        case 4: return left_multiply_by_T<4>(prod);
        case 5: return left_multiply_by_T<5>(prod);
        case 6: return left_multiply_by_T<6>(prod);
        case 7: return left_multiply_by_T<7>(prod);
        case 8: return left_multiply_by_T<8>(prod);
        case 9: return left_multiply_by_T<9>(prod);
        case 10: return left_multiply_by_T<10>(prod);
        case 11: return left_multiply_by_T<11>(prod);
        case 12: return left_multiply_by_T<12>(prod);
        case 13: return left_multiply_by_T<13>(prod);
        case 14: return left_multiply_by_T<14>(prod);
        default: throw std::invalid_argument("Invalid value for i");
    }
}

/**
 * @brief Transforms the current object into its canonical form.
 *
 * This function performs the following steps:
 * 1. Retrieves the row equivalence classes and copies them into the Row array.
 * 2. Retrieves the column equivalence classes and copies them into the Col array.
 * 3. Initializes row and column permutation arrays.
 * 4. Iterates over all possible sign conventions (32 in total).
 *    - For each sign convention:
 *      a. Copies the row equivalence classes into the row permutation array.
 *      b. Sorts each subset in the column equivalence classes independently based on the current sign convention.
 *      c. Copies the sorted column equivalence classes into the column permutation array.
 *      d. Checks if the current permutation is better than the previous one.
 *         - If it is, updates the Row and Col arrays with the current permutation and sets the sign convention.
 *    - Continues to the next equivalence class permutation.
 */


bool SO6::is_better_permutation(const Lehmer6& row_perm, const Lehmer6& col_perm, const uint16_t sign_perm) {
    // Decode only for comparison: build raw arrays via operator[] for both current and candidate perms
    uint8_t cur_row_a[6];
    uint8_t cur_col_a[6];
    uint8_t cand_row_a[6];
    uint8_t cand_col_a[6];
    for (int i = 0; i < 6; ++i) {
        // Decode current and candidate from Lehmer via operator[]
        cur_row_a[i]  = row_perm_lh_[i];
        cur_col_a[i]  = col_perm_lh_[i];
        cand_row_a[i] = row_perm[i];
        cand_col_a[i] = col_perm[i];
    }

    struct ArrayColIter {
        const SO6& s;
        const uint8_t* row;   // size 6
        const uint8_t* col;   // size 6
        int col_idx;          // 0..5
        int i;                // 0..6
        Z2 operator*() const {
            const int c = col ? col[col_idx] : col_idx;
            const int r = row ? row[i] : i;
            ASSUME(unsigned(c) < 6u);
            ASSUME(unsigned(r) < 6u);
            return s.get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c));
        }
        ArrayColIter& operator++() { ++i; return *this; }
        bool operator!=(const ArrayColIter& other) const { return i != other.i; }
    };

    for (int col = 0; col < 6; ++col) {
        ArrayColIter cur_begin{*this, cur_row_a, cur_col_a, col, 0};
        ArrayColIter cur_end  {*this, cur_row_a, cur_col_a, col, 6};
        ArrayColIter cand_begin{*this, cand_row_a, cand_col_a, col, 0};
        ArrayColIter cand_end  {*this, cand_row_a, cand_col_a, col, 6};
        auto comparison = utils::lex_order(cur_begin, cur_end, cand_begin, cand_end, sign_convention, sign_perm);
        if (comparison == Equal) continue;
        return comparison == Greater;
    }
    return false;
}

bool SO6::is_better_permutation(const uint8_t* cand_row, const uint8_t* cand_col, const uint16_t sign_perm) {
    // Decode current from Lehmer once for comparison
    uint8_t cur_row_a[6];
    uint8_t cur_col_a[6];
    for (int i = 0; i < 6; ++i) {
        cur_row_a[i] = row_perm_lh_[i];
        cur_col_a[i] = col_perm_lh_[i];
    }

    struct ArrayColIter {
        const SO6& s;
        const uint8_t* row;   // size 6
        const uint8_t* col;   // size 6
        int col_idx;          // 0..5
        int i;                // 0..6
        Z2 operator*() const {
            const int c = col ? col[col_idx] : col_idx;
            const int r = row ? row[i] : i;
            ASSUME(unsigned(c) < 6u);
            ASSUME(unsigned(r) < 6u);
            return s.get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c));
        }
        ArrayColIter& operator++() { ++i; return *this; }
        bool operator!=(const ArrayColIter& other) const { return i != other.i; }
    };

    for (int col = 0; col < 6; ++col) {
        ArrayColIter cur_begin{*this, cur_row_a, cur_col_a, col, 0};
        ArrayColIter cur_end  {*this, cur_row_a, cur_col_a, col, 6};
        ArrayColIter cand_begin{*this, cand_row, cand_col, col, 0};
        ArrayColIter cand_end  {*this, cand_row, cand_col, col, 6};
        auto comparison = utils::lex_order(cur_begin, cur_end, cand_begin, cand_end, sign_convention, sign_perm);
        if (comparison == Equal) continue;
        return comparison == Greater;
    }
    return false;
}

// Removed fully-decoded overload (not used)

/**
 * @brief Computes the row equivalence classes for the SO6 object.
 *
 * This function iterates through the rows of the SO6 object and groups them
 * into equivalence classes based on their frequency distribution. The result
 * is a map where the keys are maps representing the frequency distribution of
 * elements in each row, and the values are vectors containing the indices of
 * rows that share the same frequency distribution.
 *
 * @return A map where each key is a map of Z2 to int representing the frequency
 *         distribution of a row, and each value is a vector of row indices that
 *         have the same frequency distribution.
 */
// canonicalization and equivalence class helpers moved to src/algo/Canonicalizer.cpp

const std::strong_ordering SO6::operator<=>(const SO6 &other) const
{   
    std::strong_ordering comp = col_hash <=> other.col_hash;
    // return comp;

    if (comp == Equal) {   
        // Decode only for comparison from Lehmer6: build raw arrays via operator[]
        uint8_t this_row_a[6];
        uint8_t this_col_a[6];
        uint8_t other_row_a[6];
        uint8_t other_col_a[6];
        for (int i = 0; i < 6; ++i) {
            this_row_a[i]  = row_perm_lh_[i];
            this_col_a[i]  = col_perm_lh_[i];
            other_row_a[i] = other.row_perm_lh_[i];
            other_col_a[i] = other.col_perm_lh_[i];
        }

        struct ArrayColIter {
            const SO6& s;
            const uint8_t* row;   // size 6
            const uint8_t* col;   // size 6
            int col_idx;          // 0..5 (unpermuted column index)
            int i;                // 0..6 (row step)
            Z2 operator*() const {
                const int c = col ? col[col_idx] : col_idx;
                const int r = row ? row[i] : i;
                ASSUME(unsigned(c) < 6u);
                ASSUME(unsigned(r) < 6u);
                return s.get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c)); // c*6 + r
            }
            ArrayColIter& operator++() { ++i; return *this; }
            bool operator!=(const ArrayColIter& other) const { return i != other.i; }
        };

        for (int col = 0; col < 5; ++col) {
            ArrayColIter a_begin{*this, this_row_a, this_col_a, col, 0};
            ArrayColIter a_end  {*this, this_row_a, this_col_a, col, 6};
            ArrayColIter b_begin{other, other_row_a, other_col_a, col, 0};
            ArrayColIter b_end  {other, other_row_a, other_col_a, col, 6};

            auto result = utils::lex_order(a_begin, a_end, b_begin, b_end, sign_convention, other.sign_convention);
            if (result != Equal) return result;
        }
    }
    return  comp;
}

// Stream operator<< for SO6 removed (unused)
