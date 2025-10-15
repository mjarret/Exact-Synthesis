# Exact-Synthesis

Exact-Synthesis is a C++20 project for generating and analyzing 6×6 matrices over Z[√2] (SO6) under T‑operator products. It builds layered lookup tables (LUTs) by T‑depth, with parallel generation and compact finalization.

- Language: C++20
- Parallelism: Intel oneTBB
- Build: Makefile (g++)
- UI: Lightweight CLI (header‑only cxxopts shim)
- Progress: Trimmed, vendored indicators (MIT) with terminal bars

## Quick Start

Requirements
- g++ with C++20
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
  - `so6/`         SO6 core (`SO6.hpp`, `LUT.hpp`, `Signatures.inl`)
  - `policy/`      Hash policy (`HashPolicy.hpp`)
  - `ds/`          Data structures (SmallFreqMap, Perm6)
  - `iter/`        Iterators for SO6
  - `util/`        Progress tracker and I/O helpers (uses trimmed indicators)
  - `sys/`         Memory helpers
  - `third_party/` Header‑only CLI shim (cxxopts)
- `src/`           Implementations (`SO6.cpp`, `algo/*.cpp`)
- `tests/`         Small tests/bench helpers
- `docs/`          Architecture and code map
- `tools/`         Maintenance scripts

## Build Targets
- `make`               Build `main.out`
- `make clean`         Remove objects and binary
- `make debug`         Build with `-g` and link with `-ltcmalloc` (if installed)
- `make asan`          Build with AddressSanitizer
- `make ubsan`         Build with UBSan
- `make docs`          Generate Doxygen docs (if `doxygen` is installed)

## Performance & Memory
- Generation uses `tbb::parallel_for_each` and a per‑layer working set; finalization compacts into a `robin_hood::unordered_flat_set` to reduce memory overhead.
- Progress bars show elapsed/remaining time and process RSS.

## Third‑Party and Licensing
- Indicators (MIT) reduced to required parts; see `include/indicators/NOTICE` and `include/indicators/LICENSE`.
- termcolor (BSD) and Unicode wcwidth (Markus Kuhn) are included under their original notices; see `include/indicators/*`.
- CLI uses a lightweight, vendored `cxxopts.hpp` shim (`include/third_party/cxxopts.hpp`).

## Contributing
See `CONTRIBUTING.md` for guidelines, style, and workflow.

## Troubleshooting
- oneTBB missing: `fatal error: tbb/...` → `sudo apt install -y libtbb-dev`
- Link errors on Linux: ensure `-ltbb` is available and the library path is discoverable.
- Progress bars misaligned: check terminal width reporting and fonts; disable if needed in `util/progress_tracker.hpp`.

