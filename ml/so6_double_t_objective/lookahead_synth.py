#!/usr/bin/env python3
"""Test the multi-move insight: a COARSE (interpretable) potential fails single-step greedy
(no per-move gradient) but should work when the synthesizer commits over a >1 double-T horizon
(the Li-et-al move-block idea).  Compares L=1 (single move) vs L=2 (2-step lookahead) using the
SAME coarse histogram->depth tree as the potential.
"""
from __future__ import annotations
import argparse, numpy as np
import readonly_lut
import measure_approx_factor as M
import extract_decision_features as E
from extract_decision_tree import RegTree, hist_feats, sample_hist


def lookahead_choice(lut, sub, tree, B):
    """Return, per state, the first double-T move whose best 2-step-reachable potential is lowest."""
    k = sub.shape[0]
    succ1 = M.expand_all(lut, sub)                                            # (k,165,108)
    est1 = tree.predict(hist_feats(E.cells_from_feats(succ1.reshape(k * M.NA, M.NF)))).reshape(k, M.NA)
    topB = np.argsort(est1, axis=1)[:, :B]                                    # (k,B) best first moves
    child = np.take_along_axis(succ1, topB[:, :, None], axis=1).reshape(k * B, M.NF)
    child_states = M.features_to_states(child)
    succ2 = M.expand_all(lut, child_states)                                   # (k*B,165,108)
    est2 = tree.predict(hist_feats(E.cells_from_feats(succ2.reshape(k * B * M.NA, M.NF)))).reshape(k, B, M.NA)
    reach = est1.copy()
    np.put_along_axis(reach, topB,
                      np.minimum(np.take_along_axis(est1, topB, 1), est2.min(2)), axis=1)
    return reach.argmin(1), succ1, est1


def run(lut, init_states, D, tree, L, B=12, cap_mult=4):
    Mn = init_states.shape[0]; cur = init_states.copy()
    path = np.zeros(Mn, int); active = np.ones(Mn, bool)
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
        if L == 1:
            succ = M.expand_all(lut, sub)
            cur_est = tree.predict(hist_feats(E.cells_from_states(sub)))
            est = tree.predict(hist_feats(E.cells_from_feats(succ.reshape(k * M.NA, M.NF)))).reshape(k, M.NA)
            masked = np.where(est < cur_est[:, None] - 1e-9, est, np.inf)
            choice = np.where(np.isfinite(masked.min(1)), masked.argmin(1), -1)
            if (choice < 0).any():
                stalled[idx2[choice < 0]] = True
                choice = np.where(choice < 0, est.argmin(1), choice)
        else:
            choice, succ, est = lookahead_choice(lut, sub, tree, B)
            cur_est = tree.predict(hist_feats(E.cells_from_states(sub)))
            # flag a stall when even the 2-step-reachable best is not below current (diagnostic only)
            stalled[idx2[est.min(1) >= cur_est - 1e-9]] = True
        cur[idx2] = M.features_to_states(succ[np.arange(k), choice]); path[idx2] += 1
    nonterm = active & ~reached0
    f = path[reached0] / (D / 2)
    return reached0.mean(), stalled.mean(), nonterm.mean(), f


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--m", type=int, default=400)
    ap.add_argument("--beam", type=int, default=12)
    ap.add_argument("--n-per-depth", type=int, default=8000)
    args = ap.parse_args()
    lut = readonly_lut.ReadOnlyLUT(max_t_depth=10, threads=0, progress_mode="off",
                                   verbose_build=False, debug=False, log_calls=False,
                                   cached_depths=[2, 4, 6, 8, 10])
    Xtr, ytr = sample_hist(lut, [2, 4, 6, 8, 10], args.n_per_depth, seed=1)
    tree = RegTree(max_depth=8, min_leaf=40).fit(Xtr, ytr)
    print("same coarse histogram->depth tree (max_depth=8) used as the potential.\n")
    print(f"  {'horizon':>8} {'depth':>5} {'success':>8} {'stall':>7} {'nonterm':>8} {'f_mean':>8} {'f_med':>7} {'f_max':>7}")
    for L in (1, 2):
        for D in (8, 10):
            _, states, _ = lut.sample_labeled_features_at_t_depth(D, args.m, 7000 + D)
            sr, st, nt, f = run(lut, np.asarray(states, np.uint32), D, tree, L, B=args.beam)
            fm = float(f.mean()) if f.size else float("nan")
            lbl = "L=1" if L == 1 else f"L=2(b{args.beam})"
            print(f"  {lbl:>8} {D:>5} {100*sr:>7.1f}% {100*st:>6.1f}% {100*nt:>7.1f}% "
                  f"{fm:>8.3f} {float(np.median(f)) if f.size else float('nan'):>7.3f} "
                  f"{float(f.max()) if f.size else float('nan'):>7.3f}", flush=True)


if __name__ == "__main__":
    main()
