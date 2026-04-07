#pragma once

#include <array>
#include <cstdint>
#include "so6/SO6.hpp"
#include "DyadicSqrt2.hpp"

/**
 * @file TT_Operator.hpp
 * @brief Fully expanded paired T-gate (TT) operator for SO6 BFS.
 *
 * 165 compound alphabets = 45 disjoint + 120 overlapping.
 *
 * Pruning rules (14 skipped per last_T, 151 remain):
 *   1. t_first == last_T  =>  T_{last_T}^2 = I, collapses to zero depth
 *   2. Disjoint pair with t_second == last_T  =>  T_{t2}*T_{t1}*T_{t2}
 *      = T_{t1}*T_{t2}^2 = T_{t1}, collapses to depth 1
 */

struct TT_Entry {
    uint8_t t_first;
    uint8_t t_second;
};

inline constexpr std::array<TT_Entry, 165> TT_ALPHABET = {{
    {0, 1}, // [0] T(0,1) then T(0,2)
    {0, 2}, // [1] T(0,1) then T(0,3)
    {0, 3}, // [2] T(0,1) then T(0,4)
    {0, 4}, // [3] T(0,1) then T(0,5)
    {0, 5}, // [4] T(0,1) then T(1,2)
    {0, 6}, // [5] T(0,1) then T(1,3)
    {0, 7}, // [6] T(0,1) then T(1,4)
    {0, 8}, // [7] T(0,1) then T(1,5)
    {0, 9}, // [8] T(0,1) then T(2,3)
    {0, 10}, // [9] T(0,1) then T(2,4)
    {0, 11}, // [10] T(0,1) then T(2,5)
    {0, 12}, // [11] T(0,1) then T(3,4)
    {0, 13}, // [12] T(0,1) then T(3,5)
    {0, 14}, // [13] T(0,1) then T(4,5)
    {1, 0}, // [14] T(0,2) then T(0,1)
    {1, 2}, // [15] T(0,2) then T(0,3)
    {1, 3}, // [16] T(0,2) then T(0,4)
    {1, 4}, // [17] T(0,2) then T(0,5)
    {1, 5}, // [18] T(0,2) then T(1,2)
    {1, 6}, // [19] T(0,2) then T(1,3)
    {1, 7}, // [20] T(0,2) then T(1,4)
    {1, 8}, // [21] T(0,2) then T(1,5)
    {1, 9}, // [22] T(0,2) then T(2,3)
    {1, 10}, // [23] T(0,2) then T(2,4)
    {1, 11}, // [24] T(0,2) then T(2,5)
    {1, 12}, // [25] T(0,2) then T(3,4)
    {1, 13}, // [26] T(0,2) then T(3,5)
    {1, 14}, // [27] T(0,2) then T(4,5)
    {2, 0}, // [28] T(0,3) then T(0,1)
    {2, 1}, // [29] T(0,3) then T(0,2)
    {2, 3}, // [30] T(0,3) then T(0,4)
    {2, 4}, // [31] T(0,3) then T(0,5)
    {2, 5}, // [32] T(0,3) then T(1,2)
    {2, 6}, // [33] T(0,3) then T(1,3)
    {2, 7}, // [34] T(0,3) then T(1,4)
    {2, 8}, // [35] T(0,3) then T(1,5)
    {2, 9}, // [36] T(0,3) then T(2,3)
    {2, 10}, // [37] T(0,3) then T(2,4)
    {2, 11}, // [38] T(0,3) then T(2,5)
    {2, 12}, // [39] T(0,3) then T(3,4)
    {2, 13}, // [40] T(0,3) then T(3,5)
    {2, 14}, // [41] T(0,3) then T(4,5)
    {3, 0}, // [42] T(0,4) then T(0,1)
    {3, 1}, // [43] T(0,4) then T(0,2)
    {3, 2}, // [44] T(0,4) then T(0,3)
    {3, 4}, // [45] T(0,4) then T(0,5)
    {3, 5}, // [46] T(0,4) then T(1,2)
    {3, 6}, // [47] T(0,4) then T(1,3)
    {3, 7}, // [48] T(0,4) then T(1,4)
    {3, 8}, // [49] T(0,4) then T(1,5)
    {3, 9}, // [50] T(0,4) then T(2,3)
    {3, 10}, // [51] T(0,4) then T(2,4)
    {3, 11}, // [52] T(0,4) then T(2,5)
    {3, 12}, // [53] T(0,4) then T(3,4)
    {3, 13}, // [54] T(0,4) then T(3,5)
    {3, 14}, // [55] T(0,4) then T(4,5)
    {4, 0}, // [56] T(0,5) then T(0,1)
    {4, 1}, // [57] T(0,5) then T(0,2)
    {4, 2}, // [58] T(0,5) then T(0,3)
    {4, 3}, // [59] T(0,5) then T(0,4)
    {4, 5}, // [60] T(0,5) then T(1,2)
    {4, 6}, // [61] T(0,5) then T(1,3)
    {4, 7}, // [62] T(0,5) then T(1,4)
    {4, 8}, // [63] T(0,5) then T(1,5)
    {4, 9}, // [64] T(0,5) then T(2,3)
    {4, 10}, // [65] T(0,5) then T(2,4)
    {4, 11}, // [66] T(0,5) then T(2,5)
    {4, 12}, // [67] T(0,5) then T(3,4)
    {4, 13}, // [68] T(0,5) then T(3,5)
    {4, 14}, // [69] T(0,5) then T(4,5)
    {5, 0}, // [70] T(1,2) then T(0,1)
    {5, 1}, // [71] T(1,2) then T(0,2)
    {5, 6}, // [72] T(1,2) then T(1,3)
    {5, 7}, // [73] T(1,2) then T(1,4)
    {5, 8}, // [74] T(1,2) then T(1,5)
    {5, 9}, // [75] T(1,2) then T(2,3)
    {5, 10}, // [76] T(1,2) then T(2,4)
    {5, 11}, // [77] T(1,2) then T(2,5)
    {5, 12}, // [78] T(1,2) then T(3,4)
    {5, 13}, // [79] T(1,2) then T(3,5)
    {5, 14}, // [80] T(1,2) then T(4,5)
    {6, 0}, // [81] T(1,3) then T(0,1)
    {6, 2}, // [82] T(1,3) then T(0,3)
    {6, 5}, // [83] T(1,3) then T(1,2)
    {6, 7}, // [84] T(1,3) then T(1,4)
    {6, 8}, // [85] T(1,3) then T(1,5)
    {6, 9}, // [86] T(1,3) then T(2,3)
    {6, 10}, // [87] T(1,3) then T(2,4)
    {6, 11}, // [88] T(1,3) then T(2,5)
    {6, 12}, // [89] T(1,3) then T(3,4)
    {6, 13}, // [90] T(1,3) then T(3,5)
    {6, 14}, // [91] T(1,3) then T(4,5)
    {7, 0}, // [92] T(1,4) then T(0,1)
    {7, 3}, // [93] T(1,4) then T(0,4)
    {7, 5}, // [94] T(1,4) then T(1,2)
    {7, 6}, // [95] T(1,4) then T(1,3)
    {7, 8}, // [96] T(1,4) then T(1,5)
    {7, 9}, // [97] T(1,4) then T(2,3)
    {7, 10}, // [98] T(1,4) then T(2,4)
    {7, 11}, // [99] T(1,4) then T(2,5)
    {7, 12}, // [100] T(1,4) then T(3,4)
    {7, 13}, // [101] T(1,4) then T(3,5)
    {7, 14}, // [102] T(1,4) then T(4,5)
    {8, 0}, // [103] T(1,5) then T(0,1)
    {8, 4}, // [104] T(1,5) then T(0,5)
    {8, 5}, // [105] T(1,5) then T(1,2)
    {8, 6}, // [106] T(1,5) then T(1,3)
    {8, 7}, // [107] T(1,5) then T(1,4)
    {8, 9}, // [108] T(1,5) then T(2,3)
    {8, 10}, // [109] T(1,5) then T(2,4)
    {8, 11}, // [110] T(1,5) then T(2,5)
    {8, 12}, // [111] T(1,5) then T(3,4)
    {8, 13}, // [112] T(1,5) then T(3,5)
    {8, 14}, // [113] T(1,5) then T(4,5)
    {9, 1}, // [114] T(2,3) then T(0,2)
    {9, 2}, // [115] T(2,3) then T(0,3)
    {9, 5}, // [116] T(2,3) then T(1,2)
    {9, 6}, // [117] T(2,3) then T(1,3)
    {9, 10}, // [118] T(2,3) then T(2,4)
    {9, 11}, // [119] T(2,3) then T(2,5)
    {9, 12}, // [120] T(2,3) then T(3,4)
    {9, 13}, // [121] T(2,3) then T(3,5)
    {9, 14}, // [122] T(2,3) then T(4,5)
    {10, 1}, // [123] T(2,4) then T(0,2)
    {10, 3}, // [124] T(2,4) then T(0,4)
    {10, 5}, // [125] T(2,4) then T(1,2)
    {10, 7}, // [126] T(2,4) then T(1,4)
    {10, 9}, // [127] T(2,4) then T(2,3)
    {10, 11}, // [128] T(2,4) then T(2,5)
    {10, 12}, // [129] T(2,4) then T(3,4)
    {10, 13}, // [130] T(2,4) then T(3,5)
    {10, 14}, // [131] T(2,4) then T(4,5)
    {11, 1}, // [132] T(2,5) then T(0,2)
    {11, 4}, // [133] T(2,5) then T(0,5)
    {11, 5}, // [134] T(2,5) then T(1,2)
    {11, 8}, // [135] T(2,5) then T(1,5)
    {11, 9}, // [136] T(2,5) then T(2,3)
    {11, 10}, // [137] T(2,5) then T(2,4)
    {11, 12}, // [138] T(2,5) then T(3,4)
    {11, 13}, // [139] T(2,5) then T(3,5)
    {11, 14}, // [140] T(2,5) then T(4,5)
    {12, 2}, // [141] T(3,4) then T(0,3)
    {12, 3}, // [142] T(3,4) then T(0,4)
    {12, 6}, // [143] T(3,4) then T(1,3)
    {12, 7}, // [144] T(3,4) then T(1,4)
    {12, 9}, // [145] T(3,4) then T(2,3)
    {12, 10}, // [146] T(3,4) then T(2,4)
    {12, 13}, // [147] T(3,4) then T(3,5)
    {12, 14}, // [148] T(3,4) then T(4,5)
    {13, 2}, // [149] T(3,5) then T(0,3)
    {13, 4}, // [150] T(3,5) then T(0,5)
    {13, 6}, // [151] T(3,5) then T(1,3)
    {13, 8}, // [152] T(3,5) then T(1,5)
    {13, 9}, // [153] T(3,5) then T(2,3)
    {13, 11}, // [154] T(3,5) then T(2,5)
    {13, 12}, // [155] T(3,5) then T(3,4)
    {13, 14}, // [156] T(3,5) then T(4,5)
    {14, 3}, // [157] T(4,5) then T(0,4)
    {14, 4}, // [158] T(4,5) then T(0,5)
    {14, 7}, // [159] T(4,5) then T(1,4)
    {14, 8}, // [160] T(4,5) then T(1,5)
    {14, 10}, // [161] T(4,5) then T(2,4)
    {14, 11}, // [162] T(4,5) then T(2,5)
    {14, 12}, // [163] T(4,5) then T(3,4)
    {14, 13} // [164] T(4,5) then T(3,5)
}};

static_assert(TT_ALPHABET.size() == 165);

struct TT_RowInfo {
    uint8_t r1a, r2a;
    uint8_t r1b, r2b;
    bool disjoint;
};

inline constexpr std::array<std::pair<uint8_t,uint8_t>, 15> TT_ROW_PAIRS = {{
    {0,1},
    {0,2},
    {0,3},
    {0,4},
    {0,5},
    {1,2},
    {1,3},
    {1,4},
    {1,5},
    {2,3},
    {2,4},
    {2,5},
    {3,4},
    {3,5},
    {4,5}
}};

inline constexpr auto TT_ROW_INFO = []() {
    std::array<TT_RowInfo, 165> result{};
    for (int a = 0; a < 165; ++a) {
        auto [r1a, r2a] = TT_ROW_PAIRS[TT_ALPHABET[a].t_first];
        auto [r1b, r2b] = TT_ROW_PAIRS[TT_ALPHABET[a].t_second];
        result[a] = {r1a, r2a, r1b, r2b,
                     (r1a != r1b && r1a != r2b && r2a != r1b && r2a != r2b)};
    }
    return result;
}();

inline __attribute__((always_inline))
void apply_T_kernel_raw(SO6& S, uint8_t r1, uint8_t r2, uint8_t col) {
    const uint8_t idx1 = col * 6 + r1;
    const uint8_t idx2 = col * 6 + r2;
    DyadicSqrt2 a = S.arr_[idx1];
    DyadicSqrt2 b = S.arr_[idx2];
    const DyadicSqrt2 a_old = a;
    a += b;
    b -= a_old;
    b = -b;
    a.denom_exp += (a.int_c != 0);
    b.denom_exp += (b.int_c != 0);
    S.arr_[idx1] = a;
    S.arr_[idx2] = b;
}

/// Fused disjoint: two T gates on 4 independent rows, single column pass.
template<uint8_t R1A, uint8_t R2A, uint8_t R1B, uint8_t R2B>
inline __attribute__((always_inline))
void apply_TT_disjoint(SO6& S) {
    for (uint8_t col = 0; col < 6; ++col) {
        const uint8_t base = col * 6;
        DyadicSqrt2 a = S.arr_[base + R1A];
        DyadicSqrt2 b = S.arr_[base + R2A];
        const DyadicSqrt2 a0 = a;
        a += b; b -= a0; b = -b;
        a.denom_exp += (a.int_c != 0);
        b.denom_exp += (b.int_c != 0);
        S.arr_[base + R1A] = a;
        S.arr_[base + R2A] = b;
        DyadicSqrt2 c = S.arr_[base + R1B];
        DyadicSqrt2 d = S.arr_[base + R2B];
        const DyadicSqrt2 c0 = c;
        c += d; d -= c0; d = -d;
        c.denom_exp += (c.int_c != 0);
        d.denom_exp += (d.int_c != 0);
        S.arr_[base + R1B] = c;
        S.arr_[base + R2B] = d;
    }
}

/// Fused overlapping: two T gates sharing one row. T_first then T_second.
template<uint8_t R1A, uint8_t R2A, uint8_t R1B, uint8_t R2B>
inline __attribute__((always_inline))
void apply_TT_overlap(SO6& S) {
    for (uint8_t col = 0; col < 6; ++col) {
        apply_T_kernel_raw(S, R1A, R2A, col);
        apply_T_kernel_raw(S, R1B, R2B, col);
    }
}

struct TT_Dispatch {
    uint8_t alpha_idx;
    uint8_t t_second;
};

inline constexpr std::array<uint8_t, 16> TT_CANDIDATE_COUNTS = {{
    151, // last_T=0
    151, // last_T=1
    151, // last_T=2
    151, // last_T=3
    151, // last_T=4
    151, // last_T=5
    151, // last_T=6
    151, // last_T=7
    151, // last_T=8
    151, // last_T=9
    151, // last_T=10
    151, // last_T=11
    151, // last_T=12
    151, // last_T=13
    151, // last_T=14
    165 // last_T=15 (no previous)
}};

