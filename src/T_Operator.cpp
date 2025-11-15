#include "so6/T_Operator.hpp"

// Explicit template instantiations for all 15 T operators.
// Having them in a single TU improves build times and allows per-file flags.
template struct T_Operator<0, 1>;
template struct T_Operator<0, 2>;
template struct T_Operator<0, 3>;
template struct T_Operator<0, 4>;
template struct T_Operator<0, 5>;
template struct T_Operator<1, 2>;
template struct T_Operator<1, 3>;
template struct T_Operator<1, 4>;
template struct T_Operator<1, 5>;
template struct T_Operator<2, 3>;
template struct T_Operator<2, 4>;
template struct T_Operator<2, 5>;
template struct T_Operator<3, 4>;
template struct T_Operator<3, 5>;
template struct T_Operator<4, 5>;
