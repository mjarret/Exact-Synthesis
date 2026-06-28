#!/usr/bin/env python3
"""Measure the empirical approximation factor of greedy SO(6) double-T synthesizers.

A synthesizer repeatedly picks a double-T move and applies it to drive an operator down to
depth 0 (identity coset). The optimal double-T count for a state sampled at T-depth D is exactly
D/2 (every reducing path is equal-length-optimal), so factor = moves_to_reach_0 / (D/2).

Three policies:
  optimal : always take a true reducing move (ground truth via reducing_mask_of) -> factor 1.0.
  lde     : take the move with smallest successor LDE among those strictly lowering it
            (denominator-only greedy proxy; a LOWER BOUND on the true Li-et-al algorithm,
            which uses a case roadmap to avoid stalls).
  ruler   : take the move with smallest *estimated* successor depth (row-denominator-pattern
            -> majority depth, ~99% accurate) among those strictly lowering the estimate.

Requires the depth_of / reducing_mask_of methods added to the readonly_lut binding.
"""
from __future__ import annotations
import argparse, time
from collections import defaultdict, Counter
import numpy as np
import readonly_lut

NA, NF, SIDE, CELLS = 165, 108, 6, 36
ALL_ALPHAS = np.arange(NA, dtype=np.int64)
BIG = np.int64(1 << 30)


# ---------------------------------------------------------------- packing helpers
def features_to_states(feats: np.ndarray) -> np.ndarray:
    """Inverse of write_feature_ptr o write_state_ptr.  feats (N,108) f32 -> states (N,6,6) u32."""
    n = feats.shape[0]
    c = np.rint(feats.reshape(n, CELLS, 3)).astype(np.int64)
    int_c = c[..., 0] & 0xFF
    sqrt2_c = c[..., 1] & 0xFF
    denom = c[..., 2] & 0xFF
    data = (denom << 16) | (sqrt2_c << 8) | int_c
    return np.ascontiguousarray(data.reshape(n, SIDE, SIDE).astype(np.uint32))


def _denom_keys(denom_grid: np.ndarray) -> list:
    """row-localized denominator pattern -> hashable key (per-row sorted multiset, rows sorted)."""
    ds = np.sort(denom_grid, axis=2)                       # sort within each row
    base = (np.int64(16) ** np.arange(SIDE)).astype(np.int64)
    rc = (ds * base).sum(2)                                # one int code per row
    rcs = np.sort(rc, axis=1)                              # sort the 6 row codes
    return [rcs[x].tobytes() for x in range(rcs.shape[0])]


def keys_from_feats(feats: np.ndarray) -> list:
    n = feats.shape[0]
    denom = np.rint(feats.reshape(n, CELLS, 3)[..., 2]).astype(np.int64).reshape(n, SIDE, SIDE)
    return _denom_keys(denom)


def keys_from_states(states: np.ndarray) -> list:
    n = states.shape[0]
    denom = ((states.astype(np.int64) >> 16) & 0xFF).reshape(n, SIDE, SIDE)
    return _denom_keys(denom)


def lde_from_states(states: np.ndarray) -> np.ndarray:
    n = states.shape[0]
    return ((states.astype(np.int64) >> 16) & 0xFF).reshape(n, -1).max(1)


def lde_from_succ(succ: np.ndarray) -> np.ndarray:
    n = succ.shape[0]
    return np.rint(succ.reshape(n, NA, CELLS, 3)[..., 2]).astype(np.int64).max(2)   # (n,165)


# ---------------------------------------------------------------- ruler
def build_ruler(lut, depths=(2, 4, 6, 8, 10), n_per_depth=20000, seed=0):
    acc = defaultdict(Counter)
    for d in depths:
        feats, _states, _m = lut.sample_labeled_features_at_t_depth(d, n_per_depth, seed + d)
        for k in keys_from_feats(np.asarray(feats, np.float32)):
            acc[k][d] += 1
    ruler = {k: c.most_common(1)[0][0] for k, c in acc.items()}
    ruler[np.zeros(SIDE, np.int64).tobytes()] = 0          # identity / all-zero denominators
    # accuracy/coverage diagnostics
    tot = sum(sum(c.values()) for c in acc.values())
    correct = sum(c.most_common(1)[0][1] for c in acc.values())
    return ruler, correct / max(tot, 1)


def estimate_depth(keys: list, ruler: dict, default: int) -> np.ndarray:
    return np.fromiter((ruler.get(k, default) for k in keys), np.int64, len(keys))


