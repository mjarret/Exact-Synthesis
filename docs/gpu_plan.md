# Plan: GPU-Accelerate C++ LUT Generator (Exact-Synthesis)

## Context

The C++ LUT generator in `Exact-Synthesis/` (currently on branch `zhirui/gpu-tt-operator`, forked from `zhirui/tt-operator`) is CPU-only. It uses TBB for parallelism and a concurrent hash set (`tbb::concurrent_unordered_set<SO6>`) for cross-layer deduplication. Recent work added the paired TT operator (165-alphabet) in `include/so6/TT_Operator.hpp` for doubled-step BFS.

The Python equivalent (`lut/lut_gpu.py`) demonstrates a working GPU strategy using CuPy: vectorized row-pair apply_T, batched canonical hashing, and sort-based dedup on GPU (no hash tables). We want to port this GPU strategy to the C++ codebase using CUDA/Thrust/CUB, preserving the TT-operator pruning (165 alphabet) from the recent commit.

**Goal**: Build a CUDA-backed layer expansion path in Exact-Synthesis that achieves ≥10× speedup on large layers (depth ≥8 where layer size >10K) while keeping the existing CPU path as a fallback.

## Strategy: Mirror Python `lut_gpu.py` in C++/CUDA

### Key design decisions (copied from Python GPU)
1. **Packed matrix storage** — convert SO6 from `std::array<DyadicSqrt2, 36>` (288B, 8B per element due to alignment) to `int64[36]` packed (288B), matching Python's `Z2_PACK_DTYPE=int64` format. Same memory footprint; the gain is **uniform alignment and vectorizable layout** (no bitfield access) that benefits GPU kernels.
2. **Vectorized batch apply_T/TT** — one CUDA kernel per T-gate (or TT alphabet entry) operating on `(N, 6, 6)` int32 arrays. Use template specialization like CPU's `T_Operator<R1, R2>::apply()`.
3. **Sort + searchsorted dedup** — replace `tbb::concurrent_unordered_set<SO6>` with Thrust sort + CUB binary search on canonical hash arrays (int64). Maintain `cumul_hashes` as sorted GPU array.
4. **Keep TT_CANDIDATES pruning** from the CPU TT path — it drops invalid TT pairs based on `last_T`, saving 14/165 candidates per source matrix.

### Module layout

| File | Role |
|------|------|
| `Exact-Synthesis/include/gpu/SO6_packed.hpp` | **new** — int32 packed representation + pack/unpack to/from DyadicSqrt2 |
| `Exact-Synthesis/src/gpu/apply_TT.cu` | **new** — CUDA kernels for 165-alphabet TT application (fused disjoint + overlap cases) |
| `Exact-Synthesis/src/gpu/canonical_hash.cu` | **new** — batched canonical hash kernel (port `core/canonical_ops.py::canonical_hash_batch`) |
| `Exact-Synthesis/src/gpu/LUT_GPU.cu` | **new** — mirrors Python `lut_gpu.py::LUT_GPU`: per-layer expand with sort-based dedup using Thrust/CUB |
| `Exact-Synthesis/src/algo/Generate.cpp` | **modify** — add `create_lookup_table_TT_gpu()` alongside existing `create_lookup_table_TT()`; CLI flag `--gpu` in `apps/main.cpp` selects backend |
| `Exact-Synthesis/Makefile` | **modify** — add `CUDA_HOME`, `nvcc` rule for `.cu` files, link `-lcudart -ltbb` |

### Reuse of existing components
- **TT_ALPHABET and TT_CANDIDATES** in `include/so6/TT_Operator.hpp` (lines 25-191, 306-321) — copy as `__constant__` GPU arrays.
- **TT_APPLY_TABLE dispatch pattern** (lines 494-660) — replicate as CUDA kernel launch table.
- **`apply_TT_disjoint<R1A,R2A,R1B,R2B>()`** (lines 248-269) — template specialize for each of 45 disjoint pairs as CUDA `__device__` functions.
- **`apply_TT_overlap<>()`** (lines 272-279) — same, sequential fallback for non-disjoint pairs.
- **Python `canonical_hash_batch`** (`core/canonical_ops.py`) — reference implementation for the GPU hash kernel.

## Implementation Plan

### Phase 1: Infrastructure (no functional change)
1. Add `CUDA_HOME`, `NVCC`, `-lcudart` to `Makefile`. Guard with `ifdef CUDA_ENABLED` so CPU-only build still works.
2. Create `include/gpu/` and `src/gpu/` directories.
3. Add `SO6_packed.hpp`: `struct SO6Packed { int32_t data[36]; };` + `pack(SO6)` and `unpack(SO6Packed)` helpers.
4. Write a tiny smoke test `apps/test_gpu_init.cu`: allocate `(N, 6, 6)` int32 on device, set zero, copy back, assert.

