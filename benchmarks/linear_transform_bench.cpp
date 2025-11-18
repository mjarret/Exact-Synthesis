#include <benchmark/benchmark.h>
#include <cstdint>
#include <array>
#include <random>

#include "so6/T_Operator.hpp"
#include "ds/LinearTransform.hpp"

// ------------------- Helpers: deterministic test matrices -------------------

static inline uint32_t xorshift32(uint32_t& x) {
    x ^= x << 13;
    x ^= x >> 17;
    x ^= x << 5;
    return x;
}

static SO6 MakeTestSO6(uint32_t seed) {
    SO6 S;
    uint32_t x = seed ? seed : 1u;
    for (uint8_t r = 0; r < 6; ++r) {
        for (uint8_t c = 0; c < 6; ++c) {
            int v = static_cast<int>(xorshift32(x) % 7u) - 3; // values in [-3,3]
            if (v == 0) v = 1;                                // avoid too many zeros
            Z2 z(static_cast<uint8_t>(v), static_cast<uint8_t>(0), static_cast<uint8_t>(0));
            S.set_element(r, c, z);
        }
    }
    S.canonical_form();
    return S;
}

// One canonicalized origin matrix per (Row1, Row2). All variants copy from this.
template<int Row1, int Row2>
static const SO6& OrigForPair() {
    static SO6 s = MakeTestSO6(
        0xA5A5A5A5u
        ^ (static_cast<uint32_t>(Row1) * 0x9E3779B9u)
        ^ (static_cast<uint32_t>(Row2) * 0x85EBCA6Bu));
    return s;
}

// Optional: correctness check (disabled by default)
// #define VERIFY_EQUAL_RESULTS 1
#ifdef VERIFY_EQUAL_RESULTS
template<int Row1, int Row2>
static void VerifyOne() {
    SO6 base = OrigForPair<Row1, Row2>();

    SO6 a = base, b = base, c = base, d = base, e = base, f = base;

    // (1) your operator
    T_Operator<Row1, Row2>::apply_inplace(a);

    // (2) abstraction compile-time T
    apply_inplace_T<Row1, Row2>(b);

    // (3) runtime typed RowPairT
    apply_inplace_row_kernel(c, RowPairTKernel{static_cast<uint8_t>(Row1), static_cast<uint8_t>(Row2)});

    // (4) runtime typed RowPairLinear equivalent to T
    RowPairLinear linT = make_T_rowpair(static_cast<uint8_t>(Row1), static_cast<uint8_t>(Row2));
    apply_inplace_row_kernel(d, linT);

    // (5) AnyKernel
    AnyKernel anyT = make_any(RowPairTKernel{static_cast<uint8_t>(Row1), static_cast<uint8_t>(Row2)});
    apply_inplace_any_kernel(e, anyT);

    // (6) Pipeline with one op
    Pipeline<4> P;
    P.push(anyT);
    P.apply_inplace(f);

    auto elems_equal = [&](const SO6& L, const SO6& R) {
        for (uint8_t rr = 0; rr < 6; ++rr)
            for (uint8_t cc = 0; cc < 6; ++cc)
                if (!(L.get_element(rr, cc) == R.get_element(rr, cc))) return false;
        return true;
    };

    if (!elems_equal(b, a) || !elems_equal(c, a) || !elems_equal(d, a)
     || !elems_equal(e, a) || !elems_equal(f, a)) {
        fprintf(stderr, "Verification failed for pair (%d,%d)\n", Row1, Row2);
        std::abort();
    }
}
#endif

// ---------------------- Bench templates (same input per pair) ----------------------

template<int Row1, int Row2>
static void BM_TOperator_CT(benchmark::State& state) {
#ifdef VERIFY_EQUAL_RESULTS
    static bool verified = (VerifyOne<Row1, Row2>(), true);
    (void)verified;
#endif
    const SO6& orig = OrigForPair<Row1, Row2>();
    for (auto _ : state) {
        SO6 S = orig;
        benchmark::DoNotOptimize(S);
        T_Operator<Row1, Row2>::apply_inplace(S);
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(S.hash);
        benchmark::DoNotOptimize(S.col_hash);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 36); // 6x6
}

