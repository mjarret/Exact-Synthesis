# Contributing to Exact-Synthesis

Thanks for your interest! This document outlines how to build, run, and submit changes.

## Build and Test
- Dependencies: g++, oneTBB (`libtbb-dev` on Debian/Ubuntu)
- Build: `make`
- Clean: `make clean`
- Debug: `make debug`
- Sanitizers: `make asan` or `make ubsan`

## Layout
- Core: `include/so6`, `src/`
- Algorithms: `src/algo`, `include/algo`
- Data structures: `include/ds`
- Utilities (I/O, progress): `include/util`
- Apps: `apps/`
- Docs: `docs/`

## Coding Style
- C++20
- Prefer small, focused headers; avoid heavy includes in public headers
- Keep changes minimal and localized; avoid unrelated refactors in the same PR
- Don’t add license headers unless requested; third‑party notices live alongside vendored code

## Submitting Changes
1. Fork and create a feature branch
2. Ensure it builds (`make`) and runs (`./main.out --help` at minimum)
3. Add or update docs if behavior changes (README/docs/)
4. Open a PR with a concise description:
   - Problem / motivation
   - What changed
   - How to validate

## Performance Notes
- Be mindful of allocations in hot loops; prefer fixed‑capacity structures
- Avoid per‑element heap allocations in generation paths
- Verify parallel code paths don’t introduce data races

## Third‑Party
- Indicators (trimmed), cxxopts, and ankerl headers live under `include/third_party`
- Preserve NOTICE and LICENSE files when touching vendored code

Thanks for contributing!
