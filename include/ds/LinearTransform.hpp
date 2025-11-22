#pragma once

#include <array>
#include <cstdint>
#include <type_traits>
#include <utility>
#include <new>
#include <stdexcept>
#include <cstring>     // std::memcpy

#include "so6/SO6.hpp"

/*
    ----------------------------------------------------------------------------
    Matrix-free linear operator abstractions for SO6 (left-acting row transforms)
    ----------------------------------------------------------------------------

    What you get:
      - apply_inplace_row_kernel(S, K): typed, zero-overhead path (fully inlined)
      - AnyKernel: tiny type-erased kernel with SBO (no heap, no virtual)
      - apply_inplace_any_kernel(S, AnyKernel): runtime-chosen kernels
      - Pipeline<MaxOps>: compose multiple kernels; single column pass; optional fuse()

    Kernels included:
      - RowPairTKernel         : your T over a runtime (r1, r2)
      - RowPairLinear          : arbitrary 2x2 block on (r1, r2)
      - RowBlockLinear<K>      : arbitrary KxK block on specified rows
      - CSRLeftOp              : arbitrary sparse left-multiply (targets as CSR over source rows)

    Notes:
      - The "driver" owns hash/col_hash/canonical_form book-keeping.
      - Kernels only touch entries and may apply DyadicSqrt2-specific post-update normalization.
      - For correctness when sources overlap targets, kernels must READ BEFORE WRITE.

    Build:
      - Header-only; compile with -O2/-O3. Tested with GCC/Clang syntax.
*/

namespace detail {

// SFINAE: detect static rows_mask_static
template<class, class = void>
struct has_rows_mask_static : std::false_type {};

template<class T>
struct has_rows_mask_static<T, std::void_t<decltype(T::rows_mask_static)>> : std::true_type {};

// SFINAE: detect instance rows_mask() const
template<class, class = void>
struct has_rows_mask_method : std::false_type {};

template<class T>
struct has_rows_mask_method<T, std::void_t<decltype(std::declval<const T&>().rows_mask())>> : std::true_type {};

inline constexpr uint8_t bit(uint8_t r) { return static_cast<uint8_t>(1u << r); }

inline uint8_t bitmask_from_rows(const uint8_t* rows, int n) {
    uint8_t m = 0;
    for (int i = 0; i < n; ++i) m = static_cast<uint8_t>(m | bit(rows[i]));
    return m;
}

template<int K>
inline uint8_t bitmask_from_rows(const std::array<uint8_t, K>& rows) {
    uint8_t m = 0;
    for (int i = 0; i < K; ++i) m = static_cast<uint8_t>(m | bit(rows[i]));
    return m;
}

} // namespace detail

// -----------------------------------------------------------------------------
// 1) Typed, zero-overhead driver for "rowwise kernels"
// -----------------------------------------------------------------------------

// Row-kernel driver with optional canonicalization
template<class K, bool Canonicalize = true>
inline __attribute__((always_inline))
SO6& apply_inplace_row_kernel_opt(SO6& S, const K& k) {
    for (uint8_t col = 0; col < 6; ++col) k.transform_column(S, col);
    if constexpr (Canonicalize) {S.canonical_reset();}
    return S;
}

// Backward-compatible default that canonicalizes
template<class K>
inline __attribute__((always_inline))
SO6& apply_inplace_row_kernel(SO6& S, const K& k) {
    return apply_inplace_row_kernel_opt<K, true>(S, k);
}

// Convenience: a typed "apply-copy"
template<class K, bool Canonicalize = true>
inline __attribute__((always_inline))
SO6 apply_row_kernel_opt(const SO6& in, const K& k) {
    SO6 copy = in;
    apply_inplace_row_kernel_opt<K, Canonicalize>(copy, k);
    return copy;
}

// Backward-compatible default that canonicalizes
template<class K>
inline __attribute__((always_inline))
SO6 apply_row_kernel(const SO6& in, const K& k) {
    return apply_row_kernel_opt<K, true>(in, k);
}

// -----------------------------------------------------------------------------
// 2) Type-erased runtime kernel with SBO (no heap, no virtuals)
// -----------------------------------------------------------------------------

class AnyKernel {
public:
    enum class Kind : uint8_t {
        Generic = 0,
        RowPairLinear,
        RowPairT,
        RowBlockK,
        CSR
    };

    using ApplyFn = void(*)(const void*, SO6&, uint8_t);
    using FuseFn  = bool(*)(const void* a, const void* b, void* out); // optional, for kernel fusion

