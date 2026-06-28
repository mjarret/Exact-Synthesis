#!/usr/bin/env python3
"""MITM-as-oracle: test whether the depth<=10 structure (every state has a 1-move reducer; reducers
lower the LDE; worst block k<=2) HOLDS at T-counts BEYOND the LUT range (12-16), using MITM to compute
true optimal depth. For each state at MITM-depth D, apply double-T moves, MITM the most-promising
(lowest delta_LDE) successors, and check which truly reduce (optimal depth D-2)."""
from __future__ import annotations
import os, numpy as np, time
import readonly_lut
import measure_approx_factor as M

NA, MRD, TO = 165, 8, 25.0   # MRD = max right-side depth (left LUT is depth 10; reaches depth 18)
POOL = "runs/mitm_pool.npz"


def lde_states(s):
    return ((s.astype(np.int64) >> 16) & 0xFF).reshape(s.shape[0], -1).max(1)


def lde_feats(f):
    n = f.shape[0]; return np.rint(f.reshape(n, 36, 3)[..., 2]).astype(np.int64).max(1)


def chain(lut, k, seed):
    r = np.random.default_rng(seed); s = np.ascontiguousarray(np.eye(6, dtype=np.uint32)[None])
    for _ in range(k):
        a = np.array([[int(r.integers(0, NA))]], np.int64)
        s = M.features_to_states(np.asarray(lut.apply_tt_features_batch(s, a), np.float32).reshape(1, 108))
    return s[0]


def gen_pool(lut, n_chains=160, seed0=0):
    if os.path.exists(POOL):
        z = np.load(POOL); return {int(d[1:]): z[d] for d in z.files}
    states = np.stack([chain(lut, (6, 7, 8)[i % 3], seed0 + i) for i in range(n_chains)]).astype(np.uint32)
    md = np.asarray(lut.mitm_optimal_depth(np.ascontiguousarray(states), MRD, TO), np.int64)
    print("  depth hist: " + ", ".join(f"{d}:{int((md==d).sum())}" for d in sorted(set(md.tolist())) if d >= 0), flush=True)
    pools = {d: states[md == d] for d in (12, 14, 16) if (md == d).sum() >= 8}
    np.savez(POOL, **{f"d{d}": v for d, v in pools.items()})
    return pools


def analyze(lut, states, D, topk=8):
    n = states.shape[0]
    succ = M.expand_all(lut, states)
    delta = lde_feats(succ.reshape(n * NA, 108)).reshape(n, NA) - lde_states(states)[:, None]
    with_red, stalls, found_deltas = 0, 0, []
    for i in range(n):
        order = np.argsort(delta[i])[:topk]                 # lowest-delta_LDE moves first
        cs = M.features_to_states(succ[i, order])
        cd = np.asarray(lut.mitm_optimal_depth(np.ascontiguousarray(cs), MRD, TO), np.int64)
        red = order[cd == D - 2]
        if red.size:
            with_red += 1; found_deltas += delta[i, red].tolist()
        else:
            stalls += 1
    return with_red, stalls, found_deltas


def main():
    lut = readonly_lut.ReadOnlyLUT(max_t_depth=10, threads=0, progress_mode="off",
                                   verbose_build=False, debug=False, log_calls=False,
                                   cached_depths=[2, 4, 6, 8, 10])
    print("building / loading depth-12/14/16 pool (MITM-binned) ...", flush=True)
    t = time.time(); pools = gen_pool(lut)
    print(f"pool: { {d: len(v) for d, v in pools.items()} }  ({time.time()-t:.0f}s)\n", flush=True)
    print("== does the depth<=10 structure hold BEYOND the LUT?  (MITM ground truth) ==")
    print(f"  {'depth':>5} {'states':>7} {'%with 1-move reducer':>21} {'reducer delta_LDE dist':>26}")
    for D in sorted(pools):
        t = time.time()
        wr, st, fd = analyze(lut, pools[D], D)
        n = len(pools[D]); dd = np.array(fd)
        dist = {int(v): int((dd == v).sum()) for v in sorted(set(dd.tolist()))} if dd.size else {}
        print(f"  {D:>5} {n:>7} {100*wr/n:>20.1f}% {str(dist):>26}   ({time.time()-t:.0f}s)", flush=True)
    print("\n(reducer = double-T move with MITM optimal depth D-2, among the 8 lowest-delta_LDE moves;"
          " delta dist shows whether reducers still lower the max denominator at high T-count.)")


if __name__ == "__main__":
    main()
