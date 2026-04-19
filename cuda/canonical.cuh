// canonical.cuh — canonical-form hash kernel for SO6 batch.
//
// Algorithm per matrix (mirrors core/canonical_ops._canonical_core_nb_v2):
//
//   1.  Compute float64 representation v[6][6] and abs_v.
//   2.  Group rows into equivalence classes by sorted-abs-row signature.
//   3.  Group columns into equivalence classes by sorted-abs-col signature.
//   4.  Enumerate all within-class row permutations (N_rp total) and all 32
//       row-sign combinations.  For each combo:
//         a. Apply row perm + signs (both to float and to packed int triplets).
//         b. Column-sign normalisation: for each column, if the first nonzero
//            float entry is negative, flip the column's signs.
//         c. Sort columns inside each col-class by lex order on float values
//            (insertion sort).
//         d. Final column-sign normalisation based on int_c (flip col if its
//            first nonzero int_c is negative).
//         e. Lexicographically compare the 36-element float vector against the
//            running best; keep the candidate's (ic, sc, dc) triplets if better.
//   5.  Hash 108 bytes (6x6x3 int32 triplet) with FNV-1a → int64.
//
// One thread per matrix.  The number of combos per matrix is bounded by
// N_rp * 32 with N_rp ≤ 720; typical matrices at depth <= 10 have N_rp ≤ 24.

#pragma once
#include <cstdint>
#include <cmath>
#include "z2_device.cuh"

#define CANON_EPS 1e-12

// FNV-1a 64-bit over 108-byte canonical triplet.
__device__ __forceinline__ uint64_t fnv1a64(const int32_t* bytes108_as_ints, int n_ints) {
    uint64_t h = 0xcbf29ce484222325ULL;
    const uint64_t p = 0x100000001b3ULL;
    const uint8_t* b = reinterpret_cast<const uint8_t*>(bytes108_as_ints);
    for (int i = 0; i < n_ints * 4; ++i) {
        h ^= (uint64_t)b[i];
        h *= p;
    }
    return h;
}

// Sort 6 doubles ascending (insertion sort).
__device__ __forceinline__ void sort6_double(double* a) {
    for (int i = 1; i < 6; ++i) {
        double k = a[i];
        int j = i - 1;
        while (j >= 0 && a[j] > k) { a[j + 1] = a[j]; --j; }
        a[j + 1] = k;
    }
}

// Compare two 6-tuples of doubles lexicographically.  Returns -1/0/1.
__device__ __forceinline__ int cmp6_double(const double* a, const double* b) {
    for (int i = 0; i < 6; ++i) {
        double d = a[i] - b[i];
        if (d < -CANON_EPS) return -1;
        if (d >  CANON_EPS) return  1;
    }
    return 0;
}

// Lex compare two (36) float vectors.  Returns <0, 0, >0.
__device__ __forceinline__ int cmp36_double(const double* a, const double* b) {
    for (int i = 0; i < 36; ++i) {
        double d = a[i] - b[i];
        if (d < -CANON_EPS) return -1;
        if (d >  CANON_EPS) return  1;
    }
    return 0;
}

// Build row equivalence classes by sorted-abs-row signature.
//   row_sorted_abs : (6, 6) float64 (each row sorted)
// Output: group assignment per row (0..k-1), and class ordering.
//
// Because we need a stable group-id ordering sorted by class signature, we
// explicitly compute it.
struct ClassGroups6 {
    int n_classes;
    int group_of[6];           // group_of[r] = class id
    int class_members[6][6];   // class_members[g][0..size-1]
    int class_size[6];
    int class_order[6];        // ordering of classes by signature (ascending)
};