### Phase 2: GPU apply_TT kernel
1. `apply_TT.cu`: Define two `__device__` inlines `apply_T_device<R1, R2>(SO6Packed*)` (mirrors CPU `T_Operator::apply`) and `apply_TT_disjoint_device<R1A,R2A,R1B,R2B>` / `apply_TT_overlap_device<Ta,Tb>`.
2. Top-level kernel: `__global__ void apply_TT_kernel(SO6Packed* in, SO6Packed* out, int alpha_idx, int N)` dispatches to the right templated device function based on `alpha_idx`.
3. Host wrapper: `void launch_apply_TT(SO6Packed* batch, int alpha_idx, int N, cudaStream_t stream)`.
4. Verify byte-for-byte agreement vs CPU path for 1000 random SO6 inputs.

### Phase 3: GPU canonical hash
1. `canonical_hash.cu`: port `canonical_hash_batch` from `core/canonical_ops.py`. Each thread hashes one matrix into int64.
2. Verify output matches Python CPU hash on the same input (hash values are deterministic).

### Phase 4: GPU BFS layer expansion
1. `LUT_GPU.cu` class with state: `thrust::device_vector<SO6Packed> layer_mats[D]`, `thrust::device_vector<int64_t> cumul_hashes`, `thrust::device_vector<int8_t> layer_last_T[D]`.
2. `expand_one_layer()`:
   - For each of 165 TT alphabet entries, filter by `TT_CANDIDATES[last_T]` (on device via `thrust::copy_if`).
   - Launch `apply_TT_kernel` to produce `N_kept` candidates per alphabet entry.
   - Concatenate all candidates into single device buffer.
   - Launch `canonical_hash_kernel` → `(M,) int64`.
   - `thrust::sort_by_key(hashes, candidates)`.
   - Flag internal duplicates: `thrust::adjacent_difference` + `thrust::unique`.
   - Cross-layer dedup: `thrust::binary_search(cumul_hashes, unique_hashes)` → keep only not-seen.
   - Append to layer, merge new hashes into `cumul_hashes` (sort).
3. `build(depth)` loop and `layer_size(d)`.

### Phase 5: Integration
1. `Generate.cpp`: add `create_lookup_table_TT_gpu()` that uses `LUT_GPU`.
2. `apps/main.cpp`: parse `--gpu` CLI flag (default off). When on, call GPU version; when off, keep existing CPU path.
3. CLI flag `--verify` runs both CPU and GPU back-to-back and compares layer sizes + hash sets for equality up to depth 6.

### Phase 6: Benchmarks
- Reuse existing `benchmarks/` directory to compare CPU TBB vs GPU throughput per layer.
- Report: build time for depth 8, 9, 10 on CPU vs GPU; peak GPU memory; matrices/sec.

## Critical files (to read before coding)
- `Exact-Synthesis/apps/main.cpp:90-190` — CLI entry
- `Exact-Synthesis/src/algo/Generate.cpp:19-102` — BFS loop to mirror
- `Exact-Synthesis/include/so6/TT_Operator.hpp:25-686` — TT alphabet, CANDIDATES, APPLY_TABLE
- `Exact-Synthesis/include/so6/T_Operator.hpp:36-60` — T application pattern
- `Exact-Synthesis/include/SO6.hpp:78-155` — matrix layout and hash methods
- `Exact-Synthesis/include/DyadicSqrt2.hpp:27-51` — element representation (source for packing)
- `lut/lut_gpu.py:40-206` — Python reference
- `lut/lut_even.py` — reference for 165-alphabet BFS with last_T pruning
- `core/canonical_ops.py` — hash function to port
- `core/so6_array.py::apply_T` — vectorized T-application reference

## Verification
1. **Correctness**: run CPU and GPU side-by-side up to depth 6 (the small layers LUT completes in <1s). Assert identical layer sizes and identical sorted hash sets.
2. **Single-layer kernel test**: apply TT alpha=0 to 1000 random SO6, compare packed int32 outputs to `unpack(apply_TT_cpu(pack(M)))` — must match bit-for-bit.
3. **Hash test**: for 10000 random SO6, CPU hash equals GPU hash for every matrix.
4. **Performance**: benchmark `depth=10` build on GPU vs CPU; goal is ≥5× speedup on layer 10 (largest layer, 846K matrices).
5. **Memory**: monitor `nvidia-smi` during `depth=10` build; confirm no out-of-memory.

## Non-goals (defer)
- Streaming mode for when LUT exceeds GPU memory — not needed for depth ≤10 on 24GB GPU (estimate ~1GB peak).
- Multi-GPU support — single device is enough for current targets.
- GPU canonical form (full minimization over 720 permutations) — CPU canonical is fast enough; only the hash is ported.

---

