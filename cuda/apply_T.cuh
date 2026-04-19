// apply_T.cuh — T-gate batch kernel.
//
// Each T_k acts on two rows (r1, r2) of an SO6 matrix:
//     new_r1[c] = (old_r1[c] + old_r2[c]) / sqrt(2)
//     new_r2[c] = (old_r1[c] - old_r2[c]) / sqrt(2)
// All other rows are copied unchanged.  See core/so6_array.apply_T.
//
// Launch: one thread per output element — grid = (ceil(N / TPB), 36), block = (TPB, 1).

#pragma once
#include "z2_device.cuh"

// row pairs for T_0..T_14 (matches core/so6_array.py::T_PAIRS).
__device__ __constant__ int8_t T_ROWS[15][2] = {
    {0,1},{0,2},{0,3},{0,4},{0,5},
    {1,2},{1,3},{1,4},{1,5},
    {2,3},{2,4},{2,5},
    {3,4},{3,5},
    {4,5},
};

// Apply T_t to every input matrix, writing into `out`.
//   in  : (N, 6, 6) packed z2_t, row-major contiguous
//   out : (N, 6, 6) packed z2_t
// One thread per (matrix, output element index e in 0..35).
__global__ void apply_T_kernel(const z2_t* __restrict__ in,
                               z2_t* __restrict__ out,
                               int N,
                               int t_idx)
{
    int n = blockIdx.x * blockDim.x + threadIdx.x;
    int e = blockIdx.y;                // 0..35
    if (n >= N) return;

    int r1 = T_ROWS[t_idx][0];
    int r2 = T_ROWS[t_idx][1];

    int i = e / 6;
    int j = e % 6;

    const z2_t* M = in  + (size_t)n * 36;
    z2_t*       O = out + (size_t)n * 36;

    if (i == r1) {
        // (old_r1[j] + old_r2[j]) / sqrt2, reduced
        z2_t a = M[r1 * 6 + j];
        z2_t b = M[r2 * 6 + j];
        z2_t s = z2_add_packed(a, b);
        s = z2_div_sqrt2(s);
        s = z2_reduce_packed(s);
        O[i * 6 + j] = s;
    } else if (i == r2) {
        z2_t a = M[r1 * 6 + j];
        z2_t b = M[r2 * 6 + j];
        z2_t s = z2_sub_packed(a, b);
        s = z2_div_sqrt2(s);
        s = z2_reduce_packed(s);
        O[i * 6 + j] = s;
    } else {
        O[i * 6 + j] = M[i * 6 + j];
    }
}

// Gather-and-apply variant:  input selects rows via `src_idx` (length K) from
// `in`, applies T_t, and writes results packed contiguously in `out` (K, 6, 6).
// Used after filtering `cur_mats` with last_T != t mask.
__global__ void apply_T_gather_kernel(const z2_t* __restrict__ in,
                                      const int*  __restrict__ src_idx,
                                      z2_t* __restrict__ out,
                                      int K,
                                      int t_idx)
{
    int n = blockIdx.x * blockDim.x + threadIdx.x;
    int e = blockIdx.y;
    if (n >= K) return;
    int src = src_idx[n];

    int r1 = T_ROWS[t_idx][0];
    int r2 = T_ROWS[t_idx][1];
    int i = e / 6;
    int j = e % 6;

    const z2_t* M = in  + (size_t)src * 36;
    z2_t*       O = out + (size_t)n   * 36;

    if (i == r1) {
        z2_t a = M[r1 * 6 + j];
        z2_t b = M[r2 * 6 + j];
        z2_t s = z2_add_packed(a, b);
        s = z2_div_sqrt2(s);
        s = z2_reduce_packed(s);
        O[i * 6 + j] = s;
    } else if (i == r2) {
        z2_t a = M[r1 * 6 + j];
        z2_t b = M[r2 * 6 + j];
        z2_t s = z2_sub_packed(a, b);
        s = z2_div_sqrt2(s);
        s = z2_reduce_packed(s);
        O[i * 6 + j] = s;
    } else {
        O[i * 6 + j] = M[i * 6 + j];
    }
}
