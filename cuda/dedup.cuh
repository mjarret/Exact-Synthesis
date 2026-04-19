// dedup.cuh — GPU dedup helpers built on Thrust.
//
// 1. sort + within-batch unique (filter adjacent equal hashes).
// 2. binary-search against a sorted global-seen array (remove cross-layer dups).

#pragma once
#include <thrust/device_vector.h>
#include <thrust/sort.h>
#include <thrust/binary_search.h>
#include <thrust/copy.h>
#include <thrust/iterator/counting_iterator.h>
#include <thrust/execution_policy.h>
#include <thrust/merge.h>
#include <cstdint>

// Mark, in `keep`, whether each index is (a) the first of its value group after sort
// and (b) not already present in `global_sorted`.  Input `sorted_hashes` must already
// be sorted.
__global__ void dedup_mark_kernel(const uint64_t* __restrict__ sorted_hashes,
                                  int M,
                                  const uint64_t* __restrict__ global_sorted,
                                  int G,
                                  unsigned char* __restrict__ keep)
{
    int i = blockIdx.x * blockDim.x + threadIdx.x;
    if (i >= M) return;
    uint64_t h = sorted_hashes[i];
    bool first = (i == 0) || (sorted_hashes[i - 1] != h);
    bool seen = false;
    if (first && G > 0) {
        // Binary search for h in global_sorted.
        int lo = 0, hi = G;
        while (lo < hi) {
            int mid = (lo + hi) >> 1;
            if (global_sorted[mid] < h) lo = mid + 1;
            else hi = mid;
        }
        seen = (lo < G && global_sorted[lo] == h);
    }
    keep[i] = (first && !seen) ? 1 : 0;
}