__device__ void build_row_classes(const double abs_sorted[6][6], ClassGroups6& g) {
    // tag[r] = smallest r' with same signature; class id = tag.
    int tag[6];
    for (int r = 0; r < 6; ++r) tag[r] = r;
    for (int r = 0; r < 6; ++r) {
        for (int r2 = 0; r2 < r; ++r2) {
            if (cmp6_double(abs_sorted[r], abs_sorted[r2]) == 0) {
                tag[r] = tag[r2];
                break;
            }
        }
    }
    // Dense-rank tags into 0..k-1, then fill members.
    int dense[6] = {-1,-1,-1,-1,-1,-1};
    int k = 0;
    for (int r = 0; r < 6; ++r) {
        if (dense[tag[r]] == -1) dense[tag[r]] = k++;
    }
    for (int i = 0; i < 6; ++i) g.class_size[i] = 0;
    for (int r = 0; r < 6; ++r) {
        int c = dense[tag[r]];
        g.group_of[r] = c;
        g.class_members[c][g.class_size[c]++] = r;
    }
    g.n_classes = k;
    // Sort class order ascending by signature (sigs are same for all members → use first).
    for (int i = 0; i < k; ++i) g.class_order[i] = i;
    for (int i = 1; i < k; ++i) {
        int key = g.class_order[i];
        int j = i - 1;
        while (j >= 0) {
            int a = g.class_order[j];
            if (cmp6_double(abs_sorted[g.class_members[a][0]],
                            abs_sorted[g.class_members[key][0]]) > 0) {
                g.class_order[j + 1] = g.class_order[j];
                --j;
            } else break;
        }
        g.class_order[j + 1] = key;
    }
}

// Generate the next row-permutation of class members.  Returns false when done.
// perm_idx[c] ranges over permutations of class_members[c][0..size[c]-1].
// We use a factoradic counter per class; advancing is standard mixed-radix increment.
struct PermState {
    int counter[6];      // per-class permutation index, 0..size!-1
    int sizes[6];        // size of each class (0..6)
    int n_classes;
};

__device__ __forceinline__ int factorial6(int k) {
    static const int F[7] = {1,1,2,6,24,120,720};
    return F[k];
}

// Unrank factoradic: given idx in [0, n!), produce a permutation (0..n-1).
__device__ __forceinline__ void factoradic_unrank(int idx, int n, int* out) {
    int used[6] = {0,0,0,0,0,0};
    for (int i = n - 1; i >= 0; --i) {
        int f = factorial6(i);
        int d = idx / f;
        idx  -= d * f;
        int count = -1;
        for (int k = 0; k < n; ++k) {
            if (!used[k]) {
                ++count;
                if (count == d) { out[n - 1 - i] = k; used[k] = 1; break; }
            }
        }
    }
}

