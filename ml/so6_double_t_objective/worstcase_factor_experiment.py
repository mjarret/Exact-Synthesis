#!/usr/bin/env python3
"""Empirical WORST-CASE approximation factor of greedy double-T synthesizers (2-qubit Clifford+T).

Conjecture under test: 2-qubit worst-case factor <= 2^{2w-1} = 8 for w=2 qubits.
factor = synthesizer double-T move count / optimal (D/2).

Reuses measure_approx_factor (M): LUT build, ruler, run_synth, expand_all, lde helpers.
"""
from __future__ import annotations
import time
import numpy as np
import measure_approx_factor as M

POLICIES = ("optimal", "lde", "ruler")
TEST_DEPTHS = (4, 6, 8, 10)
M_TEST = 10000
RULER_DEPTHS = (2, 4, 6, 8, 10)
RULER_N = 20000
MAX_T_DEPTH = 10
import readonly_lut


def fstats(f):
    if f.size == 0:
        return dict(n=0, mean=float("nan"), med=float("nan"), p99=float("nan"), mx=float("nan"))
    return dict(n=int(f.size), mean=float(f.mean()), med=float(np.median(f)),
                p99=float(np.percentile(f, 99)), mx=float(f.max()))


def no_lde_lowering_fraction(lut, states):
    """Fraction of states for which EVERY reducing (D->D-2) move has delta_LDE >= 0,
    i.e. no single double-T move both reduces T-count AND strictly lowers LDE.
    These are the states where naive single-move LDE-greedy must stall."""
    states = np.ascontiguousarray(states)
    n = states.shape[0]
    cur_lde = M.lde_from_states(states)                       # (n,)
    red = np.asarray(lut.reducing_mask_of(states), np.uint8).astype(bool)   # (n,165) ground-truth reducing moves
    succ = M.expand_all(lut, states)                          # (n,165,108)
    succ_lde = M.lde_from_succ(succ)                          # (n,165)
    # delta_LDE for reducing moves only; non-reducing set to +inf so they don't count
    BIG = np.int64(1 << 30)
    red_succ_lde = np.where(red, succ_lde, BIG)               # (n,165)
    min_red_lde = red_succ_lde.min(1)                         # smallest successor LDE among reducing moves
    has_reducing = red.any(1)
    # "lowers LDE" means some reducing move has successor LDE < current LDE
    has_lowering_reducer = has_reducing & (min_red_lde < cur_lde)
    # stall states = have a reducing move but NONE of them lowers LDE
    stall_states = has_reducing & ~has_lowering_reducer
    return dict(frac_no_lowering=float(stall_states.mean()),
                frac_has_reducing=float(has_reducing.mean()),
                n=n, n_stall=int(stall_states.sum()))


