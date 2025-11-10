# Code Map (File Index)

This index lists the primary headers and sources by topic to reduce time spent hunting through folders.

## Core

- Numbers: Z[√2]
  - Headers: `include/Z2.hpp`
  - Impl: `src/Z2.cpp`

- Hashing helpers live inline in `include/so6/SO6.hpp`.

## SO6 (6×6 over Z[√2])

- Main class:
  - `include/so6/SO6.hpp` (API, inline helpers, frequency hash wrappers)
  - `src/SO6.cpp` (ctor, identity, operator*, operator<=>, left_multiply_by_T dispatcher)
  - Inline frequency hashing: `include/so6/Signatures.inl`

- Algorithms
  - Canonicalization & equivalence classes: `src/algo/Canonicalizer.cpp`
  - Generation / layering: `src/algo/Generate.cpp`

## Data Structures (ds)

- Frequency map: `include/ds/SmallFreqMap.hpp`
- 6‑Permutation (enumerated table + rank/unrank): `include/ds/Perm6Enum.hpp`
- Flat set prototype (benchmarks): `include/ds/so6_flat_set.hpp`

## Storage & Lookup

- Layered lookup table (finalized layers + working set):
  - `include/so6/graph/LUT.hpp`

## Utilities

- Lex compare + sign masks: `include/util/utils.hpp`
- I/O utils and signal handling: `include/util/io_utils.hpp`
- Progress tracker: `include/util/progress_tracker.hpp`
- Memory reporting: `include/sys/memory.hpp`

## Config

- Global configuration: `include/config/Globals.hpp`

## Applications & Benchmarks

- Main: `apps/main.cpp`
- Benchmarks: `apps/*bench*.cpp`
- Tests: `tests/*.cpp`
