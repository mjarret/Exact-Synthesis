#!/usr/bin/env python3
"""Does adding numerator-parity features let a COMPACT decision tree act as a near-optimal
greedy synthesis potential for 2-qubit Clifford+T exact synthesis?

Baseline (prior experiment): a CART regression tree on the denominator HISTOGRAM ALONE
(hist_feats = counts n0..n10 + LDE, 12-dim) is a ~93% depth classifier but FAILS as the greedy
potential (stalls everywhere) -- the conclusion was "factor-1 needs ~a 12k-entry lookup ruler".

Here we add a numerator-parity refinement: a 2D histogram of the 36 cells over
(denom_level k in 0..10) x (parity p in {0,1,2,3}, p = 2*(|int_c|&1) + (|sqrt2_c|&1)) -> 44 counts
plus LDE = 45 features. We fit trees on both feature sets, measure held-out depth-accuracy, and
CRUCIALLY use each tree as the greedy potential and report success/stall/factor at depth 10.
"""
from __future__ import annotations
import argparse, time
import numpy as np
import readonly_lut
import measure_approx_factor as M
import extract_decision_features as E
import extract_decision_tree as T

K_LEVELS = 11                                          # denom levels 0..10
N_PAR = 4                                              # parity classes 0..3

# feature names for the parity-refined vector: 44 (k,p) bins then "LDE"
PNAMES = [f"k{k}p{p}" for k in range(K_LEVELS) for p in range(N_PAR)] + ["LDE"]


# ---------------------------- parity-refined feature ---------------------------
def pfeats(cells):                                     # cells (n,36,3) -> (n, 45)
    """2D histogram over (denom_level, parity) + LDE.  p = 2*(|int|&1) + (|sqrt2|&1)."""
    int_c = cells[..., 0]
    sqrt2_c = cells[..., 1]
    de = cells[..., 2]
    par = (np.abs(int_c) & 1) * 2 + (np.abs(sqrt2_c) & 1)          # (n,36) in 0..3
    # clip denom level into 0..10 for binning (baseline hist_feats also buckets only 0..10);
    # the true (unclipped) max is preserved separately as LDE.
    kde = np.clip(de, 0, K_LEVELS - 1)
    code = kde * N_PAR + par                                       # (n,36) in 0..43
    n = cells.shape[0]
    H = np.zeros((n, K_LEVELS * N_PAR), np.int64)
    rows = np.repeat(np.arange(n), 36)
    np.add.at(H, (rows, code.reshape(-1)), 1)
    lde = de.max(1)[:, None]
    return np.concatenate([H, lde], 1).astype(np.float64)


# ---------------------------- sampling training pairs --------------------------
def sample_feats(lut, featfn, depths, n_per_depth, seed):
    Xs, ys = [], []
    for d in depths:
        feats, _, _ = lut.sample_labeled_features_at_t_depth(d, n_per_depth, seed + d)
        Xs.append(featfn(E.cells_from_feats(np.asarray(feats, np.float32))))
        ys.append(np.full(len(Xs[-1]), d))
    return np.concatenate(Xs), np.concatenate(ys)


# ---------------------------- generic tree-guided synthesizer ------------------
def run_synth_tree(lut, init_states, D, tree, featfn, cap_mult=4):
    """Greedy descent using tree.predict(featfn(state)) as the potential.
    Picks the move strictly lowering the prediction (argmin among those); argmin fallback on stall.
    """
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
        cur_est = tree.predict(featfn(E.cells_from_states(sub)))
        succ_est = tree.predict(featfn(E.cells_from_feats(succ.reshape(k * M.NA, M.NF)))).reshape(k, M.NA)
        masked = np.where(succ_est < cur_est[:, None] - 1e-9, succ_est, np.inf)
        choice = np.where(np.isfinite(masked.min(1)), masked.argmin(1), -1)
        stall = choice < 0
        if stall.any():
            stalled[idx2[stall]] = True; choice = np.where(stall, succ_est.argmin(1), choice)
        cur[idx2] = M.features_to_states(succ[np.arange(k), choice]); path[idx2] += 1
    nonterm = active & ~reached0
    return reached0.mean(), stalled.mean(), nonterm.mean(), path[reached0] / (D / 2)


def count_leaves(nd):
    return 1 if "feature" not in nd else count_leaves(nd["left"]) + count_leaves(nd["right"])


def print_tree(nd, names, depth=0, lbl="root: ", max_show=3):
    ind = "   " * depth
    if "feature" not in nd:
        print(f"{ind}{lbl}=> depth ~= {nd['value']:.2f}  (n={nd['n']})"); return
    if depth > max_show:
        print(f"{ind}{lbl}... (subtree, leaf-mean ~= {nd['value']:.2f}, n={nd['n']})"); return
    print(f"{ind}{lbl}if {names[nd['feature']]} <= {nd['thresh']:.0f}:")
    print_tree(nd["left"], names, depth + 1, "then ", max_show)
    print_tree(nd["right"], names, depth + 1, "else ", max_show)


