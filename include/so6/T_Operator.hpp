#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>

#include "so6/SO6.hpp"
#include "ds/LinearTransform.hpp"

/**
 * @brief Lightweight representation of a T operator acting on SO6 implemented as a linear map.
 *
 * The template parameters encode the pair of rows affected by the T operator.
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
        apply_inplace_T<Row1, Row2, true>(S);
        S.last_T = index;
        return S;
    }

    static inline __attribute__((always_inline)) SO6 apply(const SO6& input) {
        SO6 copy = input;
        apply_inplace(copy);
        return copy;
    }

    // Apply without canonical_form (hash is still recomputed to keep metadata consistent)
    static inline __attribute__((always_inline)) SO6& apply_inplace_no_canonical(SO6& S) {
        apply_inplace_T<Row1, Row2, false>(S);
        S.last_T = index;
        return S;
    }

    // Multiply without canonical_form; mirrors operator* but skips canonicalization
    static inline __attribute__((always_inline)) SO6 multiply_no_metadata(const SO6& input) {
        SO6 copy = input;
        apply_inplace_no_canonical(copy);
        return copy;
    }
};

template<int Row1, int Row2>
inline SO6 operator*(const T_Operator<Row1, Row2>&, const SO6& rhs) {
    return T_Operator<Row1, Row2>::apply(rhs);
}

template<int Row1, int Row2>
inline SO6& multiply_no_metadata(SO6& right) {
    T_Operator<Row1, Row2>::apply_inplace(right);
    return right;
}

/**
 * @brief Runtime wrapper for selecting a T operator by index.
 */
class T_OperatorRuntime {
public:
    explicit constexpr T_OperatorRuntime(uint8_t idx, bool canonicalize = true)
        : index_(idx), canonicalize_(canonicalize) {}

    uint8_t index() const { return index_; }

    inline SO6 apply(const SO6& S) const {
        return (canonicalize_ ? TABLE_CAN[index_] : TABLE_NC[index_])(S);
    }

private:
    using ApplyFn = SO6 (*)(const SO6&);

    template<int Row1, int Row2>
    static inline SO6 apply_copy_can(const SO6& S) {
        return T_Operator<Row1, Row2>::apply(S);
    }
    template<int Row1, int Row2>
    static inline SO6 apply_copy_nc(const SO6& S) {
        return T_Operator<Row1, Row2>::multiply_no_metadata(S);
    }

    static constexpr std::array<ApplyFn, 15> init_table_can() {
        return std::array<ApplyFn, 15>{
            apply_copy_can<0,1>,  apply_copy_can<0,2>,  apply_copy_can<0,3>,  apply_copy_can<0,4>,  apply_copy_can<0,5>,
            apply_copy_can<1,2>,  apply_copy_can<1,3>,  apply_copy_can<1,4>,  apply_copy_can<1,5>,
            apply_copy_can<2,3>,  apply_copy_can<2,4>,  apply_copy_can<2,5>,
            apply_copy_can<3,4>,  apply_copy_can<3,5>,
            apply_copy_can<4,5>
        };
    }
    static constexpr std::array<ApplyFn, 15> init_table_nc() {
        return std::array<ApplyFn, 15>{
            apply_copy_nc<0,1>,  apply_copy_nc<0,2>,  apply_copy_nc<0,3>,  apply_copy_nc<0,4>,  apply_copy_nc<0,5>,
            apply_copy_nc<1,2>,  apply_copy_nc<1,3>,  apply_copy_nc<1,4>,  apply_copy_nc<1,5>,
            apply_copy_nc<2,3>,  apply_copy_nc<2,4>,  apply_copy_nc<2,5>,
            apply_copy_nc<3,4>,  apply_copy_nc<3,5>,
            apply_copy_nc<4,5>
        };
    }

    static inline const std::array<ApplyFn, 15> TABLE_CAN = init_table_can();
    static inline const std::array<ApplyFn, 15> TABLE_NC  = init_table_nc();

    uint8_t index_;
    bool canonicalize_;
};

inline SO6 operator*(const T_OperatorRuntime& op, const SO6& rhs) {
    return op.apply(rhs);
}
