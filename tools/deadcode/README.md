# Dead Code Pruning Toolkit

This toolkit helps you identify and optionally remove headers and sources that are not actually used by:
- main build (based on compile_commands.json)
- tests (in tests/*.cpp)
- benchmarks (in apps/*bench*.cpp and benchmarks/*.cpp)

It works in two phases:
- Graph-based include reachability to find unused headers and sources.
- Optional compile pass to surface unused symbols via warnings (manual review recommended).

## Files

- `prune_dead_code.py` — main script (dry-run by default)
- `run.sh` — convenience wrapper for common modes

## Quick Start

1) Dry-run (list candidates only):

```
python3 tools/deadcode/prune_dead_code.py --verbose
```

2) Apply removals (be careful; creates a `.deadcode.backup` list):

```
python3 tools/deadcode/prune_dead_code.py --apply --verbose
```

3) Keep tests/benchmarks as roots (default):

```
python3 tools/deadcode/prune_dead_code.py --include-tests --include-bench
```

4) Run with a custom root list (advanced):

```
python3 tools/deadcode/prune_dead_code.py --roots src apps/main.cpp
```

## Notes

- The script relies on `compile_commands.json` at repository root to find translation units. If missing, you can generate it via your build system (e.g., CMake, bear, or your current export).
- For function-level dead code, consider compiling with extra warnings (e.g., `-Wall -Wextra -Wunused`) and manually inspecting results; automatic edits are intentionally conservative.