/// Precomputed dispatch table: TT_CANDIDATES[last_T] lists valid candidates.
/// Entries where t_first==last_T or (disjoint && t_second==last_T) are excluded.
inline constexpr auto TT_CANDIDATES = []() {
    std::array<std::array<TT_Dispatch, 165>, 16> result{};
    // last_T=0: 151 candidates
    result[0][0] = {14, 0};
    result[0][1] = {15, 2};
    result[0][2] = {16, 3};
    result[0][3] = {17, 4};
    result[0][4] = {18, 5};
    result[0][5] = {19, 6};
    result[0][6] = {20, 7};
    result[0][7] = {21, 8};
    result[0][8] = {22, 9};
    result[0][9] = {23, 10};
    result[0][10] = {24, 11};
    result[0][11] = {25, 12};
    result[0][12] = {26, 13};
    result[0][13] = {27, 14};
    result[0][14] = {28, 0};
    result[0][15] = {29, 1};
    result[0][16] = {30, 3};
    result[0][17] = {31, 4};
    result[0][18] = {32, 5};
    result[0][19] = {33, 6};
    result[0][20] = {34, 7};
    result[0][21] = {35, 8};
    result[0][22] = {36, 9};
    result[0][23] = {37, 10};
    result[0][24] = {38, 11};
    result[0][25] = {39, 12};
    result[0][26] = {40, 13};
    result[0][27] = {41, 14};
    result[0][28] = {42, 0};
    result[0][29] = {43, 1};
    result[0][30] = {44, 2};
    result[0][31] = {45, 4};
    result[0][32] = {46, 5};
    result[0][33] = {47, 6};
    result[0][34] = {48, 7};
    result[0][35] = {49, 8};
    result[0][36] = {50, 9};
    result[0][37] = {51, 10};
    result[0][38] = {52, 11};
    result[0][39] = {53, 12};
    result[0][40] = {54, 13};
    result[0][41] = {55, 14};
    result[0][42] = {56, 0};
    result[0][43] = {57, 1};
    result[0][44] = {58, 2};
    result[0][45] = {59, 3};
    result[0][46] = {60, 5};
    result[0][47] = {61, 6};
    result[0][48] = {62, 7};
    result[0][49] = {63, 8};
    result[0][50] = {64, 9};
    result[0][51] = {65, 10};
    result[0][52] = {66, 11};
    result[0][53] = {67, 12};
    result[0][54] = {68, 13};
    result[0][55] = {69, 14};
    result[0][56] = {70, 0};
    result[0][57] = {71, 1};
    result[0][58] = {72, 6};
    result[0][59] = {73, 7};
    result[0][60] = {74, 8};
    result[0][61] = {75, 9};
    result[0][62] = {76, 10};
    result[0][63] = {77, 11};
    result[0][64] = {78, 12};
    result[0][65] = {79, 13};
    result[0][66] = {80, 14};
    result[0][67] = {81, 0};
    result[0][68] = {82, 2};
    result[0][69] = {83, 5};
    result[0][70] = {84, 7};
    result[0][71] = {85, 8};
    result[0][72] = {86, 9};
    result[0][73] = {87, 10};
    result[0][74] = {88, 11};
    result[0][75] = {89, 12};
    result[0][76] = {90, 13};
    result[0][77] = {91, 14};
    result[0][78] = {92, 0};
    result[0][79] = {93, 3};
    result[0][80] = {94, 5};
    result[0][81] = {95, 6};
    result[0][82] = {96, 8};
    result[0][83] = {97, 9};
    result[0][84] = {98, 10};
    result[0][85] = {99, 11};
    result[0][86] = {100, 12};
    result[0][87] = {101, 13};
    result[0][88] = {102, 14};
    result[0][89] = {103, 0};
    result[0][90] = {104, 4};
    result[0][91] = {105, 5};
    result[0][92] = {106, 6};
    result[0][93] = {107, 7};
    result[0][94] = {108, 9};
    result[0][95] = {109, 10};
    result[0][96] = {110, 11};
    result[0][97] = {111, 12};
    result[0][98] = {112, 13};
    result[0][99] = {113, 14};
    result[0][100] = {114, 1};
    result[0][101] = {115, 2};
    result[0][102] = {116, 5};
    result[0][103] = {117, 6};
    result[0][104] = {118, 10};
    result[0][105] = {119, 11};
    result[0][106] = {120, 12};
    result[0][107] = {121, 13};
    result[0][108] = {122, 14};
    result[0][109] = {123, 1};
    result[0][110] = {124, 3};
    result[0][111] = {125, 5};
    result[0][112] = {126, 7};
    result[0][113] = {127, 9};
    result[0][114] = {128, 11};
    result[0][115] = {129, 12};
    result[0][116] = {130, 13};
    result[0][117] = {131, 14};
    result[0][118] = {132, 1};
    result[0][119] = {133, 4};
    result[0][120] = {134, 5};
    result[0][121] = {135, 8};
    result[0][122] = {136, 9};
    result[0][123] = {137, 10};
    result[0][124] = {138, 12};
    result[0][125] = {139, 13};
    result[0][126] = {140, 14};
    result[0][127] = {141, 2};
    result[0][128] = {142, 3};
    result[0][129] = {143, 6};
    result[0][130] = {144, 7};
    result[0][131] = {145, 9};
    result[0][132] = {146, 10};
    result[0][133] = {147, 13};
    result[0][134] = {148, 14};
    result[0][135] = {149, 2};
    result[0][136] = {150, 4};
    result[0][137] = {151, 6};
    result[0][138] = {152, 8};
    result[0][139] = {153, 9};
    result[0][140] = {154, 11};
    result[0][141] = {155, 12};
    result[0][142] = {156, 14};
    result[0][143] = {157, 3};
    result[0][144] = {158, 4};
    result[0][145] = {159, 7};
    result[0][146] = {160, 8};
    result[0][147] = {161, 10};
    result[0][148] = {162, 11};
    result[0][149] = {163, 12};
    result[0][150] = {164, 13};
    // last_T=1: 151 candidates
    result[1][0] = {0, 1};
    result[1][1] = {1, 2};
    result[1][2] = {2, 3};
    result[1][3] = {3, 4};
    result[1][4] = {4, 5};
    result[1][5] = {5, 6};
    result[1][6] = {6, 7};
    result[1][7] = {7, 8};
    result[1][8] = {8, 9};
    result[1][9] = {9, 10};
    result[1][10] = {10, 11};
    result[1][11] = {11, 12};
    result[1][12] = {12, 13};
    result[1][13] = {13, 14};
    result[1][14] = {28, 0};
    result[1][15] = {29, 1};
    result[1][16] = {30, 3};
    result[1][17] = {31, 4};
    result[1][18] = {32, 5};
    result[1][19] = {33, 6};
    result[1][20] = {34, 7};
    result[1][21] = {35, 8};
    result[1][22] = {36, 9};
    result[1][23] = {37, 10};
    result[1][24] = {38, 11};
    result[1][25] = {39, 12};
    result[1][26] = {40, 13};
    result[1][27] = {41, 14};
    result[1][28] = {42, 0};
    result[1][29] = {43, 1};
    result[1][30] = {44, 2};
    result[1][31] = {45, 4};
    result[1][32] = {46, 5};
    result[1][33] = {47, 6};
    result[1][34] = {48, 7};
    result[1][35] = {49, 8};
    result[1][36] = {50, 9};
    result[1][37] = {51, 10};
    result[1][38] = {52, 11};
    result[1][39] = {53, 12};
    result[1][40] = {54, 13};
    result[1][41] = {55, 14};
    result[1][42] = {56, 0};
    result[1][43] = {57, 1};
    result[1][44] = {58, 2};
    result[1][45] = {59, 3};
    result[1][46] = {60, 5};
    result[1][47] = {61, 6};
    result[1][48] = {62, 7};
    result[1][49] = {63, 8};
    result[1][50] = {64, 9};
    result[1][51] = {65, 10};
    result[1][52] = {66, 11};
    result[1][53] = {67, 12};
    result[1][54] = {68, 13};
    result[1][55] = {69, 14};
    result[1][56] = {70, 0};
    result[1][57] = {71, 1};
    result[1][58] = {72, 6};
    result[1][59] = {73, 7};
    result[1][60] = {74, 8};
    result[1][61] = {75, 9};
    result[1][62] = {76, 10};
    result[1][63] = {77, 11};
    result[1][64] = {78, 12};
    result[1][65] = {79, 13};
    result[1][66] = {80, 14};
    result[1][67] = {81, 0};
    result[1][68] = {82, 2};
    result[1][69] = {83, 5};
    result[1][70] = {84, 7};
    result[1][71] = {85, 8};
    result[1][72] = {86, 9};
    result[1][73] = {87, 10};
    result[1][74] = {88, 11};
    result[1][75] = {89, 12};
    result[1][76] = {90, 13};
    result[1][77] = {91, 14};
    result[1][78] = {92, 0};
    result[1][79] = {93, 3};
    result[1][80] = {94, 5};
    result[1][81] = {95, 6};
    result[1][82] = {96, 8};
    result[1][83] = {97, 9};
    result[1][84] = {98, 10};
    result[1][85] = {99, 11};
    result[1][86] = {100, 12};
    result[1][87] = {101, 13};
    result[1][88] = {102, 14};
    result[1][89] = {103, 0};
    result[1][90] = {104, 4};
    result[1][91] = {105, 5};
    result[1][92] = {106, 6};
    result[1][93] = {107, 7};
    result[1][94] = {108, 9};
    result[1][95] = {109, 10};
    result[1][96] = {110, 11};
    result[1][97] = {111, 12};
    result[1][98] = {112, 13};
    result[1][99] = {113, 14};
    result[1][100] = {114, 1};
    result[1][101] = {115, 2};
    result[1][102] = {116, 5};
    result[1][103] = {117, 6};
    result[1][104] = {118, 10};
    result[1][105] = {119, 11};
    result[1][106] = {120, 12};
    result[1][107] = {121, 13};
    result[1][108] = {122, 14};
    result[1][109] = {123, 1};
    result[1][110] = {124, 3};
    result[1][111] = {125, 5};
    result[1][112] = {126, 7};
    result[1][113] = {127, 9};
    result[1][114] = {128, 11};
    result[1][115] = {129, 12};
    result[1][116] = {130, 13};
    result[1][117] = {131, 14};
    result[1][118] = {132, 1};
    result[1][119] = {133, 4};
    result[1][120] = {134, 5};
    result[1][121] = {135, 8};
    result[1][122] = {136, 9};
    result[1][123] = {137, 10};
    result[1][124] = {138, 12};
    result[1][125] = {139, 13};
    result[1][126] = {140, 14};
    result[1][127] = {141, 2};
    result[1][128] = {142, 3};
    result[1][129] = {143, 6};
    result[1][130] = {144, 7};
    result[1][131] = {145, 9};
    result[1][132] = {146, 10};
    result[1][133] = {147, 13};
    result[1][134] = {148, 14};
    result[1][135] = {149, 2};
    result[1][136] = {150, 4};
    result[1][137] = {151, 6};
    result[1][138] = {152, 8};
    result[1][139] = {153, 9};
    result[1][140] = {154, 11};
    result[1][141] = {155, 12};
    result[1][142] = {156, 14};
    result[1][143] = {157, 3};
    result[1][144] = {158, 4};
    result[1][145] = {159, 7};
    result[1][146] = {160, 8};
    result[1][147] = {161, 10};
    result[1][148] = {162, 11};
    result[1][149] = {163, 12};
    result[1][150] = {164, 13};
    // last_T=2: 151 candidates
    result[2][0] = {0, 1};
    result[2][1] = {1, 2};
    result[2][2] = {2, 3};
    result[2][3] = {3, 4};
    result[2][4] = {4, 5};
    result[2][5] = {5, 6};
    result[2][6] = {6, 7};
    result[2][7] = {7, 8};
    result[2][8] = {8, 9};
    result[2][9] = {9, 10};
    result[2][10] = {10, 11};
    result[2][11] = {11, 12};
    result[2][12] = {12, 13};
    result[2][13] = {13, 14};
    result[2][14] = {14, 0};
    result[2][15] = {15, 2};
    result[2][16] = {16, 3};
    result[2][17] = {17, 4};
    result[2][18] = {18, 5};
    result[2][19] = {19, 6};
    result[2][20] = {20, 7};
    result[2][21] = {21, 8};
    result[2][22] = {22, 9};
    result[2][23] = {23, 10};
    result[2][24] = {24, 11};
    result[2][25] = {25, 12};
    result[2][26] = {26, 13};
    result[2][27] = {27, 14};
    result[2][28] = {42, 0};
    result[2][29] = {43, 1};
    result[2][30] = {44, 2};
    result[2][31] = {45, 4};
    result[2][32] = {46, 5};
    result[2][33] = {47, 6};
    result[2][34] = {48, 7};
    result[2][35] = {49, 8};
    result[2][36] = {50, 9};
    result[2][37] = {51, 10};
    result[2][38] = {52, 11};
    result[2][39] = {53, 12};
    result[2][40] = {54, 13};
    result[2][41] = {55, 14};
    result[2][42] = {56, 0};
    result[2][43] = {57, 1};
    result[2][44] = {58, 2};
    result[2][45] = {59, 3};
    result[2][46] = {60, 5};
    result[2][47] = {61, 6};
    result[2][48] = {62, 7};
    result[2][49] = {63, 8};
    result[2][50] = {64, 9};
    result[2][51] = {65, 10};
    result[2][52] = {66, 11};
    result[2][53] = {67, 12};
    result[2][54] = {68, 13};
    result[2][55] = {69, 14};
    result[2][56] = {70, 0};
    result[2][57] = {71, 1};
    result[2][58] = {72, 6};
    result[2][59] = {73, 7};
    result[2][60] = {74, 8};
    result[2][61] = {75, 9};
    result[2][62] = {76, 10};
    result[2][63] = {77, 11};
    result[2][64] = {78, 12};
    result[2][65] = {79, 13};
    result[2][66] = {80, 14};
    result[2][67] = {81, 0};
    result[2][68] = {82, 2};
    result[2][69] = {83, 5};
    result[2][70] = {84, 7};
    result[2][71] = {85, 8};
    result[2][72] = {86, 9};
    result[2][73] = {87, 10};
    result[2][74] = {88, 11};
    result[2][75] = {89, 12};
    result[2][76] = {90, 13};
    result[2][77] = {91, 14};
    result[2][78] = {92, 0};
    result[2][79] = {93, 3};
    result[2][80] = {94, 5};
    result[2][81] = {95, 6};
    result[2][82] = {96, 8};
    result[2][83] = {97, 9};
    result[2][84] = {98, 10};
    result[2][85] = {99, 11};
    result[2][86] = {100, 12};
    result[2][87] = {101, 13};
    result[2][88] = {102, 14};
    result[2][89] = {103, 0};
    result[2][90] = {104, 4};
    result[2][91] = {105, 5};
    result[2][92] = {106, 6};
    result[2][93] = {107, 7};
    result[2][94] = {108, 9};
    result[2][95] = {109, 10};
    result[2][96] = {110, 11};
    result[2][97] = {111, 12};
    result[2][98] = {112, 13};
    result[2][99] = {113, 14};
    result[2][100] = {114, 1};
    result[2][101] = {115, 2};
    result[2][102] = {116, 5};
    result[2][103] = {117, 6};
    result[2][104] = {118, 10};
    result[2][105] = {119, 11};
    result[2][106] = {120, 12};
    result[2][107] = {121, 13};
    result[2][108] = {122, 14};
    result[2][109] = {123, 1};
    result[2][110] = {124, 3};
    result[2][111] = {125, 5};
    result[2][112] = {126, 7};
    result[2][113] = {127, 9};
    result[2][114] = {128, 11};
    result[2][115] = {129, 12};
    result[2][116] = {130, 13};
    result[2][117] = {131, 14};
    result[2][118] = {132, 1};
    result[2][119] = {133, 4};
    result[2][120] = {134, 5};
    result[2][121] = {135, 8};
    result[2][122] = {136, 9};
    result[2][123] = {137, 10};
    result[2][124] = {138, 12};
    result[2][125] = {139, 13};
    result[2][126] = {140, 14};
    result[2][127] = {141, 2};
    result[2][128] = {142, 3};
    result[2][129] = {143, 6};
    result[2][130] = {144, 7};
    result[2][131] = {145, 9};
    result[2][132] = {146, 10};
    result[2][133] = {147, 13};
    result[2][134] = {148, 14};
    result[2][135] = {149, 2};
    result[2][136] = {150, 4};
    result[2][137] = {151, 6};
    result[2][138] = {152, 8};
    result[2][139] = {153, 9};
    result[2][140] = {154, 11};
    result[2][141] = {155, 12};
    result[2][142] = {156, 14};
    result[2][143] = {157, 3};
    result[2][144] = {158, 4};
    result[2][145] = {159, 7};
    result[2][146] = {160, 8};
    result[2][147] = {161, 10};
    result[2][148] = {162, 11};
    result[2][149] = {163, 12};
    result[2][150] = {164, 13};
    // last_T=3: 151 candidates
    result[3][0] = {0, 1};
    result[3][1] = {1, 2};
    result[3][2] = {2, 3};
    result[3][3] = {3, 4};
    result[3][4] = {4, 5};
    result[3][5] = {5, 6};
    result[3][6] = {6, 7};
    result[3][7] = {7, 8};
    result[3][8] = {8, 9};
    result[3][9] = {9, 10};
    result[3][10] = {10, 11};
    result[3][11] = {11, 12};
    result[3][12] = {12, 13};
    result[3][13] = {13, 14};
    result[3][14] = {14, 0};
    result[3][15] = {15, 2};
    result[3][16] = {16, 3};
    result[3][17] = {17, 4};
    result[3][18] = {18, 5};
    result[3][19] = {19, 6};
    result[3][20] = {20, 7};
    result[3][21] = {21, 8};
    result[3][22] = {22, 9};
    result[3][23] = {23, 10};
    result[3][24] = {24, 11};
    result[3][25] = {25, 12};
    result[3][26] = {26, 13};
    result[3][27] = {27, 14};
    result[3][28] = {28, 0};
    result[3][29] = {29, 1};
    result[3][30] = {30, 3};
    result[3][31] = {31, 4};
    result[3][32] = {32, 5};
    result[3][33] = {33, 6};
    result[3][34] = {34, 7};
    result[3][35] = {35, 8};
    result[3][36] = {36, 9};
    result[3][37] = {37, 10};
    result[3][38] = {38, 11};
    result[3][39] = {39, 12};
    result[3][40] = {40, 13};
    result[3][41] = {41, 14};
    result[3][42] = {56, 0};
    result[3][43] = {57, 1};
    result[3][44] = {58, 2};
    result[3][45] = {59, 3};
    result[3][46] = {60, 5};
    result[3][47] = {61, 6};
    result[3][48] = {62, 7};
    result[3][49] = {63, 8};
    result[3][50] = {64, 9};
    result[3][51] = {65, 10};
    result[3][52] = {66, 11};
    result[3][53] = {67, 12};
    result[3][54] = {68, 13};
    result[3][55] = {69, 14};
    result[3][56] = {70, 0};
    result[3][57] = {71, 1};
    result[3][58] = {72, 6};
    result[3][59] = {73, 7};
    result[3][60] = {74, 8};
    result[3][61] = {75, 9};
    result[3][62] = {76, 10};
    result[3][63] = {77, 11};
    result[3][64] = {78, 12};
    result[3][65] = {79, 13};
    result[3][66] = {80, 14};
    result[3][67] = {81, 0};
    result[3][68] = {82, 2};
    result[3][69] = {83, 5};
    result[3][70] = {84, 7};
    result[3][71] = {85, 8};
    result[3][72] = {86, 9};
    result[3][73] = {87, 10};
    result[3][74] = {88, 11};
    result[3][75] = {89, 12};
    result[3][76] = {90, 13};
    result[3][77] = {91, 14};
    result[3][78] = {92, 0};
    result[3][79] = {93, 3};
    result[3][80] = {94, 5};
    result[3][81] = {95, 6};
    result[3][82] = {96, 8};
    result[3][83] = {97, 9};
    result[3][84] = {98, 10};
    result[3][85] = {99, 11};
    result[3][86] = {100, 12};
    result[3][87] = {101, 13};
    result[3][88] = {102, 14};
    result[3][89] = {103, 0};
    result[3][90] = {104, 4};
    result[3][91] = {105, 5};
    result[3][92] = {106, 6};
    result[3][93] = {107, 7};
    result[3][94] = {108, 9};
    result[3][95] = {109, 10};
    result[3][96] = {110, 11};
    result[3][97] = {111, 12};
    result[3][98] = {112, 13};
    result[3][99] = {113, 14};
    result[3][100] = {114, 1};
    result[3][101] = {115, 2};
    result[3][102] = {116, 5};
    result[3][103] = {117, 6};
    result[3][104] = {118, 10};
    result[3][105] = {119, 11};
    result[3][106] = {120, 12};
    result[3][107] = {121, 13};
    result[3][108] = {122, 14};
    result[3][109] = {123, 1};
    result[3][110] = {124, 3};
    result[3][111] = {125, 5};
    result[3][112] = {126, 7};
    result[3][113] = {127, 9};
    result[3][114] = {128, 11};
    result[3][115] = {129, 12};
    result[3][116] = {130, 13};
    result[3][117] = {131, 14};
    result[3][118] = {132, 1};
    result[3][119] = {133, 4};
    result[3][120] = {134, 5};
    result[3][121] = {135, 8};
    result[3][122] = {136, 9};
    result[3][123] = {137, 10};
    result[3][124] = {138, 12};
    result[3][125] = {139, 13};
    result[3][126] = {140, 14};
    result[3][127] = {141, 2};
    result[3][128] = {142, 3};
    result[3][129] = {143, 6};
    result[3][130] = {144, 7};
    result[3][131] = {145, 9};
    result[3][132] = {146, 10};
    result[3][133] = {147, 13};
    result[3][134] = {148, 14};
    result[3][135] = {149, 2};
    result[3][136] = {150, 4};
    result[3][137] = {151, 6};
    result[3][138] = {152, 8};
    result[3][139] = {153, 9};
    result[3][140] = {154, 11};
    result[3][141] = {155, 12};
    result[3][142] = {156, 14};
    result[3][143] = {157, 3};
    result[3][144] = {158, 4};
    result[3][145] = {159, 7};
    result[3][146] = {160, 8};
    result[3][147] = {161, 10};
    result[3][148] = {162, 11};
    result[3][149] = {163, 12};
    result[3][150] = {164, 13};
    // last_T=4: 151 candidates
    result[4][0] = {0, 1};
    result[4][1] = {1, 2};
    result[4][2] = {2, 3};
    result[4][3] = {3, 4};
    result[4][4] = {4, 5};
    result[4][5] = {5, 6};
    result[4][6] = {6, 7};
    result[4][7] = {7, 8};
    result[4][8] = {8, 9};
    result[4][9] = {9, 10};
    result[4][10] = {10, 11};
    result[4][11] = {11, 12};
    result[4][12] = {12, 13};
    result[4][13] = {13, 14};
    result[4][14] = {14, 0};
    result[4][15] = {15, 2};
    result[4][16] = {16, 3};
    result[4][17] = {17, 4};
    result[4][18] = {18, 5};
    result[4][19] = {19, 6};
    result[4][20] = {20, 7};
    result[4][21] = {21, 8};
    result[4][22] = {22, 9};
    result[4][23] = {23, 10};
    result[4][24] = {24, 11};
    result[4][25] = {25, 12};
    result[4][26] = {26, 13};
    result[4][27] = {27, 14};
    result[4][28] = {28, 0};
    result[4][29] = {29, 1};
    result[4][30] = {30, 3};
    result[4][31] = {31, 4};
    result[4][32] = {32, 5};
    result[4][33] = {33, 6};
    result[4][34] = {34, 7};
    result[4][35] = {35, 8};
    result[4][36] = {36, 9};
    result[4][37] = {37, 10};
    result[4][38] = {38, 11};
    result[4][39] = {39, 12};
    result[4][40] = {40, 13};
    result[4][41] = {41, 14};
    result[4][42] = {42, 0};
    result[4][43] = {43, 1};
    result[4][44] = {44, 2};
    result[4][45] = {45, 4};
    result[4][46] = {46, 5};
    result[4][47] = {47, 6};
    result[4][48] = {48, 7};
    result[4][49] = {49, 8};
    result[4][50] = {50, 9};
    result[4][51] = {51, 10};
    result[4][52] = {52, 11};
    result[4][53] = {53, 12};
    result[4][54] = {54, 13};
    result[4][55] = {55, 14};
    result[4][56] = {70, 0};
    result[4][57] = {71, 1};
    result[4][58] = {72, 6};
    result[4][59] = {73, 7};
    result[4][60] = {74, 8};
    result[4][61] = {75, 9};
    result[4][62] = {76, 10};
    result[4][63] = {77, 11};
    result[4][64] = {78, 12};
    result[4][65] = {79, 13};
    result[4][66] = {80, 14};
    result[4][67] = {81, 0};
    result[4][68] = {82, 2};
    result[4][69] = {83, 5};
    result[4][70] = {84, 7};
    result[4][71] = {85, 8};
    result[4][72] = {86, 9};
    result[4][73] = {87, 10};
    result[4][74] = {88, 11};
    result[4][75] = {89, 12};
    result[4][76] = {90, 13};
    result[4][77] = {91, 14};
    result[4][78] = {92, 0};
    result[4][79] = {93, 3};
    result[4][80] = {94, 5};
    result[4][81] = {95, 6};
    result[4][82] = {96, 8};
    result[4][83] = {97, 9};
    result[4][84] = {98, 10};
    result[4][85] = {99, 11};
    result[4][86] = {100, 12};
    result[4][87] = {101, 13};
    result[4][88] = {102, 14};
    result[4][89] = {103, 0};
    result[4][90] = {104, 4};
    result[4][91] = {105, 5};
    result[4][92] = {106, 6};
    result[4][93] = {107, 7};
    result[4][94] = {108, 9};
    result[4][95] = {109, 10};
    result[4][96] = {110, 11};
    result[4][97] = {111, 12};
    result[4][98] = {112, 13};
    result[4][99] = {113, 14};
    result[4][100] = {114, 1};
    result[4][101] = {115, 2};
    result[4][102] = {116, 5};
    result[4][103] = {117, 6};
    result[4][104] = {118, 10};
    result[4][105] = {119, 11};
    result[4][106] = {120, 12};
    result[4][107] = {121, 13};
    result[4][108] = {122, 14};
    result[4][109] = {123, 1};
    result[4][110] = {124, 3};
    result[4][111] = {125, 5};
    result[4][112] = {126, 7};
    result[4][113] = {127, 9};
    result[4][114] = {128, 11};
    result[4][115] = {129, 12};
    result[4][116] = {130, 13};
    result[4][117] = {131, 14};
    result[4][118] = {132, 1};
    result[4][119] = {133, 4};
    result[4][120] = {134, 5};
    result[4][121] = {135, 8};
    result[4][122] = {136, 9};
    result[4][123] = {137, 10};
    result[4][124] = {138, 12};
    result[4][125] = {139, 13};
    result[4][126] = {140, 14};
    result[4][127] = {141, 2};
    result[4][128] = {142, 3};
    result[4][129] = {143, 6};
    result[4][130] = {144, 7};
    result[4][131] = {145, 9};
    result[4][132] = {146, 10};
    result[4][133] = {147, 13};
    result[4][134] = {148, 14};
    result[4][135] = {149, 2};
    result[4][136] = {150, 4};
    result[4][137] = {151, 6};
    result[4][138] = {152, 8};
    result[4][139] = {153, 9};
    result[4][140] = {154, 11};
    result[4][141] = {155, 12};
    result[4][142] = {156, 14};
    result[4][143] = {157, 3};
    result[4][144] = {158, 4};
    result[4][145] = {159, 7};
    result[4][146] = {160, 8};
    result[4][147] = {161, 10};
    result[4][148] = {162, 11};
    result[4][149] = {163, 12};
    result[4][150] = {164, 13};
    // last_T=5: 151 candidates
    result[5][0] = {0, 1};
    result[5][1] = {1, 2};
    result[5][2] = {2, 3};
    result[5][3] = {3, 4};
    result[5][4] = {4, 5};
    result[5][5] = {5, 6};
    result[5][6] = {6, 7};
    result[5][7] = {7, 8};
    result[5][8] = {8, 9};
    result[5][9] = {9, 10};
    result[5][10] = {10, 11};
    result[5][11] = {11, 12};
    result[5][12] = {12, 13};
    result[5][13] = {13, 14};
    result[5][14] = {14, 0};
    result[5][15] = {15, 2};
    result[5][16] = {16, 3};
    result[5][17] = {17, 4};
    result[5][18] = {18, 5};
    result[5][19] = {19, 6};
    result[5][20] = {20, 7};
    result[5][21] = {21, 8};
    result[5][22] = {22, 9};
    result[5][23] = {23, 10};
    result[5][24] = {24, 11};
    result[5][25] = {25, 12};
    result[5][26] = {26, 13};
    result[5][27] = {27, 14};
    result[5][28] = {28, 0};
    result[5][29] = {29, 1};
    result[5][30] = {30, 3};
    result[5][31] = {31, 4};
    result[5][32] = {33, 6};
    result[5][33] = {34, 7};
    result[5][34] = {35, 8};
    result[5][35] = {36, 9};
    result[5][36] = {37, 10};
    result[5][37] = {38, 11};
    result[5][38] = {39, 12};
    result[5][39] = {40, 13};
    result[5][40] = {41, 14};
    result[5][41] = {42, 0};
    result[5][42] = {43, 1};
    result[5][43] = {44, 2};
    result[5][44] = {45, 4};
    result[5][45] = {47, 6};
    result[5][46] = {48, 7};
    result[5][47] = {49, 8};
    result[5][48] = {50, 9};
    result[5][49] = {51, 10};
    result[5][50] = {52, 11};
    result[5][51] = {53, 12};
    result[5][52] = {54, 13};
    result[5][53] = {55, 14};
    result[5][54] = {56, 0};
    result[5][55] = {57, 1};
    result[5][56] = {58, 2};
    result[5][57] = {59, 3};
    result[5][58] = {61, 6};
    result[5][59] = {62, 7};
    result[5][60] = {63, 8};
    result[5][61] = {64, 9};
    result[5][62] = {65, 10};
    result[5][63] = {66, 11};
    result[5][64] = {67, 12};
    result[5][65] = {68, 13};
    result[5][66] = {69, 14};
    result[5][67] = {81, 0};
    result[5][68] = {82, 2};
    result[5][69] = {83, 5};
    result[5][70] = {84, 7};
    result[5][71] = {85, 8};
    result[5][72] = {86, 9};
    result[5][73] = {87, 10};
    result[5][74] = {88, 11};
    result[5][75] = {89, 12};
    result[5][76] = {90, 13};
    result[5][77] = {91, 14};
    result[5][78] = {92, 0};
    result[5][79] = {93, 3};
    result[5][80] = {94, 5};
    result[5][81] = {95, 6};
    result[5][82] = {96, 8};
    result[5][83] = {97, 9};
    result[5][84] = {98, 10};
    result[5][85] = {99, 11};
    result[5][86] = {100, 12};
    result[5][87] = {101, 13};
    result[5][88] = {102, 14};
    result[5][89] = {103, 0};
    result[5][90] = {104, 4};
    result[5][91] = {105, 5};
    result[5][92] = {106, 6};
    result[5][93] = {107, 7};
    result[5][94] = {108, 9};
    result[5][95] = {109, 10};
    result[5][96] = {110, 11};
    result[5][97] = {111, 12};
    result[5][98] = {112, 13};
    result[5][99] = {113, 14};
    result[5][100] = {114, 1};
    result[5][101] = {115, 2};
    result[5][102] = {116, 5};
    result[5][103] = {117, 6};
    result[5][104] = {118, 10};
    result[5][105] = {119, 11};
    result[5][106] = {120, 12};
    result[5][107] = {121, 13};
    result[5][108] = {122, 14};
    result[5][109] = {123, 1};
    result[5][110] = {124, 3};
    result[5][111] = {125, 5};
    result[5][112] = {126, 7};
    result[5][113] = {127, 9};
    result[5][114] = {128, 11};
    result[5][115] = {129, 12};
    result[5][116] = {130, 13};
    result[5][117] = {131, 14};
    result[5][118] = {132, 1};
    result[5][119] = {133, 4};
    result[5][120] = {134, 5};
    result[5][121] = {135, 8};
    result[5][122] = {136, 9};
    result[5][123] = {137, 10};
    result[5][124] = {138, 12};
    result[5][125] = {139, 13};
    result[5][126] = {140, 14};
    result[5][127] = {141, 2};
    result[5][128] = {142, 3};
    result[5][129] = {143, 6};
    result[5][130] = {144, 7};
    result[5][131] = {145, 9};
    result[5][132] = {146, 10};
    result[5][133] = {147, 13};
    result[5][134] = {148, 14};
    result[5][135] = {149, 2};
    result[5][136] = {150, 4};
    result[5][137] = {151, 6};
    result[5][138] = {152, 8};
    result[5][139] = {153, 9};
    result[5][140] = {154, 11};
    result[5][141] = {155, 12};
    result[5][142] = {156, 14};
    result[5][143] = {157, 3};
    result[5][144] = {158, 4};
    result[5][145] = {159, 7};
    result[5][146] = {160, 8};
    result[5][147] = {161, 10};
    result[5][148] = {162, 11};
    result[5][149] = {163, 12};
    result[5][150] = {164, 13};
    // last_T=6: 151 candidates
    result[6][0] = {0, 1};
    result[6][1] = {1, 2};
    result[6][2] = {2, 3};
    result[6][3] = {3, 4};
    result[6][4] = {4, 5};
    result[6][5] = {5, 6};
    result[6][6] = {6, 7};
    result[6][7] = {7, 8};
    result[6][8] = {8, 9};
    result[6][9] = {9, 10};
    result[6][10] = {10, 11};
    result[6][11] = {11, 12};
    result[6][12] = {12, 13};
    result[6][13] = {13, 14};
    result[6][14] = {14, 0};
    result[6][15] = {15, 2};
    result[6][16] = {16, 3};
    result[6][17] = {17, 4};
    result[6][18] = {18, 5};
    result[6][19] = {20, 7};
    result[6][20] = {21, 8};
    result[6][21] = {22, 9};
    result[6][22] = {23, 10};
    result[6][23] = {24, 11};
    result[6][24] = {25, 12};
    result[6][25] = {26, 13};
    result[6][26] = {27, 14};
    result[6][27] = {28, 0};
    result[6][28] = {29, 1};
    result[6][29] = {30, 3};
    result[6][30] = {31, 4};
    result[6][31] = {32, 5};
    result[6][32] = {33, 6};
    result[6][33] = {34, 7};
    result[6][34] = {35, 8};
    result[6][35] = {36, 9};
    result[6][36] = {37, 10};
    result[6][37] = {38, 11};
    result[6][38] = {39, 12};
    result[6][39] = {40, 13};
    result[6][40] = {41, 14};
    result[6][41] = {42, 0};
    result[6][42] = {43, 1};
    result[6][43] = {44, 2};
    result[6][44] = {45, 4};
    result[6][45] = {46, 5};
    result[6][46] = {48, 7};
    result[6][47] = {49, 8};
    result[6][48] = {50, 9};
    result[6][49] = {51, 10};
    result[6][50] = {52, 11};
    result[6][51] = {53, 12};
    result[6][52] = {54, 13};
    result[6][53] = {55, 14};
    result[6][54] = {56, 0};
    result[6][55] = {57, 1};
    result[6][56] = {58, 2};
    result[6][57] = {59, 3};
    result[6][58] = {60, 5};
    result[6][59] = {62, 7};
    result[6][60] = {63, 8};
    result[6][61] = {64, 9};
    result[6][62] = {65, 10};
    result[6][63] = {66, 11};
    result[6][64] = {67, 12};
    result[6][65] = {68, 13};
    result[6][66] = {69, 14};
    result[6][67] = {70, 0};
    result[6][68] = {71, 1};
    result[6][69] = {72, 6};
    result[6][70] = {73, 7};
    result[6][71] = {74, 8};
    result[6][72] = {75, 9};
    result[6][73] = {76, 10};
    result[6][74] = {77, 11};
    result[6][75] = {78, 12};
    result[6][76] = {79, 13};
    result[6][77] = {80, 14};
    result[6][78] = {92, 0};
    result[6][79] = {93, 3};
    result[6][80] = {94, 5};
    result[6][81] = {95, 6};
    result[6][82] = {96, 8};
    result[6][83] = {97, 9};
    result[6][84] = {98, 10};
    result[6][85] = {99, 11};
    result[6][86] = {100, 12};
    result[6][87] = {101, 13};
    result[6][88] = {102, 14};
    result[6][89] = {103, 0};
    result[6][90] = {104, 4};
    result[6][91] = {105, 5};
    result[6][92] = {106, 6};
    result[6][93] = {107, 7};
    result[6][94] = {108, 9};
    result[6][95] = {109, 10};
    result[6][96] = {110, 11};
    result[6][97] = {111, 12};
    result[6][98] = {112, 13};
    result[6][99] = {113, 14};
    result[6][100] = {114, 1};
    result[6][101] = {115, 2};
    result[6][102] = {116, 5};
    result[6][103] = {117, 6};
    result[6][104] = {118, 10};
    result[6][105] = {119, 11};
    result[6][106] = {120, 12};
    result[6][107] = {121, 13};
    result[6][108] = {122, 14};
    result[6][109] = {123, 1};
    result[6][110] = {124, 3};
    result[6][111] = {125, 5};
    result[6][112] = {126, 7};
    result[6][113] = {127, 9};
    result[6][114] = {128, 11};
    result[6][115] = {129, 12};
    result[6][116] = {130, 13};
    result[6][117] = {131, 14};
    result[6][118] = {132, 1};
    result[6][119] = {133, 4};
    result[6][120] = {134, 5};
    result[6][121] = {135, 8};
    result[6][122] = {136, 9};
    result[6][123] = {137, 10};
    result[6][124] = {138, 12};
    result[6][125] = {139, 13};
    result[6][126] = {140, 14};
    result[6][127] = {141, 2};
    result[6][128] = {142, 3};
    result[6][129] = {143, 6};
    result[6][130] = {144, 7};
    result[6][131] = {145, 9};
    result[6][132] = {146, 10};
    result[6][133] = {147, 13};
    result[6][134] = {148, 14};
    result[6][135] = {149, 2};
    result[6][136] = {150, 4};
    result[6][137] = {151, 6};
    result[6][138] = {152, 8};
    result[6][139] = {153, 9};
    result[6][140] = {154, 11};
    result[6][141] = {155, 12};
    result[6][142] = {156, 14};
    result[6][143] = {157, 3};
    result[6][144] = {158, 4};
    result[6][145] = {159, 7};
    result[6][146] = {160, 8};
    result[6][147] = {161, 10};
    result[6][148] = {162, 11};
    result[6][149] = {163, 12};
    result[6][150] = {164, 13};
    // last_T=7: 151 candidates
    result[7][0] = {0, 1};
    result[7][1] = {1, 2};
    result[7][2] = {2, 3};
    result[7][3] = {3, 4};
    result[7][4] = {4, 5};
    result[7][5] = {5, 6};
    result[7][6] = {6, 7};
    result[7][7] = {7, 8};
    result[7][8] = {8, 9};
    result[7][9] = {9, 10};
    result[7][10] = {10, 11};
    result[7][11] = {11, 12};
    result[7][12] = {12, 13};
    result[7][13] = {13, 14};
    result[7][14] = {14, 0};
    result[7][15] = {15, 2};
    result[7][16] = {16, 3};
    result[7][17] = {17, 4};
    result[7][18] = {18, 5};
    result[7][19] = {19, 6};
    result[7][20] = {21, 8};
    result[7][21] = {22, 9};
    result[7][22] = {23, 10};
    result[7][23] = {24, 11};
    result[7][24] = {25, 12};
    result[7][25] = {26, 13};
    result[7][26] = {27, 14};
    result[7][27] = {28, 0};
    result[7][28] = {29, 1};
    result[7][29] = {30, 3};
    result[7][30] = {31, 4};
    result[7][31] = {32, 5};
    result[7][32] = {33, 6};
    result[7][33] = {35, 8};
    result[7][34] = {36, 9};
    result[7][35] = {37, 10};
    result[7][36] = {38, 11};
    result[7][37] = {39, 12};
    result[7][38] = {40, 13};
    result[7][39] = {41, 14};
    result[7][40] = {42, 0};
    result[7][41] = {43, 1};
    result[7][42] = {44, 2};
    result[7][43] = {45, 4};
    result[7][44] = {46, 5};
    result[7][45] = {47, 6};
    result[7][46] = {48, 7};
    result[7][47] = {49, 8};
    result[7][48] = {50, 9};
    result[7][49] = {51, 10};
    result[7][50] = {52, 11};
    result[7][51] = {53, 12};
    result[7][52] = {54, 13};
    result[7][53] = {55, 14};
    result[7][54] = {56, 0};
    result[7][55] = {57, 1};
    result[7][56] = {58, 2};
    result[7][57] = {59, 3};
    result[7][58] = {60, 5};
    result[7][59] = {61, 6};
    result[7][60] = {63, 8};
    result[7][61] = {64, 9};
    result[7][62] = {65, 10};
    result[7][63] = {66, 11};
    result[7][64] = {67, 12};
    result[7][65] = {68, 13};
    result[7][66] = {69, 14};
    result[7][67] = {70, 0};
    result[7][68] = {71, 1};
    result[7][69] = {72, 6};
    result[7][70] = {73, 7};
    result[7][71] = {74, 8};
    result[7][72] = {75, 9};
    result[7][73] = {76, 10};
    result[7][74] = {77, 11};
    result[7][75] = {78, 12};
    result[7][76] = {79, 13};
    result[7][77] = {80, 14};
    result[7][78] = {81, 0};
    result[7][79] = {82, 2};
    result[7][80] = {83, 5};
    result[7][81] = {84, 7};
    result[7][82] = {85, 8};
    result[7][83] = {86, 9};
    result[7][84] = {87, 10};
    result[7][85] = {88, 11};
    result[7][86] = {89, 12};
    result[7][87] = {90, 13};
    result[7][88] = {91, 14};
    result[7][89] = {103, 0};
    result[7][90] = {104, 4};
    result[7][91] = {105, 5};
    result[7][92] = {106, 6};
    result[7][93] = {107, 7};
    result[7][94] = {108, 9};
    result[7][95] = {109, 10};
    result[7][96] = {110, 11};
    result[7][97] = {111, 12};
    result[7][98] = {112, 13};
    result[7][99] = {113, 14};
    result[7][100] = {114, 1};
    result[7][101] = {115, 2};
    result[7][102] = {116, 5};
    result[7][103] = {117, 6};
    result[7][104] = {118, 10};
    result[7][105] = {119, 11};
    result[7][106] = {120, 12};
    result[7][107] = {121, 13};
    result[7][108] = {122, 14};
    result[7][109] = {123, 1};
    result[7][110] = {124, 3};
    result[7][111] = {125, 5};
    result[7][112] = {126, 7};
    result[7][113] = {127, 9};
    result[7][114] = {128, 11};
    result[7][115] = {129, 12};
    result[7][116] = {130, 13};
    result[7][117] = {131, 14};
    result[7][118] = {132, 1};
    result[7][119] = {133, 4};
    result[7][120] = {134, 5};
    result[7][121] = {135, 8};
    result[7][122] = {136, 9};
    result[7][123] = {137, 10};
    result[7][124] = {138, 12};
    result[7][125] = {139, 13};
    result[7][126] = {140, 14};
    result[7][127] = {141, 2};
    result[7][128] = {142, 3};
    result[7][129] = {143, 6};
    result[7][130] = {144, 7};
    result[7][131] = {145, 9};
    result[7][132] = {146, 10};
    result[7][133] = {147, 13};
    result[7][134] = {148, 14};
    result[7][135] = {149, 2};
    result[7][136] = {150, 4};
    result[7][137] = {151, 6};
    result[7][138] = {152, 8};
    result[7][139] = {153, 9};
    result[7][140] = {154, 11};
    result[7][141] = {155, 12};
    result[7][142] = {156, 14};
    result[7][143] = {157, 3};
    result[7][144] = {158, 4};
    result[7][145] = {159, 7};
    result[7][146] = {160, 8};
    result[7][147] = {161, 10};
    result[7][148] = {162, 11};
    result[7][149] = {163, 12};
    result[7][150] = {164, 13};
    // last_T=8: 151 candidates
    result[8][0] = {0, 1};
    result[8][1] = {1, 2};
    result[8][2] = {2, 3};
    result[8][3] = {3, 4};
    result[8][4] = {4, 5};
    result[8][5] = {5, 6};
    result[8][6] = {6, 7};
    result[8][7] = {7, 8};
    result[8][8] = {8, 9};
    result[8][9] = {9, 10};
    result[8][10] = {10, 11};
    result[8][11] = {11, 12};
    result[8][12] = {12, 13};
    result[8][13] = {13, 14};
    result[8][14] = {14, 0};
    result[8][15] = {15, 2};
    result[8][16] = {16, 3};
    result[8][17] = {17, 4};
    result[8][18] = {18, 5};
    result[8][19] = {19, 6};
    result[8][20] = {20, 7};
    result[8][21] = {22, 9};
    result[8][22] = {23, 10};
    result[8][23] = {24, 11};
    result[8][24] = {25, 12};
    result[8][25] = {26, 13};
    result[8][26] = {27, 14};
    result[8][27] = {28, 0};
    result[8][28] = {29, 1};
    result[8][29] = {30, 3};
    result[8][30] = {31, 4};
    result[8][31] = {32, 5};
    result[8][32] = {33, 6};
    result[8][33] = {34, 7};
    result[8][34] = {36, 9};
    result[8][35] = {37, 10};
    result[8][36] = {38, 11};
    result[8][37] = {39, 12};
    result[8][38] = {40, 13};
    result[8][39] = {41, 14};
    result[8][40] = {42, 0};
    result[8][41] = {43, 1};
    result[8][42] = {44, 2};
    result[8][43] = {45, 4};
    result[8][44] = {46, 5};
    result[8][45] = {47, 6};
    result[8][46] = {48, 7};
    result[8][47] = {50, 9};
    result[8][48] = {51, 10};
    result[8][49] = {52, 11};
    result[8][50] = {53, 12};
    result[8][51] = {54, 13};
    result[8][52] = {55, 14};
    result[8][53] = {56, 0};
    result[8][54] = {57, 1};
    result[8][55] = {58, 2};
    result[8][56] = {59, 3};
    result[8][57] = {60, 5};
    result[8][58] = {61, 6};
    result[8][59] = {62, 7};
    result[8][60] = {63, 8};
    result[8][61] = {64, 9};
    result[8][62] = {65, 10};
    result[8][63] = {66, 11};
    result[8][64] = {67, 12};
    result[8][65] = {68, 13};
    result[8][66] = {69, 14};
    result[8][67] = {70, 0};
    result[8][68] = {71, 1};
    result[8][69] = {72, 6};
    result[8][70] = {73, 7};
    result[8][71] = {74, 8};
    result[8][72] = {75, 9};
    result[8][73] = {76, 10};
    result[8][74] = {77, 11};
    result[8][75] = {78, 12};
    result[8][76] = {79, 13};
    result[8][77] = {80, 14};
    result[8][78] = {81, 0};
    result[8][79] = {82, 2};
    result[8][80] = {83, 5};
    result[8][81] = {84, 7};
    result[8][82] = {85, 8};
    result[8][83] = {86, 9};
    result[8][84] = {87, 10};
    result[8][85] = {88, 11};
    result[8][86] = {89, 12};
    result[8][87] = {90, 13};
    result[8][88] = {91, 14};
    result[8][89] = {92, 0};
    result[8][90] = {93, 3};
    result[8][91] = {94, 5};
    result[8][92] = {95, 6};
    result[8][93] = {96, 8};
    result[8][94] = {97, 9};
    result[8][95] = {98, 10};
    result[8][96] = {99, 11};
    result[8][97] = {100, 12};
    result[8][98] = {101, 13};
    result[8][99] = {102, 14};
    result[8][100] = {114, 1};
    result[8][101] = {115, 2};
    result[8][102] = {116, 5};
    result[8][103] = {117, 6};
    result[8][104] = {118, 10};
    result[8][105] = {119, 11};
    result[8][106] = {120, 12};
    result[8][107] = {121, 13};
    result[8][108] = {122, 14};
    result[8][109] = {123, 1};
    result[8][110] = {124, 3};
    result[8][111] = {125, 5};
    result[8][112] = {126, 7};
    result[8][113] = {127, 9};
    result[8][114] = {128, 11};
    result[8][115] = {129, 12};
    result[8][116] = {130, 13};
    result[8][117] = {131, 14};
    result[8][118] = {132, 1};
    result[8][119] = {133, 4};
    result[8][120] = {134, 5};
    result[8][121] = {135, 8};
    result[8][122] = {136, 9};
    result[8][123] = {137, 10};
    result[8][124] = {138, 12};
    result[8][125] = {139, 13};
    result[8][126] = {140, 14};
    result[8][127] = {141, 2};
    result[8][128] = {142, 3};
    result[8][129] = {143, 6};
    result[8][130] = {144, 7};
    result[8][131] = {145, 9};
    result[8][132] = {146, 10};
    result[8][133] = {147, 13};
    result[8][134] = {148, 14};
    result[8][135] = {149, 2};
    result[8][136] = {150, 4};
    result[8][137] = {151, 6};
    result[8][138] = {152, 8};
    result[8][139] = {153, 9};
    result[8][140] = {154, 11};
    result[8][141] = {155, 12};
    result[8][142] = {156, 14};
    result[8][143] = {157, 3};
    result[8][144] = {158, 4};
    result[8][145] = {159, 7};
    result[8][146] = {160, 8};
    result[8][147] = {161, 10};
    result[8][148] = {162, 11};
    result[8][149] = {163, 12};
    result[8][150] = {164, 13};
    // last_T=9: 151 candidates
    result[9][0] = {0, 1};
    result[9][1] = {1, 2};
    result[9][2] = {2, 3};
    result[9][3] = {3, 4};
    result[9][4] = {4, 5};
    result[9][5] = {5, 6};
    result[9][6] = {6, 7};
    result[9][7] = {7, 8};
    result[9][8] = {9, 10};
    result[9][9] = {10, 11};
    result[9][10] = {11, 12};
    result[9][11] = {12, 13};
    result[9][12] = {13, 14};
    result[9][13] = {14, 0};
    result[9][14] = {15, 2};
    result[9][15] = {16, 3};
    result[9][16] = {17, 4};
    result[9][17] = {18, 5};
    result[9][18] = {19, 6};
    result[9][19] = {20, 7};
    result[9][20] = {21, 8};
    result[9][21] = {22, 9};
    result[9][22] = {23, 10};
    result[9][23] = {24, 11};
    result[9][24] = {25, 12};
    result[9][25] = {26, 13};
    result[9][26] = {27, 14};
    result[9][27] = {28, 0};
    result[9][28] = {29, 1};
    result[9][29] = {30, 3};
    result[9][30] = {31, 4};
    result[9][31] = {32, 5};
    result[9][32] = {33, 6};
    result[9][33] = {34, 7};
    result[9][34] = {35, 8};
    result[9][35] = {36, 9};
    result[9][36] = {37, 10};
    result[9][37] = {38, 11};
    result[9][38] = {39, 12};
    result[9][39] = {40, 13};
    result[9][40] = {41, 14};
    result[9][41] = {42, 0};
    result[9][42] = {43, 1};
    result[9][43] = {44, 2};
    result[9][44] = {45, 4};
    result[9][45] = {46, 5};
    result[9][46] = {47, 6};
    result[9][47] = {48, 7};
    result[9][48] = {49, 8};
    result[9][49] = {51, 10};
    result[9][50] = {52, 11};
    result[9][51] = {53, 12};
    result[9][52] = {54, 13};
    result[9][53] = {55, 14};
    result[9][54] = {56, 0};
    result[9][55] = {57, 1};
    result[9][56] = {58, 2};
    result[9][57] = {59, 3};
    result[9][58] = {60, 5};
    result[9][59] = {61, 6};
    result[9][60] = {62, 7};
    result[9][61] = {63, 8};
    result[9][62] = {65, 10};
    result[9][63] = {66, 11};
    result[9][64] = {67, 12};
    result[9][65] = {68, 13};
    result[9][66] = {69, 14};
    result[9][67] = {70, 0};
    result[9][68] = {71, 1};
    result[9][69] = {72, 6};
    result[9][70] = {73, 7};
    result[9][71] = {74, 8};
    result[9][72] = {75, 9};
    result[9][73] = {76, 10};
    result[9][74] = {77, 11};
    result[9][75] = {78, 12};
    result[9][76] = {79, 13};
    result[9][77] = {80, 14};
    result[9][78] = {81, 0};
    result[9][79] = {82, 2};
    result[9][80] = {83, 5};
    result[9][81] = {84, 7};
    result[9][82] = {85, 8};
    result[9][83] = {86, 9};
    result[9][84] = {87, 10};
    result[9][85] = {88, 11};
    result[9][86] = {89, 12};
    result[9][87] = {90, 13};
    result[9][88] = {91, 14};
    result[9][89] = {92, 0};
    result[9][90] = {93, 3};
    result[9][91] = {94, 5};
    result[9][92] = {95, 6};
    result[9][93] = {96, 8};
    result[9][94] = {98, 10};
    result[9][95] = {99, 11};
    result[9][96] = {100, 12};
    result[9][97] = {101, 13};
    result[9][98] = {102, 14};
    result[9][99] = {103, 0};
    result[9][100] = {104, 4};
    result[9][101] = {105, 5};
    result[9][102] = {106, 6};
    result[9][103] = {107, 7};
    result[9][104] = {109, 10};
    result[9][105] = {110, 11};
    result[9][106] = {111, 12};
    result[9][107] = {112, 13};
    result[9][108] = {113, 14};
    result[9][109] = {123, 1};
    result[9][110] = {124, 3};
    result[9][111] = {125, 5};
    result[9][112] = {126, 7};
    result[9][113] = {127, 9};
    result[9][114] = {128, 11};
    result[9][115] = {129, 12};
    result[9][116] = {130, 13};
    result[9][117] = {131, 14};
    result[9][118] = {132, 1};
    result[9][119] = {133, 4};
    result[9][120] = {134, 5};
    result[9][121] = {135, 8};
    result[9][122] = {136, 9};
    result[9][123] = {137, 10};
    result[9][124] = {138, 12};
    result[9][125] = {139, 13};
    result[9][126] = {140, 14};
    result[9][127] = {141, 2};
    result[9][128] = {142, 3};
    result[9][129] = {143, 6};
    result[9][130] = {144, 7};
    result[9][131] = {145, 9};
    result[9][132] = {146, 10};
    result[9][133] = {147, 13};
    result[9][134] = {148, 14};
    result[9][135] = {149, 2};
    result[9][136] = {150, 4};
    result[9][137] = {151, 6};
    result[9][138] = {152, 8};
    result[9][139] = {153, 9};
    result[9][140] = {154, 11};
    result[9][141] = {155, 12};
    result[9][142] = {156, 14};
    result[9][143] = {157, 3};
    result[9][144] = {158, 4};
    result[9][145] = {159, 7};
    result[9][146] = {160, 8};
    result[9][147] = {161, 10};
    result[9][148] = {162, 11};
    result[9][149] = {163, 12};
    result[9][150] = {164, 13};
    // last_T=10: 151 candidates
    result[10][0] = {0, 1};
    result[10][1] = {1, 2};
    result[10][2] = {2, 3};
    result[10][3] = {3, 4};
    result[10][4] = {4, 5};
    result[10][5] = {5, 6};
    result[10][6] = {6, 7};
    result[10][7] = {7, 8};
    result[10][8] = {8, 9};
    result[10][9] = {10, 11};
    result[10][10] = {11, 12};
    result[10][11] = {12, 13};
    result[10][12] = {13, 14};
    result[10][13] = {14, 0};
    result[10][14] = {15, 2};
    result[10][15] = {16, 3};
    result[10][16] = {17, 4};
    result[10][17] = {18, 5};
    result[10][18] = {19, 6};
    result[10][19] = {20, 7};
    result[10][20] = {21, 8};
    result[10][21] = {22, 9};
    result[10][22] = {23, 10};
    result[10][23] = {24, 11};
    result[10][24] = {25, 12};
    result[10][25] = {26, 13};
    result[10][26] = {27, 14};
    result[10][27] = {28, 0};
    result[10][28] = {29, 1};
    result[10][29] = {30, 3};
    result[10][30] = {31, 4};
    result[10][31] = {32, 5};
    result[10][32] = {33, 6};
    result[10][33] = {34, 7};
    result[10][34] = {35, 8};
    result[10][35] = {36, 9};
    result[10][36] = {38, 11};
    result[10][37] = {39, 12};
    result[10][38] = {40, 13};
    result[10][39] = {41, 14};
    result[10][40] = {42, 0};
    result[10][41] = {43, 1};
    result[10][42] = {44, 2};
    result[10][43] = {45, 4};
    result[10][44] = {46, 5};
    result[10][45] = {47, 6};
    result[10][46] = {48, 7};
    result[10][47] = {49, 8};
    result[10][48] = {50, 9};
    result[10][49] = {51, 10};
    result[10][50] = {52, 11};
    result[10][51] = {53, 12};
    result[10][52] = {54, 13};
    result[10][53] = {55, 14};
    result[10][54] = {56, 0};
    result[10][55] = {57, 1};
    result[10][56] = {58, 2};
    result[10][57] = {59, 3};
    result[10][58] = {60, 5};
    result[10][59] = {61, 6};
    result[10][60] = {62, 7};
    result[10][61] = {63, 8};
    result[10][62] = {64, 9};
    result[10][63] = {66, 11};
    result[10][64] = {67, 12};
    result[10][65] = {68, 13};
    result[10][66] = {69, 14};
    result[10][67] = {70, 0};
    result[10][68] = {71, 1};
    result[10][69] = {72, 6};
    result[10][70] = {73, 7};
    result[10][71] = {74, 8};
    result[10][72] = {75, 9};
    result[10][73] = {76, 10};
    result[10][74] = {77, 11};
    result[10][75] = {78, 12};
    result[10][76] = {79, 13};
    result[10][77] = {80, 14};
    result[10][78] = {81, 0};
    result[10][79] = {82, 2};
    result[10][80] = {83, 5};
    result[10][81] = {84, 7};
    result[10][82] = {85, 8};
    result[10][83] = {86, 9};
    result[10][84] = {88, 11};
    result[10][85] = {89, 12};
    result[10][86] = {90, 13};
    result[10][87] = {91, 14};
    result[10][88] = {92, 0};
    result[10][89] = {93, 3};
    result[10][90] = {94, 5};
    result[10][91] = {95, 6};
    result[10][92] = {96, 8};
    result[10][93] = {97, 9};
    result[10][94] = {98, 10};
    result[10][95] = {99, 11};
    result[10][96] = {100, 12};
    result[10][97] = {101, 13};
    result[10][98] = {102, 14};
    result[10][99] = {103, 0};
    result[10][100] = {104, 4};
    result[10][101] = {105, 5};
    result[10][102] = {106, 6};
    result[10][103] = {107, 7};
    result[10][104] = {108, 9};
    result[10][105] = {110, 11};
    result[10][106] = {111, 12};
    result[10][107] = {112, 13};
    result[10][108] = {113, 14};
    result[10][109] = {114, 1};
    result[10][110] = {115, 2};
    result[10][111] = {116, 5};
    result[10][112] = {117, 6};
    result[10][113] = {118, 10};
    result[10][114] = {119, 11};
    result[10][115] = {120, 12};
    result[10][116] = {121, 13};
    result[10][117] = {122, 14};
    result[10][118] = {132, 1};
    result[10][119] = {133, 4};
    result[10][120] = {134, 5};
    result[10][121] = {135, 8};
    result[10][122] = {136, 9};
    result[10][123] = {137, 10};
    result[10][124] = {138, 12};
    result[10][125] = {139, 13};
    result[10][126] = {140, 14};
    result[10][127] = {141, 2};
    result[10][128] = {142, 3};
    result[10][129] = {143, 6};
    result[10][130] = {144, 7};
    result[10][131] = {145, 9};
    result[10][132] = {146, 10};
    result[10][133] = {147, 13};
    result[10][134] = {148, 14};
    result[10][135] = {149, 2};
    result[10][136] = {150, 4};
    result[10][137] = {151, 6};
    result[10][138] = {152, 8};
    result[10][139] = {153, 9};
    result[10][140] = {154, 11};
    result[10][141] = {155, 12};
    result[10][142] = {156, 14};
    result[10][143] = {157, 3};
    result[10][144] = {158, 4};
    result[10][145] = {159, 7};
    result[10][146] = {160, 8};
    result[10][147] = {161, 10};
    result[10][148] = {162, 11};
    result[10][149] = {163, 12};
    result[10][150] = {164, 13};
    // last_T=11: 151 candidates
    result[11][0] = {0, 1};
    result[11][1] = {1, 2};
    result[11][2] = {2, 3};
    result[11][3] = {3, 4};
    result[11][4] = {4, 5};
    result[11][5] = {5, 6};
    result[11][6] = {6, 7};
    result[11][7] = {7, 8};
    result[11][8] = {8, 9};
    result[11][9] = {9, 10};
    result[11][10] = {11, 12};
    result[11][11] = {12, 13};
    result[11][12] = {13, 14};
    result[11][13] = {14, 0};
    result[11][14] = {15, 2};
    result[11][15] = {16, 3};
    result[11][16] = {17, 4};
    result[11][17] = {18, 5};
    result[11][18] = {19, 6};
    result[11][19] = {20, 7};
    result[11][20] = {21, 8};
    result[11][21] = {22, 9};
    result[11][22] = {23, 10};
    result[11][23] = {24, 11};
    result[11][24] = {25, 12};
    result[11][25] = {26, 13};
    result[11][26] = {27, 14};
    result[11][27] = {28, 0};
    result[11][28] = {29, 1};
    result[11][29] = {30, 3};
    result[11][30] = {31, 4};
    result[11][31] = {32, 5};
    result[11][32] = {33, 6};
    result[11][33] = {34, 7};
    result[11][34] = {35, 8};
    result[11][35] = {36, 9};
    result[11][36] = {37, 10};
    result[11][37] = {39, 12};
    result[11][38] = {40, 13};
    result[11][39] = {41, 14};
    result[11][40] = {42, 0};
    result[11][41] = {43, 1};
    result[11][42] = {44, 2};
    result[11][43] = {45, 4};
    result[11][44] = {46, 5};
    result[11][45] = {47, 6};
    result[11][46] = {48, 7};
    result[11][47] = {49, 8};
    result[11][48] = {50, 9};
    result[11][49] = {51, 10};
    result[11][50] = {53, 12};
    result[11][51] = {54, 13};
    result[11][52] = {55, 14};
    result[11][53] = {56, 0};
    result[11][54] = {57, 1};
    result[11][55] = {58, 2};
    result[11][56] = {59, 3};
    result[11][57] = {60, 5};
    result[11][58] = {61, 6};
    result[11][59] = {62, 7};
    result[11][60] = {63, 8};
    result[11][61] = {64, 9};
    result[11][62] = {65, 10};
    result[11][63] = {66, 11};
    result[11][64] = {67, 12};
    result[11][65] = {68, 13};
    result[11][66] = {69, 14};
    result[11][67] = {70, 0};
    result[11][68] = {71, 1};
    result[11][69] = {72, 6};
    result[11][70] = {73, 7};
    result[11][71] = {74, 8};
    result[11][72] = {75, 9};
    result[11][73] = {76, 10};
    result[11][74] = {77, 11};
    result[11][75] = {78, 12};
    result[11][76] = {79, 13};
    result[11][77] = {80, 14};
    result[11][78] = {81, 0};
    result[11][79] = {82, 2};
    result[11][80] = {83, 5};
    result[11][81] = {84, 7};
    result[11][82] = {85, 8};
    result[11][83] = {86, 9};
    result[11][84] = {87, 10};
    result[11][85] = {89, 12};
    result[11][86] = {90, 13};
    result[11][87] = {91, 14};
    result[11][88] = {92, 0};
    result[11][89] = {93, 3};
    result[11][90] = {94, 5};
    result[11][91] = {95, 6};
    result[11][92] = {96, 8};
    result[11][93] = {97, 9};
    result[11][94] = {98, 10};
    result[11][95] = {100, 12};
    result[11][96] = {101, 13};
    result[11][97] = {102, 14};
    result[11][98] = {103, 0};
    result[11][99] = {104, 4};
    result[11][100] = {105, 5};
    result[11][101] = {106, 6};
    result[11][102] = {107, 7};
    result[11][103] = {108, 9};
    result[11][104] = {109, 10};
    result[11][105] = {110, 11};
    result[11][106] = {111, 12};
    result[11][107] = {112, 13};
    result[11][108] = {113, 14};
    result[11][109] = {114, 1};
    result[11][110] = {115, 2};
    result[11][111] = {116, 5};
    result[11][112] = {117, 6};
    result[11][113] = {118, 10};
    result[11][114] = {119, 11};
    result[11][115] = {120, 12};
    result[11][116] = {121, 13};
    result[11][117] = {122, 14};
    result[11][118] = {123, 1};
    result[11][119] = {124, 3};
    result[11][120] = {125, 5};
    result[11][121] = {126, 7};
    result[11][122] = {127, 9};
    result[11][123] = {128, 11};
    result[11][124] = {129, 12};
    result[11][125] = {130, 13};
    result[11][126] = {131, 14};
    result[11][127] = {141, 2};
    result[11][128] = {142, 3};
    result[11][129] = {143, 6};
    result[11][130] = {144, 7};
    result[11][131] = {145, 9};
    result[11][132] = {146, 10};
    result[11][133] = {147, 13};
    result[11][134] = {148, 14};
    result[11][135] = {149, 2};
    result[11][136] = {150, 4};
    result[11][137] = {151, 6};
    result[11][138] = {152, 8};
    result[11][139] = {153, 9};
    result[11][140] = {154, 11};
    result[11][141] = {155, 12};
    result[11][142] = {156, 14};
    result[11][143] = {157, 3};
    result[11][144] = {158, 4};
    result[11][145] = {159, 7};
    result[11][146] = {160, 8};
    result[11][147] = {161, 10};
    result[11][148] = {162, 11};
    result[11][149] = {163, 12};
    result[11][150] = {164, 13};
    // last_T=12: 151 candidates
    result[12][0] = {0, 1};
    result[12][1] = {1, 2};
    result[12][2] = {2, 3};
    result[12][3] = {3, 4};
    result[12][4] = {4, 5};
    result[12][5] = {5, 6};
    result[12][6] = {6, 7};
    result[12][7] = {7, 8};
    result[12][8] = {8, 9};
    result[12][9] = {9, 10};
    result[12][10] = {10, 11};
    result[12][11] = {12, 13};
    result[12][12] = {13, 14};
    result[12][13] = {14, 0};
    result[12][14] = {15, 2};
    result[12][15] = {16, 3};
    result[12][16] = {17, 4};
    result[12][17] = {18, 5};
    result[12][18] = {19, 6};
    result[12][19] = {20, 7};
    result[12][20] = {21, 8};
    result[12][21] = {22, 9};
    result[12][22] = {23, 10};
    result[12][23] = {24, 11};
    result[12][24] = {26, 13};
    result[12][25] = {27, 14};
    result[12][26] = {28, 0};
    result[12][27] = {29, 1};
    result[12][28] = {30, 3};
    result[12][29] = {31, 4};
    result[12][30] = {32, 5};
    result[12][31] = {33, 6};
    result[12][32] = {34, 7};
    result[12][33] = {35, 8};
    result[12][34] = {36, 9};
    result[12][35] = {37, 10};
    result[12][36] = {38, 11};
    result[12][37] = {39, 12};
    result[12][38] = {40, 13};
    result[12][39] = {41, 14};
    result[12][40] = {42, 0};
    result[12][41] = {43, 1};
    result[12][42] = {44, 2};
    result[12][43] = {45, 4};
    result[12][44] = {46, 5};
    result[12][45] = {47, 6};
    result[12][46] = {48, 7};
    result[12][47] = {49, 8};
    result[12][48] = {50, 9};
    result[12][49] = {51, 10};
    result[12][50] = {52, 11};
    result[12][51] = {53, 12};
    result[12][52] = {54, 13};
    result[12][53] = {55, 14};
    result[12][54] = {56, 0};
    result[12][55] = {57, 1};
    result[12][56] = {58, 2};
    result[12][57] = {59, 3};
    result[12][58] = {60, 5};
    result[12][59] = {61, 6};
    result[12][60] = {62, 7};
    result[12][61] = {63, 8};
    result[12][62] = {64, 9};
    result[12][63] = {65, 10};
    result[12][64] = {66, 11};
    result[12][65] = {68, 13};
    result[12][66] = {69, 14};
    result[12][67] = {70, 0};
    result[12][68] = {71, 1};
    result[12][69] = {72, 6};
    result[12][70] = {73, 7};
    result[12][71] = {74, 8};
    result[12][72] = {75, 9};
    result[12][73] = {76, 10};
    result[12][74] = {77, 11};
    result[12][75] = {79, 13};
    result[12][76] = {80, 14};
    result[12][77] = {81, 0};
    result[12][78] = {82, 2};
    result[12][79] = {83, 5};
    result[12][80] = {84, 7};
    result[12][81] = {85, 8};
    result[12][82] = {86, 9};
    result[12][83] = {87, 10};
    result[12][84] = {88, 11};
    result[12][85] = {89, 12};
    result[12][86] = {90, 13};
    result[12][87] = {91, 14};
    result[12][88] = {92, 0};
    result[12][89] = {93, 3};
    result[12][90] = {94, 5};
    result[12][91] = {95, 6};
    result[12][92] = {96, 8};
    result[12][93] = {97, 9};
    result[12][94] = {98, 10};
    result[12][95] = {99, 11};
    result[12][96] = {100, 12};
    result[12][97] = {101, 13};
    result[12][98] = {102, 14};
    result[12][99] = {103, 0};
    result[12][100] = {104, 4};
    result[12][101] = {105, 5};
    result[12][102] = {106, 6};
    result[12][103] = {107, 7};
    result[12][104] = {108, 9};
    result[12][105] = {109, 10};
    result[12][106] = {110, 11};
    result[12][107] = {112, 13};
    result[12][108] = {113, 14};
    result[12][109] = {114, 1};
    result[12][110] = {115, 2};
    result[12][111] = {116, 5};
    result[12][112] = {117, 6};
    result[12][113] = {118, 10};
    result[12][114] = {119, 11};
    result[12][115] = {120, 12};
    result[12][116] = {121, 13};
    result[12][117] = {122, 14};
    result[12][118] = {123, 1};
    result[12][119] = {124, 3};
    result[12][120] = {125, 5};
    result[12][121] = {126, 7};
    result[12][122] = {127, 9};
    result[12][123] = {128, 11};
    result[12][124] = {129, 12};
    result[12][125] = {130, 13};
    result[12][126] = {131, 14};
    result[12][127] = {132, 1};
    result[12][128] = {133, 4};
    result[12][129] = {134, 5};
    result[12][130] = {135, 8};
    result[12][131] = {136, 9};
    result[12][132] = {137, 10};
    result[12][133] = {139, 13};
    result[12][134] = {140, 14};
    result[12][135] = {149, 2};
    result[12][136] = {150, 4};
    result[12][137] = {151, 6};
    result[12][138] = {152, 8};
    result[12][139] = {153, 9};
    result[12][140] = {154, 11};
    result[12][141] = {155, 12};
    result[12][142] = {156, 14};
    result[12][143] = {157, 3};
    result[12][144] = {158, 4};
    result[12][145] = {159, 7};
    result[12][146] = {160, 8};
    result[12][147] = {161, 10};
    result[12][148] = {162, 11};
    result[12][149] = {163, 12};
    result[12][150] = {164, 13};
    // last_T=13: 151 candidates
    result[13][0] = {0, 1};
    result[13][1] = {1, 2};
    result[13][2] = {2, 3};
    result[13][3] = {3, 4};
    result[13][4] = {4, 5};
    result[13][5] = {5, 6};
    result[13][6] = {6, 7};
    result[13][7] = {7, 8};
    result[13][8] = {8, 9};
    result[13][9] = {9, 10};
    result[13][10] = {10, 11};
    result[13][11] = {11, 12};
    result[13][12] = {13, 14};
    result[13][13] = {14, 0};
    result[13][14] = {15, 2};
    result[13][15] = {16, 3};
    result[13][16] = {17, 4};
    result[13][17] = {18, 5};
    result[13][18] = {19, 6};
    result[13][19] = {20, 7};
    result[13][20] = {21, 8};
    result[13][21] = {22, 9};
    result[13][22] = {23, 10};
    result[13][23] = {24, 11};
    result[13][24] = {25, 12};
    result[13][25] = {27, 14};
    result[13][26] = {28, 0};
    result[13][27] = {29, 1};
    result[13][28] = {30, 3};
    result[13][29] = {31, 4};
    result[13][30] = {32, 5};
    result[13][31] = {33, 6};
    result[13][32] = {34, 7};
    result[13][33] = {35, 8};
    result[13][34] = {36, 9};
    result[13][35] = {37, 10};
    result[13][36] = {38, 11};
    result[13][37] = {39, 12};
    result[13][38] = {40, 13};
    result[13][39] = {41, 14};
    result[13][40] = {42, 0};
    result[13][41] = {43, 1};
    result[13][42] = {44, 2};
    result[13][43] = {45, 4};
    result[13][44] = {46, 5};
    result[13][45] = {47, 6};
    result[13][46] = {48, 7};
    result[13][47] = {49, 8};
    result[13][48] = {50, 9};
    result[13][49] = {51, 10};
    result[13][50] = {52, 11};
    result[13][51] = {53, 12};
    result[13][52] = {55, 14};
    result[13][53] = {56, 0};
    result[13][54] = {57, 1};
    result[13][55] = {58, 2};
    result[13][56] = {59, 3};
    result[13][57] = {60, 5};
    result[13][58] = {61, 6};
    result[13][59] = {62, 7};
    result[13][60] = {63, 8};
    result[13][61] = {64, 9};
    result[13][62] = {65, 10};
    result[13][63] = {66, 11};
    result[13][64] = {67, 12};
    result[13][65] = {68, 13};
    result[13][66] = {69, 14};
    result[13][67] = {70, 0};
    result[13][68] = {71, 1};
    result[13][69] = {72, 6};
    result[13][70] = {73, 7};
    result[13][71] = {74, 8};
    result[13][72] = {75, 9};
    result[13][73] = {76, 10};
    result[13][74] = {77, 11};
    result[13][75] = {78, 12};
    result[13][76] = {80, 14};
    result[13][77] = {81, 0};
    result[13][78] = {82, 2};
    result[13][79] = {83, 5};
    result[13][80] = {84, 7};
    result[13][81] = {85, 8};
    result[13][82] = {86, 9};
    result[13][83] = {87, 10};
    result[13][84] = {88, 11};
    result[13][85] = {89, 12};
    result[13][86] = {90, 13};
    result[13][87] = {91, 14};
    result[13][88] = {92, 0};
    result[13][89] = {93, 3};
    result[13][90] = {94, 5};
    result[13][91] = {95, 6};
    result[13][92] = {96, 8};
    result[13][93] = {97, 9};
    result[13][94] = {98, 10};
    result[13][95] = {99, 11};
    result[13][96] = {100, 12};
    result[13][97] = {102, 14};
    result[13][98] = {103, 0};
    result[13][99] = {104, 4};
    result[13][100] = {105, 5};
    result[13][101] = {106, 6};
    result[13][102] = {107, 7};
    result[13][103] = {108, 9};
    result[13][104] = {109, 10};
    result[13][105] = {110, 11};
    result[13][106] = {111, 12};
    result[13][107] = {112, 13};
    result[13][108] = {113, 14};
    result[13][109] = {114, 1};
    result[13][110] = {115, 2};
    result[13][111] = {116, 5};
    result[13][112] = {117, 6};
    result[13][113] = {118, 10};
    result[13][114] = {119, 11};
    result[13][115] = {120, 12};
    result[13][116] = {121, 13};
    result[13][117] = {122, 14};
    result[13][118] = {123, 1};
    result[13][119] = {124, 3};
    result[13][120] = {125, 5};
    result[13][121] = {126, 7};
    result[13][122] = {127, 9};
    result[13][123] = {128, 11};
    result[13][124] = {129, 12};
    result[13][125] = {131, 14};
    result[13][126] = {132, 1};
    result[13][127] = {133, 4};
    result[13][128] = {134, 5};
    result[13][129] = {135, 8};
    result[13][130] = {136, 9};
    result[13][131] = {137, 10};
    result[13][132] = {138, 12};
    result[13][133] = {139, 13};
    result[13][134] = {140, 14};
    result[13][135] = {141, 2};
    result[13][136] = {142, 3};
    result[13][137] = {143, 6};
    result[13][138] = {144, 7};
    result[13][139] = {145, 9};
    result[13][140] = {146, 10};
    result[13][141] = {147, 13};
    result[13][142] = {148, 14};
    result[13][143] = {157, 3};
    result[13][144] = {158, 4};
    result[13][145] = {159, 7};
    result[13][146] = {160, 8};
    result[13][147] = {161, 10};
    result[13][148] = {162, 11};
    result[13][149] = {163, 12};
    result[13][150] = {164, 13};
    // last_T=14: 151 candidates
    result[14][0] = {0, 1};
    result[14][1] = {1, 2};
    result[14][2] = {2, 3};
    result[14][3] = {3, 4};
    result[14][4] = {4, 5};
    result[14][5] = {5, 6};
    result[14][6] = {6, 7};
    result[14][7] = {7, 8};
    result[14][8] = {8, 9};
    result[14][9] = {9, 10};
    result[14][10] = {10, 11};
    result[14][11] = {11, 12};
    result[14][12] = {12, 13};
    result[14][13] = {14, 0};
    result[14][14] = {15, 2};
    result[14][15] = {16, 3};
    result[14][16] = {17, 4};
    result[14][17] = {18, 5};
    result[14][18] = {19, 6};
    result[14][19] = {20, 7};
    result[14][20] = {21, 8};
    result[14][21] = {22, 9};
    result[14][22] = {23, 10};
    result[14][23] = {24, 11};
    result[14][24] = {25, 12};
    result[14][25] = {26, 13};
    result[14][26] = {28, 0};
    result[14][27] = {29, 1};
    result[14][28] = {30, 3};
    result[14][29] = {31, 4};
    result[14][30] = {32, 5};
    result[14][31] = {33, 6};
    result[14][32] = {34, 7};
    result[14][33] = {35, 8};
    result[14][34] = {36, 9};
    result[14][35] = {37, 10};
    result[14][36] = {38, 11};
    result[14][37] = {39, 12};
    result[14][38] = {40, 13};
    result[14][39] = {42, 0};
    result[14][40] = {43, 1};
    result[14][41] = {44, 2};
    result[14][42] = {45, 4};
    result[14][43] = {46, 5};
    result[14][44] = {47, 6};
    result[14][45] = {48, 7};
    result[14][46] = {49, 8};
    result[14][47] = {50, 9};
    result[14][48] = {51, 10};
    result[14][49] = {52, 11};
    result[14][50] = {53, 12};
    result[14][51] = {54, 13};
    result[14][52] = {55, 14};
    result[14][53] = {56, 0};
    result[14][54] = {57, 1};
    result[14][55] = {58, 2};
    result[14][56] = {59, 3};
    result[14][57] = {60, 5};
    result[14][58] = {61, 6};
    result[14][59] = {62, 7};
    result[14][60] = {63, 8};
    result[14][61] = {64, 9};
    result[14][62] = {65, 10};
    result[14][63] = {66, 11};
    result[14][64] = {67, 12};
    result[14][65] = {68, 13};
    result[14][66] = {69, 14};
    result[14][67] = {70, 0};
    result[14][68] = {71, 1};
    result[14][69] = {72, 6};
    result[14][70] = {73, 7};
    result[14][71] = {74, 8};
    result[14][72] = {75, 9};
    result[14][73] = {76, 10};
    result[14][74] = {77, 11};
    result[14][75] = {78, 12};
    result[14][76] = {79, 13};
    result[14][77] = {81, 0};
    result[14][78] = {82, 2};
    result[14][79] = {83, 5};
    result[14][80] = {84, 7};
    result[14][81] = {85, 8};
    result[14][82] = {86, 9};
    result[14][83] = {87, 10};
    result[14][84] = {88, 11};
    result[14][85] = {89, 12};
    result[14][86] = {90, 13};
    result[14][87] = {92, 0};
    result[14][88] = {93, 3};
    result[14][89] = {94, 5};
    result[14][90] = {95, 6};
    result[14][91] = {96, 8};
    result[14][92] = {97, 9};
    result[14][93] = {98, 10};
    result[14][94] = {99, 11};
    result[14][95] = {100, 12};
    result[14][96] = {101, 13};
    result[14][97] = {102, 14};
    result[14][98] = {103, 0};
    result[14][99] = {104, 4};
    result[14][100] = {105, 5};
    result[14][101] = {106, 6};
    result[14][102] = {107, 7};
    result[14][103] = {108, 9};
    result[14][104] = {109, 10};
    result[14][105] = {110, 11};
    result[14][106] = {111, 12};
    result[14][107] = {112, 13};
    result[14][108] = {113, 14};
    result[14][109] = {114, 1};
    result[14][110] = {115, 2};
    result[14][111] = {116, 5};
    result[14][112] = {117, 6};
    result[14][113] = {118, 10};
    result[14][114] = {119, 11};
    result[14][115] = {120, 12};
    result[14][116] = {121, 13};
    result[14][117] = {123, 1};
    result[14][118] = {124, 3};
    result[14][119] = {125, 5};
    result[14][120] = {126, 7};
    result[14][121] = {127, 9};
    result[14][122] = {128, 11};
    result[14][123] = {129, 12};
    result[14][124] = {130, 13};
    result[14][125] = {131, 14};
    result[14][126] = {132, 1};
    result[14][127] = {133, 4};
    result[14][128] = {134, 5};
    result[14][129] = {135, 8};
    result[14][130] = {136, 9};
    result[14][131] = {137, 10};
    result[14][132] = {138, 12};
    result[14][133] = {139, 13};
    result[14][134] = {140, 14};
    result[14][135] = {141, 2};
    result[14][136] = {142, 3};
    result[14][137] = {143, 6};
    result[14][138] = {144, 7};
    result[14][139] = {145, 9};
    result[14][140] = {146, 10};
    result[14][141] = {147, 13};
    result[14][142] = {148, 14};
    result[14][143] = {149, 2};
    result[14][144] = {150, 4};
    result[14][145] = {151, 6};
    result[14][146] = {152, 8};
    result[14][147] = {153, 9};
    result[14][148] = {154, 11};
    result[14][149] = {155, 12};
    result[14][150] = {156, 14};
    // last_T=15: 165 candidates
    result[15][0] = {0, 1};
    result[15][1] = {1, 2};
    result[15][2] = {2, 3};
    result[15][3] = {3, 4};
    result[15][4] = {4, 5};
    result[15][5] = {5, 6};
    result[15][6] = {6, 7};
    result[15][7] = {7, 8};
    result[15][8] = {8, 9};
    result[15][9] = {9, 10};
    result[15][10] = {10, 11};
    result[15][11] = {11, 12};
    result[15][12] = {12, 13};
    result[15][13] = {13, 14};
    result[15][14] = {14, 0};
    result[15][15] = {15, 2};
    result[15][16] = {16, 3};
    result[15][17] = {17, 4};
    result[15][18] = {18, 5};
    result[15][19] = {19, 6};
    result[15][20] = {20, 7};
    result[15][21] = {21, 8};
    result[15][22] = {22, 9};
    result[15][23] = {23, 10};
    result[15][24] = {24, 11};
    result[15][25] = {25, 12};
    result[15][26] = {26, 13};
    result[15][27] = {27, 14};
    result[15][28] = {28, 0};
    result[15][29] = {29, 1};
    result[15][30] = {30, 3};
    result[15][31] = {31, 4};
    result[15][32] = {32, 5};
    result[15][33] = {33, 6};
    result[15][34] = {34, 7};
    result[15][35] = {35, 8};
    result[15][36] = {36, 9};
    result[15][37] = {37, 10};
    result[15][38] = {38, 11};
    result[15][39] = {39, 12};
    result[15][40] = {40, 13};
    result[15][41] = {41, 14};
    result[15][42] = {42, 0};
    result[15][43] = {43, 1};
    result[15][44] = {44, 2};
    result[15][45] = {45, 4};
    result[15][46] = {46, 5};
    result[15][47] = {47, 6};
    result[15][48] = {48, 7};
    result[15][49] = {49, 8};
    result[15][50] = {50, 9};
    result[15][51] = {51, 10};
    result[15][52] = {52, 11};
    result[15][53] = {53, 12};
    result[15][54] = {54, 13};
    result[15][55] = {55, 14};
    result[15][56] = {56, 0};
    result[15][57] = {57, 1};
    result[15][58] = {58, 2};
    result[15][59] = {59, 3};
    result[15][60] = {60, 5};
    result[15][61] = {61, 6};
    result[15][62] = {62, 7};
    result[15][63] = {63, 8};
    result[15][64] = {64, 9};
    result[15][65] = {65, 10};
    result[15][66] = {66, 11};
    result[15][67] = {67, 12};
    result[15][68] = {68, 13};
    result[15][69] = {69, 14};
    result[15][70] = {70, 0};
    result[15][71] = {71, 1};
    result[15][72] = {72, 6};
    result[15][73] = {73, 7};
    result[15][74] = {74, 8};
    result[15][75] = {75, 9};
    result[15][76] = {76, 10};
    result[15][77] = {77, 11};
    result[15][78] = {78, 12};
    result[15][79] = {79, 13};
    result[15][80] = {80, 14};
    result[15][81] = {81, 0};
    result[15][82] = {82, 2};
    result[15][83] = {83, 5};
    result[15][84] = {84, 7};
    result[15][85] = {85, 8};
    result[15][86] = {86, 9};
    result[15][87] = {87, 10};
    result[15][88] = {88, 11};
    result[15][89] = {89, 12};
    result[15][90] = {90, 13};
    result[15][91] = {91, 14};
    result[15][92] = {92, 0};
    result[15][93] = {93, 3};
    result[15][94] = {94, 5};
    result[15][95] = {95, 6};
    result[15][96] = {96, 8};
    result[15][97] = {97, 9};
    result[15][98] = {98, 10};
    result[15][99] = {99, 11};
    result[15][100] = {100, 12};
    result[15][101] = {101, 13};
    result[15][102] = {102, 14};
    result[15][103] = {103, 0};
    result[15][104] = {104, 4};
    result[15][105] = {105, 5};
    result[15][106] = {106, 6};
    result[15][107] = {107, 7};
    result[15][108] = {108, 9};
    result[15][109] = {109, 10};
    result[15][110] = {110, 11};
    result[15][111] = {111, 12};
    result[15][112] = {112, 13};
    result[15][113] = {113, 14};
    result[15][114] = {114, 1};
    result[15][115] = {115, 2};
    result[15][116] = {116, 5};
    result[15][117] = {117, 6};
    result[15][118] = {118, 10};
    result[15][119] = {119, 11};
    result[15][120] = {120, 12};
    result[15][121] = {121, 13};
    result[15][122] = {122, 14};
    result[15][123] = {123, 1};
    result[15][124] = {124, 3};
    result[15][125] = {125, 5};
    result[15][126] = {126, 7};
    result[15][127] = {127, 9};
    result[15][128] = {128, 11};
    result[15][129] = {129, 12};
    result[15][130] = {130, 13};
    result[15][131] = {131, 14};
    result[15][132] = {132, 1};
    result[15][133] = {133, 4};
    result[15][134] = {134, 5};
    result[15][135] = {135, 8};
    result[15][136] = {136, 9};
    result[15][137] = {137, 10};
    result[15][138] = {138, 12};
    result[15][139] = {139, 13};
    result[15][140] = {140, 14};
    result[15][141] = {141, 2};
    result[15][142] = {142, 3};
    result[15][143] = {143, 6};
    result[15][144] = {144, 7};
    result[15][145] = {145, 9};
    result[15][146] = {146, 10};
    result[15][147] = {147, 13};
    result[15][148] = {148, 14};
    result[15][149] = {149, 2};
    result[15][150] = {150, 4};
    result[15][151] = {151, 6};
    result[15][152] = {152, 8};
    result[15][153] = {153, 9};
    result[15][154] = {154, 11};
    result[15][155] = {155, 12};
    result[15][156] = {156, 14};
    result[15][157] = {157, 3};
    result[15][158] = {158, 4};
    result[15][159] = {159, 7};
    result[15][160] = {160, 8};
    result[15][161] = {161, 10};
    result[15][162] = {162, 11};
    result[15][163] = {163, 12};
    result[15][164] = {164, 13};
    return result;
}();

