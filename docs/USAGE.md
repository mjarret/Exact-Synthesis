# Usage Guide

This guide covers running the generator, options, and output.

## Basic Runs
- Help: `./main.out --help`
- Default run (T=8, auto depth, N-1 threads): `./main.out`
- Custom: `./main.out -t 10 -s 5 -n max`

## Options (from src/Globals.cpp)
- `-h, --help`                     Show help
- `-t, --tcount <int>`             Target T count (default: 8)
- `-s, --stored_depth <int>`       Maximum stored depth (default: 0; auto‑normalized)
- `-f, --pattern_file <path>`      Pattern file (optional)
- `-n, --threads <N|max>`          Number of threads or `max`
- `-r, --root <string>`            Root circuit string (optional)
- `-c, --cases`                    Flag for specific cases (not used)

## Progress & Output
- Progress bars show per‑layer progress (count and RSS).
- Intermediate results are kept in memory; final layers are compacted.
- If a pattern file is provided, I/O threads dump selected information under `./data/`.

## Tips
- If memory pressure is high, reduce `--tcount` or `--stored_depth`.
- Use fewer threads (`-n 1` or `-n <N>`) for deterministic runs.
- `make asan`/`make ubsan` help diagnose runtime issues during development.