// Build ic/sc/dc canonical triplet bytes for the current candidate,
// apply signs/perms, compute lex-min, return hash.
//
// One thread per matrix.
__device__ uint64_t canonical_hash_single(const z2_t* __restrict__ M)
{
    // ----- 1. Unpack into registers + compute v/abs_v --------------------
    int32_t ic[6][6], sc[6][6], dc[6][6];
    double  v [6][6];
    double  av[6][6];
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            z2_t p = M[r * 6 + c];
            int32_t i_, s_, d_;
            z2_unpack(p, i_, s_, d_);
            ic[r][c] = i_;
            sc[r][c] = s_;
            dc[r][c] = d_;
            double x = (double)i_ + (double)s_ * 1.4142135623730951;
            int32_t half = d_ >> 1;
            if (half > 0) x = ldexp(x, -half);
            if (d_ & 1)   x *= 0.7071067811865476;
            v [r][c] = x;
            av[r][c] = fabs(x);
        }
    }

    // ----- 2. Row classes (by sorted abs row) ----------------------------
    double row_sorted[6][6], col_sorted[6][6];
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) row_sorted[r][c] = av[r][c];
        sort6_double(row_sorted[r]);
    }
    // col sorted: column c of av → col_sorted[c]
    for (int c = 0; c < 6; ++c) {
        for (int r = 0; r < 6; ++r) col_sorted[c][r] = av[r][c];
        sort6_double(col_sorted[c]);
    }

    ClassGroups6 rg, cg;
    build_row_classes(row_sorted, rg);
    build_row_classes(col_sorted, cg);

    // col_perm_base[0..5] = member indices laid out class-by-class in class_order.
    int col_base[6]; int col_group_sizes[6]; int col_group_start[6];
    {
        int pos = 0;
        for (int k = 0; k < cg.n_classes; ++k) {
            int gid = cg.class_order[k];
            col_group_start[k] = pos;
            col_group_sizes[k] = cg.class_size[gid];
            for (int u = 0; u < cg.class_size[gid]; ++u) {
                col_base[pos++] = cg.class_members[gid][u];
            }
        }
    }

    // ----- 3. Enumerate combos -------------------------------------------
    // Per row class, iterate factorial(size) permutations.
    int n_rc = rg.n_classes;
    int factsize[6];
    long N_rp = 1;
    for (int k = 0; k < n_rc; ++k) {
        int gid = rg.class_order[k];
        factsize[k] = factorial6(rg.class_size[gid]);
        N_rp *= factsize[k];
    }

    double  best_v[6][6];
    int32_t best_ic[6][6], best_sc[6][6], best_dc[6][6];
    bool    have_best = false;

    // Temporaries
    int row_perm[6];
    int perm_counter[6];
    for (int k = 0; k < n_rc; ++k) perm_counter[k] = 0;

    for (long rp = 0; rp < N_rp; ++rp) {
        // Build row_perm so that row_perm[dest] is the source row at destination
        // `dest`.  Destinations are laid out class-by-class in sorted-class order
        // (matching Python `_gen_all`: classes concatenated in the order stored
        // in class_order, and within each class we iterate its |class|!
        // permutations).  So dest positions 0..sz0-1 get members of the first
        // class in some order, dest positions sz0..sz0+sz1-1 get members of the
        // second class, etc.
        int rp_tmp[6];
        int dest_pos = 0;
        for (int k = 0; k < n_rc; ++k) {
            int gid = rg.class_order[k];
            int sz  = rg.class_size[gid];
            factoradic_unrank(perm_counter[k], sz, rp_tmp);
            for (int u = 0; u < sz; ++u) {
                int src_row = rg.class_members[gid][rp_tmp[u]];
                row_perm[dest_pos++] = src_row;
            }
        }

        // Apply row permutation once per rp
        double  vp[6][6];
        int32_t icp[6][6], scp[6][6], dcp[6][6];
        for (int r = 0; r < 6; ++r) {
            int pr = row_perm[r];
            for (int c = 0; c < 6; ++c) {
                vp [r][c] = v [pr][c];
                icp[r][c] = ic[pr][c];
                scp[r][c] = sc[pr][c];
                dcp[r][c] = dc[pr][c];
            }
        }

        for (int s = 0; s < 32; ++s) {
            // Row sign flips on rows 1..5
            double  vs[6][6];
            int32_t ics[6][6], scs[6][6];
            for (int r = 0; r < 6; ++r) {
                double sgn = 1.0; int32_t isgn = 1;
                if (r > 0 && (s & (1 << (r - 1)))) { sgn = -1.0; isgn = -1; }
                for (int c = 0; c < 6; ++c) {
                    vs [r][c] = vp [r][c] * sgn;
                    ics[r][c] = icp[r][c] * isgn;
                    scs[r][c] = scp[r][c] * isgn;
                }
            }

            // Column sign normalization (float based).
            double  v_col [6][6];
            int32_t ic_col[6][6], sc_col[6][6];
            for (int c = 0; c < 6; ++c) {
                double col_sgn = 1.0;
                for (int r = 0; r < 6; ++r) {
                    double val = vs[r][c];
                    if (val >  CANON_EPS) { col_sgn =  1.0; break; }
                    if (val < -CANON_EPS) { col_sgn = -1.0; break; }
                }
                int32_t isgn = (col_sgn < 0.0) ? -1 : 1;
                for (int r = 0; r < 6; ++r) {
                    v_col [r][c] = vs [r][c] * col_sgn;
                    ic_col[r][c] = ics[r][c] * isgn;
                    sc_col[r][c] = scs[r][c] * isgn;
                }
            }

            // Column permutation: insertion-sort within each col class
            int col_perm[6];
            int pos = 0;
            for (int k = 0; k < cg.n_classes; ++k) {
                int sz = col_group_sizes[k];
                int grp[6];
                for (int u = 0; u < sz; ++u) grp[u] = col_base[col_group_start[k] + u];
                // Insertion sort by lex-order of v_col columns
                for (int i = 1; i < sz; ++i) {
                    int key = grp[i];
                    int j = i - 1;
                    while (j >= 0) {
                        int c0 = grp[j];
                        int c1 = key;
                        int c0_vs_c1 = 0;
                        for (int r = 0; r < 6; ++r) {
                            double d = v_col[r][c0] - v_col[r][c1];
                            if (d >  CANON_EPS) { c0_vs_c1 =  1; break; }
                            if (d < -CANON_EPS) { c0_vs_c1 = -1; break; }
                        }
                        if (c0_vs_c1 > 0) { grp[j + 1] = grp[j]; --j; }
                        else break;
                    }
                    grp[j + 1] = key;
                }
                for (int u = 0; u < sz; ++u) col_perm[pos++] = grp[u];
            }

            // Apply column permutation
            double  vc [6][6];
            int32_t icc[6][6], scc[6][6], dcc[6][6];
            for (int out_c = 0; out_c < 6; ++out_c) {
                int orig_c = col_perm[out_c];
                for (int r = 0; r < 6; ++r) {
                    vc [r][out_c] = v_col [r][orig_c];
                    icc[r][out_c] = ic_col[r][orig_c];
                    scc[r][out_c] = sc_col[r][orig_c];
                    dcc[r][out_c] = dcp  [r][orig_c];
                }
            }

            // Final column-sign normalization based on int_c
            for (int out_c = 0; out_c < 6; ++out_c) {
                for (int r = 0; r < 6; ++r) {
                    if (icc[r][out_c] != 0) {
                        if (icc[r][out_c] < 0) {
                            for (int rr = 0; rr < 6; ++rr) {
                                vc [rr][out_c] = -vc [rr][out_c];
                                icc[rr][out_c] = -icc[rr][out_c];
                                scc[rr][out_c] = -scc[rr][out_c];
                            }
                        }
                        break;
                    }
                }
            }

            // Lex-compare vs best
            if (!have_best) {
                for (int r = 0; r < 6; ++r)
                    for (int c = 0; c < 6; ++c) {
                        best_v [r][c] = vc [r][c];
                        best_ic[r][c] = icc[r][c];
                        best_sc[r][c] = scc[r][c];
                        best_dc[r][c] = dcc[r][c];
                    }
                have_best = true;
            } else {
                int cmp = cmp36_double(&vc[0][0], &best_v[0][0]);
                if (cmp < 0) {
                    for (int r = 0; r < 6; ++r)
                        for (int c = 0; c < 6; ++c) {
                            best_v [r][c] = vc [r][c];
                            best_ic[r][c] = icc[r][c];
                            best_sc[r][c] = scc[r][c];
                            best_dc[r][c] = dcc[r][c];
                        }
                }
            }
        }

        // Advance perm_counter (mixed-radix)
        for (int k = 0; k < n_rc; ++k) {
            if (++perm_counter[k] < factsize[k]) break;
            perm_counter[k] = 0;
        }
    }

    // ----- 4. Canonical bytes → FNV-1a 64 --------------------------------
    // Build 108 bytes = 36 * 3 int32 triplets, layout (r, c, {ic, sc, dc}).
    int32_t bytes[108];
    int idx = 0;
    for (int r = 0; r < 6; ++r)
        for (int c = 0; c < 6; ++c) {
            bytes[idx++] = best_ic[r][c];
            bytes[idx++] = best_sc[r][c];
            bytes[idx++] = best_dc[r][c];
        }
    return fnv1a64(bytes, 108);
}

__global__ void canonical_hash_kernel(const z2_t* __restrict__ in,
                                      uint64_t* __restrict__ out,
                                      int N)
{
    int n = blockIdx.x * blockDim.x + threadIdx.x;
    if (n >= N) return;
    out[n] = canonical_hash_single(in + (size_t)n * 36);
}

// Cheap raw-hash kernel (Level-1 prefilter), invariant over int32 triplet of a
// specific permutation = the packed bytes themselves; used to skip obvious
// duplicates without computing the canonical form.
__global__ void raw_hash_kernel(const z2_t* __restrict__ in,
                                uint64_t* __restrict__ out,
                                int N)
{
    int n = blockIdx.x * blockDim.x + threadIdx.x;
    if (n >= N) return;
    uint64_t h = 0xcbf29ce484222325ULL;
    const uint64_t p = 0x100000001b3ULL;
    const z2_t* M = in + (size_t)n * 36;
    for (int i = 0; i < 36; ++i) {
        uint64_t x = (uint64_t)M[i];
        for (int k = 0; k < 8; ++k) {
            h ^= (x & 0xFF);
            h *= p;
            x >>= 8;
        }
    }
    out[n] = h;
}