def parity_used_in_top(nd, depth=0, max_depth=3):
    """Return set of feature names appearing in splits at tree-depth <= max_depth."""
    used = set()
    if "feature" not in nd or depth > max_depth:
        return used
    used.add(PNAMES[nd["feature"]])
    used |= parity_used_in_top(nd["left"], depth + 1, max_depth)
    used |= parity_used_in_top(nd["right"], depth + 1, max_depth)
    return used


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--max-t-depth", type=int, default=10)
    ap.add_argument("--n-per-depth", type=int, default=6000)
    ap.add_argument("--n-test", type=int, default=3000)
    ap.add_argument("--m", type=int, default=1500)
    ap.add_argument("--min-leaf", type=int, default=30)
    args = ap.parse_args()

    t0 = time.time()
    lut = readonly_lut.ReadOnlyLUT(max_t_depth=args.max_t_depth, threads=0, progress_mode="off",
                                   verbose_build=False, debug=False, log_calls=False,
                                   cached_depths=list(range(2, args.max_t_depth + 1, 2)))
    print(f"LUT ready ({time.time()-t0:.1f}s)", flush=True)
    train_depths = list(range(2, args.max_t_depth + 1, 2))

    # feature sets: name -> (function, feature-name-list)
    FSETS = {
        "hist(12)": (T.hist_feats, T.NAMES),
        "parity(45)": (pfeats, PNAMES),
    }

    # training / test data per feature set
    data = {}
    for fname, (fn, _) in FSETS.items():
        Xtr, ytr = sample_feats(lut, fn, train_depths, args.n_per_depth, seed=1)
        Xte, yte = sample_feats(lut, fn, train_depths, args.n_test, seed=9999)
        data[fname] = (Xtr, ytr, Xte, yte)
        print(f"  sampled {fname}: train {Xtr.shape}, test {Xte.shape}", flush=True)

    # depth-10 test states for the synthesizer (shared across all configs)
    _, synth_states, _ = lut.sample_labeled_features_at_t_depth(10, args.m, 7010)
    synth_states = np.asarray(synth_states, np.uint32)

    DEPTHS_MD = [6, 8, 10, 12, 14]
    print("\n================ tree-config -> (#leaves, depth-acc, synth) ================")
    print(f"  {'featset':>11} {'max_depth':>9} {'#leaves':>8} {'test-acc':>9} | "
          f"{'success':>8} {'stall':>7} {'nonterm':>8} {'f_mean':>8} {'f_max':>7}")
    results = {fname: [] for fname in FSETS}
    trees = {fname: {} for fname in FSETS}
    for fname, (fn, _) in FSETS.items():
        Xtr, ytr, Xte, yte = data[fname]
        for md in DEPTHS_MD:
            tr = T.RegTree(max_depth=md, min_leaf=args.min_leaf).fit(Xtr, ytr)
            trees[fname][md] = tr
            nleaf = count_leaves(tr.root)
            pred_even = 2 * np.round(tr.predict(Xte) / 2)
            acc = float((pred_even == yte).mean())
            sr, st, nt, f = run_synth_tree(lut, synth_states, 10, tr, fn)
            fm = float(f.mean()) if f.size else float("nan")
            fmx = float(f.max()) if f.size else float("nan")
            results[fname].append((md, nleaf, acc, sr, st, nt, fm, fmx))
            print(f"  {fname:>11} {md:>9} {nleaf:>8} {100*acc:>8.1f}% | "
                  f"{100*sr:>7.1f}% {100*st:>6.1f}% {100*nt:>7.1f}% {fm:>8.3f} {fmx:>7.3f}", flush=True)

    # smallest tree achieving factor<=1.05 AND success>=95% at depth 10, per feature set
    print("\n================ smallest WORKING tree (factor<=1.05 & success>=95%) ================")
    smallest = {}
    for fname in FSETS:
        ok = [r for r in results[fname] if r[6] <= 1.05 and r[3] >= 0.95]
        if ok:
            best = min(ok, key=lambda r: r[1])     # fewest leaves
            smallest[fname] = best
            print(f"  {fname:>11}: max_depth={best[0]}  #leaves={best[1]}  "
                  f"acc={100*best[2]:.1f}%  success={100*best[3]:.1f}%  factor={best[6]:.3f}", flush=True)
        else:
            smallest[fname] = None
            print(f"  {fname:>11}: NONE of the tested trees reached factor<=1.05 & success>=95%", flush=True)

    # is parity actually used in the top splits?
    print("\n================ parity tree top splits (depth<=3) ================")
    # use the smallest working parity tree if any, else the deepest tested
    if smallest["parity(45)"] is not None:
        md_show = smallest["parity(45)"][0]
    else:
        md_show = DEPTHS_MD[-1]
    ptree = trees["parity(45)"][md_show]
    used = parity_used_in_top(ptree.root, max_depth=3)
    par_used = sorted(u for u in used if u != "LDE")
    print(f"  (parity tree at max_depth={md_show}, #leaves={count_leaves(ptree.root)})")
    print(f"  features in splits at tree-depth<=3: {sorted(used)}")
    parity_bins = [u for u in par_used if u.startswith('k')]
    print(f"  parity-refined (k_p) bins used in top splits: {parity_bins if parity_bins else 'NONE'}")
    print("  --- readable parity tree (depth<=3) ---")
    print_tree(ptree.root, PNAMES, max_show=3)

    # SANITY references: ruler and optimal at depth 10
    print("\n================ SANITY references at depth 10 (M=%d) ================" % args.m)
    ruler_map, ruler_acc = M.build_ruler(lut, depths=tuple(train_depths), n_per_depth=15000, seed=0)
    print(f"  ruler keys={len(ruler_map)} train_acc={ruler_acc:.4f}", flush=True)
    for name in ("optimal", "ruler"):
        r = M.run_synth(lut, name, synth_states, 10, ruler=ruler_map, default=args.max_t_depth)
        f = r["factor"]
        fm = float(f.mean()) if f.size else float("nan")
        fmx = float(f.max()) if f.size else float("nan")
        print(f"  {name:>8}: success={100*r['success']:.1f}% stall={100*r['stall']:.1f}% "
              f"nonterm={100*r['nonterm']:.1f}% factor_mean={fm:.3f} factor_max={fmx:.3f}", flush=True)

    print(f"\nTOTAL {time.time()-t0:.1f}s")


if __name__ == "__main__":
    main()
