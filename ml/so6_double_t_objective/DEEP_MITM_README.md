# Deep MITM structure probe — run on a large-memory machine

This pushes the SO(6) double-T move-structure check to high T-count (depth 16–24), using
meet-in-the-middle (MITM) as a **ground-truth optimal-depth oracle** so the results are exact
well beyond the stored depth-≤10 LUT. It exists because the 14 GB dev box OOMs on the
right-side LUT build needed for depth ≥ 18.

## What it answers

At each target depth D (true optimal double-T depth, MITM-verified), over a sample of random
states, it reports three numbers:

| metric | meaning | what we've seen at D ≤ 16 |
|---|---|---|
| `pct_1move_reducer` | % of states with one double-T move that lowers optimal depth to D−2 | ~97–100% |
| `reducer_delta_lde_dist` | distribution of max-denominator-exponent change of those reducers; **key `0`** = lowers true T-count *without* lowering the max denominator (the "LDE-blind" family) | key `0` ≈ 16% at every depth |
| `pct_quadT_k1` | % with a depth-4-reducing quad-T (a 2-double-T composite, D→D−4) | 100% at depth 12 |

The question this run settles: **does this structure stay the same out at depth 18/20/22/24,
or does something new appear at high T-count?** (The user's worry: maybe we haven't been at
high enough T-count to observe everything.)

## Prerequisites

- A C++20 compiler (`g++` ≥ 10 / `clang++` ≥ 12) and **Intel TBB** (`apt install libtbb-dev g++`).
- Python 3 with `numpy` and `pybind11` (the script pip-installs the latter two if missing).
- The **full Exact-Synthesis repo**, copied/cloned to the machine (the binding compiles
  `src/SO6.cpp`, `src/MITM.cpp`, `src/TT_Operator.cpp`, etc.). The MITM-oracle additions live
  in `ml/so6_double_t_objective/readonly_lut_exact_synthesis.cpp` and `setup.py` — make sure you
  copy the working tree, not a clean checkout that predates them.
- **Do not copy a prebuilt `.so`** — the build uses `-march=native`, so it must be compiled on
  the machine that runs it (a foreign `.so` SIGILLs).

## Run

```bash
cd ml/so6_double_t_objective
LEFT_DEPTH=12 DEPTHS="16 18 20 22 24" M=12 NCHAINS=250 TIMEOUT=600 ./run_deep_mitm.sh
```

It builds the binding, then writes `runs/deep_results.json` (and prints a table). **Send that
JSON back** and I'll interpret it.

## How to size it to your machine

Strategy: **deep left, shallow right.** The left LUT is built **once** to `LEFT_DEPTH`; a target
at depth D then needs the right side grown only to `D − LEFT_DEPTH`. So a deeper left LUT makes
every per-target right build smaller — that's what gives both reach and speed.

The two sides grow at different rates. The left LUT is identity-rooted and canonical; the
identity ball is symmetry-invariant, so canonicalization collapses it. A *target*-rooted right
ball isn't symmetry-invariant, so it doesn't collapse — the right side is bigger at the same
radius (and is rebuilt per target). Both reasons say: keep the right shallow.

**Measured** identity-rooted single-T layer sizes (this codebase), growth ~8×/level and rising:

| d | 6 | 7 | 8 | 9 | 10 | 12 (extrap) | 13 (extrap) | 14 (extrap) | 16 (extrap) |
|---|---|---|---|---|---|---|---|---|---|
| states | 371 | 2.2e3 | 1.4e4 | 1.1e5 | 8.5e5 | ~6e7 | ~5e8 | ~5e9 | ~4e11 |

At ~300 bytes/state, the **left LUT** costs (cumulative, +~70% build transient):

| `LEFT_DEPTH` | left LUT size | builds on 1.5 TB? | depth-24 right build |
|---|---|---|---|
| 12 | ~21 GB | trivially | right-to-12 ≈ 21 GB/target (slow) |
| **13** | **~185 GB** (~315 GB build) | **comfortably — recommended** | right-to-11 ≈ 2.4 GB/target (fast) |
| 14 | ~1.7 TB (~2.8 TB build) | **no — overflows** | right-to-10 ≈ 0.3 GB/target |
| 16 | ~80 TB | impossible | — |

So **`LEFT_DEPTH=13` is the sweet spot on 1.5 TB**: the left LUT fits with room to spare, and
every per-target right build for depth ≤ 24 stays ≤ ~2.4 GB (the reducer checks during analysis
are capped even shallower). `LEFT_DEPTH=12` also reaches 24 but with ~21 GB right builds per
depth-24 target (slower). `LEFT_DEPTH≥14` overflows — don't.

**Check before you commit a multi-hour build:**
```bash
python deep_mitm_probe.py --left-depth 13 --depths 16 18 20 22 24 --estimate-only
```
prints the projected left-LUT memory and per-target right-build sizes, and warns if it would
exceed ~1.5 TB. (`run_deep_mitm.sh` defaults to `LEFT_DEPTH=13`.)

**It degrades gracefully.** Results are written after each depth and the pool is cached
(`runs/deep_pool.npz`), so if the deepest depth OOMs you keep everything shallower, and
re-running resumes the pool binning. If a depth is too slow or OOMs, lower `DEPTHS`, lower
`TIMEOUT`, or `LEFT_DEPTH`.

Knobs: `LEFT_DEPTH`, `DEPTHS`, `M` (states/depth), `NCHAINS` (pool size — deeper targets are
rarer in a random walk, so raise this if a deep bin is thin), `TIMEOUT` (per-MITM-call seconds),
`MAXRD` (override the right-build cap), `PYTHON` (interpreter). `--no-quad` skips the quad-T check.

## Output (`runs/deep_results.json`)

```json
{
  "params": { ... },
  "legend": { ...per-metric explanation... },
  "pool_depth_histogram": { "12": 40, "14": 33, "16": 21, "18": 15, ... },
  "results": [
    { "depth": 18, "n": 12, "pct_1move_reducer": 100.0,
      "reducer_delta_lde_dist": {"-2": 28, "-1": 41, "0": 14},
      "pct_quadT_k1": 100.0, "seconds": 312.0, "peak_rss_gib": 41.3 },
    ...
  ]
}
```

The `pool_depth_histogram` also tells me how deep your machine actually reached and how many
samples each depth got, so even a partial run is interpretable.
