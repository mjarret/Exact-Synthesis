# Exact-Synthesis

Exact-Synthesis is a C++20 project for generating and analyzing 6×6 matrices over Z[√2] (SO6) under T‑operator products. It builds layered lookup tables (LUTs) by T‑depth, with parallel generation and compact finalization.

- Language: C++20
- Parallelism: Intel oneTBB
- Build: Makefile (clang by default; `COMPILER=gcc` selects g++)
- UI: Lightweight CLI (header‑only cxxopts shim)
- Progress: Trimmed, vendored indicators (MIT) with terminal bars

## Quick Start

Requirements
- g++ or clang with C++20
- oneTBB development libraries (e.g., `sudo apt install -y libtbb-dev`)

Build
```
make
```
Run
```
./main.out --help
```
Example
```
./main.out -t 8 -s 4 -n max
```

## CLI Options
The CLI is defined in `src/Globals.cpp` and supports:

- `-h, --help`                     Show help
- `-t, --tcount <int>`             Target T count (default: 8)
- `-s, --stored_depth <int>`       Maximum stored depth (default: 0; auto‑adjusted in configure)
- `-f, --pattern_file <path>`      Pattern file (optional)
- `-n, --threads <N|max>`          Number of threads or `max` (default: hardware_concurrency-1)
- `-r, --root <string>`            Root circuit string (optional)
- `-c, --cases`                    Flag for specific cases (not used; prints mode)

Notes
- `stored_depth` is normalized in `Globals::configure()` to `[ceil(tcount/2), tcount-1]`.
- When `--threads max` is given, the app uses all hardware threads. Otherwise parses an integer.

## Repository Layout
- `apps/`          Executables (entrypoints)
- `include/`       Public headers
  - `DyadicSqrt2.hpp`  Numeric core: Z[1/√2] (packed 24‑bit elements)
  - `so6/`         SO6 core (`SO6.hpp`, `T_Operator.hpp`)
  - `ds/LUT.hpp`   Rooted BFS/LUT
  - `ds/`          Data structures (SmallFreqMap, Lehmer6, …)
  - `util/`        Progress tracker and I/O helpers (uses trimmed indicators)
  - `sys/`         Memory helpers
  - `third_party/` Vendored header‑only deps (cxxopts, ankerl, indicators)
- `src/`           Implementations (`SO6.cpp`, `algo/*.cpp`)
- `tests/`         Small tests/bench helpers
- `docs/`          Architecture and code map
- `benchmarks/`    Google Benchmark microbenchmarks

## Build Targets
- `make`               Build `main.out`
- `make clean`         Remove objects and binary
- `make debug`         Build with `-g` and link with `-ltcmalloc` (if installed)
- `make asan`          Build with AddressSanitizer
- `make ubsan`         Build with UBSan
- `make docs`          Generate Doxygen docs (if `doxygen` is installed)

## Performance & Memory
- Generation uses `tbb::parallel_for_each` and a per‑layer working set; finalization compacts into a hash set.
- Progress bars show elapsed/remaining time and process RSS.

## Hash Backends

Container choice is fixed in `include/ds/LUT.hpp`; there is no compile‑time backend switch.

- Working set (during layer expansion): `tbb::concurrent_unordered_set<SO6>`.
- Finalized layers: `ankerl::unordered_dense::set<SO6, FinalizedSetHash, std::equal_to<SO6>>`,
  keyed by `SO6::primary_hash()` (a precomputed 16‑bit signature). Structural equality
  (`std::equal_to<SO6>`) is the authoritative match, so the 16‑bit hash only steers bucket
  distribution.

The numeric backend is likewise unified on `DyadicSqrt2` (see the Makefile note: "unified on Dyadic; no compile‑time switch"). The only vendored hash header is `ankerl` (`include/third_party/ankerl/unordered_dense.h`).

## Third‑Party and Licensing
- Indicators (MIT) reduced to required parts; see `include/third_party/indicators/NOTICE` and `include/third_party/indicators/LICENSE`.
- termcolor (BSD) and Unicode wcwidth (Markus Kuhn) are included under their original notices; see `include/third_party/indicators/*`.
- CLI uses a lightweight, vendored `cxxopts.hpp` shim (`include/third_party/cxxopts.hpp`).

## History & Authorship
- The current codebase is a from‑scratch rewrite (2025) authored by Michael Jarret; algorithms co‑authored by Sam Mendelson. See `AUTHORS.md`.
- Prior contributors to earlier versions are acknowledged in `ACKNOWLEDGMENTS.md`. Full history is preserved on the `legacy/full-history` branch.

## Contributing
See `CONTRIBUTING.md` for guidelines, style, and workflow.

## Troubleshooting
- oneTBB missing: `fatal error: tbb/...` → `sudo apt install -y libtbb-dev`
- Link errors on Linux: ensure `-ltbb` is available and the library path is discoverable.
- Progress bars misaligned: check terminal width reporting and fonts; disable if needed in `util/progress_tracker.hpp`.
