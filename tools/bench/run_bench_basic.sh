#!/usr/bin/env bash
set -euo pipefail

# Extremely simple runner:
# - Sequentially: make clean && make BACKEND=<name>
# - Run: ./main.out -t <T> -s <S>
# - Save full output per-backend to bench/simple/logs/<backend>.log
# - Parse last "Time: ...s" and last "RSS: ... MB" into a compact table

T=12
S=9
while [[ $# -gt 0 ]]; do
  case "$1" in
    -t|--tcount) T="$2"; shift 2;;
    -s|--stored) S="$2"; shift 2;;
    *) echo "Unknown arg: $1" >&2; exit 2;;
  esac
done

BACKENDS=(boost ankerl)

LOG_DIR="bench/simple/logs"
RES_DIR="bench/simple"
mkdir -p "$LOG_DIR" "$RES_DIR"

CSV="$RES_DIR/results_T${T}_S${S}.csv"
MD="$RES_DIR/results_T${T}_S${S}.md"
echo "backend,build_exit,run_exit,time_s,rss_mb" > "$CSV"

for B in "${BACKENDS[@]}"; do
  echo "=== Backend: $B ==="
  make clean >/dev/null 2>&1 || true

  set +e
  make -j"$(nproc)" BACKEND="$B" >/dev/null 2>&1
  BUILD_EXIT=$?
  if [[ $BUILD_EXIT -ne 0 ]]; then
    echo "Build failed for $B (exit $BUILD_EXIT)" >&2
    echo "$B,$BUILD_EXIT,NA,NA,NA" >> "$CSV"
    continue
  fi

  ./main.out -t "$T" -s "$S" > "$LOG_DIR/$B.log" 2>&1
  RUN_EXIT=$?
  set -e

  # Parse last occurrences (robust against progress redraws)
  TIME_S=$(sed -nE 's/.*Time:\s*([0-9.]+)s.*/\1/p' "$LOG_DIR/$B.log" | tail -n 1)
  RSS_MB=$(sed -nE 's/.*RSS:\s*([0-9.]+) MB.*/\1/p' "$LOG_DIR/$B.log" | tail -n 1)
  [[ -z "$TIME_S" ]] && TIME_S=NA
  [[ -z "$RSS_MB" ]] && RSS_MB=NA

  echo "$B,$BUILD_EXIT,$RUN_EXIT,$TIME_S,$RSS_MB" >> "$CSV"
done

{
  echo "| backend | build_exit | run_exit | time_s | rss_mb |"
  echo "|---|---:|---:|---:|---:|"
  tail -n +2 "$CSV" | while IFS=, read -r b be re ts rm; do
    echo "| $b | $be | $re | $ts | $rm |"
  done
} > "$MD"

echo "\nResults written to:"
echo "  $CSV"
echo "  $MD"
