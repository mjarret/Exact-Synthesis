// z2_device.cuh — device-side Z2 = Z[1/sqrt(2)] arithmetic for packed int64.
//
// Packing layout (mirrors core/z2_ops.py with Z2_PACK_DTYPE="int64"):
//   bits [15:0]  : int_c     (int16, signed)
//   bits [31:16] : sqrt2_c   (int16, signed)
//   bits [47:32] : denom_exp (uint16)
//   bits [63:48] : reserved  (0)
//
// A packed value represents the Z2 number (int_c + sqrt2_c*sqrt(2)) / sqrt(2)^denom_exp.

#pragma once
#include <cstdint>
#include <cuda_runtime.h>

using z2_t = int64_t;

__device__ __forceinline__ void z2_unpack(z2_t p, int32_t& ic, int32_t& sc, int32_t& dc) {
    int32_t raw_i = (int32_t)(p & 0xFFFF);
    if (raw_i >= 0x8000) raw_i -= 0x10000;
    int32_t raw_s = (int32_t)((p >> 16) & 0xFFFF);
    if (raw_s >= 0x8000) raw_s -= 0x10000;
    ic = raw_i;
    sc = raw_s;
    dc = (int32_t)((p >> 32) & 0xFFFF);
}

__device__ __forceinline__ z2_t z2_pack(int32_t ic, int32_t sc, int32_t dc) {
    return ((z2_t)(ic & 0xFFFF))
         | (((z2_t)(sc & 0xFFFF)) << 16)
         | (((z2_t)(dc & 0xFFFF)) << 32);
}

// Multiplication: (a_i + a_s sqrt2)(b_i + b_s sqrt2) = (a_i b_i + 2 a_s b_s) + (a_i b_s + a_s b_i) sqrt2.
__device__ __forceinline__ void z2_mul(
        int32_t ai, int32_t as, int32_t ad,
        int32_t bi, int32_t bs, int32_t bd,
        int32_t& ri, int32_t& rs, int32_t& rd)
{
    ri = ai * bi + 2 * as * bs;
    rs = ai * bs + as * bi;
    rd = ad + bd;
    if (ri == 0 && rs == 0) rd = 0;
}

// Addition of two Z2 triplets.  Handles the five (sign, parity) cases on
// diff = ad - bd following core/z2_ops.py::z2_add.
__device__ __forceinline__ void z2_add(
        int32_t ai, int32_t as_, int32_t ad,
        int32_t bi, int32_t bs, int32_t bd,
        int32_t& ri, int32_t& rs, int32_t& rd)
{
    int32_t diff = ad - bd;
    if (diff == 0) {
        ri = ai + bi;
        rs = as_ + bs;
        rd = ad;
    } else if (diff > 0) {
        if ((diff & 1) == 0) {
            int32_t k = diff >> 1;
            int32_t f = 1 << k;
            ri = ai + bi * f;
            rs = as_ + bs * f;
            rd = ad;
        } else {
            int32_t k_lo = (diff - 1) >> 1;
            int32_t k_hi = (diff + 1) >> 1;
            int32_t f_lo = 1 << k_lo;
            int32_t f_hi = 1 << k_hi;
            ri = ai + bs * f_hi;
            rs = as_ + bi * f_lo;
            rd = ad;
        }
    } else {
        int32_t ad_ = -diff;
        if ((ad_ & 1) == 0) {
            int32_t k = ad_ >> 1;
            int32_t f = 1 << k;
            ri = ai * f + bi;
            rs = as_ * f + bs;
            rd = bd;
        } else {
            int32_t k_lo = (ad_ - 1) >> 1;
            int32_t k_hi = (ad_ + 1) >> 1;
            int32_t f_lo = 1 << k_lo;
            int32_t f_hi = 1 << k_hi;
            ri = as_ * f_hi + bi;
            rs = ai  * f_lo + bs;
            rd = bd;
        }
    }
    if (ri == 0 && rs == 0) rd = 0;
}

