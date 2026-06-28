#!/usr/bin/env python3
"""
deep_mitm_probe.py  --  Push the SO(6) double-T move-structure check to high T-count.

WHAT IT MEASURES (using meet-in-the-middle as a *ground-truth* optimal-depth oracle, so it
is exact well beyond the stored LUT):

  For random states at true optimal double-T depth D (D = 16,18,20,22,24,...):
    (1) %1move  : fraction with at least one single double-T move that lowers optimal depth
                  to D-2 (a "reducer").  Below the LUT this is ~100%.  Does it hold deep?
    (2) delta   : distribution of the max-denominator-exponent change (delta_LDE) of those
                  reducers.  The delta_LDE = 0 family (reducers that lower true T-count WITHOUT
                  lowering the max denominator) was ~16% at depth <=16 -- does it persist?
    (3) %quadT  : fraction with a depth-4-reducing quad-T (a 2-double-T composite: D -> D-4).
                  k=1 under quad-T was confirmed at depth 12 -- does it hold deep?

Runs on a LARGE-MEMORY machine.  The left LUT is built once to --left-depth; each target at
optimal depth D then needs the right side grown only to D-left_depth, so a deeper left LUT
makes every per-target build smaller (this is what makes depth 20-24 feasible).

OUTPUT: a JSON file (--out) I can interpret directly, plus a human-readable table on stdout.
Results are written incrementally (after each depth) and the pool is cached, so an OOM on the
deepest depth still leaves you the shallower results, and re-running resumes.

MEMORY GUIDANCE (single-T LUT layer sizes: depth8~1.4e4, depth10~8.5e5, depth12~1.7e7):
  --left-depth 10 : left LUT ~0.5 GB.  Right build to depth d expands each frontier state x15
                    BEFORE dedup, so peak ~ 15 x (layer size at d).  depth-20 target -> right
                    to 10 -> transient tens of GB.  (This is what OOMs a 14 GB laptop at D>=18.)
  --left-depth 12 : left LUT ~10-20 GB built once; right builds are 2 layers shallower per
                    target.  Recommended for depth 20-24 on a >=128 GB machine.
Start with the defaults; if pool-gen or a depth OOMs, lower --depths / --max-right-depth, or
raise --left-depth (more upfront RAM, cheaper per-target builds).
"""
from __future__ import annotations
import argparse, json, os, resource, threading, time
import numpy as np
import readonly_lut

NA = 165                                   # number of double-T generators
IDENT = np.eye(6, dtype=np.uint32)
_T0 = time.time()                          # wall clock for elapsed-time stamps


def _el():
    return time.time() - _T0


class Heartbeat:
    """Print a liveness line every `interval`s while a blocking/silent phase runs."""
    def __init__(self, label, interval=15):
        self.label, self.interval, self._stop = label, interval, threading.Event()
        self._t = threading.Thread(target=self._run, daemon=True)

    def _run(self):
        while not self._stop.wait(self.interval):
            print(f"    ... {self.label}: +{_el():.0f}s elapsed, RSS {rss_gib():.1f} GiB", flush=True)

    def __enter__(self):
        self._t.start(); return self

    def __exit__(self, *a):
        self._stop.set()

# Measured identity-rooted canonical single-T layer sizes d=0..10 (this codebase). Growth is
# ~8x/level and still rising toward an asymptote ~9.8x; we extrapolate to project memory so a
# deep left-LUT build doesn't OOM after hours. ~300 bytes/state measured (final stored LUT).
MEASURED = [1, 1, 2, 6, 19, 77, 371, 2159, 14455, 106833, 846341]
BYTES_PER_STATE = 300


def project_layer_sizes(depth, asymptote=9.8, decay=0.78):
    sizes = [float(x) for x in MEASURED]
    g0 = sizes[10] / sizes[9]                                   # 7.92 at d10
    while len(sizes) - 1 < depth:
        k = len(sizes) - 10                                     # 1,2,3,...
        factor = asymptote - (asymptote - g0) * (decay ** k)    # rises toward the asymptote
        sizes.append(sizes[-1] * factor)
    return sizes


def _gb(states):
    return states * BYTES_PER_STATE / 1e9


