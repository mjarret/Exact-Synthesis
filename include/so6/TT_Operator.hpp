#pragma once
// ----------------------------------------------------------------------------
// TT operators: one move applies T(second) * T(first) to an SO6.
//
// Two dispatch surfaces:
//   - HOT path: apply_T_values_only_rt(t, S) -- a 15-entry value-only single-T
//     dispatch used by the shared-first-T fan-out (apply the first T once to a
//     shared intermediate, then one second-T butterfly per candidate). This is the
//     ONLY transform machinery the TT BFS hot loop uses.
//   - COLD path: TT_OperatorRuntime / TT_Operator<...> -- the fused one-shot kernel
//     used for path reconstruction, the brute-force extension, and tests. Dispatched
//     by move id through a 165-entry table (defined in src/TT_Operator.cpp).
// ----------------------------------------------------------------------------

#include <array>
#include <cstdint>
#include <utility>

#include "so6/SO6.hpp"
#include "so6/T_Operator.hpp"
#include "so6/TT_Alphabet.hpp"
#include "ds/LinearTransform.hpp"

namespace tt {

// ---- HOT: value-only single-T dispatch (15 entries, indexed by T index 0..14) ----
using TValueFn = SO6& (*)(SO6&);

namespace detail {
template <std::size_t... Is>
constexpr std::array<TValueFn, 15> make_value_table(std::index_sequence<Is...>) {
    return { &::apply_T_values_only<kPairOf[Is][0], kPairOf[Is][1]>... };
}
} // namespace detail

inline const std::array<TValueFn, 15> kTValueTable =
    detail::make_value_table(std::make_index_sequence<15>{});

// Apply the t-th individual T to S as a pure value transform (no metadata, no canon).
inline __attribute__((always_inline)) SO6& apply_T_values_only_rt(uint8_t t, SO6& S) {
    return kTValueTable[t](S);
}

} // namespace tt

// ---- Compile-time TT operator (fused one-shot; cold path) ----
template <int A1, int A2, int B1, int B2>
struct TT_Operator {
    static constexpr uint8_t first_index  = T_Operator<A1, A2>::index;   // 0..14
    static constexpr uint8_t second_index = T_Operator<B1, B2>::index;   // 0..14
    static constexpr bool    disjoint     = TTKernelCT<A1, A2, B1, B2>::disjoint;
    static constexpr uint8_t move_index   = tt::kMoveId[first_index][second_index];
    static_assert(move_index != 0xFF,
                  "TT_Operator instantiated with a non-canonical/invalid (first,second) pair");

    // Full public op: both butterflies in one pass + single canonical reset, then set
    // the only TT history field. last_T is NOT maintained for TT states (derive the
    // boundary T from last_TT via the alphabet when pruning).
    static inline __attribute__((always_inline)) SO6& apply_inplace(SO6& S) {
        apply_inplace_TT<A1, A2, B1, B2, /*Canonicalize=*/true>(S);
        S.last_TT = move_index;
        return S;
    }
    static inline __attribute__((always_inline)) SO6 apply(const SO6& in) {
        SO6 c = in;
        apply_inplace(c);
        return c;
    }
    // Value-only one-shot (no canon, no history) -- used to verify parents in path_to
    // and by the brute-force extension where canonicalization is deferred.
    static inline __attribute__((always_inline)) SO6 multiply_no_metadata(const SO6& in) {
        SO6 c = in;
        apply_inplace_TT<A1, A2, B1, B2, /*Canonicalize=*/false>(c);
        return c;
    }
};

// ---- Runtime TT operator selected by move id (cold path) ----
class TT_OperatorRuntime {
public:
    explicit TT_OperatorRuntime(uint8_t move, bool canonicalize = true)
        : move_(move), canonicalize_(canonicalize) {}

    uint8_t move() const { return move_; }

    // Defined in src/TT_Operator.cpp via a 165-entry dispatch table.
    SO6 apply(const SO6& S) const;

private:
    uint8_t move_;
    bool canonicalize_;
};

inline SO6 operator*(const TT_OperatorRuntime& op, const SO6& rhs) {
    return op.apply(rhs);
}
