# Exact-Synthesis Architecture Overview

This document orients you to the major modules, files, and responsibilities in the repo. It’s intended as a fast map of where to look for what, without requiring you to memorize file paths.

## High‑Level Modules

- Core Numerics (Z2)
  - `include/Z2.hpp`, `src/Z2.cpp`
  - Implements the compact number type Z[√2] with bit‑packed representation and arithmetic.

- SO6 (6×6 matrices over Z[√2])
  - `include/so6/SO6.hpp`, `src/SO6.cpp`
  - `include/iter/SO6Iterator.hpp` (iterator for permuted access)
  - `include/so6/Signatures.inl` (inline frequency hashing helpers)
  - Canonicalization & equivalence classes: `src/algo/Canonicalizer.cpp`
  - Generation: `src/algo/Generate.cpp`

- Hashing
  - `include/policy/HashPolicy.hpp` (policy for mixers and element hash helpers)
  - `include/hash_types.hpp` (project‑wide hash width alias)

- Data Structures (ds)
  - `include/ds/SmallFreqMap.hpp` (fixed‑capacity freq map for rows/cols)
  - `include/ds/Perm6.hpp` (packed 6‑perm), `include/ds/Perm6Enum.hpp` (rank/unrank + table)
  - Experimental/aux DS live under `include/ds/` and are not wired into production paths.

- Storage & Lookup
  - `include/so6/LUT.hpp` (layered lookup table by T‑depth, finalized layers + working set)

- Utilities
  - `include/util/utils.hpp` (lexicographic compare + sign masks)
  - `include/util/io_utils.hpp` (I/O helpers)
  - `include/sys/memory.hpp` (available memory, used by LUT finalization)

- Apps & Tests
  - Main: `apps/main.cpp`
  - Benchmarks: `apps/*bench*.cpp`
  - Tests: `tests/*.cpp`

## Data Flow (Generation)

1. Start with identity `SO6` (see `SO6::identity()`).
2. Build layers in `algo::create_lookup_table` using `algo::get_next_T_count`:
   - Take the current finalized layer (LUT `current()`);
   - For each `SO6`, enumerate left‑multiplication by `T` (`SO6::left_multiply_by_T`);
   - Deduplicate against `prior()`;
   - Push into working set, finalize to the next layer.
3. LUT manages finalized layers as `robin_hood::unordered_flat_set<SO6>`.

## Equality, Ordering, Hashing

- Equality: structural on `SO6` (`std::hash<SO6>` uses `SO6::hash` as precomputed signature).
- Ordering (`operator<=>`): compares `col_hash` first, then column‑wise lex ordering via `utils::lex_order` using `Row`/`Col` permutations and sign convention.
- Frequency hashing (`so6/Signatures.inl`): hashes row/col frequency maps via `hashpolicy::z_freq_hash`.

## Key Design Choices

- `SmallFreqMap` is fixed‑capacity and inline to avoid allocations in the hot path.
- Canonicalization is isolated in `src/algo/Canonicalizer.cpp` and avoids recomputing heavy structures when signatures suffice.
- The `policy/HashPolicy.hpp` concentrates hash mixer variants behind a single policy interface.

## How to Navigate Quickly

- Looking for matrix math / multiplication: `src/SO6.cpp`.
- Looking for canonicalization: `src/algo/Canonicalizer.cpp`.
- Looking for row/col permutations or ranks: `include/ds/Perm6*.hpp`.
- Looking for hashing: `include/policy/HashPolicy.hpp`, `include/so6/Signatures.inl`.
- Looking for generation loop / LUT orchestration: `src/algo/Generate.cpp`, `include/so6/LUT.hpp`.