def preflight(left_depth, want):
    """Print projected memory for the once-built left LUT and the per-target right builds."""
    sizes = project_layer_sizes(max(left_depth, max(want) - left_depth, 10))
    left_cum = sum(sizes[:left_depth + 1])
    measured = left_depth <= 10
    print(f"  left LUT to single-T depth {left_depth}: ~{left_cum:.3g} states  "
          f"~{_gb(left_cum):.0f} GB final (~{_gb(left_cum)*1.7:.0f} GB peak build)"
          f"{'  [MEASURED]' if measured else '  [extrapolated]'}")
    if _gb(left_cum) * 1.7 > 1400:
        print(f"  !! WARNING: projected build peak ~{_gb(left_cum)*1.7:.0f} GB may exceed ~1.5 TB. "
              f"Use a shallower --left-depth (13 is the practical max).")
    print(f"  per-target right builds (cap = D - left_depth):")
    for D in want:
        r = D - left_depth
        if r < 0:
            print(f"    depth {D}: target already <= left LUT; instant (no right build)")
        else:
            rcum = sum(sizes[:r + 1])
            speed = "fast" if r <= 9 else ("moderate, ~min/target" if r <= 11 else "heavy, minutes/target")
            print(f"    depth {D}: right to single-T {r}  ~{rcum:.3g} states  ~{_gb(rcum):.1f} GB ({speed})")


# ---- packing helpers (inverse of the binding's feature writer) --------------------------
def features_to_states(feats):
    n = feats.shape[0]
    c = np.rint(feats.reshape(n, 36, 3)).astype(np.int64)
    int_c, sqrt2_c, denom = c[..., 0], c[..., 1], c[..., 2]
    data = ((denom & 0xFF) << 16) | ((sqrt2_c & 0xFF) << 8) | (int_c & 0xFF)
    return np.ascontiguousarray(data.reshape(n, 6, 6).astype(np.uint32))


def lde_states(s):
    return ((s.astype(np.int64) >> 16) & 0xFF).reshape(s.shape[0], -1).max(1)


def lde_feats(f):
    n = f.shape[0]
    return np.rint(f.reshape(n, 36, 3)[..., 2]).astype(np.int64).max(1)


def expand_all(lut, states):
    """Apply all 165 double-T generators to each state -> (n,165,108) raw features."""
    n = states.shape[0]
    alpha = np.ascontiguousarray(np.broadcast_to(np.arange(NA, dtype=np.int64), (n, NA)))
    out = np.asarray(lut.apply_tt_features_batch(np.ascontiguousarray(states), alpha), np.float32)
    return out.reshape(n, NA, 108)


def mitm(lut, states, mrd, to):
    """Optimal double-T depth via MITM, capped: returns depth if <= left+mrd else -1."""
    arr = np.ascontiguousarray(states.astype(np.uint32))
    return np.asarray(lut.mitm_optimal_depth(arr, int(mrd), float(to)), np.int64)


def chain(lut, k, seed):
    r = np.random.default_rng(seed)
    s = np.ascontiguousarray(IDENT[None].copy())
    for _ in range(k):
        a = np.array([[int(r.integers(0, NA))]], np.int64)
        s = features_to_states(np.asarray(lut.apply_tt_features_batch(s, a), np.float32).reshape(1, 108))
    return s[0]


def rss_gib():
    return resource.getrusage(resource.RUSAGE_SELF).ru_maxrss / 1024 / 1024


