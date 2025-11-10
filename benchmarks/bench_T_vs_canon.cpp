// Benchmark: compare raw left_multiply_by_T cost vs canonical_form
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <array>
#include <random>
#include <string>

#include "so6/SO6.hpp"

using clock_type = std::chrono::steady_clock;
using dur_ms = std::chrono::duration<double, std::milli>;

// A "raw" T-move that mirrors SO6::left_multiply_by_T<i> but skips canonical_form.
// Keeps signatures consistent by recomputing scan-based signatures; omits canonicalization.
template<int i>
static inline void left_multiply_by_T_raw(SO6 &S) {
    static_assert(i >= 0 && i < 15, "left_multiply_by_T_raw: i out of range");
    static constexpr std::array<std::pair<int,int>, 15> pairs{{
        {0,1},{0,2},{0,3},{0,4},{0,5},
        {1,2},{1,3},{1,4},{1,5},
        {2,3},{2,4},{2,5},
        {3,4},{3,5},
        {4,5}
    }};
    constexpr int row1 = pairs[i].first;
    constexpr int row2 = pairs[i].second;

    size_t row_freq = SO6::row_frequency_signature(S, row1) + SO6::row_frequency_signature(S, row2);
    S.hash -= static_cast<uint16_t>(row_freq);

    for (int col = 0; col < 6; col++) {
        size_t col_freq = SO6::col_frequency_signature(S, col);
        size_t col_sig = col_freq ^ (col_freq >> 1);
        S.hash     -= static_cast<uint16_t>(col_sig);
        S.col_hash -= static_cast<uint16_t>(col_sig);

        Z2 a = S.get_element(static_cast<uint8_t>(row1), static_cast<uint8_t>(col));
        Z2 b = S.get_element(static_cast<uint8_t>(row2), static_cast<uint8_t>(col));
        const Z2 a_old = a;
        const Z2 b_old = b;
        // No stored frequency maps in simplified policy

        // Update elements
        a += b_old;
        b -= a_old;
        a.denom_exp += (a.int_c != 0);
        b.denom_exp += (b.int_c != 0);

        const Z2 a_abs = std::abs(a);
        const Z2 b_abs = std::abs(b);

        // No stored frequency maps in simplified policy

        S.set_element(static_cast<uint8_t>(row1), static_cast<uint8_t>(col), a);
        S.set_element(static_cast<uint8_t>(row2), static_cast<uint8_t>(col), b);

        col_freq = SO6::col_frequency_signature(S, col);
        col_sig = col_freq ^ (col_freq >> 1);
        S.hash     += static_cast<uint16_t>(col_sig);
        S.col_hash += static_cast<uint16_t>(col_sig);
    }

    // Skip canonical_form(); only finalize hash and bookkeeping
    row_freq = SO6::row_frequency_signature(S, row1) + SO6::row_frequency_signature(S, row2);
    S.hash = static_cast<uint16_t>(S.hash + static_cast<uint16_t>(row_freq));
    S.last_T = static_cast<unsigned char>(i);
}

static inline void left_multiply_by_T_raw_rt(SO6& S, uint8_t idx) {
    switch (idx % 15) {
        case 0:  left_multiply_by_T_raw<0>(S);  break;
        case 1:  left_multiply_by_T_raw<1>(S);  break;
        case 2:  left_multiply_by_T_raw<2>(S);  break;
        case 3:  left_multiply_by_T_raw<3>(S);  break;
        case 4:  left_multiply_by_T_raw<4>(S);  break;
        case 5:  left_multiply_by_T_raw<5>(S);  break;
        case 6:  left_multiply_by_T_raw<6>(S);  break;
        case 7:  left_multiply_by_T_raw<7>(S);  break;
        case 8:  left_multiply_by_T_raw<8>(S);  break;
        case 9:  left_multiply_by_T_raw<9>(S);  break;
        case 10: left_multiply_by_T_raw<10>(S); break;
        case 11: left_multiply_by_T_raw<11>(S); break;
        case 12: left_multiply_by_T_raw<12>(S); break;
        case 13: left_multiply_by_T_raw<13>(S); break;
        case 14: left_multiply_by_T_raw<14>(S); break;
    }
}