using TT_ApplyFn = void (*)(SO6&);

namespace detail {
inline void tt_apply_0(SO6& S) { apply_TT_overlap<0,1,0,2>(S); }
inline void tt_apply_1(SO6& S) { apply_TT_overlap<0,1,0,3>(S); }
inline void tt_apply_2(SO6& S) { apply_TT_overlap<0,1,0,4>(S); }
inline void tt_apply_3(SO6& S) { apply_TT_overlap<0,1,0,5>(S); }
inline void tt_apply_4(SO6& S) { apply_TT_overlap<0,1,1,2>(S); }
inline void tt_apply_5(SO6& S) { apply_TT_overlap<0,1,1,3>(S); }
inline void tt_apply_6(SO6& S) { apply_TT_overlap<0,1,1,4>(S); }
inline void tt_apply_7(SO6& S) { apply_TT_overlap<0,1,1,5>(S); }
inline void tt_apply_8(SO6& S) { apply_TT_disjoint<0,1,2,3>(S); }
inline void tt_apply_9(SO6& S) { apply_TT_disjoint<0,1,2,4>(S); }
inline void tt_apply_10(SO6& S) { apply_TT_disjoint<0,1,2,5>(S); }
inline void tt_apply_11(SO6& S) { apply_TT_disjoint<0,1,3,4>(S); }
inline void tt_apply_12(SO6& S) { apply_TT_disjoint<0,1,3,5>(S); }
inline void tt_apply_13(SO6& S) { apply_TT_disjoint<0,1,4,5>(S); }
inline void tt_apply_14(SO6& S) { apply_TT_overlap<0,2,0,1>(S); }
inline void tt_apply_15(SO6& S) { apply_TT_overlap<0,2,0,3>(S); }
inline void tt_apply_16(SO6& S) { apply_TT_overlap<0,2,0,4>(S); }
inline void tt_apply_17(SO6& S) { apply_TT_overlap<0,2,0,5>(S); }
inline void tt_apply_18(SO6& S) { apply_TT_overlap<0,2,1,2>(S); }
inline void tt_apply_19(SO6& S) { apply_TT_disjoint<0,2,1,3>(S); }
inline void tt_apply_20(SO6& S) { apply_TT_disjoint<0,2,1,4>(S); }
inline void tt_apply_21(SO6& S) { apply_TT_disjoint<0,2,1,5>(S); }
inline void tt_apply_22(SO6& S) { apply_TT_overlap<0,2,2,3>(S); }
inline void tt_apply_23(SO6& S) { apply_TT_overlap<0,2,2,4>(S); }
inline void tt_apply_24(SO6& S) { apply_TT_overlap<0,2,2,5>(S); }
inline void tt_apply_25(SO6& S) { apply_TT_disjoint<0,2,3,4>(S); }
inline void tt_apply_26(SO6& S) { apply_TT_disjoint<0,2,3,5>(S); }
inline void tt_apply_27(SO6& S) { apply_TT_disjoint<0,2,4,5>(S); }
inline void tt_apply_28(SO6& S) { apply_TT_overlap<0,3,0,1>(S); }
inline void tt_apply_29(SO6& S) { apply_TT_overlap<0,3,0,2>(S); }
inline void tt_apply_30(SO6& S) { apply_TT_overlap<0,3,0,4>(S); }
inline void tt_apply_31(SO6& S) { apply_TT_overlap<0,3,0,5>(S); }
inline void tt_apply_32(SO6& S) { apply_TT_disjoint<0,3,1,2>(S); }
inline void tt_apply_33(SO6& S) { apply_TT_overlap<0,3,1,3>(S); }
inline void tt_apply_34(SO6& S) { apply_TT_disjoint<0,3,1,4>(S); }
inline void tt_apply_35(SO6& S) { apply_TT_disjoint<0,3,1,5>(S); }
inline void tt_apply_36(SO6& S) { apply_TT_overlap<0,3,2,3>(S); }
inline void tt_apply_37(SO6& S) { apply_TT_disjoint<0,3,2,4>(S); }
inline void tt_apply_38(SO6& S) { apply_TT_disjoint<0,3,2,5>(S); }
inline void tt_apply_39(SO6& S) { apply_TT_overlap<0,3,3,4>(S); }
inline void tt_apply_40(SO6& S) { apply_TT_overlap<0,3,3,5>(S); }
inline void tt_apply_41(SO6& S) { apply_TT_disjoint<0,3,4,5>(S); }
inline void tt_apply_42(SO6& S) { apply_TT_overlap<0,4,0,1>(S); }
inline void tt_apply_43(SO6& S) { apply_TT_overlap<0,4,0,2>(S); }
inline void tt_apply_44(SO6& S) { apply_TT_overlap<0,4,0,3>(S); }
inline void tt_apply_45(SO6& S) { apply_TT_overlap<0,4,0,5>(S); }
inline void tt_apply_46(SO6& S) { apply_TT_disjoint<0,4,1,2>(S); }
inline void tt_apply_47(SO6& S) { apply_TT_disjoint<0,4,1,3>(S); }
inline void tt_apply_48(SO6& S) { apply_TT_overlap<0,4,1,4>(S); }
inline void tt_apply_49(SO6& S) { apply_TT_disjoint<0,4,1,5>(S); }
inline void tt_apply_50(SO6& S) { apply_TT_disjoint<0,4,2,3>(S); }
inline void tt_apply_51(SO6& S) { apply_TT_overlap<0,4,2,4>(S); }
inline void tt_apply_52(SO6& S) { apply_TT_disjoint<0,4,2,5>(S); }
inline void tt_apply_53(SO6& S) { apply_TT_overlap<0,4,3,4>(S); }
inline void tt_apply_54(SO6& S) { apply_TT_disjoint<0,4,3,5>(S); }
inline void tt_apply_55(SO6& S) { apply_TT_overlap<0,4,4,5>(S); }
inline void tt_apply_56(SO6& S) { apply_TT_overlap<0,5,0,1>(S); }
inline void tt_apply_57(SO6& S) { apply_TT_overlap<0,5,0,2>(S); }
inline void tt_apply_58(SO6& S) { apply_TT_overlap<0,5,0,3>(S); }
inline void tt_apply_59(SO6& S) { apply_TT_overlap<0,5,0,4>(S); }
inline void tt_apply_60(SO6& S) { apply_TT_disjoint<0,5,1,2>(S); }
inline void tt_apply_61(SO6& S) { apply_TT_disjoint<0,5,1,3>(S); }
inline void tt_apply_62(SO6& S) { apply_TT_disjoint<0,5,1,4>(S); }
inline void tt_apply_63(SO6& S) { apply_TT_overlap<0,5,1,5>(S); }
inline void tt_apply_64(SO6& S) { apply_TT_disjoint<0,5,2,3>(S); }
inline void tt_apply_65(SO6& S) { apply_TT_disjoint<0,5,2,4>(S); }
inline void tt_apply_66(SO6& S) { apply_TT_overlap<0,5,2,5>(S); }
inline void tt_apply_67(SO6& S) { apply_TT_disjoint<0,5,3,4>(S); }
inline void tt_apply_68(SO6& S) { apply_TT_overlap<0,5,3,5>(S); }
inline void tt_apply_69(SO6& S) { apply_TT_overlap<0,5,4,5>(S); }
inline void tt_apply_70(SO6& S) { apply_TT_overlap<1,2,0,1>(S); }
inline void tt_apply_71(SO6& S) { apply_TT_overlap<1,2,0,2>(S); }
inline void tt_apply_72(SO6& S) { apply_TT_overlap<1,2,1,3>(S); }
inline void tt_apply_73(SO6& S) { apply_TT_overlap<1,2,1,4>(S); }
inline void tt_apply_74(SO6& S) { apply_TT_overlap<1,2,1,5>(S); }
inline void tt_apply_75(SO6& S) { apply_TT_overlap<1,2,2,3>(S); }
inline void tt_apply_76(SO6& S) { apply_TT_overlap<1,2,2,4>(S); }
inline void tt_apply_77(SO6& S) { apply_TT_overlap<1,2,2,5>(S); }
inline void tt_apply_78(SO6& S) { apply_TT_disjoint<1,2,3,4>(S); }
inline void tt_apply_79(SO6& S) { apply_TT_disjoint<1,2,3,5>(S); }
inline void tt_apply_80(SO6& S) { apply_TT_disjoint<1,2,4,5>(S); }
inline void tt_apply_81(SO6& S) { apply_TT_overlap<1,3,0,1>(S); }
inline void tt_apply_82(SO6& S) { apply_TT_overlap<1,3,0,3>(S); }
inline void tt_apply_83(SO6& S) { apply_TT_overlap<1,3,1,2>(S); }
inline void tt_apply_84(SO6& S) { apply_TT_overlap<1,3,1,4>(S); }
inline void tt_apply_85(SO6& S) { apply_TT_overlap<1,3,1,5>(S); }
inline void tt_apply_86(SO6& S) { apply_TT_overlap<1,3,2,3>(S); }
inline void tt_apply_87(SO6& S) { apply_TT_disjoint<1,3,2,4>(S); }
inline void tt_apply_88(SO6& S) { apply_TT_disjoint<1,3,2,5>(S); }
inline void tt_apply_89(SO6& S) { apply_TT_overlap<1,3,3,4>(S); }
inline void tt_apply_90(SO6& S) { apply_TT_overlap<1,3,3,5>(S); }
inline void tt_apply_91(SO6& S) { apply_TT_disjoint<1,3,4,5>(S); }
inline void tt_apply_92(SO6& S) { apply_TT_overlap<1,4,0,1>(S); }
inline void tt_apply_93(SO6& S) { apply_TT_overlap<1,4,0,4>(S); }
inline void tt_apply_94(SO6& S) { apply_TT_overlap<1,4,1,2>(S); }
inline void tt_apply_95(SO6& S) { apply_TT_overlap<1,4,1,3>(S); }
inline void tt_apply_96(SO6& S) { apply_TT_overlap<1,4,1,5>(S); }
inline void tt_apply_97(SO6& S) { apply_TT_disjoint<1,4,2,3>(S); }
inline void tt_apply_98(SO6& S) { apply_TT_overlap<1,4,2,4>(S); }
inline void tt_apply_99(SO6& S) { apply_TT_disjoint<1,4,2,5>(S); }
inline void tt_apply_100(SO6& S) { apply_TT_overlap<1,4,3,4>(S); }
inline void tt_apply_101(SO6& S) { apply_TT_disjoint<1,4,3,5>(S); }
inline void tt_apply_102(SO6& S) { apply_TT_overlap<1,4,4,5>(S); }
inline void tt_apply_103(SO6& S) { apply_TT_overlap<1,5,0,1>(S); }
inline void tt_apply_104(SO6& S) { apply_TT_overlap<1,5,0,5>(S); }
inline void tt_apply_105(SO6& S) { apply_TT_overlap<1,5,1,2>(S); }
inline void tt_apply_106(SO6& S) { apply_TT_overlap<1,5,1,3>(S); }
inline void tt_apply_107(SO6& S) { apply_TT_overlap<1,5,1,4>(S); }
inline void tt_apply_108(SO6& S) { apply_TT_disjoint<1,5,2,3>(S); }
inline void tt_apply_109(SO6& S) { apply_TT_disjoint<1,5,2,4>(S); }
inline void tt_apply_110(SO6& S) { apply_TT_overlap<1,5,2,5>(S); }
inline void tt_apply_111(SO6& S) { apply_TT_disjoint<1,5,3,4>(S); }
inline void tt_apply_112(SO6& S) { apply_TT_overlap<1,5,3,5>(S); }
inline void tt_apply_113(SO6& S) { apply_TT_overlap<1,5,4,5>(S); }
inline void tt_apply_114(SO6& S) { apply_TT_overlap<2,3,0,2>(S); }
inline void tt_apply_115(SO6& S) { apply_TT_overlap<2,3,0,3>(S); }
inline void tt_apply_116(SO6& S) { apply_TT_overlap<2,3,1,2>(S); }
inline void tt_apply_117(SO6& S) { apply_TT_overlap<2,3,1,3>(S); }
inline void tt_apply_118(SO6& S) { apply_TT_overlap<2,3,2,4>(S); }
inline void tt_apply_119(SO6& S) { apply_TT_overlap<2,3,2,5>(S); }
inline void tt_apply_120(SO6& S) { apply_TT_overlap<2,3,3,4>(S); }
inline void tt_apply_121(SO6& S) { apply_TT_overlap<2,3,3,5>(S); }
inline void tt_apply_122(SO6& S) { apply_TT_disjoint<2,3,4,5>(S); }
inline void tt_apply_123(SO6& S) { apply_TT_overlap<2,4,0,2>(S); }
inline void tt_apply_124(SO6& S) { apply_TT_overlap<2,4,0,4>(S); }
inline void tt_apply_125(SO6& S) { apply_TT_overlap<2,4,1,2>(S); }
inline void tt_apply_126(SO6& S) { apply_TT_overlap<2,4,1,4>(S); }
inline void tt_apply_127(SO6& S) { apply_TT_overlap<2,4,2,3>(S); }
inline void tt_apply_128(SO6& S) { apply_TT_overlap<2,4,2,5>(S); }
inline void tt_apply_129(SO6& S) { apply_TT_overlap<2,4,3,4>(S); }
inline void tt_apply_130(SO6& S) { apply_TT_disjoint<2,4,3,5>(S); }
inline void tt_apply_131(SO6& S) { apply_TT_overlap<2,4,4,5>(S); }
inline void tt_apply_132(SO6& S) { apply_TT_overlap<2,5,0,2>(S); }
inline void tt_apply_133(SO6& S) { apply_TT_overlap<2,5,0,5>(S); }
inline void tt_apply_134(SO6& S) { apply_TT_overlap<2,5,1,2>(S); }
inline void tt_apply_135(SO6& S) { apply_TT_overlap<2,5,1,5>(S); }
inline void tt_apply_136(SO6& S) { apply_TT_overlap<2,5,2,3>(S); }
inline void tt_apply_137(SO6& S) { apply_TT_overlap<2,5,2,4>(S); }
inline void tt_apply_138(SO6& S) { apply_TT_disjoint<2,5,3,4>(S); }
inline void tt_apply_139(SO6& S) { apply_TT_overlap<2,5,3,5>(S); }
inline void tt_apply_140(SO6& S) { apply_TT_overlap<2,5,4,5>(S); }
inline void tt_apply_141(SO6& S) { apply_TT_overlap<3,4,0,3>(S); }
inline void tt_apply_142(SO6& S) { apply_TT_overlap<3,4,0,4>(S); }
inline void tt_apply_143(SO6& S) { apply_TT_overlap<3,4,1,3>(S); }
inline void tt_apply_144(SO6& S) { apply_TT_overlap<3,4,1,4>(S); }
inline void tt_apply_145(SO6& S) { apply_TT_overlap<3,4,2,3>(S); }
inline void tt_apply_146(SO6& S) { apply_TT_overlap<3,4,2,4>(S); }
inline void tt_apply_147(SO6& S) { apply_TT_overlap<3,4,3,5>(S); }
inline void tt_apply_148(SO6& S) { apply_TT_overlap<3,4,4,5>(S); }
inline void tt_apply_149(SO6& S) { apply_TT_overlap<3,5,0,3>(S); }
inline void tt_apply_150(SO6& S) { apply_TT_overlap<3,5,0,5>(S); }
inline void tt_apply_151(SO6& S) { apply_TT_overlap<3,5,1,3>(S); }
inline void tt_apply_152(SO6& S) { apply_TT_overlap<3,5,1,5>(S); }
inline void tt_apply_153(SO6& S) { apply_TT_overlap<3,5,2,3>(S); }
inline void tt_apply_154(SO6& S) { apply_TT_overlap<3,5,2,5>(S); }
inline void tt_apply_155(SO6& S) { apply_TT_overlap<3,5,3,4>(S); }
inline void tt_apply_156(SO6& S) { apply_TT_overlap<3,5,4,5>(S); }
inline void tt_apply_157(SO6& S) { apply_TT_overlap<4,5,0,4>(S); }
inline void tt_apply_158(SO6& S) { apply_TT_overlap<4,5,0,5>(S); }
inline void tt_apply_159(SO6& S) { apply_TT_overlap<4,5,1,4>(S); }
inline void tt_apply_160(SO6& S) { apply_TT_overlap<4,5,1,5>(S); }
inline void tt_apply_161(SO6& S) { apply_TT_overlap<4,5,2,4>(S); }
inline void tt_apply_162(SO6& S) { apply_TT_overlap<4,5,2,5>(S); }
inline void tt_apply_163(SO6& S) { apply_TT_overlap<4,5,3,4>(S); }
inline void tt_apply_164(SO6& S) { apply_TT_overlap<4,5,3,5>(S); }
} // namespace detail

