#!/usr/bin/env python3
"""Model the decision potential (denominator histogram -> T-depth) as an explicit decision tree.

The minimal decision feature is the denominator-exponent histogram (count of the 36 cells at each
level) + LDE. Here we fit a CART regression tree to histogram -> T-depth, read it out as rules,
measure its depth-prediction accuracy, and confirm a tree-guided greedy synthesizer still reaches
factor ~= 1.00 (so the explicit tree IS the algorithm, not just a description of one).
"""
from __future__ import annotations
import argparse, numpy as np
import readonly_lut
import measure_approx_factor as M
import extract_decision_features as E

K_LEVELS = 11                                   # denom levels 0..10
NAMES = [f"n{k}" for k in range(K_LEVELS)] + ["LDE"]


def hist_feats(cells):                          # cells (n,36,3) -> (n, 12) histogram + LDE
    de = cells[..., 2]
    H = np.stack([(de == k).sum(1) for k in range(K_LEVELS)], 1)
    return np.concatenate([H, de.max(1)[:, None]], 1).astype(np.float64)


# ----------------------------- tiny CART regression tree -----------------------
class RegTree:
    def __init__(self, max_depth=6, min_leaf=50):
        self.max_depth, self.min_leaf, self.root = max_depth, min_leaf, None

    def fit(self, X, y):
        self.root = self._build(X, y, 0); return self

    def _build(self, X, y, depth):
        node = {"value": float(y.mean()), "n": int(len(y))}
        if depth >= self.max_depth or len(y) < 2 * self.min_leaf or np.ptp(y) == 0:
            return node
        base = ((y - y.mean()) ** 2).sum()
        best = None
        for f in range(X.shape[1]):
            xf = X[:, f]
            for t in np.unique(xf)[:-1]:
                L = xf <= t
                nl = int(L.sum()); nr = len(y) - nl
                if nl < self.min_leaf or nr < self.min_leaf:
                    continue
                yl, yr = y[L], y[~L]
                sse = ((yl - yl.mean()) ** 2).sum() + ((yr - yr.mean()) ** 2).sum()
                if best is None or sse < best[0]:
                    best = (sse, f, float(t))
        if best is None or best[0] >= base - 1e-9:
            return node
        _, f, t = best
        L = X[:, f] <= t
        node.update(feature=f, thresh=t,
                    left=self._build(X[L], y[L], depth + 1),
                    right=self._build(X[~L], y[~L], depth + 1))
        return node

    def predict(self, X):
        out = np.empty(len(X)); stack = [(self.root, np.arange(len(X)))]
        while stack:
            nd, ii = stack.pop()
            if "feature" not in nd:
                out[ii] = nd["value"]; continue
            L = X[ii, nd["feature"]] <= nd["thresh"]
            stack.append((nd["left"], ii[L])); stack.append((nd["right"], ii[~L]))
        return out


def print_tree(nd, depth=0, lbl="root: "):
    ind = "   " * depth
    if "feature" not in nd:
        print(f"{ind}{lbl}=> depth ~= {nd['value']:.2f}  (n={nd['n']})"); return
    print(f"{ind}{lbl}if {NAMES[nd['feature']]} <= {nd['thresh']:.0f}:")
    print_tree(nd["left"], depth + 1, "then ")
    print_tree(nd["right"], depth + 1, "else ")


# ----------------------------- tree-guided synthesizer -------------------------
def run_synth_tree(lut, init_states, D, tree, cap_mult=4):
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
        cur_est = tree.predict(hist_feats(E.cells_from_states(sub)))
        succ_est = tree.predict(hist_feats(E.cells_from_feats(succ.reshape(k * M.NA, M.NF)))).reshape(k, M.NA)
        masked = np.where(succ_est < cur_est[:, None] - 1e-9, succ_est, np.inf)
        choice = np.where(np.isfinite(masked.min(1)), masked.argmin(1), -1)
        stall = choice < 0
        if stall.any():
            stalled[idx2[stall]] = True; choice = np.where(stall, succ_est.argmin(1), choice)
        cur[idx2] = M.features_to_states(succ[np.arange(k), choice]); path[idx2] += 1
    nonterm = active & ~reached0
    return reached0.mean(), stalled.mean(), nonterm.mean(), path[reached0] / (D / 2)


def sample_hist(lut, depths, n_per_depth, seed):
    Xs, ys = [], []
    for d in depths:
        feats, _, _ = lut.sample_labeled_features_at_t_depth(d, n_per_depth, seed + d)
        Xs.append(hist_feats(E.cells_from_feats(np.asarray(feats, np.float32))))
        ys.append(np.full(len(Xs[-1]), d))
    return np.concatenate(Xs), np.concatenate(ys)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max-t-depth", type=int, default=10)
    ap.add_argument("--n-per-depth", type=int, default=8000)
    ap.add_argument("--m", type=int, default=1500)
    args = ap.parse_args()
    lut = readonly_lut.ReadOnlyLUT(max_t_depth=args.max_t_depth, threads=0, progress_mode="off",
                                   verbose_build=False, debug=False, log_calls=False,
                                   cached_depths=list(range(2, args.max_t_depth + 1, 2)))
    train_depths = list(range(2, args.max_t_depth + 1, 2))
    Xtr, ytr = sample_hist(lut, train_depths, args.n_per_depth, seed=1)
    Xte, yte = sample_hist(lut, train_depths, args.n_per_depth // 2, seed=9999)

    print("denominator-histogram -> T-depth, as a CART regression tree\n")
    print(f"  {'max_depth':>9} {'#leaves':>8} {'test depth-acc':>15}")
    trees = {}
    for md in (2, 3, 4, 6, 8):
        tr = RegTree(max_depth=md, min_leaf=40).fit(Xtr, ytr)
        trees[md] = tr
        pred_even = 2 * np.round(tr.predict(Xte) / 2)
        acc = float((pred_even == yte).mean())
        nleaf = _count_leaves(tr.root)
        print(f"  {md:>9} {nleaf:>8} {100*acc:>14.1f}%")

    print("\n=== readable rule (max_depth=3) ===")
    print_tree(trees[3].root)

    print("\n=== tree-guided synthesizer (uses the depth-8 tree as the potential) ===")
    print(f"  {'depth':>5} {'success':>8} {'stall':>7} {'nonterm':>8} {'f_mean':>8} {'f_med':>7} {'f_max':>7}")
    for D in (8, 10):
        _, states, _ = lut.sample_labeled_features_at_t_depth(D, args.m, 7000 + D)
        states = np.asarray(states, np.uint32)
        sr, st, nt, f = run_synth_tree(lut, states, D, trees[8])
        fm = float(f.mean()) if f.size else float("nan")
        print(f"  {D:>5} {100*sr:>7.1f}% {100*st:>6.1f}% {100*nt:>7.1f}% "
              f"{fm:>8.3f} {float(np.median(f)):>7.3f} {float(f.max()):>7.3f}")


def _count_leaves(nd):
    return 1 if "feature" not in nd else _count_leaves(nd["left"]) + _count_leaves(nd["right"])


if __name__ == "__main__":
    main()