# ---- pool generation: random chains, MITM-binned by true optimal depth (resumable) ------
def gen_pool(lut, want, left_depth, mrd, to, n_chains, per, chunk=8, seed0=0,
             cache="runs/deep_pool.npz"):
    os.makedirs(os.path.dirname(cache), exist_ok=True)
    max_k = max(want) // 2 + 3                                               # chain length to exceed deepest target
    ks = np.clip(np.linspace(min(want) // 2, max_k, n_chains).round().astype(int), 1, None)
    if os.path.exists(cache):
        z = np.load(cache)
        chains, depths = z["chains"].astype(np.uint32), z["depths"].astype(np.int64)
        print(f"  resuming pool from {cache}: {(depths != -2).sum()}/{len(depths)} chains binned", flush=True)
    else:
        print(f"  [+{_el():.0f}s] generating {n_chains} random chains (k in [{ks.min()},{ks.max()}]) ...", flush=True)
        with Heartbeat("generating chains"):
            chains = np.stack([chain(lut, int(ks[i]), seed0 + i) for i in range(n_chains)]).astype(np.uint32)
        depths = np.full(n_chains, -2, np.int64)                                # -2 = not yet binned
    todo = np.where(depths == -2)[0]
    t_start, done0 = time.time(), int((depths != -2).sum())
    for c0 in range(0, len(todo), chunk):
        idx = todo[c0:c0 + chunk]
        t = time.time()
        depths[idx] = mitm(lut, chains[idx], mrd, to)
        np.savez(cache, chains=chains, depths=depths)
        done = int((depths != -2).sum())
        rate = max(done - done0, 1) / max(time.time() - t_start, 1e-6)       # chains/s over this run
        eta = (n_chains - done) / rate
        bins = {int(d): int((depths == d).sum()) for d in want}              # the bins we actually want
        miss = int((depths == -1).sum())                                    # >cap (deeper than reachable) / timeout
        print(f"  [+{_el():.0f}s] binned {done}/{n_chains} ({100*done/n_chains:.0f}%)  "
              f"+{len(idx)} in {time.time()-t:.1f}s (~{eta:.0f}s left)  "
              f"target bins {bins}  dropped>{left_depth+mrd}:{miss}  RSS {rss_gib():.1f}G", flush=True)
    pools = {d: chains[depths == d][:per] for d in want if (depths == d).sum() >= 3}
    return pools, {int(d): int((depths == d).sum()) for d in sorted(set(depths[depths >= 0].tolist()))}


# ---- per-depth structure analysis -------------------------------------------------------
def analyze(lut, states, D, left_depth, to, topk=8, do_quad=True):
    n = states.shape[0]
    succ = expand_all(lut, states)                                              # (n,165,108)
    delta = lde_feats(succ.reshape(n * NA, 108)).reshape(n, NA) - lde_states(states)[:, None]
    red_cap = max(0, (D - 2) - left_depth)                                      # confirm D-2 cheaply
    quad_cap = max(0, (D - 4) - left_depth)
    print(f"  [+{_el():.0f}s] [analyze depth {D}] {n} states  (reducer right-cap {red_cap}, "
          f"quad right-cap {quad_cap})", flush=True)
    with_red, with_quad, found_deltas = 0, 0, []
    for i in range(n):
        order = np.argsort(delta[i])[:topk]                                     # most promising first
        cs = features_to_states(succ[i, order])
        cd = mitm(lut, cs, red_cap, to)
        red_local = (cd >= 0) & (cd < D)                                        # found in cap => reduced
        red = order[red_local]
        quad_hit, best = False, None
        if red.size:
            with_red += 1
            found_deltas += delta[i, red].tolist()
            best = int(delta[i, red].min())                                     # best (most negative) reducer delta_LDE
            if do_quad:                                                         # follow the first reducer
                r = int(red[0])
                g = expand_all(lut, features_to_states(succ[i, r][None]))[0]    # (165,108) grandchildren
                gdelta = lde_feats(g) - lde_states(features_to_states(succ[i, r][None]))[0]
                go = np.argsort(gdelta)[:topk]
                gd = mitm(lut, features_to_states(g[go]), quad_cap, to)
                quad_hit = bool(((gd >= 0) & (gd <= D - 4)).any())
                with_quad += int(quad_hit)
        print(f"      state {i+1:>2}/{n}: reducer={'YES' if red.size else 'no '}"
              f"  ΔLDE={'%+d' % best if best is not None else ' . '}"
              f"  quadT={'yes' if quad_hit else '-'}"
              f"   (running: {with_red}/{i+1} reduce, {with_quad}/{i+1} quad)", flush=True)
    dd = np.array(found_deltas)
    dist = {int(v): int((dd == v).sum()) for v in sorted(set(dd.tolist()))} if dd.size else {}
    return {
        "depth": D, "n": int(n),
        "pct_1move_reducer": round(100 * with_red / n, 1),
        "reducer_delta_lde_dist": dist,
        "pct_quadT_k1": round(100 * with_quad / n, 1) if do_quad else None,
    }


def main():
    p = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    p.add_argument("--left-depth", type=int, default=12,
                   help="single-T depth of the once-built left LUT (12 ~21GB; 13 ~185GB is the 1.5TB max)")
    p.add_argument("--depths", type=int, nargs="+", default=[16, 18, 20],
                   help="target single-T depths; with left=12 these need right<=8 (fast). Reaches depth 20.")
    p.add_argument("--max-right-depth", type=int, default=None,
                   help="right-side build cap for pool binning (default = max(depths) - left_depth)")
    p.add_argument("--m", type=int, default=12, help="states analyzed per depth")
    p.add_argument("--n-chains", type=int, default=250)
    p.add_argument("--timeout", type=float, default=600.0, help="per-MITM-call timeout (s)")
    p.add_argument("--no-quad", action="store_true", help="skip the quad-T k=1 check")
    p.add_argument("--estimate-only", action="store_true", help="print projected memory and exit (no build)")
    p.add_argument("--out", default="runs/deep_results.json")
    p.add_argument("--pool", default="runs/deep_pool.npz")
    args = p.parse_args()
    want = sorted(args.depths)
    mrd = args.max_right_depth if args.max_right_depth is not None else max(want) - args.left_depth
    cached = list(range(2, args.left_depth + 1, 2))

    print("== projected memory (preflight) ==")
    preflight(args.left_depth, want)
    print()
    if args.estimate_only:
        return

    os.makedirs(os.path.dirname(args.out), exist_ok=True)
    meta = {"params": {**vars(args), "want": want, "pool_max_right_depth": mrd},
            "legend": {
                "pct_1move_reducer": "fraction of states with a single double-T move reaching optimal depth D-2",
                "reducer_delta_lde_dist": "delta of max-denominator-exponent for those reducers; key 0 = lowers T-count WITHOUT lowering max denominator",
                "pct_quadT_k1": "fraction with a depth-4-reducing quad-T (2-double-T composite, D -> D-4)"},
            "results": []}

    t0 = time.time()
    print(f"\n== [phase 1/3] building left LUT to single-T depth {args.left_depth} (cached {cached}) ==\n"
          f"  (silent C++ build; heartbeat every 15s -- watch RSS climb toward "
          f"~{_gb(sum(project_layer_sizes(args.left_depth)[:args.left_depth+1])):.0f} GB)", flush=True)
    with Heartbeat("building left LUT"):
        lut = readonly_lut.ReadOnlyLUT(max_t_depth=args.left_depth, threads=0, progress_mode="off",
                                       verbose_build=False, debug=False, log_calls=False,
                                       cached_depths=cached)
    print(f"  [+{_el():.0f}s] left LUT ready in {time.time()-t0:.0f}s, RSS {rss_gib():.1f} GiB", flush=True)

    print(f"\n== [phase 2/3] binning pool by MITM (ground truth, reaches depth {args.left_depth + mrd}) ==", flush=True)
    pools, hist = gen_pool(lut, want, args.left_depth, mrd, args.timeout, args.n_chains, args.m,
                           cache=args.pool)
    meta["pool_depth_histogram"] = hist
    print(f"  [+{_el():.0f}s] pool ready: { {d: len(v) for d, v in pools.items()} }   (full hist: {hist})", flush=True)

    print(f"\n== [phase 3/3] analyzing structure per depth ==", flush=True)
    print(f"  {'depth':>5} {'n':>4} {'%1move':>7} {'%quadT':>7}  reducer delta_LDE dist")
    for D in sorted(pools):
        t = time.time()
        r = analyze(lut, pools[D], D, args.left_depth, args.timeout, do_quad=not args.no_quad)
        r["seconds"] = round(time.time() - t, 1)
        r["peak_rss_gib"] = round(rss_gib(), 2)
        meta["results"].append(r)
        with open(args.out, "w") as f:                                          # incremental write
            json.dump(meta, f, indent=2)
        q = "-" if r["pct_quadT_k1"] is None else f"{r['pct_quadT_k1']:.0f}%"
        print(f"  {D:>5} {r['n']:>4} {r['pct_1move_reducer']:>6.0f}% {q:>7}  "
              f"{r['reducer_delta_lde_dist']}   ({r['seconds']:.0f}s, peak {r['peak_rss_gib']:.1f}G)",
              flush=True)

    meta["total_seconds"] = round(time.time() - t0, 1)
    meta["peak_rss_gib"] = round(rss_gib(), 2)
    with open(args.out, "w") as f:
        json.dump(meta, f, indent=2)
    print(f"\nwrote {args.out}  ({meta['total_seconds']:.0f}s total, peak {meta['peak_rss_gib']:.1f} GiB)")
    print("Send me that JSON file and I'll interpret it.")


if __name__ == "__main__":
    main()