def main():
    t0 = time.time()
    lut = readonly_lut.ReadOnlyLUT(max_t_depth=MAX_T_DEPTH, threads=0, progress_mode="off",
                                   verbose_build=False, debug=False, log_calls=False,
                                   cached_depths=[2, 4, 6, 8, 10])
    print(f"[{time.time()-t0:.1f}s] LUT ready", flush=True)

    ruler_map, ruler_acc = M.build_ruler(lut, depths=RULER_DEPTHS, n_per_depth=RULER_N, seed=0)
    print(f"[{time.time()-t0:.1f}s] ruler built: {len(ruler_map)} keys, train-accuracy {ruler_acc:.4f}", flush=True)

    # ---- main per-policy per-depth table
    print("\n==== TABLE 1: empirical approximation factor (factor = moves / optimal(D/2)) ====", flush=True)
    print(f"{'D':>3} {'policy':>8} {'success%':>8} {'stall%':>7} {'nonterm%':>9} "
          f"{'f_mean':>8} {'f_med':>7} {'f_p99':>7} {'f_MAX':>8} {'nsucc':>7}", flush=True)
    results = {}
    # for the terminating-only worst case of lde we keep its raw factor array per depth
    for D in TEST_DEPTHS:
        feats, states, _m = lut.sample_labeled_features_at_t_depth(D, M_TEST, 7000 + D)
        states = np.ascontiguousarray(np.asarray(states, np.uint32))
        for name in POLICIES:
            r = M.run_synth(lut, name, states, D, ruler=ruler_map, default=MAX_T_DEPTH, stall="sideways")
            s = fstats(r["factor"])
            results[(D, name)] = (r, s)
            print(f"{D:>3} {name:>8} {100*r['success']:>7.1f}% {100*r['stall']:>6.1f}% "
                  f"{100*r['nonterm']:>8.1f}% {s['mean']:>8.3f} {s['med']:>7.3f} "
                  f"{s['p99']:>7.3f} {s['mx']:>8.3f} {s['n']:>7d}", flush=True)
            if name == "optimal":
                assert abs(r["success"] - 1.0) < 1e-9 and (r["factor"].size == 0 or np.allclose(r["factor"], 1.0)), \
                    f"OPTIMAL SANITY FAILED at D={D}"
        print(flush=True)

    # ---- stall-source analysis for lde: no-LDE-lowering reducing-move fraction
    print("==== TABLE 2: lde stall sources (no single reducing move lowers LDE) ====", flush=True)
    print(f"{'D':>3} {'n':>7} {'has_reducing%':>14} {'no_LDE_lowering%':>17} {'n_stall':>8}", flush=True)
    stall_frac = {}
    for D in TEST_DEPTHS:
        _f, states, _m = lut.sample_labeled_features_at_t_depth(D, M_TEST, 7000 + D)
        states = np.ascontiguousarray(np.asarray(states, np.uint32))
        nl = no_lde_lowering_fraction(lut, states)
        stall_frac[D] = nl
        print(f"{D:>3} {nl['n']:>7d} {100*nl['frac_has_reducing']:>13.2f}% "
              f"{100*nl['frac_no_lowering']:>16.3f}% {nl['n_stall']:>8d}", flush=True)
    print(flush=True)

    # ---- TABLE 3: lde worst-case restricted to terminating runs + nonterm trend
    print("==== TABLE 3: lde policy restricted to TERMINATING runs (success rows only) ====", flush=True)
    print(f"{'D':>3} {'nonterm%':>9} {'success%':>8} {'term_f_MAX':>11} {'term_f_p99':>11} {'term_f_mean':>12}", flush=True)
    for D in TEST_DEPTHS:
        r, s = results[(D, "lde")]
        # factor array already only over successful (terminating) rows
        print(f"{D:>3} {100*r['nonterm']:>8.1f}% {100*r['success']:>7.1f}% "
              f"{s['mx']:>11.3f} {s['p99']:>11.3f} {s['mean']:>12.3f}", flush=True)
    print(flush=True)

    # ---- compact verdict numbers
    lde_max = max(results[(D, 'lde')][1]['mx'] for D in TEST_DEPTHS)
    ruler_max = max(results[(D, 'ruler')][1]['mx'] for D in TEST_DEPTHS)
    lde_nonterm = {D: results[(D, 'lde')][0]['nonterm'] for D in TEST_DEPTHS}
    ruler_nonterm = {D: results[(D, 'ruler')][0]['nonterm'] for D in TEST_DEPTHS}
    print("==== SUMMARY NUMBERS ====", flush=True)
    print(f"lde   overall MAX factor (terminating rows) across depths = {lde_max:.3f}  (bound 8 -> slack {8.0/lde_max:.2f}x)", flush=True)
    print(f"ruler overall MAX factor (terminating rows) across depths = {ruler_max:.3f}  (bound 8 -> slack {8.0/ruler_max:.2f}x)", flush=True)
    print(f"lde   nonterm rate by depth   = " + ", ".join(f"D{D}:{100*lde_nonterm[D]:.1f}%" for D in TEST_DEPTHS), flush=True)
    print(f"ruler nonterm rate by depth   = " + ", ".join(f"D{D}:{100*ruler_nonterm[D]:.1f}%" for D in TEST_DEPTHS), flush=True)
    print(f"no-LDE-lowering reducing frac = " + ", ".join(f"D{D}:{100*stall_frac[D]['frac_no_lowering']:.2f}%" for D in TEST_DEPTHS), flush=True)
    print(f"[{time.time()-t0:.1f}s] done", flush=True)


if __name__ == "__main__":
    main()
