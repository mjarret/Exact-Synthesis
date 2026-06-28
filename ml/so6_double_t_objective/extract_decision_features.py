#!/usr/bin/env python3
"""Find the MINIMAL feature family the near-optimal ruler-guided synthesizer actually needs.

Ablation: rebuild the depth-estimator (ruler) on progressively coarser feature families and run
the same greedy synthesizer; report factor/success at the discriminating depths. The minimal
family that still yields factor ~= 1.00 is the decision's true input; greedy descent on the depth
it implies is "the algorithm".

Reuses primitives from measure_approx_factor.py (packing, expand_all, driver bits).
"""
from __future__ import annotations
import argparse, time
from collections import defaultdict, Counter
import numpy as np
import readonly_lut
import measure_approx_factor as M

NA, NF = M.NA, M.NF
BIG = M.BIG


# ---- cells (int_c, sqrt2_c, denom) from feats or from packed states ------------
def cells_from_feats(feats):
    n = feats.shape[0]
    return np.rint(feats.reshape(n, 36, 3)).astype(np.int64)


def cells_from_states(states):
    n = states.shape[0]
    s = states.astype(np.int64).reshape(n, 36)
    int_c = (s & 0xFF).astype(np.uint8).astype(np.int8).astype(np.int64)
    sqrt2_c = ((s >> 8) & 0xFF).astype(np.uint8).astype(np.int8).astype(np.int64)
    denom = (s >> 16) & 0xFF
    return np.stack([int_c, sqrt2_c, denom], axis=2)


# ---- feature families (coarse -> fine), each: cells (n,36,3) -> list of keys ---
def f_lde(cells):                                   # scalar max denominator exponent
    return [int(v) for v in cells[..., 2].max(1)]

def f_lde_count(cells):                             # (LDE, #cells at LDE)
    de = cells[..., 2]; lde = de.max(1); cnt = (de == lde[:, None]).sum(1)
    return list(zip(lde.tolist(), cnt.tolist()))

def f_denom_hist(cells):                            # sorted multiset of all 36 denom exps (global)
    de = np.sort(cells[..., 2], 1)
    return [r.tobytes() for r in de]

def f_row_max(cells):                               # per-row max denom, rows sorted (6 numbers)
    n = cells.shape[0]; de = cells[..., 2].reshape(n, 6, 6)
    return [r.tobytes() for r in np.sort(de.max(2), 1)]

def f_row_denom(cells):                             # per-row sorted denom multiset, rows sorted (the current ruler)
    n = cells.shape[0]; de = cells[..., 2].reshape(n, 6, 6)
    return M._denom_keys(de)

def f_row_gf2(cells):                               # row_denom + numerator parity (denom & GF(2) residue)
    n = cells.shape[0]
    de = cells[..., 2].reshape(n, 6, 6)
    par = ((np.abs(cells[..., 0]) & 1) * 2 + (np.abs(cells[..., 1]) & 1)).reshape(n, 6, 6)  # 0..3
    code = de * 4 + par
    ds = np.sort(code, 2); base = (np.int64(64) ** np.arange(6))
    rcs = np.sort((ds * base).sum(2), 1)
    return [rcs[x].tobytes() for x in range(n)]

LADDER = [("LDE", f_lde), ("LDE+count", f_lde_count), ("denom_hist", f_denom_hist),
          ("row_max", f_row_max), ("row_denom", f_row_denom), ("row_gf2", f_row_gf2)]


def est(keys, ruler, default):
    return np.fromiter((ruler.get(k, default) for k in keys), np.int64, len(keys))


def build_ruler(lut, keyfn, depths, n_per_depth, seed):
    acc = defaultdict(Counter)
    for d in depths:
        feats, _, _ = lut.sample_labeled_features_at_t_depth(d, n_per_depth, seed + d)
        for k in keyfn(cells_from_feats(np.asarray(feats, np.float32))):
            acc[k][d] += 1
    ruler = {k: c.most_common(1)[0][0] for k, c in acc.items()}
    ruler[keyfn(cells_from_states(np.eye(6, dtype=np.uint32)[None]))[0]] = 0   # identity -> depth 0
    tot = sum(sum(c.values()) for c in acc.values()); cor = sum(c.most_common(1)[0][1] for c in acc.values())
    return ruler, len(ruler), cor / max(tot, 1)


