#!/usr/bin/env bash
set -euo pipefail

# Usage:
#   tools/cleanup/clean.sh        # remove prior binaries/logs/artifacts
#   tools/cleanup/clean.sh -n     # dry-run (show what would be removed)

DRY=0
if [[ "${1:-}" == "-n" || "${1:-}" == "--dry-run" ]]; then
  DRY=1
fi

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
cd "$ROOT_DIR"

declare -a PATHS=(
  # root binaries/outputs
  "main.out"
  "perm6_bench" "perm6_compact_bench" "perm6_compact_bench.out"
  "perm6_compact_test" "perm6_packed_bench" "perm6_packed_test" "perm6_test"
  # logs / prior dumps
  "*.log" "hash_sweep_T*.log" "sig_sweep.log" "my_sig.log" "comparison_log.txt"
)

declare -a FINDS=(
  # object and dep files anywhere
  "-name '*.o'" "-o" "-name '*.d'" "-o" "-name '*.out'"
)

removed=0
for p in "${PATHS[@]}"; do
  for f in $p; do
    if [[ -e "$f" ]]; then
      if [[ $DRY -eq 1 ]]; then
        echo "[dry] rm -f $f"
      else
        rm -f "$f" && echo "[rm] $f" || true
        removed=$((removed+1))
      fi
    fi
  done
done

# benchmark artifacts inside benchmarks/
if [[ -d benchmarks ]]; then
  if [[ $DRY -eq 1 ]]; then
    echo "[dry] find benchmarks -type f -name '*.o' -delete"
    echo "[dry] find benchmarks -type f -name '*.out' -delete"
    echo "[dry] find benchmarks -type f -name '*.d' -delete"
  else
    find benchmarks -type f \( -name '*.o' -o -name '*.out' -o -name '*.d' \) -delete -print || true
  fi
fi

echo "Cleanup complete." >&2