# ---------------------------------------------------------------- expansion + selectors
def expand_all(lut, states: np.ndarray) -> np.ndarray:
    n = states.shape[0]
    alpha = np.ascontiguousarray(np.broadcast_to(ALL_ALPHAS, (n, NA)))
    raw = lut.apply_tt_features_batch(np.ascontiguousarray(states), alpha)
    return np.asarray(raw, np.float32).reshape(n, NA, NF)


def select_optimal(lut, states):
    m = np.asarray(lut.reducing_mask_of(np.ascontiguousarray(states)), np.uint8)
    return np.where(m.any(1), m.argmax(1), -1)


def _argmin_strict(score, cur):
    masked = np.where(score < cur[:, None], score, BIG)
    return np.where(masked.min(1) < BIG, masked.argmin(1), -1), score.argmin(1)


def select_lde(succ, cur_lde):
    return _argmin_strict(lde_from_succ(succ), cur_lde)            # (strict_choice, fallback_argmin)


def select_ruler(succ, ruler, cur_est, default):
    n = succ.shape[0]
    est = estimate_depth(keys_from_feats(succ.reshape(n * NA, NF)), ruler, default).reshape(n, NA)
    return _argmin_strict(est, cur_est)


# ---------------------------------------------------------------- driver
def run_synth(lut, name, init_states, D, ruler=None, default=10, cap_mult=4, stall="sideways"):
    M = init_states.shape[0]
    cur = init_states.copy()
    path = np.zeros(M, np.int64)
    active = np.ones(M, bool)
    reached0 = np.zeros(M, bool)
    stalled_ever = np.zeros(M, bool)
    max_steps = cap_mult * (D // 2)
    for step in range(max_steps + 1):
        idx = np.where(active)[0]
        if idx.size == 0:
            break
        depths = np.asarray(lut.depth_of(np.ascontiguousarray(cur[idx])), np.int64)
        done = depths == 0
        reached0[idx[done]] = True
        active[idx[done]] = False
        idx2 = idx[~done]
        if idx2.size == 0 or step == max_steps:
            continue
        sub = np.ascontiguousarray(cur[idx2])
        succ = expand_all(lut, sub)
        if name == "optimal":
            choice = select_optimal(lut, sub)
            fb = np.zeros(idx2.size, np.int64)
        elif name == "lde":
            choice, fb = select_lde(succ, lde_from_states(sub))
        else:
            cur_est = estimate_depth(keys_from_states(sub), ruler, default)
            choice, fb = select_ruler(succ, ruler, cur_est, default)
        stall_rows = choice < 0
        if stall_rows.any():
            stalled_ever[idx2[stall_rows]] = True
            if stall == "sideways":
                choice = np.where(stall_rows, fb, choice)
            else:                                              # hard-stop
                active[idx2[stall_rows]] = False
        valid = choice >= 0
        rows = idx2[valid]
        chosen_feat = succ[np.arange(idx2.size)[valid], choice[valid]]
        cur[rows] = features_to_states(chosen_feat)
        path[rows] += 1
    nonterm = active & ~reached0
    factor = path[reached0] / (D / 2)
    return dict(name=name, D=D, M=M, success=reached0.mean(), stall=stalled_ever.mean(),
                nonterm=nonterm.mean(), factor=factor)


# ---------------------------------------------------------------- verification
def verify(lut, ruler, default, depths=(2, 4, 6, 8, 10)):
    print("== verification ==", flush=True)
    # 2: depth_of of sampled states, identity, and repacked states
    assert int(np.asarray(lut.depth_of(np.eye(SIDE, dtype=np.uint32)[None]))[0]) == 0, "depth_of(I)!=0"
    for d in depths:
        feats, states, masks = lut.sample_labeled_features_at_t_depth(d, 1000, 11 + d)
        feats = np.asarray(feats, np.float32); states = np.asarray(states, np.uint32); masks = np.asarray(masks, np.uint8)
        dd = np.asarray(lut.depth_of(np.ascontiguousarray(states)), np.int64)
        assert np.all(dd == d), f"depth_of(sampled depth {d}) mismatch: {np.unique(dd)}"
        rs = features_to_states(feats)
        assert np.array_equal(rs, states), f"repack mismatch at depth {d}"   # exact repack
        dd2 = np.asarray(lut.depth_of(np.ascontiguousarray(rs)), np.int64)
        assert np.all(dd2 == d), f"depth_of(repacked depth {d}) mismatch"
        # 3: reducing_mask_of matches sampler bit-for-bit
        rm = np.asarray(lut.reducing_mask_of(np.ascontiguousarray(states)), np.uint8)
        assert np.array_equal(rm.astype(bool), masks.astype(bool)), f"reducing_mask mismatch at depth {d}"
        # 4: successor depths in {d-2,d,d+2}; the d-2 ones are exactly the reducing bits
        k = min(64, len(states))
        succ = expand_all(lut, states[:k])
        sd = np.full((k, NA), -1, np.int64)
        for a in range(NA):
            sd[:, a] = np.asarray(lut.depth_of(features_to_states(succ[:, a])), np.int64)
        in_range = np.isin(sd, [d - 2, d, d + 2]) | (sd == -1)
        assert in_range.all(), f"successor depth out of {{d-2,d,d+2}} at depth {d}"
        red_from_depth = (sd == d - 2)
        assert np.array_equal(red_from_depth, masks[:k].astype(bool)), f"d-2 successors != reducing bits at depth {d}"
        print(f"  depth {d}: depth_of OK, repack exact, reducing_mask bit-for-bit OK, successor depths OK", flush=True)
    print(f"  ruler train accuracy (majority purity) = {ruler[1]:.4f}", flush=True)
    print("  verification PASS\n", flush=True)


# ---------------------------------------------------------------- main
def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max-t-depth", type=int, default=10)
    ap.add_argument("--depths", type=str, default="4,6,8,10")
    ap.add_argument("--m", type=int, default=2000)
    ap.add_argument("--ruler-n", type=int, default=20000)
    ap.add_argument("--seed", type=int, default=0)
    ap.add_argument("--stall", choices=["sideways", "hardstop"], default="sideways")
    args = ap.parse_args()
    test_depths = [int(x) for x in args.depths.split(",")]

    t0 = time.time()
    lut = readonly_lut.ReadOnlyLUT(max_t_depth=args.max_t_depth, threads=0, progress_mode="off",
                                   verbose_build=False, debug=False, log_calls=False,
                                   cached_depths=list(range(2, args.max_t_depth + 1, 2)))
    print(f"LUT ready ({time.time()-t0:.1f}s)", flush=True)
    ruler_map, ruler_acc = build_ruler(lut, depths=tuple(range(2, args.max_t_depth + 1, 2)),
                                       n_per_depth=args.ruler_n, seed=args.seed)
    ruler = (ruler_map, ruler_acc)  # carry acc for verify
    print(f"ruler built: {len(ruler_map)} keys, train-accuracy {ruler_acc:.4f}", flush=True)

    verify(lut, ruler, default=args.max_t_depth)

    print("== empirical approximation factor (factor = moves / optimal(D/2)) ==")
    print(f"  {'depth':>5} {'policy':>8} {'success':>8} {'stall':>7} {'nonterm':>8} "
          f"{'f_mean':>8} {'f_med':>7} {'f_p90':>7} {'f_max':>7}")
    for D in test_depths:
        feats, states, _ = lut.sample_labeled_features_at_t_depth(D, args.m, 7000 + D)
        states = np.asarray(states, np.uint32)
        for name in ("optimal", "lde", "ruler"):
            r = run_synth(lut, name, states, D, ruler=ruler_map, default=args.max_t_depth, stall=args.stall)
            f = r["factor"]
            fmean = float(f.mean()) if f.size else float("nan")
            fmed = float(np.median(f)) if f.size else float("nan")
            fp90 = float(np.percentile(f, 90)) if f.size else float("nan")
            fmax = float(f.max()) if f.size else float("nan")
            print(f"  {D:>5} {name:>8} {100*r['success']:>7.1f}% {100*r['stall']:>6.1f}% "
                  f"{100*r['nonterm']:>7.1f}% {fmean:>8.3f} {fmed:>7.3f} {fp90:>7.3f} {fmax:>7.3f}", flush=True)
            if name == "optimal":
                assert abs(r["success"] - 1.0) < 1e-9 and (f.size == 0 or np.allclose(f, 1.0)), \
                    f"OPTIMAL SANITY FAILED at depth {D}: success={r['success']} factor!=1"
    print("\noptimal-sanity PASS (factor==1.0, 100% success at every depth)")


if __name__ == "__main__":
    main()