    AnyKernel() noexcept
        : apply_(nullptr), fuse_(nullptr), kind_(Kind::Generic), mask_(0), size_(0) {}

    template<class K>
    static AnyKernel make(const K& k, uint8_t rows_mask, Kind kind = Kind::Generic, FuseFn fuse = nullptr) {
        static_assert(std::is_trivially_destructible_v<K>, "K must be trivially destructible for SBO AnyKernel");
        AnyKernel a;
        a.kind_ = kind;
        a.mask_ = rows_mask;
        a.size_ = static_cast<uint8_t>(sizeof(K));
        static_assert(sizeof(K) <= StorageSize, "Kernel too large for AnyKernel SBO storage");
        new (a.storage_) K(k);
        a.apply_ = +[](const void* p, SO6& S, uint8_t col) {
            auto& kk = *reinterpret_cast<const K*>(p);
            kk.transform_column(S, col);
        };
        a.fuse_ = fuse; // may be null
        return a;
    }

    // Basic accessors
    inline uint8_t rows_mask() const noexcept { return mask_; }
    inline Kind kind() const noexcept { return kind_; }

    inline void transform_column(SO6& S, uint8_t col) const {
        apply_(storage_, S, col);
    }

    // Try to fuse `this` followed by `next` into `this`. Returns true if fused.
    bool try_fuse_with(const AnyKernel& next) {
        if (!fuse_ || kind_ != next.kind_ || next.fuse_ != fuse_) return false;
        alignas(std::max_align_t) unsigned char out[StorageSize];
        if (!fuse_(storage_, next.storage_, out)) return false;
        // Replace storage with fused content (same type/size presumed by fuse_)
        std::memcpy(storage_, out, StorageSize);
        // rows_mask remains the same under our fusers; adjust if your fuser changes it.
        return true;
    }

private:
    // Increase SBO storage to accommodate larger kernels when using Dyadic backend
    static constexpr size_t StorageSize = 512;

    alignas(std::max_align_t) unsigned char storage_[StorageSize];
    ApplyFn apply_;
    FuseFn  fuse_;
    Kind    kind_;
    uint8_t mask_;
    uint8_t size_; // reserved (not currently used)
};

// Runtime driver (type-erased)
inline SO6& apply_inplace_any_kernel(SO6& S, const AnyKernel& k) {
    for (uint8_t col = 0; col < 6; ++col) k.transform_column(S, col);
    return S;
}

inline SO6 apply_any_kernel(const SO6& in, const AnyKernel& k) {
    SO6 copy = in;
    apply_inplace_any_kernel(copy, k);
    return copy;
}

// -----------------------------------------------------------------------------
// 3) Ready-to-use kernels
// -----------------------------------------------------------------------------

// 3a) Your T on a runtime (r1, r2)
struct RowPairTKernel {
    uint8_t r1, r2; // 0..5

    inline uint8_t rows_mask() const noexcept {
        return static_cast<uint8_t>((1u<<r1) | (1u<<r2));
    }

    inline __attribute__((always_inline))
    void transform_column(SO6& S, uint8_t col) const {
        DyadicSqrt2 a = S.get_element(r1, col);
        DyadicSqrt2 b = S.get_element(r2, col);
        const DyadicSqrt2 a_old = a;

        a += b;
        b -= a_old;
        b = -b;

        // Same post-update normalization policy as your T
        a.denom_exp += (a.int_c != 0);
        b.denom_exp += (b.int_c != 0);

        S.set_element(r1, col, a);
        S.set_element(r2, col, b);
    }
};

// 3b) Arbitrary 2x2 row-block
struct RowPairLinear {
    uint8_t r1, r2;
    DyadicSqrt2 m00, m01, m10, m11;

    inline uint8_t rows_mask() const noexcept {
        return static_cast<uint8_t>((1u<<r1) | (1u<<r2));
    }

    inline __attribute__((always_inline))
    void transform_column(SO6& S, uint8_t col) const {
        DyadicSqrt2 x0 = S.get_element(r1, col);
        DyadicSqrt2 x1 = S.get_element(r2, col);

        DyadicSqrt2 y0 = m00 * x0 + m01 * x1;
        DyadicSqrt2 y1 = m10 * x0 + m11 * x1;

        // Keep DyadicSqrt2's normalization policy
        y0.denom_exp += (y0.int_c != 0);
        y1.denom_exp += (y1.int_c != 0);

        S.set_element(r1, col, y0);
        S.set_element(r2, col, y1);
    }

