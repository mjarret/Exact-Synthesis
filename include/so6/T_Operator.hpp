#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

#include "so6/SO6.hpp"

/**
 * @brief Lightweight representation of a T operator acting on SO6.
 *
 * The template parameters encode the pair of rows affected by the T move.
 * All logic is kept inline so the compiler can optimize exactly as before.
 */
template<int Row1, int Row2>
struct T_Operator {
    static_assert(Row1 >= 0 && Row1 < Row2 && Row2 < 6, "T_Operator row pair out of range");

    static constexpr uint8_t row1 = static_cast<uint8_t>(Row1);
    static constexpr uint8_t row2 = static_cast<uint8_t>(Row2);

private:
    static constexpr uint8_t compute_index() {
        uint8_t idx = 0;
        for (int r = 0; r < Row1; ++r) {
            idx = static_cast<uint8_t>(idx + static_cast<uint8_t>(5 - r));
        }
        idx = static_cast<uint8_t>(idx + static_cast<uint8_t>(Row2 - Row1 - 1));
        return idx;
    }

public:
    static constexpr uint8_t index = compute_index();

    static inline __attribute__((always_inline)) SO6& apply_inplace(SO6& S) {
        S.hash = static_cast<uint16_t>(S.hash
            - SO6::row_frequency_signature(S, row1)
            - SO6::row_frequency_signature(S, row2));

        for (int col = 0; col < 6; ++col) {
            uint16_t col_freq = SO6::col_frequency_signature(S, col);
            uint16_t col_sig = static_cast<uint16_t>(col_freq ^ (col_freq >> 1));
            S.hash = static_cast<uint16_t>(S.hash - col_sig);
            S.col_hash = static_cast<uint16_t>(S.col_hash - col_sig);

            Z2 a = S.get_element(row1, static_cast<uint8_t>(col));
            Z2 b = S.get_element(row2, static_cast<uint8_t>(col));
            const Z2 a_old = a;

            a += b;
            b -= a_old;
            b = -b;
            a.denom_exp += (a.int_c != 0);
            b.denom_exp += (b.int_c != 0);

            S.set_element(row1, static_cast<uint8_t>(col), a);
            S.set_element(row2, static_cast<uint8_t>(col), b);

            col_freq = SO6::col_frequency_signature(S, col);
            col_sig = static_cast<uint16_t>(col_freq ^ (col_freq >> 1));
            S.hash = static_cast<uint16_t>(S.hash + col_sig);
            S.col_hash = static_cast<uint16_t>(S.col_hash + col_sig);
        }

        S.canonical_form();
        S.hash = static_cast<uint16_t>(S.hash
            + SO6::row_frequency_signature(S, row1)
            + SO6::row_frequency_signature(S, row2));
        S.last_T = index;
        return S;
    }

    static inline __attribute__((always_inline)) SO6 apply(const SO6& input) {
        SO6 copy = input;
        apply_inplace(copy);
        return copy;
    }
};

template<int Row1, int Row2>
inline SO6 operator*(const T_Operator<Row1, Row2>&, const SO6& rhs) {
    return T_Operator<Row1, Row2>::apply(rhs);
}

/**
 * @brief Runtime wrapper for selecting a T operator by index.
 */
class T_OperatorRuntime {
public:
    explicit constexpr T_OperatorRuntime(uint8_t idx) : index_(idx) {}

    uint8_t index() const { return index_; }

    inline SO6 apply(const SO6& S) const {
        return TABLE[index_](S);
    }

private:
    using ApplyFn = SO6 (*)(const SO6&);

    template<int Row1, int Row2>
    static inline SO6 apply_copy(const SO6& S) {
        return T_Operator<Row1, Row2>::apply(S);
    }

    static constexpr std::array<ApplyFn, 15> init_table() {
        return std::array<ApplyFn, 15>{
            apply_copy<0,1>,  apply_copy<0,2>,  apply_copy<0,3>,  apply_copy<0,4>,  apply_copy<0,5>,
            apply_copy<1,2>,  apply_copy<1,3>,  apply_copy<1,4>,  apply_copy<1,5>,
            apply_copy<2,3>,  apply_copy<2,4>,  apply_copy<2,5>,
            apply_copy<3,4>,  apply_copy<3,5>,
            apply_copy<4,5>
        };
    }

    static inline const std::array<ApplyFn, 15> TABLE = init_table();

    uint8_t index_;
};

inline SO6 operator*(const T_OperatorRuntime& op, const SO6& rhs) {
    return op.apply(rhs);
}
