#!/usr/bin/env python3
"""DISCOVER the residue->block tree by search (not by hand-coding Li et al's case analysis).

For each state, find the SHORTEST sequence of double-T moves that strictly lowers the invariant
(LDE = max denominator exponent), via batched BFS with LDE-non-increasing pruning. Report:
 (1) the worst-case block length k (is it <= 6?), and the distribution;
 (2) whether the block is RESIDUE-DETERMINED -- i.e. do states sharing a GF(2) residue/syndrome key
     share the same minimal block length (and first move)?  If yes, a compact residue->block tree EXISTS
     and we've discovered its size, without ever writing the rules down.
"""
from __future__ import annotations
import numpy as np
from collections import defaultdict, Counter
import readonly_lut
import measure_approx_factor as M
import extract_decision_features as E

CAP = 6


def lde_states(s):
    return ((s.astype(np.int64) >> 16) & 0xFF).reshape(s.shape[0], -1).max(1)


def lde_feats(f):
    n = f.shape[0]
    return np.rint(f.reshape(n, 36, 3)[..., 2]).astype(np.int64).max(1)


def residue_key(states):
    """GF(2) residue/syndrome at the top LDE level, signed-perm invariant (row-localized).
    per cell at LDE: (int_c&1, sqrt2_c&1); cells below LDE -> 'absent' marker. row-sorted."""
    cells = E.cells_from_states(states)            # (n,36,3) int
    n = states.shape[0]
    de = cells[..., 2].reshape(n, 6, 6)
    lde = de.max((1, 2))
    at = (de == lde[:, None, None])                # (n,6,6) cell at top level
    p = (np.abs(cells[..., 0]) & 1) * 2 + (np.abs(cells[..., 1]) & 1)  # parity class 0..3
    p = p.reshape(n, 6, 6)
    code = np.where(at, p + 1, 0)                  # 0 = below LDE, 1..4 = parity class at LDE
    ds = np.sort(code, 2); base = (np.int64(8) ** np.arange(6))
    rcs = np.sort((ds * base).sum(2), 1)
    return [rcs[x].tobytes() for x in range(n)]


def discover_blocks(lut, init_states, cap=CAP):
    n = init_states.shape[0]
    L0 = lde_states(init_states)
    block = np.full(n, -1, np.int64)
    first_move = np.full(n, -1, np.int64)
    frontiers = [{init_states[i].tobytes(): (init_states[i], -1)} for i in range(n)]  # state -> (arr, first_move)
    for depth in range(1, cap + 1):
        flat, owner, fmv = [], [], []
        for i in range(n):
            if block[i] >= 0:
                continue
            for arr, fm in frontiers[i].values():
                flat.append(arr); owner.append(i); fmv.append(fm)
        if not flat:
            break
        flat = np.stack(flat).astype(np.uint32); owner = np.array(owner); fmv = np.array(fmv)
        succ = M.expand_all(lut, flat)             # (F,165,108)
        sl = lde_feats(succ.reshape(len(flat) * M.NA, M.NF)).reshape(len(flat), M.NA)
        reduced = np.zeros(n, bool)
        new_f = [dict() for _ in range(n)]
        for fi in range(len(flat)):
            i = owner[fi]
            if reduced[i]:
                continue
            red = np.where(sl[fi] < L0[i])[0]
            if red.size:
                reduced[i] = True
                first_move[i] = fmv[fi] if depth > 1 else int(red[0])  # first move of the discovered block
                continue
            for a in np.where(sl[fi] <= L0[i])[0]:
                st = M.features_to_states(succ[fi, a][None])[0]
                fm = fmv[fi] if depth > 1 else int(a)
                new_f[i][st.tobytes()] = (st, fm)
        for i in range(n):
            if block[i] >= 0:
                continue
            if reduced[i]:
                block[i] = depth
            else:
                frontiers[i] = new_f[i]
    block[block < 0] = cap + 1
    return block, first_move


def main():
    lut = readonly_lut.ReadOnlyLUT(max_t_depth=10, threads=0, progress_mode="off",
                                   verbose_build=False, debug=False, log_calls=False,
                                   cached_depths=[2, 4, 6, 8, 10])
    print(f"DISCOVERING shortest LDE-reducing block per state (cap={CAP} double-T moves)\n")
    print(f"  {'depth':>5} {'n':>5} {'block-length distribution (1..6, >cap)':>44} {'max_k':>6}")
    allkeys = defaultdict(list)        # residue key -> list of block lengths (pooled across depths)
    allfirst = defaultdict(list)       # residue key -> list of first moves
    for D in (4, 6, 8, 10):
        f, states, _ = lut.sample_labeled_features_at_t_depth(D, 3000, 4000 + D)
        states = np.asarray(states, np.uint32)
        block, fmv = discover_blocks(lut, states)
        dist = {k: int((block == k).sum()) for k in range(1, CAP + 2)}
        kmax = int(block.max())
        ds = " ".join(f"{k}:{dist[k]}" for k in range(1, CAP + 2) if dist[k])
        print(f"  {D:>5} {len(states):>5}   {ds:<44} {kmax:>6}", flush=True)
        keys = residue_key(states)
        for i, k in enumerate(keys):
            allkeys[k].append(int(block[i])); allfirst[k].append(int(fmv[i]))

    # residue-determinism: within a residue class, is the block length / first move constant?
    multi = {k: v for k, v in allkeys.items() if len(v) >= 2}
    blk_pure = sum(len(v) for v in multi.values() if len(set(v)) == 1)
    fm_pure = sum(len(allfirst[k]) for k in multi if len(set(allfirst[k])) == 1)
    tot = sum(len(v) for v in multi.values())
    print(f"\nresidue-determinism (states in residue classes seen >=2x, pooled all depths):")
    print(f"  #residue classes={len(allkeys)} | testable states={tot}")
    print(f"  block-length determined by residue: {100*blk_pure/max(tot,1):.1f}%")
    print(f"  first-move determined by residue:   {100*fm_pure/max(tot,1):.1f}%")
    print(f"  => if high, a compact residue->block TREE exists (discovered, not hand-coded).")


if __name__ == "__main__":
    main()