    // Compose: this(A) followed by B -> C = B * A  (matrix product order)
    static RowPairLinear compose(const RowPairLinear& A, const RowPairLinear& B) {
        if (A.r1 != B.r1 || A.r2 != B.r2) {
            throw std::invalid_argument("RowPairLinear::compose requires identical row pairs");
        }
        RowPairLinear C;
        C.r1 = A.r1; C.r2 = A.r2;
        // C = B * A
        C.m00 = B.m00 * A.m00 + B.m01 * A.m10;
        C.m01 = B.m00 * A.m01 + B.m01 * A.m11;
        C.m10 = B.m10 * A.m00 + B.m11 * A.m10;
        C.m11 = B.m10 * A.m01 + B.m11 * A.m11;
        return C;
    }
};

// 3c) Optional multi-row block (runtime-selected rows; row0 mandatory, others optional)
//     rows[i] == 0xFF means "inactive". rows[0] must be in [0,5].
//     M is a dense 6x6, but only the active subset is used at runtime.
struct OptionalRowBlock {
    static constexpr uint8_t kMaxRows = 6;
    static constexpr uint8_t kInactive = 0xFF;

    uint8_t rows[kMaxRows] = {kInactive, kInactive, kInactive, kInactive, kInactive, kInactive};
    // dense matrix; only active rows/cols participate
    DyadicSqrt2 M[kMaxRows][kMaxRows] = {};

    inline uint8_t rows_mask() const noexcept {
        uint8_t mask = 0;
        for (uint8_t r : rows) {
            if (r != kInactive) mask = static_cast<uint8_t>(mask | (1u << r));
        }
        return mask;
    }

    inline __attribute__((always_inline))
    void transform_column(SO6& S, uint8_t col) const {
        DyadicSqrt2 x[kMaxRows] = {};
        DyadicSqrt2 y[kMaxRows] = {};
        uint8_t active[kMaxRows];
        int n = 0;

        // Collect active rows (rows[0] must be valid)
        for (int i = 0; i < kMaxRows; ++i) {
            uint8_t r = rows[i];
            if (r == kInactive) continue;
            active[n] = r;
            x[n] = S.get_element(r, col);
            ++n;
        }

        // Apply dense block on active subset
        for (int i = 0; i < n; ++i) {
            for (int j = 0; j < n; ++j) {
                y[i] += M[i][j] * x[j];
            }
            y[i].denom_exp += (y[i].int_c != 0);
        }

        // Write back only active rows
        for (int i = 0; i < n; ++i) {
            S.set_element(active[i], col, y[i]);
        }
    }
};

// 3d) KxK row-block, compile-time K for unrolling
template<int K>
struct RowBlockLinear {
    static_assert(K >= 1 && K <= 6, "RowBlockLinear<K>: K must be in [1,6]");
    std::array<uint8_t, K> rows; // rows we touch (0..5)
    DyadicSqrt2 M[K][K];                  // KxK coefficients

    inline uint8_t rows_mask() const noexcept {
        return detail::bitmask_from_rows<K>(rows);
    }

    inline __attribute__((always_inline))
    void transform_column(SO6& S, uint8_t col) const {
        DyadicSqrt2 x[K];
        for (int i = 0; i < K; ++i) x[i] = S.get_element(rows[i], col);

        DyadicSqrt2 y[K] = {};
        for (int i = 0; i < K; ++i) {
            for (int j = 0; j < K; ++j) {
                y[i] += M[i][j] * x[j];
            }
            y[i].denom_exp += (y[i].int_c != 0);
        }

        for (int i = 0; i < K; ++i) {
            S.set_element(rows[i], col, y[i]);
        }
    }
};

// 3e) Arbitrary sparse left-multiply: each target row is a sparse combo of all 6 sources.
//     CSR layout per target row (targets subset of {0..5})
struct CSRLeftOp {
    // which rows we write (subset), num_targets in [0..6], unused slots = 0xFF
    std::array<uint8_t, 6> targets{};
    uint8_t num_targets = 0;

    // CSR over source rows for each target row in 'targets':
    // row_ptr size = num_targets + 1
    std::array<uint8_t, 7>  row_ptr{};     // monotone, row_ptr[0]=0, row_ptr[num_targets]=nnz
    std::array<uint8_t, 36> col_idx{};     // indices of source rows (0..5)
    std::array<DyadicSqrt2,     36>  val{};         // coefficients

    inline uint8_t rows_mask() const noexcept {
        return detail::bitmask_from_rows(targets.data(), num_targets);
    }