def run_synth(lut, init_states, D, ruler, keyfn, default, cap_mult=4):
    Mn = init_states.shape[0]; cur = init_states.copy()
    path = np.zeros(Mn, np.int64); active = np.ones(Mn, bool)
    reached0 = np.zeros(Mn, bool); stalled = np.zeros(Mn, bool)
    max_steps = cap_mult * (D // 2)
    for step in range(max_steps + 1):
        idx = np.where(active)[0]
        if idx.size == 0: break
        depths = np.asarray(lut.depth_of(np.ascontiguousarray(cur[idx])), np.int64)
        done = depths == 0; reached0[idx[done]] = True; active[idx[done]] = False
        idx2 = idx[~done]
        if idx2.size == 0 or step == max_steps: continue
        sub = np.ascontiguousarray(cur[idx2]); k = idx2.size
        succ = M.expand_all(lut, sub)
        cur_est = est(keyfn(cells_from_states(sub)), ruler, default)
        succ_est = est(keyfn(cells_from_feats(succ.reshape(k * NA, NF))), ruler, default).reshape(k, NA)
        masked = np.where(succ_est < cur_est[:, None], succ_est, BIG)
        choice = np.where(masked.min(1) < BIG, masked.argmin(1), -1)
        stall = choice < 0
        if stall.any():
            stalled[idx2[stall]] = True
            choice = np.where(stall, succ_est.argmin(1), choice)
        cur[idx2] = M.features_to_states(succ[np.arange(k), choice]); path[idx2] += 1
    nonterm = (active & ~reached0)
    return reached0.mean(), stalled.mean(), nonterm.mean(), path[reached0] / (D / 2)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max-t-depth", type=int, default=10)
    ap.add_argument("--depths", type=str, default="8,10")
    ap.add_argument("--m", type=int, default=1500)
    ap.add_argument("--ruler-n", type=int, default=15000)
    ap.add_argument("--seed", type=int, default=0)
    args = ap.parse_args()
    test_depths = [int(x) for x in args.depths.split(",")]

    lut = readonly_lut.ReadOnlyLUT(max_t_depth=args.max_t_depth, threads=0, progress_mode="off",
                                   verbose_build=False, debug=False, log_calls=False,
                                   cached_depths=list(range(2, args.max_t_depth + 1, 2)))
    train_depths = tuple(range(2, args.max_t_depth + 1, 2))
    print("Ablation: greedy synthesizer guided by depth estimated from each feature family.")
    print("Minimal family achieving factor ~1.00 = the decision's true input.\n")

    rulers = {}
    for name, fn in LADDER:
        rulers[name] = build_ruler(lut, fn, train_depths, args.ruler_n, args.seed)
        print(f"  ruler[{name:>10}] keys={rulers[name][1]:>7} train_acc={rulers[name][2]:.4f}", flush=True)

    print(f"\n  {'feature':>11} {'depth':>5} {'success':>8} {'stall':>7} {'nonterm':>8} "
          f"{'f_mean':>8} {'f_med':>7} {'f_p90':>7} {'f_max':>7}")
    for name, fn in LADDER:
        ruler = rulers[name][0]
        for D in test_depths:
            _, states, _ = lut.sample_labeled_features_at_t_depth(D, args.m, 7000 + D)
            states = np.asarray(states, np.uint32)
            succ_rate, stall, nonterm, f = run_synth(lut, states, D, ruler, fn, default=args.max_t_depth)
            fm = float(f.mean()) if f.size else float("nan")
            fmed = float(np.median(f)) if f.size else float("nan")
            fp90 = float(np.percentile(f, 90)) if f.size else float("nan")
            fmax = float(f.max()) if f.size else float("nan")
            print(f"  {name:>11} {D:>5} {100*succ_rate:>7.1f}% {100*stall:>6.1f}% {100*nonterm:>7.1f}% "
                  f"{fm:>8.3f} {fmed:>7.3f} {fp90:>7.3f} {fmax:>7.3f}", flush=True)


if __name__ == "__main__":
    main()
