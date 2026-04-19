// bfs_gpu.cu — C++/CUDA standalone BFS LUT builder for SO6.
//
// Mirrors lut_v3/lut_gpu semantics:
//   - Start from identity SO6.
//   - Each BFS layer: apply every T_t (t = 0..14, t != last_T) to the frontier,
//     canonicalise, dedup vs all previously seen hashes, record the surviving
//     matrices as the next frontier.
//   - Per-layer counts + wall times are printed.
//
// Build:  make -C Exact-Synthesis/cuda
// Run:    ./bfs_gpu --depth 10

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <chrono>
#include <vector>
#include <string>
#include <algorithm>
#include <utility>

#include <cuda_runtime.h>
#include <thrust/device_vector.h>
#include <thrust/host_vector.h>
#include <thrust/sort.h>
#include <thrust/scan.h>
#include <thrust/merge.h>
#include <thrust/unique.h>
#include <thrust/sequence.h>
#include <thrust/execution_policy.h>

#include "z2_device.cuh"
#include "apply_T.cuh"
#include "canonical.cuh"
#include "dedup.cuh"

#define CHECK_CUDA(x) do { cudaError_t e = (x); if (e != cudaSuccess) { \
    fprintf(stderr, "CUDA error %s at %s:%d: %s\n", #x, __FILE__, __LINE__, \
            cudaGetErrorString(e)); std::exit(1); } } while(0)

// ---------------------------------------------------------------------------
// Small helper kernels (file-scope — nvcc disallows nested __global__ methods)
// ---------------------------------------------------------------------------

__global__ void mask_scan_kernel(const int8_t* __restrict__ last_T, int N, int t,
                                 int* __restrict__ flags)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N) return;
    flags[i] = (last_T[i] != (int8_t)t) ? 1 : 0;
}

__global__ void scatter_flag_idx_kernel(const int* __restrict__ flags,
                                        const int* __restrict__ scan,
                                        int N,
                                        int* __restrict__ out_idx)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N) return;
    if (flags[i]) out_idx[scan[i]] = i;
}

__global__ void clear_hits_kernel(const uint64_t* __restrict__ seen, int S,
                                  const uint64_t* __restrict__ h, int K,
                                  unsigned char* __restrict__ keep)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= K) return;
    if (!keep[i]) return;
    uint64_t x = h[i];
    int lo = 0, hi = S;
    while (lo < hi) {
        int mid = (lo + hi) >> 1;
        if (seen[mid] < x) lo = mid + 1;
        else hi = mid;
    }
    if (lo < S && seen[lo] == x) keep[i] = 0;
}

__global__ void bool_to_int_kernel(const unsigned char* __restrict__ b, int N,
                                   int* __restrict__ f)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= N) return;
    f[i] = b[i] ? 1 : 0;
}

__global__ void gather_mats_kernel(const z2_t* __restrict__ src,
                                   const int*  __restrict__ idx,
                                   z2_t*       __restrict__ dst,
                                   int K2)
{
    int n = blockIdx.x * blockDim.x + threadIdx.x;
    int e = blockIdx.y;
    if (n >= K2) return;
    dst[(size_t)n * 36 + e] = src[(size_t)idx[n] * 36 + e];
}

__global__ void gather_mats_via_lookup_kernel(const int*  __restrict__ sp,
                                              const int*  __restrict__ li,
                                              const z2_t* __restrict__ sv,
                                              z2_t*       __restrict__ dst_m,
                                              int8_t*     __restrict__ dst_l,
                                              int K3, int t)
{
    int n = blockIdx.x * blockDim.x + threadIdx.x;
    int e = blockIdx.y;
    if (n >= K3) return;
    int orig = li[sp[n]];
    dst_m[(size_t)n * 36 + e] = sv[(size_t)orig * 36 + e];
    if (e == 0) dst_l[n] = (int8_t)t;
}

__global__ void gather_hashes_via_idx_kernel(const int*      __restrict__ sp,
                                             const uint64_t* __restrict__ hs,
                                             uint64_t*       __restrict__ dst,
                                             int K3)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= K3) return;
    dst[i] = hs[sp[i]];
}

// ---------------------------------------------------------------------------
// Layer state
// ---------------------------------------------------------------------------

struct Layer {
    thrust::device_vector<z2_t>   mats;    // (N * 36) packed
    thrust::device_vector<int8_t> last_T;  // (N,)
    int N;
};