    inline __attribute__((always_inline))
    void transform_column(SO6& S, uint8_t col) const {
        // Read all possible sources once
        DyadicSqrt2 src[6];
        for (int r = 0; r < 6; ++r) src[r] = S.get_element(static_cast<uint8_t>(r), col);

        for (uint8_t t = 0; t < num_targets; ++t) {
            uint8_t i = targets[t];
            DyadicSqrt2 acc{}; // zero
            for (uint8_t p = row_ptr[t]; p < row_ptr[t+1]; ++p) {
                acc += val[p] * src[col_idx[p]];
            }
            acc.denom_exp += (acc.int_c != 0);
            S.set_element(i, col, acc);
        }
    }
};

// -----------------------------------------------------------------------------
// 4) Helpers to build AnyKernel from the above kernels, with optional fusion
// -----------------------------------------------------------------------------

// Fuse function for RowPairLinear: fuse (A then B) into C = B * A
inline bool fuse_rowpairlinear_fn(const void* a, const void* b, void* out) {
    const auto& A = *reinterpret_cast<const RowPairLinear*>(a);
    const auto& B = *reinterpret_cast<const RowPairLinear*>(b);
    if (A.r1 != B.r1 || A.r2 != B.r2) return false;
    auto C = RowPairLinear::compose(A, B); // B * A
    std::memcpy(out, &C, sizeof(C));
    return true;
}

inline AnyKernel make_any(const RowPairTKernel& k) {
    return AnyKernel::make(k, k.rows_mask(), AnyKernel::Kind::RowPairT, /*fuse=*/nullptr);
}
inline AnyKernel make_any(const RowPairLinear& k) {
    return AnyKernel::make(k, k.rows_mask(), AnyKernel::Kind::RowPairLinear, &fuse_rowpairlinear_fn);
}
template<int K>
inline AnyKernel make_any(const RowBlockLinear<K>& k) {
    return AnyKernel::make(k, k.rows_mask(), AnyKernel::Kind::RowBlockK, /*fuse=*/nullptr);
}
inline AnyKernel make_any(const CSRLeftOp& k) {
    return AnyKernel::make(k, k.rows_mask(), AnyKernel::Kind::CSR, /*fuse=*/nullptr);
}

// -----------------------------------------------------------------------------
// 5) Pipeline: compose multiple kernels in a single column pass
// -----------------------------------------------------------------------------

template<int MaxOps = 16>
class Pipeline {
public:
    Pipeline() : size_(0), mask_(0) {}

    bool push(const AnyKernel& k) {
        if (size_ >= MaxOps) return false;
        ops_[size_++] = k;
        mask_ = static_cast<uint8_t>(mask_ | k.rows_mask());
        return true;
    }

    template<class K>
    bool push_typed(const K& k) { return push(make_any(k)); }

    // Optional: fuse adjacent RowPairLinear kernels that act on the same row pair.
    void fuse() {
        if (size_ < 2) return;
        std::array<AnyKernel, MaxOps> out{};
        uint8_t n = 0;

        out[n++] = ops_[0];

        for (uint8_t i = 1; i < size_; ++i) {
            AnyKernel& prev = out[n - 1];
            const AnyKernel& cur = ops_[i];
            // Try to fuse prev with cur; if successful, prev becomes fused and we skip pushing cur
            if (prev.try_fuse_with(cur)) {
                continue;
            } else {
                out[n++] = cur;
            }
        }

        ops_ = out;
        size_ = n;
        // mask_ stays valid (fusers must preserve written rows for the fused pair)
    }

    inline SO6& apply_inplace(SO6& S) const {
        for (uint8_t col = 0; col < 6; ++col) {
            for (uint8_t i = 0; i < size_; ++i) {
                ops_[i].transform_column(S, col);
            }
        }
        return S;
    }

    inline SO6 apply(const SO6& in) const {
        SO6 copy = in;
        apply_inplace(copy);
        return copy;
    }

    inline uint8_t size() const noexcept { return size_; }

private:
    std::array<AnyKernel, MaxOps> ops_{};
    uint8_t size_;
    uint8_t mask_;
};

// -----------------------------------------------------------------------------
// 6) Convenience: compile-time T operator using the typed driver (zero overhead)
// -----------------------------------------------------------------------------

template<int Row1, int Row2>
struct TKernelCT {
    static_assert(Row1 >= 0 && Row1 < Row2 && Row2 < 6, "TKernelCT row pair out of range");
    static constexpr uint8_t rows_mask_static = static_cast<uint8_t>((1u << Row1) | (1u << Row2));

