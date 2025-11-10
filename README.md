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
  - `so6/`         SO6 core (`SO6.hpp`, `Signatures.inl`)
  - `so6/graph/`   Rooted BFS/LUT (`LUT.hpp`)
  - `policy/`      Hash policy (`HashPolicy.hpp`)
  - `ds/`          Data structures (SmallFreqMap, Lehmer6)
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
- Generation uses `tbb::parallel_for_each` and a per‑layer working set; finalization compacts into a hash set.
- Progress bars show elapsed/remaining time and process RSS.

## Hash Backend Selection

### Installed vs. vendored
- Vendored in-tree: robin_hood (include/robin_hood.h), ankerl (include/third_party/ankerl/unordered_dense.h), phmap (include/third_party/parallel_hashmap).
- System-installed via apt: Abseil (libabsl-dev), Folly (libfolly-dev).
- Boost is usually present as headers (libboost-dev).

### Installing Abseil/Folly (Ubuntu)
Run: 

	tools/install/install_backends.sh

Or manually:

	sudo apt-get update -y
	sudo apt-get install -y libabsl-dev libfolly-dev

Make will pick up Abseil via pkg-config automatically when BACKEND=absl.
Finalized tables default to `ankerl::unordered_dense` (with an automatic fallback to `std::unordered_*` if the dense hash header is unavailable).

- Default: vendored `robin_hood::unordered_flat_set` (50% max load factor via template param).
- Alternate backends (header‑detected, fallback safe):
  - `absl`     → `absl::flat_hash_set`
  - `folly`    → `folly::F14FastSet`
  - `boost`    → `boost::unordered_set`
  - `ankerl`   → `ankerl::unordered_dense::set`
  - `phmap`    → `phmap::flat_hash_set`

Build selection (Makefile variable):
```
# defaults to robin_hood
make

# choose a backend; falls back to robin_hood if headers are missing
make BACKEND=phmap
make BACKEND=boost
make BACKEND=folly

# add include paths if needed
make BACKEND=phmap EXTRA_INCLUDE="-I/path/to/phmap/include"

# add extra compiler flags if needed
make BACKEND=ankerl EXTRA_CXXFLAGS="-I/path/to/ankerl/include"
```

Notes
- Missing headers: the wrapper automatically falls back to `robin_hood` and emits a compile‑time warning, so builds keep working even if a backend isn’t installed.
- Linking: some backends (notably Abseil and Folly) may require linking additional libraries on your system. If you select such a backend and see link errors, either add the required libs via `LDFLAGS+=...` or build with `BACKEND=robin_hood`.

## Third‑Party and Licensing
- Indicators (MIT) reduced to required parts; see `include/indicators/NOTICE` and `include/indicators/LICENSE`.
- termcolor (BSD) and Unicode wcwidth (Markus Kuhn) are included under their original notices; see `include/indicators/*`.
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