static void make_identity(thrust::device_vector<z2_t>& out) {
    std::vector<z2_t> h(36, 0);
    for (int r = 0; r < 6; ++r) h[r * 6 + r] = 1;  // Z2(1,0,0) packs to int64 == 1
    out.assign(h.begin(), h.end());
}

// Merge + dedup sorted unsigned-int64 vectors.  Frees caller from Thrust boilerplate.
static thrust::device_vector<uint64_t> sorted_union(
    thrust::device_vector<uint64_t>& a,
    thrust::device_vector<uint64_t>& b)
{
    thrust::device_vector<uint64_t> merged(a.size() + b.size());
    auto end = thrust::merge(a.begin(), a.end(), b.begin(), b.end(), merged.begin());
    auto uend = thrust::unique(merged.begin(), end);
    merged.erase(uend, merged.end());
    return merged;
}

// ---------------------------------------------------------------------------
// Main BFS loop
// ---------------------------------------------------------------------------

int main(int argc, char** argv) {
    int depth = 10;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--depth" && i + 1 < argc) depth = std::atoi(argv[++i]);
    }

    int dev = 0;
    CHECK_CUDA(cudaSetDevice(dev));
    cudaDeviceProp prop;
    CHECK_CUDA(cudaGetDeviceProperties(&prop, dev));
    printf("Device: %s  SMs=%d  mem=%.1f GB\n", prop.name, prop.multiProcessorCount,
           prop.totalGlobalMem / (1024.0 * 1024.0 * 1024.0));
    printf("Depth: %d\n", depth);

    // ---- Layer 0 (identity) ---------------------------------------------
    std::vector<Layer> layers;
    Layer L0;
    make_identity(L0.mats);
    L0.last_T.resize(1);
    L0.last_T[0] = -1;
    L0.N = 1;

    thrust::device_vector<uint64_t> all_hashes;   // sorted canonical hashes (cumulative)
    thrust::device_vector<uint64_t> all_raw;      // sorted raw prefilter hashes

    {
        thrust::device_vector<uint64_t> h1(1), h2(1);
        canonical_hash_kernel<<<1, 32>>>(thrust::raw_pointer_cast(L0.mats.data()),
                                         thrust::raw_pointer_cast(h1.data()), 1);
        raw_hash_kernel<<<1, 32>>>(thrust::raw_pointer_cast(L0.mats.data()),
                                   thrust::raw_pointer_cast(h2.data()), 1);
        CHECK_CUDA(cudaDeviceSynchronize());
        all_hashes = h1;
        all_raw    = h2;
        thrust::sort(all_hashes.begin(), all_hashes.end());
        thrust::sort(all_raw.begin(),    all_raw.end());
    }
    layers.push_back(std::move(L0));
    printf("  Layer 0: %d matrices\n", layers[0].N);

    const int TPB = 256;
    const auto bg = [&](int n){ return (n + TPB - 1) / TPB; };

    for (int d = 1; d <= depth; ++d) {
        auto t_start = std::chrono::high_resolution_clock::now();

        const Layer& Lp = layers.back();
        int N = Lp.N;
        if (N == 0) break;

        thrust::device_vector<z2_t>     new_mats;
        thrust::device_vector<int8_t>   new_lt;
        thrust::device_vector<uint64_t> new_hashes;
        thrust::device_vector<uint64_t> new_raw_hashes;

        for (int t = 0; t < 15; ++t) {
            // --- Build src_idx = { i : last_T[i] != t } ---
            thrust::device_vector<int> flags(N), scan(N);
            mask_scan_kernel<<<bg(N), TPB>>>(thrust::raw_pointer_cast(Lp.last_T.data()),
                                              N, t,
                                              thrust::raw_pointer_cast(flags.data()));
            thrust::exclusive_scan(flags.begin(), flags.end(), scan.begin());
            int last_flag, last_scan;
            CHECK_CUDA(cudaMemcpy(&last_flag, thrust::raw_pointer_cast(flags.data()) + N - 1,
                                  sizeof(int), cudaMemcpyDeviceToHost));
            CHECK_CUDA(cudaMemcpy(&last_scan, thrust::raw_pointer_cast(scan.data())  + N - 1,
                                  sizeof(int), cudaMemcpyDeviceToHost));
            int K = last_scan + last_flag;
            if (K == 0) continue;

            thrust::device_vector<int> src_idx(K);
            scatter_flag_idx_kernel<<<bg(N), TPB>>>(thrust::raw_pointer_cast(flags.data()),
                                                     thrust::raw_pointer_cast(scan.data()),
                                                     N,
                                                     thrust::raw_pointer_cast(src_idx.data()));

            // --- Apply T_t to selected sources ---
            thrust::device_vector<z2_t> applied((size_t)K * 36);
            dim3 g1(bg(K), 36), b1(TPB, 1);
            apply_T_gather_kernel<<<g1, b1>>>(thrust::raw_pointer_cast(Lp.mats.data()),
                                               thrust::raw_pointer_cast(src_idx.data()),
                                               thrust::raw_pointer_cast(applied.data()),
                                               K, t);

            // --- Level 1 raw-hash prefilter ---
            thrust::device_vector<uint64_t> rh(K);
            raw_hash_kernel<<<bg(K), TPB>>>(thrust::raw_pointer_cast(applied.data()),
                                             thrust::raw_pointer_cast(rh.data()), K);

            thrust::device_vector<unsigned char> maybe_new(K, 1);
            if (all_raw.size() > 0) {
                clear_hits_kernel<<<bg(K), TPB>>>(thrust::raw_pointer_cast(all_raw.data()),
                                                   (int)all_raw.size(),
                                                   thrust::raw_pointer_cast(rh.data()), K,
                                                   thrust::raw_pointer_cast(maybe_new.data()));
            }
            if (new_raw_hashes.size() > 0) {
                clear_hits_kernel<<<bg(K), TPB>>>(thrust::raw_pointer_cast(new_raw_hashes.data()),
                                                   (int)new_raw_hashes.size(),
                                                   thrust::raw_pointer_cast(rh.data()), K,
                                                   thrust::raw_pointer_cast(maybe_new.data()));
            }

            // Compact survivors of the prefilter into surv
            thrust::device_vector<int> mflags(K), mscan(K);
            bool_to_int_kernel<<<bg(K), TPB>>>(thrust::raw_pointer_cast(maybe_new.data()),
                                                K,
                                                thrust::raw_pointer_cast(mflags.data()));
            thrust::exclusive_scan(mflags.begin(), mflags.end(), mscan.begin());
            int last_mf, last_ms;
            CHECK_CUDA(cudaMemcpy(&last_mf, thrust::raw_pointer_cast(mflags.data()) + K - 1,
                                  sizeof(int), cudaMemcpyDeviceToHost));
            CHECK_CUDA(cudaMemcpy(&last_ms, thrust::raw_pointer_cast(mscan.data())  + K - 1,
                                  sizeof(int), cudaMemcpyDeviceToHost));
            int K2 = last_ms + last_mf;
            if (K2 == 0) {
                // merge raw hashes we've now seen
                thrust::device_vector<uint64_t> rh_u = rh;
                thrust::sort(rh_u.begin(), rh_u.end());
                rh_u.erase(thrust::unique(rh_u.begin(), rh_u.end()), rh_u.end());
                new_raw_hashes = sorted_union(new_raw_hashes, rh_u);
                continue;
            }

            thrust::device_vector<int> sidx(K2);
            scatter_flag_idx_kernel<<<bg(K), TPB>>>(thrust::raw_pointer_cast(mflags.data()),
                                                    thrust::raw_pointer_cast(mscan.data()),
                                                    K,
                                                    thrust::raw_pointer_cast(sidx.data()));

            thrust::device_vector<z2_t> surv((size_t)K2 * 36);
            dim3 g2(bg(K2), 36), b2(TPB, 1);
            gather_mats_kernel<<<g2, b2>>>(thrust::raw_pointer_cast(applied.data()),
                                            thrust::raw_pointer_cast(sidx.data()),
                                            thrust::raw_pointer_cast(surv.data()),
                                            K2);

            // --- Canonical hash on survivors ---
            thrust::device_vector<uint64_t> ch(K2);
            canonical_hash_kernel<<<bg(K2), TPB>>>(thrust::raw_pointer_cast(surv.data()),
                                                    thrust::raw_pointer_cast(ch.data()), K2);

            // Sort ch, carry local_idx along
            thrust::device_vector<int> local_idx(K2);
            thrust::sequence(local_idx.begin(), local_idx.end());
            thrust::sort_by_key(ch.begin(), ch.end(), local_idx.begin());

            // Dedup vs all_hashes (global) and within sorted ch (adjacent equal)
            thrust::device_vector<unsigned char> keep(K2, 0);
            dedup_mark_kernel<<<bg(K2), TPB>>>(thrust::raw_pointer_cast(ch.data()), K2,
                                                thrust::raw_pointer_cast(all_hashes.data()),
                                                (int)all_hashes.size(),
                                                thrust::raw_pointer_cast(keep.data()));
            if (new_hashes.size() > 0) {
                clear_hits_kernel<<<bg(K2), TPB>>>(thrust::raw_pointer_cast(new_hashes.data()),
                                                    (int)new_hashes.size(),
                                                    thrust::raw_pointer_cast(ch.data()), K2,
                                                    thrust::raw_pointer_cast(keep.data()));
            }

            // Compact selected positions
            thrust::device_vector<int> kflags(K2), kscan(K2);
            bool_to_int_kernel<<<bg(K2), TPB>>>(thrust::raw_pointer_cast(keep.data()), K2,
                                                 thrust::raw_pointer_cast(kflags.data()));
            thrust::exclusive_scan(kflags.begin(), kflags.end(), kscan.begin());
            int last_kf, last_ks;
            CHECK_CUDA(cudaMemcpy(&last_kf, thrust::raw_pointer_cast(kflags.data()) + K2 - 1,
                                  sizeof(int), cudaMemcpyDeviceToHost));
            CHECK_CUDA(cudaMemcpy(&last_ks, thrust::raw_pointer_cast(kscan.data())  + K2 - 1,
                                  sizeof(int), cudaMemcpyDeviceToHost));
            int K3 = last_ks + last_kf;

            // Always merge in raw hashes from this sub-batch
            {
                thrust::device_vector<uint64_t> rh_u = rh;
                thrust::sort(rh_u.begin(), rh_u.end());
                rh_u.erase(thrust::unique(rh_u.begin(), rh_u.end()), rh_u.end());
                new_raw_hashes = sorted_union(new_raw_hashes, rh_u);
            }

            if (K3 == 0) continue;

            thrust::device_vector<int> sel_pos(K3);
            scatter_flag_idx_kernel<<<bg(K2), TPB>>>(thrust::raw_pointer_cast(kflags.data()),
                                                      thrust::raw_pointer_cast(kscan.data()),
                                                      K2,
                                                      thrust::raw_pointer_cast(sel_pos.data()));

            // Append to accumulators
            size_t old_n = new_mats.size() / 36;
            new_mats.resize(new_mats.size() + (size_t)K3 * 36);
            new_lt.resize(new_lt.size() + K3);
            dim3 g3(bg(K3), 36), b3(TPB, 1);
            gather_mats_via_lookup_kernel<<<g3, b3>>>(
                thrust::raw_pointer_cast(sel_pos.data()),
                thrust::raw_pointer_cast(local_idx.data()),
                thrust::raw_pointer_cast(surv.data()),
                thrust::raw_pointer_cast(new_mats.data()) + old_n * 36,
                thrust::raw_pointer_cast(new_lt.data())  + old_n,
                K3, t);

            // Append hashes; keep new_hashes sorted for subsequent iterations
            {
                size_t oldh = new_hashes.size();
                new_hashes.resize(oldh + K3);
                gather_hashes_via_idx_kernel<<<bg(K3), TPB>>>(
                    thrust::raw_pointer_cast(sel_pos.data()),
                    thrust::raw_pointer_cast(ch.data()),
                    thrust::raw_pointer_cast(new_hashes.data()) + oldh, K3);
                thrust::sort(new_hashes.begin(), new_hashes.end());
            }
        }  // end for t

        Layer Lc;
        Lc.mats   = std::move(new_mats);
        Lc.last_T = std::move(new_lt);
        Lc.N      = (int)(Lc.mats.size() / 36);

        if (new_hashes.size() > 0)
            all_hashes = sorted_union(all_hashes, new_hashes);
        if (new_raw_hashes.size() > 0)
            all_raw = sorted_union(all_raw, new_raw_hashes);

        auto t_end = std::chrono::high_resolution_clock::now();
        double sec = std::chrono::duration<double>(t_end - t_start).count();
        printf("  Layer %d: %d matrices  (%.3f s)\n", d, Lc.N, sec);
        layers.push_back(std::move(Lc));
    }

    return 0;
}