    inline __attribute__((always_inline))
    void transform_column(SO6& S, uint8_t col) const {
        DyadicSqrt2 a = S.get_element(static_cast<uint8_t>(Row1), col);
        DyadicSqrt2 b = S.get_element(static_cast<uint8_t>(Row2), col);
        const DyadicSqrt2 a_old = a;

        a += b;
        b -= a_old;
        b = -b;

        a.denom_exp += (a.int_c != 0);
        b.denom_exp += (b.int_c != 0);

        S.set_element(static_cast<uint8_t>(Row1), col, a);
        S.set_element(static_cast<uint8_t>(Row2), col, b);
    }
};

template<int Row1, int Row2, bool Canonicalize = true>
inline __attribute__((always_inline))
SO6& apply_inplace_T(SO6& S) {
    return apply_inplace_row_kernel_opt<TKernelCT<Row1, Row2>, Canonicalize>(S, TKernelCT<Row1, Row2>{});
}

template<int Row1, int Row2>
inline __attribute__((always_inline))
SO6 operator*(const TKernelCT<Row1, Row2>&, const SO6& rhs) {
    return apply_row_kernel_opt<TKernelCT<Row1, Row2>, true>(rhs, TKernelCT<Row1, Row2>{});
}

// -----------------------------------------------------------------------------
// 7) Operators for AnyKernel and Pipeline (ergonomic sugar)
// -----------------------------------------------------------------------------

inline SO6 operator*(const AnyKernel& k, const SO6& rhs) {
    return apply_any_kernel(rhs, k);
}

template<int MaxOps>
inline SO6 operator*(const Pipeline<MaxOps>& p, const SO6& rhs) {
    return p.apply(rhs);
}

// -----------------------------------------------------------------------------
// 8) Helpers: easy builders
// -----------------------------------------------------------------------------

// Compute the classic "pair index" (0..14) if you want to track last_T-like IDs.
inline constexpr uint8_t pair_index(uint8_t r1, uint8_t r2) {
    // same mapping as your compute_index()
    uint8_t idx = 0;
    for (int r = 0; r < r1; ++r)
        idx = static_cast<uint8_t>(idx + static_cast<uint8_t>(5 - r));
    idx = static_cast<uint8_t>(idx + static_cast<uint8_t>(r2 - r1 - 1));
    return idx;
}

// Build RowPairLinear equivalent to your T (for runtime rows)
inline RowPairLinear make_T_rowpair(uint8_t r1, uint8_t r2) {
    // Algebraically: [a'; b'] = [[1,1],[1,-1]] * [a; b], plus the same DyadicSqrt2 post-normalization
    RowPairLinear k;
    k.r1 = r1; k.r2 = r2;
    k.m00 = DyadicSqrt2{1}; k.m01 = DyadicSqrt2{1};
    k.m10 = DyadicSqrt2{1};
    k.m11 = DyadicSqrt2(static_cast<uint8_t>(-1), static_cast<uint8_t>(0), static_cast<uint8_t>(0));
    return k;
}

// -----------------------------------------------------------------------------
// 9) Convert a discovered SO6 into a LinearTransform
// -----------------------------------------------------------------------------

// Dense K=6 row-block that left-multiplies by L for each column (y = L * x).
inline RowBlockLinear<6> make_dense_leftop(const SO6& L) {
    RowBlockLinear<6> op;
    for (int i = 0; i < 6; ++i) op.rows[i] = static_cast<uint8_t>(i);
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < 6; ++j) {
            op.M[i][j] = L.get_element(static_cast<uint8_t>(i), static_cast<uint8_t>(j));
        }
    }
    return op;
}

// Sparse CSR left-op built from all non-zero entries of L.
inline CSRLeftOp make_csr_leftop(const SO6& L) {
    CSRLeftOp op;
    // All 6 rows are targets
    for (uint8_t i = 0; i < 6; ++i) op.targets[i] = i;
    op.num_targets = 6;
    op.row_ptr[0] = 0;
    uint8_t nnz = 0;
    for (uint8_t i = 0; i < 6; ++i) {
        for (uint8_t j = 0; j < 6; ++j) {
            DyadicSqrt2 v = L.get_element(i, j);
            if (v.int_c == 0) continue;
            op.col_idx[nnz] = j;
            op.val[nnz]     = v;
            ++nnz;
        }
        op.row_ptr[i + 1] = nnz;
    }
    return op;
}

// AnyKernel wrappers for convenience
inline AnyKernel make_any_dense_leftop(const SO6& L) {
    return make_any(make_dense_leftop(L));
}
inline AnyKernel make_any_csr_leftop(const SO6& L) {
    return make_any(make_csr_leftop(L));
}

 