// A "core-only" T-move: apply the row-pair update without any hash/signature
// bookkeeping or canonicalization. This isolates pure arithmetic/memory cost.
template<int i>
static inline void left_multiply_by_T_core(SO6 &S) {
    static_assert(i >= 0 && i < 15, "left_multiply_by_T_core: i out of range");
    static constexpr std::array<std::pair<int,int>, 15> pairs{{
        {0,1},{0,2},{0,3},{0,4},{0,5},
        {1,2},{1,3},{1,4},{1,5},
        {2,3},{2,4},{2,5},
        {3,4},{3,5},
        {4,5}
    }};
    constexpr int row1 = pairs[i].first;
    constexpr int row2 = pairs[i].second;
    for (int col = 0; col < 6; ++col) {
        Z2 a = S.get_element(static_cast<uint8_t>(row1), static_cast<uint8_t>(col));
        Z2 b = S.get_element(static_cast<uint8_t>(row2), static_cast<uint8_t>(col));
        const Z2 a_old = a;
        const Z2 b_old = b;
        a += b_old;
        b -= a_old;
        a.denom_exp += (a.int_c != 0);
        b.denom_exp += (b.int_c != 0);
        S.set_element(static_cast<uint8_t>(row1), static_cast<uint8_t>(col), a);
        S.set_element(static_cast<uint8_t>(row2), static_cast<uint8_t>(col), b);
    }
}

static inline void left_multiply_by_T_core_rt(SO6& S, uint8_t idx) {
    switch (idx % 15) {
        case 0:  left_multiply_by_T_core<0>(S);  break;
        case 1:  left_multiply_by_T_core<1>(S);  break;
        case 2:  left_multiply_by_T_core<2>(S);  break;
        case 3:  left_multiply_by_T_core<3>(S);  break;
        case 4:  left_multiply_by_T_core<4>(S);  break;
        case 5:  left_multiply_by_T_core<5>(S);  break;
        case 6:  left_multiply_by_T_core<6>(S);  break;
        case 7:  left_multiply_by_T_core<7>(S);  break;
        case 8:  left_multiply_by_T_core<8>(S);  break;
        case 9:  left_multiply_by_T_core<9>(S);  break;
        case 10: left_multiply_by_T_core<10>(S); break;
        case 11: left_multiply_by_T_core<11>(S); break;
        case 12: left_multiply_by_T_core<12>(S); break;
        case 13: left_multiply_by_T_core<13>(S); break;
        case 14: left_multiply_by_T_core<14>(S); break;
    }
}

int main(int argc, char** argv) {
    // Simple parameterization via env/args could be added; fixed counts for now
    const int warmup = 1000;
    const int iters_t = 200000;     // number of raw T operations
    const int iters_canon = 20000;  // number of canonical_form calls

    // Initialize matrix (identity has frequencies/signatures ready)
    SO6 S = SO6::identity();

    // Warm up: mutate S a bit
    for (int i = 0; i < warmup; ++i) left_multiply_by_T_raw_rt(S, static_cast<uint8_t>(i));

    volatile uint64_t sink = 0; // prevent dead-code elimination

    // Benchmark raw T operations
    auto t0 = clock_type::now();
    for (int i = 0; i < iters_t; ++i) {
        left_multiply_by_T_raw_rt(S, static_cast<uint8_t>(i));
        sink += S.arr24_[0];
    }
    auto t1 = clock_type::now();
    double ms_t = std::chrono::duration_cast<dur_ms>(t1 - t0).count();

    // Benchmark core-only T operations (no hashes/signatures)
    auto k0 = clock_type::now();
    for (int i = 0; i < iters_t; ++i) {
        left_multiply_by_T_core_rt(S, static_cast<uint8_t>(i));
        sink += S.arr24_[1];
    }
    auto k1 = clock_type::now();
    double ms_core = std::chrono::duration_cast<dur_ms>(k1 - k0).count();

    // Benchmark canonicalization alone on current S state
    auto c0 = clock_type::now();
    for (int i = 0; i < iters_canon; ++i) {
        S.canonical_form();
        sink += S.arr24_[2];
    }
    auto c1 = clock_type::now();
    double ms_c = std::chrono::duration_cast<dur_ms>(c1 - c0).count();

    // Report
    std::printf("Benchmark results (sink=%llu)\n", static_cast<unsigned long long>(sink));
    std::printf("- Raw left_multiply_by_T:   %d iters in %.3f ms (%.3f us/op)\n",
                iters_t, ms_t, (ms_t * 1000.0) / iters_t);
    std::printf("- Core-only T operation:  %d iters in %.3f ms (%.3f us/op)\n",
                iters_t, ms_core, (ms_core * 1000.0) / iters_t);
    std::printf("- canonical_form():       %d iters in %.3f ms (%.3f us/op)\n",
                iters_canon, ms_c, (ms_c * 1000.0) / iters_canon);

    return 0;
}