template<int Row1, int Row2>
static void BM_Abstraction_TKernelCT(benchmark::State& state) {
    const SO6& orig = OrigForPair<Row1, Row2>();
    for (auto _ : state) {
        SO6 S = orig;
        benchmark::DoNotOptimize(S);
        apply_inplace_T<Row1, Row2>(S);
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(S.hash);
        benchmark::DoNotOptimize(S.col_hash);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 36);
}

template<int Row1, int Row2>
static void BM_Abstraction_RowPairT_Typed(benchmark::State& state) {
    const SO6& orig = OrigForPair<Row1, Row2>();
    RowPairTKernel k{static_cast<uint8_t>(Row1), static_cast<uint8_t>(Row2)};
    for (auto _ : state) {
        SO6 S = orig;
        benchmark::DoNotOptimize(S);
        apply_inplace_row_kernel(S, k);
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(S.hash);
        benchmark::DoNotOptimize(S.col_hash);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 36);
}

template<int Row1, int Row2>
static void BM_Abstraction_RowPairLinear_Typed(benchmark::State& state) {
    const SO6& orig = OrigForPair<Row1, Row2>();
    RowPairLinear lin = make_T_rowpair(static_cast<uint8_t>(Row1), static_cast<uint8_t>(Row2));
    for (auto _ : state) {
        SO6 S = orig;
        benchmark::DoNotOptimize(S);
        apply_inplace_row_kernel(S, lin);
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(S.hash);
        benchmark::DoNotOptimize(S.col_hash);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 36);
}

template<int Row1, int Row2>
static void BM_Abstraction_AnyKernel(benchmark::State& state) {
    const SO6& orig = OrigForPair<Row1, Row2>();
    AnyKernel anyT = make_any(RowPairTKernel{static_cast<uint8_t>(Row1), static_cast<uint8_t>(Row2)});
    for (auto _ : state) {
        SO6 S = orig;
        benchmark::DoNotOptimize(S);
        apply_inplace_any_kernel(S, anyT);
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(S.hash);
        benchmark::DoNotOptimize(S.col_hash);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 36);
}

template<int Row1, int Row2>
static void BM_Abstraction_Pipeline1(benchmark::State& state) {
    const SO6& orig = OrigForPair<Row1, Row2>();
    Pipeline<4> P;
    P.push(make_any(RowPairTKernel{static_cast<uint8_t>(Row1), static_cast<uint8_t>(Row2)}));
    for (auto _ : state) {
        SO6 S = orig;
        benchmark::DoNotOptimize(S);
        P.apply_inplace(S);
        benchmark::ClobberMemory();
        benchmark::DoNotOptimize(S.hash);
        benchmark::DoNotOptimize(S.col_hash);
    }
    state.SetItemsProcessed(static_cast<int64_t>(state.iterations()) * 36);
}

template<int N>
static void BM_Fused(benchmark::State& st) {
  const SO6& orig = OrigForPair<0,1>();
  Pipeline<8> P;
  for (int i=0; i<N; ++i) P.push(make_any(RowPairTKernel{0,1}));
  for (auto _ : st) {
    SO6 S = orig;
    benchmark::DoNotOptimize(S);
    P.apply_inplace(S);
    benchmark::ClobberMemory();
  }
}

template<int N>
static void BM_Sequential(benchmark::State& st) {
  const SO6& orig = OrigForPair<0,1>();
  for (auto _ : st) {
    SO6 S = orig;
    benchmark::DoNotOptimize(S);
    for (int i=0; i<N; ++i) apply_inplace_T<0,1>(S);
    benchmark::ClobberMemory();
  }
}
BENCHMARK_TEMPLATE(BM_Fused, 1);
BENCHMARK_TEMPLATE(BM_Fused, 2);
BENCHMARK_TEMPLATE(BM_Fused, 4);
BENCHMARK_TEMPLATE(BM_Fused, 8);
BENCHMARK_TEMPLATE(BM_Sequential, 1);
BENCHMARK_TEMPLATE(BM_Sequential, 2);
BENCHMARK_TEMPLATE(BM_Sequential, 4);
BENCHMARK_TEMPLATE(BM_Sequential, 8);