__device__ __forceinline__ void z2_sub(
        int32_t ai, int32_t as_, int32_t ad,
        int32_t bi, int32_t bs, int32_t bd,
        int32_t& ri, int32_t& rs, int32_t& rd)
{
    int32_t nbi = -bi, nbs = -bs;
    if (bi == 0 && bs == 0) { nbi = 0; nbs = 0; }
    z2_add(ai, as_, ad, nbi, nbs, bd, ri, rs, rd);
}

// Count trailing zeros of |x|, treating x == 0 as returning 32.
__device__ __forceinline__ int32_t z2_trailing(int32_t x) {
    if (x == 0) return 32;
    uint32_t ax = (uint32_t)(x < 0 ? -x : x);
    return __ffs((int)ax) - 1;
}

// Cancel common sqrt(2) factors so int_c becomes odd (or zero).
__device__ __forceinline__ void z2_reduce(int32_t& ic, int32_t& sc, int32_t& dc) {
    if (ic == 0 && sc == 0) { ic = 0; sc = 0; dc = 0; return; }
    if ((ic & 1) != 0) return;
    int32_t int_tz  = z2_trailing(ic);
    int32_t sqrt_tz = z2_trailing(sc);
    int32_t int_exp  = 2 * int_tz;
    int32_t sqrt_exp = 2 * sqrt_tz + 1;
    int32_t min_exp;
    if (ic == 0)       min_exp = sqrt_exp;
    else if (sc == 0)  min_exp = int_exp;
    else               min_exp = (int_exp < sqrt_exp) ? int_exp : sqrt_exp;
    if (min_exp == 0 || min_exp > dc) return;
    if (min_exp > dc) min_exp = dc;   // defensive; loop already bounded above
    if ((min_exp & 1) == 0) {
        int32_t sh = min_exp >> 1;
        ic >>= sh;
        sc >>= sh;
        dc -= min_exp;
    } else {
        int32_t k  = (min_exp - 1) >> 1;
        int32_t hi = (min_exp + 1) >> 1;
        int32_t new_ic = sc >> k;
        int32_t new_sc = ic >> hi;
        ic = new_ic;
        sc = new_sc;
        dc -= min_exp;
    }
}

// Convenience wrappers on packed values.
__device__ __forceinline__ z2_t z2_add_packed(z2_t a, z2_t b) {
    int32_t ai, as_, ad, bi, bs, bd, ri, rs, rd;
    z2_unpack(a, ai, as_, ad);
    z2_unpack(b, bi, bs, bd);
    z2_add(ai, as_, ad, bi, bs, bd, ri, rs, rd);
    return z2_pack(ri, rs, rd);
}
__device__ __forceinline__ z2_t z2_sub_packed(z2_t a, z2_t b) {
    int32_t ai, as_, ad, bi, bs, bd, ri, rs, rd;
    z2_unpack(a, ai, as_, ad);
    z2_unpack(b, bi, bs, bd);
    z2_sub(ai, as_, ad, bi, bs, bd, ri, rs, rd);
    return z2_pack(ri, rs, rd);
}

// Divide by sqrt(2): bump denom_exp by 1 unless the value is zero.
__device__ __forceinline__ z2_t z2_div_sqrt2(z2_t p) {
    int32_t ic, sc, dc;
    z2_unpack(p, ic, sc, dc);
    if (ic == 0 && sc == 0) return z2_pack(0, 0, 0);
    return z2_pack(ic, sc, dc + 1);
}

__device__ __forceinline__ z2_t z2_reduce_packed(z2_t p) {
    int32_t ic, sc, dc;
    z2_unpack(p, ic, sc, dc);
    z2_reduce(ic, sc, dc);
    return z2_pack(ic, sc, dc);
}

// Convert to float64 for canonical sorting (matches python to_float).
__device__ __forceinline__ double z2_to_double(z2_t p) {
    int32_t ic, sc, dc;
    z2_unpack(p, ic, sc, dc);
    double v = (double)ic + (double)sc * 1.4142135623730951;
    // Divide by 2^(dc/2) = (sqrt(2))^dc
    int32_t half = dc >> 1;
    if (half > 0) v = ldexp(v, -half);
    if (dc & 1) v *= 0.7071067811865476;    // /sqrt(2)
    return v;
}
