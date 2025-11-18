#include <stdexcept>
#include "so6/SO6.hpp"
#include "config/Globals.hpp"
#include "util/utils.hpp"
#include "sort/sort6.hpp"

// Alias the values of std::strong_ordering for cleaner code
constexpr auto Equal = std::strong_ordering::equal;
constexpr auto Less = std::strong_ordering::less;
constexpr auto Greater = std::strong_ordering::greater;

namespace {
    // Load 24-bit packed Z2 from column base and row index (column-major, 3 bytes per entry)
    static inline __attribute__((always_inline)) Z2 load_z2_from_col_row(const uint8_t* base_col, uint8_t row) {
        const uint8_t* p = base_col + static_cast<int>(row) * 3;
        uint32_t v = static_cast<uint32_t>(p[0])
                   | (static_cast<uint32_t>(p[1]) << 8)
                   | (static_cast<uint32_t>(p[2]) << 16);
        return Z2(v);
    }

    // Compare a single column under row/col permutations and sign masks
    static inline __attribute__((always_inline)) std::strong_ordering cmp_col_fast(
        const SO6& s,
        const uint8_t* rowL, const uint8_t* colL,
        const uint8_t* rowR, const uint8_t* colR,
        uint16_t first_sign_mask, uint16_t second_sign_mask,
        int col_idx)
    {
        const int cL = colL ? colL[col_idx] : col_idx;
        const int cR = colR ? colR[col_idx] : col_idx;
        const uint8_t* baseL = s.arr24_ + cL * 18; // 6 rows * 3 bytes
        const uint8_t* baseR = s.arr24_ + cR * 18;

        int i = 0;
        std::strong_ordering comp1 = Equal;
        std::strong_ordering comp2 = Equal;

        // Phase 1: find orientation (first non-zero)
        for (; i < 6; ++i) {
            const Z2 L = load_z2_from_col_row(baseL, rowL ? rowL[i] : static_cast<uint8_t>(i));
            const Z2 R = load_z2_from_col_row(baseR, rowR ? rowR[i] : static_cast<uint8_t>(i));
            comp1 = L.int_c <=> 0;
            comp2 = R.int_c <=> 0;
            if (comp1 == Equal && comp2 == Equal) continue;
            if (comp1 == Equal) return Greater;
            if (comp2 == Equal) return Less;
            const uint8_t fsm = static_cast<uint8_t>((first_sign_mask  >> i) & utils::BITS);
            const uint8_t ssm = static_cast<uint8_t>((second_sign_mask >> i) & utils::BITS);
            if ((comp1 == Less) ^ (fsm == 1)) first_sign_mask  ^= 0x3Fu;
            if ((comp2 == Less) ^ (ssm == 1)) second_sign_mask ^= 0x3Fu;
            break;
        }

        // Phase 2: lex compare with sign masks
        for (; i < 6; ++i) {
            const Z2 L = load_z2_from_col_row(baseL, rowL ? rowL[i] : static_cast<uint8_t>(i));
            const Z2 R = load_z2_from_col_row(baseR, rowR ? rowR[i] : static_cast<uint8_t>(i));
            const bool first_is_neg  = (((first_sign_mask  >> i) & utils::BITS) == utils::NEG);
            const bool second_is_neg = (((second_sign_mask >> i) & utils::BITS) == utils::NEG);
            const std::strong_ordering cmp = (second_is_neg ? -R : R) <=> (first_is_neg ? -L : L);
            if (cmp == Equal) continue;
            if (L.int_c == 0) return Greater;
            if (R.int_c == 0) return Less;
            return cmp;
        }
        return Equal;
    }
}

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
        for (uint8_t k = 0; k < 6; k++) temp.set_element(k, k, Z2(1, 0, 0));
        temp.canonical_form();
        temp.last_T = 15;
        temp.recompute_hash();
        return temp;
    }();
    return I;
}

