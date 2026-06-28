// Cold-path TT runtime dispatch. The 165 TT_Operator<...> specializations are
// force-instantiated here (via std::make_index_sequence over the constexpr
// alphabet) so the rest of the program need not see the fused kernels. This keeps
// the per-move kernels in a single translation unit and avoids a hand-written
// multi-thousand-line dispatch table.

#include <array>
#include <cstddef>
#include <utility>

#include "so6/TT_Operator.hpp"
#include "so6/TT_Alphabet.hpp"

namespace {

using ApplyFn = SO6 (*)(const SO6&);

// move id I -> apply the corresponding TT_Operator. The (A1,A2,B1,B2) row indices
// come from the constexpr alphabet, so each table slot binds a distinct compile-time
// specialization with no runtime row branching.
template <std::size_t I>
SO6 apply_can_i(const SO6& S) {
    constexpr tt::TTMove m = tt::kAlphabet[I];
    return TT_Operator<tt::kPairOf[m.first][0], tt::kPairOf[m.first][1],
                       tt::kPairOf[m.second][0], tt::kPairOf[m.second][1]>::apply(S);
}
template <std::size_t I>
SO6 apply_nc_i(const SO6& S) {
    constexpr tt::TTMove m = tt::kAlphabet[I];
    return TT_Operator<tt::kPairOf[m.first][0], tt::kPairOf[m.first][1],
                       tt::kPairOf[m.second][0], tt::kPairOf[m.second][1]>::multiply_no_metadata(S);
}

template <std::size_t... Is>
constexpr std::array<ApplyFn, tt::kNumMoves> make_can(std::index_sequence<Is...>) {
    return { &apply_can_i<Is>... };
}
template <std::size_t... Is>
constexpr std::array<ApplyFn, tt::kNumMoves> make_nc(std::index_sequence<Is...>) {
    return { &apply_nc_i<Is>... };
}

const std::array<ApplyFn, tt::kNumMoves> TABLE_CAN =
    make_can(std::make_index_sequence<tt::kNumMoves>{});
const std::array<ApplyFn, tt::kNumMoves> TABLE_NC =
    make_nc(std::make_index_sequence<tt::kNumMoves>{});

} // namespace

SO6 TT_OperatorRuntime::apply(const SO6& S) const {
    return (canonicalize_ ? TABLE_CAN[move_] : TABLE_NC[move_])(S);
}
