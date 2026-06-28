#!/usr/bin/env python3
"""The decisive test E1 pointed to: a tree over ROW-LOCALIZED features (not the global histogram).

E1 showed the global (denom x parity) histogram tree fails as a greedy potential because it's
permutation-symmetric over the 36 cells -> coarse leaves -> no per-move gradient. The ruler works
because it uses the ROW-LOCALIZED denominator pattern. So the untested candidate is a tree over
per-row features. Does a COMPACT row-localized tree work as a near-optimal potential?
"""
from __future__ import annotations
import numpy as np
import readonly_lut
import measure_approx_factor as M
import extract_decision_features as E
from extract_decision_tree import RegTree


def rowloc_feats(cells):                     # (n,36,3) -> (n, 67): per-row denom histogram (rows sorted) + LDE
    n = cells.shape[0]
    de = cells[..., 2].reshape(n, 6, 6)
    H = np.stack([(de == k).sum(2) for k in range(11)], 2)        # (n,6,11) per-row count at each level
    base = (np.int64(8) ** np.arange(11))
    code = (H * base).sum(2)                                       # (n,6) row code
    order = np.argsort(code, axis=1)
    Hs = np.take_along_axis(H, order[:, :, None], axis=1).reshape(n, 66)
    return np.concatenate([Hs, de.max((1, 2))[:, None]], 1).astype(np.float64)


def sample_xy(lut, depths, n, seed):
    Xs, ys = [], []
    for d in depths:
        feats, _, _ = lut.sample_labeled_features_at_t_depth(d, n, seed + d)
        Xs.append(rowloc_feats(E.cells_from_feats(np.asarray(feats, np.float32))))
        ys.append(np.full(len(Xs[-1]), d))
    return np.concatenate(Xs), np.concatenate(ys)


def run_synth(lut, init_states, D, tree, cap_mult=4):
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
        succ = M.expand_all(lut, sub)
        cur_est = tree.predict(rowloc_feats(E.cells_from_states(sub)))
        succ_est = tree.predict(rowloc_feats(E.cells_from_feats(succ.reshape(k * M.NA, M.NF)))).reshape(k, M.NA)
        masked = np.where(succ_est < cur_est[:, None] - 1e-9, succ_est, np.inf)
        choice = np.where(np.isfinite(masked.min(1)), masked.argmin(1), -1)
        st = choice < 0
        if st.any():
            stalled[idx2[st]] = True; choice = np.where(st, succ_est.argmin(1), choice)
        cur[idx2] = M.features_to_states(succ[np.arange(k), choice]); path[idx2] += 1
    nonterm = active & ~reached0
    f = path[reached0] / (D / 2)
    return reached0.mean(), stalled.mean(), nonterm.mean(), f


def _leaves(nd):
    return 1 if "feature" not in nd else _leaves(nd["left"]) + _leaves(nd["right"])


def main():
    lut = readonly_lut.ReadOnlyLUT(max_t_depth=10, threads=0, progress_mode="off",
                                   verbose_build=False, debug=False, log_calls=False,
                                   cached_depths=[2, 4, 6, 8, 10])
    depths = [2, 4, 6, 8, 10]
    Xtr, ytr = sample_xy(lut, depths, 6000, 1)
    Xte, yte = sample_xy(lut, depths, 3000, 9999)
    print("ROW-LOCALIZED feature tree (per-row denom histogram, rows sorted, + LDE = 67 dims)\n")
    print(f"  {'max_depth':>9} {'#leaves':>8} {'test-acc':>9} {'D10 success':>12} {'stall':>7} {'nonterm':>8} {'f_mean':>8} {'f_max':>7}")
    for md in (8, 10, 12, 14, 16):
        tr = RegTree(max_depth=md, min_leaf=20).fit(Xtr, ytr)
        acc = float((2 * np.round(tr.predict(Xte) / 2) == yte).mean())
        _, states, _ = lut.sample_labeled_features_at_t_depth(10, 1200, 7010)
        sr, st, nt, f = run_synth(lut, np.asarray(states, np.uint32), 10, tr)
        fm = float(f.mean()) if f.size else float("nan"); fmax = float(f.max()) if f.size else float("nan")
        print(f"  {md:>9} {_leaves(tr.root):>8} {100*acc:>8.1f}% {100*sr:>11.1f}% {100*st:>6.1f}% "
              f"{100*nt:>7.1f}% {fm:>8.3f} {fmax:>7.3f}", flush=True)


if __name__ == "__main__":
    main()