inline constexpr std::array<TT_ApplyFn, 165> TT_APPLY_TABLE = {{
    &detail::tt_apply_0, // [0] O T(0,1)*T(0,2)
    &detail::tt_apply_1, // [1] O T(0,1)*T(0,3)
    &detail::tt_apply_2, // [2] O T(0,1)*T(0,4)
    &detail::tt_apply_3, // [3] O T(0,1)*T(0,5)
    &detail::tt_apply_4, // [4] O T(0,1)*T(1,2)
    &detail::tt_apply_5, // [5] O T(0,1)*T(1,3)
    &detail::tt_apply_6, // [6] O T(0,1)*T(1,4)
    &detail::tt_apply_7, // [7] O T(0,1)*T(1,5)
    &detail::tt_apply_8, // [8] D T(0,1)*T(2,3)
    &detail::tt_apply_9, // [9] D T(0,1)*T(2,4)
    &detail::tt_apply_10, // [10] D T(0,1)*T(2,5)
    &detail::tt_apply_11, // [11] D T(0,1)*T(3,4)
    &detail::tt_apply_12, // [12] D T(0,1)*T(3,5)
    &detail::tt_apply_13, // [13] D T(0,1)*T(4,5)
    &detail::tt_apply_14, // [14] O T(0,2)*T(0,1)
    &detail::tt_apply_15, // [15] O T(0,2)*T(0,3)
    &detail::tt_apply_16, // [16] O T(0,2)*T(0,4)
    &detail::tt_apply_17, // [17] O T(0,2)*T(0,5)
    &detail::tt_apply_18, // [18] O T(0,2)*T(1,2)
    &detail::tt_apply_19, // [19] D T(0,2)*T(1,3)
    &detail::tt_apply_20, // [20] D T(0,2)*T(1,4)
    &detail::tt_apply_21, // [21] D T(0,2)*T(1,5)
    &detail::tt_apply_22, // [22] O T(0,2)*T(2,3)
    &detail::tt_apply_23, // [23] O T(0,2)*T(2,4)
    &detail::tt_apply_24, // [24] O T(0,2)*T(2,5)
    &detail::tt_apply_25, // [25] D T(0,2)*T(3,4)
    &detail::tt_apply_26, // [26] D T(0,2)*T(3,5)
    &detail::tt_apply_27, // [27] D T(0,2)*T(4,5)
    &detail::tt_apply_28, // [28] O T(0,3)*T(0,1)
    &detail::tt_apply_29, // [29] O T(0,3)*T(0,2)
    &detail::tt_apply_30, // [30] O T(0,3)*T(0,4)
    &detail::tt_apply_31, // [31] O T(0,3)*T(0,5)
    &detail::tt_apply_32, // [32] D T(0,3)*T(1,2)
    &detail::tt_apply_33, // [33] O T(0,3)*T(1,3)
    &detail::tt_apply_34, // [34] D T(0,3)*T(1,4)
    &detail::tt_apply_35, // [35] D T(0,3)*T(1,5)
    &detail::tt_apply_36, // [36] O T(0,3)*T(2,3)
    &detail::tt_apply_37, // [37] D T(0,3)*T(2,4)
    &detail::tt_apply_38, // [38] D T(0,3)*T(2,5)
    &detail::tt_apply_39, // [39] O T(0,3)*T(3,4)
    &detail::tt_apply_40, // [40] O T(0,3)*T(3,5)
    &detail::tt_apply_41, // [41] D T(0,3)*T(4,5)
    &detail::tt_apply_42, // [42] O T(0,4)*T(0,1)
    &detail::tt_apply_43, // [43] O T(0,4)*T(0,2)
    &detail::tt_apply_44, // [44] O T(0,4)*T(0,3)
    &detail::tt_apply_45, // [45] O T(0,4)*T(0,5)
    &detail::tt_apply_46, // [46] D T(0,4)*T(1,2)
    &detail::tt_apply_47, // [47] D T(0,4)*T(1,3)
    &detail::tt_apply_48, // [48] O T(0,4)*T(1,4)
    &detail::tt_apply_49, // [49] D T(0,4)*T(1,5)
    &detail::tt_apply_50, // [50] D T(0,4)*T(2,3)
    &detail::tt_apply_51, // [51] O T(0,4)*T(2,4)
    &detail::tt_apply_52, // [52] D T(0,4)*T(2,5)
    &detail::tt_apply_53, // [53] O T(0,4)*T(3,4)
    &detail::tt_apply_54, // [54] D T(0,4)*T(3,5)
    &detail::tt_apply_55, // [55] O T(0,4)*T(4,5)
    &detail::tt_apply_56, // [56] O T(0,5)*T(0,1)
    &detail::tt_apply_57, // [57] O T(0,5)*T(0,2)
    &detail::tt_apply_58, // [58] O T(0,5)*T(0,3)
    &detail::tt_apply_59, // [59] O T(0,5)*T(0,4)
    &detail::tt_apply_60, // [60] D T(0,5)*T(1,2)
    &detail::tt_apply_61, // [61] D T(0,5)*T(1,3)
    &detail::tt_apply_62, // [62] D T(0,5)*T(1,4)
    &detail::tt_apply_63, // [63] O T(0,5)*T(1,5)
    &detail::tt_apply_64, // [64] D T(0,5)*T(2,3)
    &detail::tt_apply_65, // [65] D T(0,5)*T(2,4)
    &detail::tt_apply_66, // [66] O T(0,5)*T(2,5)
    &detail::tt_apply_67, // [67] D T(0,5)*T(3,4)
    &detail::tt_apply_68, // [68] O T(0,5)*T(3,5)
    &detail::tt_apply_69, // [69] O T(0,5)*T(4,5)
    &detail::tt_apply_70, // [70] O T(1,2)*T(0,1)
    &detail::tt_apply_71, // [71] O T(1,2)*T(0,2)
    &detail::tt_apply_72, // [72] O T(1,2)*T(1,3)
    &detail::tt_apply_73, // [73] O T(1,2)*T(1,4)
    &detail::tt_apply_74, // [74] O T(1,2)*T(1,5)
    &detail::tt_apply_75, // [75] O T(1,2)*T(2,3)
    &detail::tt_apply_76, // [76] O T(1,2)*T(2,4)
    &detail::tt_apply_77, // [77] O T(1,2)*T(2,5)
    &detail::tt_apply_78, // [78] D T(1,2)*T(3,4)
    &detail::tt_apply_79, // [79] D T(1,2)*T(3,5)
    &detail::tt_apply_80, // [80] D T(1,2)*T(4,5)
    &detail::tt_apply_81, // [81] O T(1,3)*T(0,1)
    &detail::tt_apply_82, // [82] O T(1,3)*T(0,3)
    &detail::tt_apply_83, // [83] O T(1,3)*T(1,2)
    &detail::tt_apply_84, // [84] O T(1,3)*T(1,4)
    &detail::tt_apply_85, // [85] O T(1,3)*T(1,5)
    &detail::tt_apply_86, // [86] O T(1,3)*T(2,3)
    &detail::tt_apply_87, // [87] D T(1,3)*T(2,4)
    &detail::tt_apply_88, // [88] D T(1,3)*T(2,5)
    &detail::tt_apply_89, // [89] O T(1,3)*T(3,4)
    &detail::tt_apply_90, // [90] O T(1,3)*T(3,5)
    &detail::tt_apply_91, // [91] D T(1,3)*T(4,5)
    &detail::tt_apply_92, // [92] O T(1,4)*T(0,1)
    &detail::tt_apply_93, // [93] O T(1,4)*T(0,4)
    &detail::tt_apply_94, // [94] O T(1,4)*T(1,2)
    &detail::tt_apply_95, // [95] O T(1,4)*T(1,3)
    &detail::tt_apply_96, // [96] O T(1,4)*T(1,5)
    &detail::tt_apply_97, // [97] D T(1,4)*T(2,3)
    &detail::tt_apply_98, // [98] O T(1,4)*T(2,4)
    &detail::tt_apply_99, // [99] D T(1,4)*T(2,5)
    &detail::tt_apply_100, // [100] O T(1,4)*T(3,4)
    &detail::tt_apply_101, // [101] D T(1,4)*T(3,5)
    &detail::tt_apply_102, // [102] O T(1,4)*T(4,5)
    &detail::tt_apply_103, // [103] O T(1,5)*T(0,1)
    &detail::tt_apply_104, // [104] O T(1,5)*T(0,5)
    &detail::tt_apply_105, // [105] O T(1,5)*T(1,2)
    &detail::tt_apply_106, // [106] O T(1,5)*T(1,3)
    &detail::tt_apply_107, // [107] O T(1,5)*T(1,4)
    &detail::tt_apply_108, // [108] D T(1,5)*T(2,3)
    &detail::tt_apply_109, // [109] D T(1,5)*T(2,4)
    &detail::tt_apply_110, // [110] O T(1,5)*T(2,5)
    &detail::tt_apply_111, // [111] D T(1,5)*T(3,4)
    &detail::tt_apply_112, // [112] O T(1,5)*T(3,5)
    &detail::tt_apply_113, // [113] O T(1,5)*T(4,5)
    &detail::tt_apply_114, // [114] O T(2,3)*T(0,2)
    &detail::tt_apply_115, // [115] O T(2,3)*T(0,3)
    &detail::tt_apply_116, // [116] O T(2,3)*T(1,2)
    &detail::tt_apply_117, // [117] O T(2,3)*T(1,3)
    &detail::tt_apply_118, // [118] O T(2,3)*T(2,4)
    &detail::tt_apply_119, // [119] O T(2,3)*T(2,5)
    &detail::tt_apply_120, // [120] O T(2,3)*T(3,4)
    &detail::tt_apply_121, // [121] O T(2,3)*T(3,5)
    &detail::tt_apply_122, // [122] D T(2,3)*T(4,5)
    &detail::tt_apply_123, // [123] O T(2,4)*T(0,2)
    &detail::tt_apply_124, // [124] O T(2,4)*T(0,4)
    &detail::tt_apply_125, // [125] O T(2,4)*T(1,2)
    &detail::tt_apply_126, // [126] O T(2,4)*T(1,4)
    &detail::tt_apply_127, // [127] O T(2,4)*T(2,3)
    &detail::tt_apply_128, // [128] O T(2,4)*T(2,5)
    &detail::tt_apply_129, // [129] O T(2,4)*T(3,4)
    &detail::tt_apply_130, // [130] D T(2,4)*T(3,5)
    &detail::tt_apply_131, // [131] O T(2,4)*T(4,5)
    &detail::tt_apply_132, // [132] O T(2,5)*T(0,2)
    &detail::tt_apply_133, // [133] O T(2,5)*T(0,5)
    &detail::tt_apply_134, // [134] O T(2,5)*T(1,2)
    &detail::tt_apply_135, // [135] O T(2,5)*T(1,5)
    &detail::tt_apply_136, // [136] O T(2,5)*T(2,3)
    &detail::tt_apply_137, // [137] O T(2,5)*T(2,4)
    &detail::tt_apply_138, // [138] D T(2,5)*T(3,4)
    &detail::tt_apply_139, // [139] O T(2,5)*T(3,5)
    &detail::tt_apply_140, // [140] O T(2,5)*T(4,5)
    &detail::tt_apply_141, // [141] O T(3,4)*T(0,3)
    &detail::tt_apply_142, // [142] O T(3,4)*T(0,4)
    &detail::tt_apply_143, // [143] O T(3,4)*T(1,3)
    &detail::tt_apply_144, // [144] O T(3,4)*T(1,4)
    &detail::tt_apply_145, // [145] O T(3,4)*T(2,3)
    &detail::tt_apply_146, // [146] O T(3,4)*T(2,4)
    &detail::tt_apply_147, // [147] O T(3,4)*T(3,5)
    &detail::tt_apply_148, // [148] O T(3,4)*T(4,5)
    &detail::tt_apply_149, // [149] O T(3,5)*T(0,3)
    &detail::tt_apply_150, // [150] O T(3,5)*T(0,5)
    &detail::tt_apply_151, // [151] O T(3,5)*T(1,3)
    &detail::tt_apply_152, // [152] O T(3,5)*T(1,5)
    &detail::tt_apply_153, // [153] O T(3,5)*T(2,3)
    &detail::tt_apply_154, // [154] O T(3,5)*T(2,5)
    &detail::tt_apply_155, // [155] O T(3,5)*T(3,4)
    &detail::tt_apply_156, // [156] O T(3,5)*T(4,5)
    &detail::tt_apply_157, // [157] O T(4,5)*T(0,4)
    &detail::tt_apply_158, // [158] O T(4,5)*T(0,5)
    &detail::tt_apply_159, // [159] O T(4,5)*T(1,4)
    &detail::tt_apply_160, // [160] O T(4,5)*T(1,5)
    &detail::tt_apply_161, // [161] O T(4,5)*T(2,4)
    &detail::tt_apply_162, // [162] O T(4,5)*T(2,5)
    &detail::tt_apply_163, // [163] O T(4,5)*T(3,4)
    &detail::tt_apply_164 // [164] O T(4,5)*T(3,5)
}};

class TT_OperatorRuntime {
public:
    explicit constexpr TT_OperatorRuntime(uint8_t alpha_idx)
        : alpha_idx_(alpha_idx) {}

    uint8_t t_first()  const { return TT_ALPHABET[alpha_idx_].t_first; }
    uint8_t t_second() const { return TT_ALPHABET[alpha_idx_].t_second; }

    inline SO6 apply(const SO6& S) const {
        SO6 result = S;
        TT_APPLY_TABLE[alpha_idx_](result);
        result.set_element(0, 0, result.get_element(0, 0));
        result.canonical_reset();
        result.last_T = TT_ALPHABET[alpha_idx_].t_second;
        return result;
    }

private:
    uint8_t alpha_idx_;
};

inline SO6 operator*(const TT_OperatorRuntime& op, const SO6& rhs) {
    return op.apply(rhs);
}