// -------- Fused vs. equivalent matrix multiply (row pair 0,1) --------

template<int N>
static const SO6& OpMatForPair01() {
    static SO6 op = [](){
        SO6 m = SO6::identity();
        for (int i = 0; i < N; ++i) {
            apply_inplace_T<0,1>(m);
        }
        return m;
    }();
    return op;
}

template<int N>
static const Pipeline<16>& FusedPipeline01() {
    static Pipeline<16> P = []{
        Pipeline<16> p;
        for (int i = 0; i < N; ++i) p.push(make_any(RowPairTKernel{0,1}));
        return p;
    }();
    return P;
}

template<int N>
static void VerifyFusedVsMatmul() {
    const SO6& orig = OrigForPair<0,1>();
    SO6 fused = orig;
    SO6 matmul_target = orig;

    FusedPipeline01<N>().apply_inplace(fused);
    matmul_target = OpMatForPair01<N>() * matmul_target;
    matmul_target.canonical_form();
    matmul_target.recompute_hash();

    auto equal = [&](const SO6& a, const SO6& b) {
        for (uint8_t r = 0; r < 6; ++r)
            for (uint8_t c = 0; c < 6; ++c)
                if (!(a.get_element(r, c) == b.get_element(r, c))) return false;
        return true;
    };
    if (!equal(fused, matmul_target)) {
        fprintf(stderr, "Fused vs matmul mismatch for N=%d\n", N);
        std::abort();
    }
}

template<int N>
static void BM_Fused_vs_Matmul(benchmark::State& st) {
    static bool verified = (VerifyFusedVsMatmul<N>(), true);
    (void)verified;
    const SO6& orig = OrigForPair<0,1>();
    const auto& P = FusedPipeline01<N>();
    const SO6& op = OpMatForPair01<N>();
    for (auto _ : st) {
        SO6 fused = orig;
        SO6 mul   = orig;

        benchmark::DoNotOptimize(fused);
        P.apply_inplace(fused);
        benchmark::ClobberMemory();

        benchmark::DoNotOptimize(mul);
        mul = op * mul;
        mul.canonical_form();
        mul.recompute_hash();
        benchmark::ClobberMemory();
    }
    st.SetItemsProcessed(static_cast<int64_t>(st.iterations()) * 36);
}

BENCHMARK_TEMPLATE(BM_Fused_vs_Matmul, 1);
BENCHMARK_TEMPLATE(BM_Fused_vs_Matmul, 2);
BENCHMARK_TEMPLATE(BM_Fused_vs_Matmul, 4);
BENCHMARK_TEMPLATE(BM_Fused_vs_Matmul, 8);

// ---------------------- Instantiate for all 15 row pairs ----------------------

#define INSTANTIATE_PAIR(R1, R2) \
    BENCHMARK_TEMPLATE(BM_TOperator_CT,                R1, R2); \
    BENCHMARK_TEMPLATE(BM_Abstraction_TKernelCT,       R1, R2); \
    BENCHMARK_TEMPLATE(BM_Abstraction_RowPairT_Typed,  R1, R2); \
    BENCHMARK_TEMPLATE(BM_Abstraction_RowPairLinear_Typed, R1, R2); \
    BENCHMARK_TEMPLATE(BM_Abstraction_AnyKernel,       R1, R2); \
    BENCHMARK_TEMPLATE(BM_Abstraction_Pipeline1,       R1, R2);

INSTANTIATE_PAIR(0,1)  INSTANTIATE_PAIR(0,2)  INSTANTIATE_PAIR(0,3)
INSTANTIATE_PAIR(0,4)  INSTANTIATE_PAIR(0,5)  INSTANTIATE_PAIR(1,2)
INSTANTIATE_PAIR(1,3)  INSTANTIATE_PAIR(1,4)  INSTANTIATE_PAIR(1,5)
INSTANTIATE_PAIR(2,3)  INSTANTIATE_PAIR(2,4)  INSTANTIATE_PAIR(2,5)
INSTANTIATE_PAIR(3,4)  INSTANTIATE_PAIR(3,5)  INSTANTIATE_PAIR(4,5)

BENCHMARK_MAIN();