void SO6::recompute_hash() {
    uint16_t hash_acc = 0;
    uint16_t col_hash_acc = 0;
    // Columns contribute their Gray-like folded signature to both hash fields
    for (int col = 0; col < 6; ++col) {
        uint16_t col_freq = col_frequency_signature(*this, col);
        uint16_t col_sig = static_cast<uint16_t>(col_freq ^ (col_freq >> 1));
        hash_acc     = static_cast<uint16_t>(hash_acc + col_sig);
        col_hash_acc = static_cast<uint16_t>(col_hash_acc + col_sig);
    }
    // Rows contribute their frequency signatures to the primary hash only
    for (int row = 0; row < 6; ++row) {
        uint16_t row_freq = row_frequency_signature(*this, row);
        hash_acc = static_cast<uint16_t>(hash_acc + row_freq);
    }
    hash = hash_acc;
    col_hash = col_hash_acc;
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
            const Z2 left_element = get_element(row, k);
            if (left_element.int_c == 0) continue;
            for (int col = 0; col < 6; ++col)
            {
                Z2 right_element = other.get_element(k, col);
                if (right_element.int_c == 0) continue;
                Z2 cur = prod.get_element(row, col);
                cur += (left_element * right_element);
                prod.set_element(row, col, cur);
            }
        }
    }
    return prod;
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
    uint8_t cur_row_a[6];
    uint8_t cur_col_a[6];
    uint8_t cand_row_a[6];
    uint8_t cand_col_a[6];
    for (int i = 0; i < 6; ++i) {
        cur_row_a[i]  = row_perm_lh_[i];
        cur_col_a[i]  = col_perm_lh_[i];
        cand_row_a[i] = row_perm[i];
        cand_col_a[i] = col_perm[i];
    }
    for (int col = 0; col < 6; ++col) {
        auto cmp = cmp_col_fast(*this, cur_row_a, cur_col_a, cand_row_a, cand_col_a, sign_convention, sign_perm, col);
        if (cmp == Equal) continue;
        return cmp == Greater;
    }
    return false;
}

bool SO6::is_better_permutation(const uint8_t* cand_row, const uint8_t* cand_col, const uint16_t sign_perm) {
    // Fast path
    uint8_t cur_row_a[6];
    uint8_t cur_col_a[6];
    for (int i = 0; i < 6; ++i) {
        cur_row_a[i] = row_perm_lh_[i];
        cur_col_a[i] = col_perm_lh_[i];
    }
    for (int col = 0; col < 6; ++col) {
        auto cmp = cmp_col_fast(*this, cur_row_a, cur_col_a, cand_row, cand_col, sign_convention, sign_perm, col);
        if (cmp == Equal) continue;
        return cmp == Greater;
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

void SO6::print_raw(std::ostream& os) const {
    for (int r = 0; r < 6; ++r) {
        os << "[";
        for (int c = 0; c < 6; ++c) {
            const auto v = get_element(r,c);
            os << v;
            if (c != 5) os << " ";
        }
        os << "]";
        if (r != 5) os << "\n";
    }
}

void SO6::print_with_perms(std::ostream& os) const {
    const auto& row_perm = Lehmer6::decode_ref(row_perm_lh_.bits());
    const auto& col_perm = Lehmer6::decode_ref(col_perm_lh_.bits());
    int count_r = 0;
    uint8_t col_sign_mask = col_sign();
    for (auto src_row : row_perm) {
        os << "[";
        bool row_neg = ((sign_convention >> src_row) & 1u) != 0;
        int count_c = 0;
        for (auto src_col : col_perm) {
            bool col_neg = ((col_sign_mask >> src_col) & 1u) != 0;
            auto v = get_element(src_row, src_col);
            if (row_neg^col_neg) v = -v;
            os << v;
            if (count_c != 5) os << " ";
            count_c++;
        }
        os << "]";
        if (count_r != 5) os << "\n";
        count_r++;
    }
}

SO6 SO6::materialize_canonical() const {
    SO6 out;
    const auto& row_perm = Lehmer6::decode_ref(row_perm_lh_.bits());
    const auto& col_perm = Lehmer6::decode_ref(col_perm_lh_.bits());
    for (uint8_t r = 0; r < 6; ++r) {
        uint8_t src_row = row_perm[r];
        bool row_neg = ((sign_convention >> src_row) & 1u) != 0;
        for (uint8_t c = 0; c < 6; ++c) {
            uint8_t src_col = col_perm[c];
            auto v = get_element(src_row, src_col);
            if (row_neg) v = -v;
            out.set_element(r, c, v);
        }
    }
    out.sign_convention = 0;
    out.row_perm_lh_ = Lehmer6(); // identity
    out.col_perm_lh_ = Lehmer6(); // identity
    out.last_T = last_T;
    out.recompute_hash();
    return out;
}

uint8_t SO6::col_sign() const {
    uint8_t mask = 0;
    for (uint8_t c = 0; c < 6; ++c) {
        for (auto r : row_perm_lh_.to_array()) {
            Z2 v = get_element(r,c);
            if (v > 0) {
                // bit remains 0 for positive
                break;
            } else if (v < 0) {
                mask |= static_cast<uint8_t>(1u << c);
                break;
            }
        }
    }
    return mask;
}
