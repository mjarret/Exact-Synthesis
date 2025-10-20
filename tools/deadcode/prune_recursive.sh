#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")"/../.. && pwd)"
cd "$ROOT_DIR"

# Backup entire repo before pruning
ts=$(date +%Y%m%d-%H%M%S)
backup="repo-backup-${ts}.zip"
echo "[deadcode] Creating backup $backup ..."
zip -q -r "$backup" . -x "$backup" || true

iter=0
while true; do
  iter=$((iter+1))
  echo "[deadcode] Iteration $iter ..."
  out=$(python3 tools/deadcode/prune_dead_code.py --apply --strict-main --verbose || true)
  echo "$out"
  if echo "$out" | grep -q "No unused headers or sources detected."; then
    echo "[deadcode] Done (no candidates)."
    break
  fi
  if echo "$out" | grep -q "Removed 0 files"; then
    echo "[deadcode] No removals in this iteration. Stopping."
    break
  fi
done

echo "[deadcode] Rebuilding ..."
make clean && make
echo "[deadcode] Completed."

