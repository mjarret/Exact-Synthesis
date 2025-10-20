#!/usr/bin/env bash
set -euo pipefail

# Bench all supported hash backends by building and running
#   ./main.out -t <T> -s <S>
# Capture the final "T= <S>" line (time) and the last RSS line (memory),
# and write a results table to bench/results.

SCRIPT_DIR=$(cd "$(dirname "$0")" && pwd)
REPO_ROOT=$(cd "$SCRIPT_DIR/../.." && pwd)
cd "$REPO_ROOT"

T=12
S=9
TIMEOUT_SECS=${TIMEOUT_SECS:-0}
# Default: do NOT pass any thread flag (some environments/CLIs may not accept "-n max").
# To force threads, run with: THREADS_ARG="-n max" or THREADS_ARG="-n 16".
THREADS_ARG=${THREADS_ARG:-}

while [[ $# -gt 0 ]]; do
  case "$1" in
    -t|--tcount) T="$2"; shift 2;;
    -s|--stored) S="$2"; shift 2;;
    --timeout) TIMEOUT_SECS="$2"; shift 2;;
    --threads) THREADS_ARG="-n $2"; shift 2;;
    *) echo "Unknown arg: $1" >&2; exit 2;;
  esac
done

BACKENDS=(boost ankerl)

LOG_DIR="bench/logs"
RES_DIR="bench/results"
mkdir -p "$LOG_DIR" "$RES_DIR"

RES_CSV="$RES_DIR/bench_T${T}_S${S}.csv"
RES_MD="$RES_DIR/bench_T${T}_S${S}.md"
echo "backend_requested,backend_used,build_status,time_s,rss_mb,tcount,stored_depth" > "$RES_CSV"

strip_ansi() {
  # Strip ANSI escapes and carriage returns
  perl -pe 's/\e\[[\d;]*[A-Za-z]//g' | tr -d '\r'
}

have_cmd() { command -v "$1" >/dev/null 2>&1; }

# Prefer coreutils stdbuf if available (Linux). On macOS, use gstdbuf if installed.
STDBUF_CMD=""
if have_cmd stdbuf; then STDBUF_CMD="stdbuf -oL -eL"; elif have_cmd gstdbuf; then STDBUF_CMD="gstdbuf -oL -eL"; fi

run_with_timeout() {
  if [[ "$TIMEOUT_SECS" != "0" ]] && have_cmd timeout; then
    timeout --preserve-status "$TIMEOUT_SECS" "$@"
  else
    "$@"
  fi
}

for B in "${BACKENDS[@]}"; do
  echo "== Building backend: $B =="
  BUILD_LOG="$LOG_DIR/build_${B}.log"
  RUN_LOG_RAW="$LOG_DIR/run_${B}.log"
  RUN_LOG_CLEAN="$LOG_DIR/run_${B}.clean.log"

  make clean >/dev/null 2>&1 || true
  BUILD_STATUS=ok
  if ! make -j"$(nproc)" BACKEND="$B" 2>&1 | tee "$BUILD_LOG" >/dev/null; then
    BUILD_STATUS=fail
    echo "build failed for $B; skipping run" >&2
    echo "$B,NA,$BUILD_STATUS,NA,NA,$T,$S" >> "$RES_CSV"
    continue
  fi

  # Detect fallback to robin_hood in compile output
  BACKEND_USED="$B"
  if grep -q "falling back to robin_hood" "$BUILD_LOG"; then
    BACKEND_USED="robin_hood(fallback)"
  fi

  echo "== Running backend: $B =="
  set +e
  if [[ -n "$THREADS_ARG" ]]; then
    # shellcheck disable=SC2206
    RUN_CMD=( ./main.out -t "$T" -s "$S" $THREADS_ARG )
  else
    RUN_CMD=( ./main.out -t "$T" -s "$S" )
  fi
  if [[ -n "$STDBUF_CMD" ]]; then
    # shellcheck disable=SC2206
    RUN_CMD=( $STDBUF_CMD "${RUN_CMD[@]}" )
  fi
  # Execute and capture the exit code of the leftmost command in the pipeline
  run_with_timeout "${RUN_CMD[@]}" 2>&1 | tee "$RUN_LOG_RAW" >/dev/null
  RUN_STATUS=${PIPESTATUS[0]}
  if [[ $RUN_STATUS -ne 0 ]]; then
    echo "run failed (exit $RUN_STATUS) for $B" >&2
    echo "$B,$BACKEND_USED,run_fail,NA,NA,$T,$S" >> "$RES_CSV"
    set -e
    continue
  fi
  set -e

  # Clean the output for grepping
  cat "$RUN_LOG_RAW" | strip_ansi > "$RUN_LOG_CLEAN"

  # Extract time from the last T= line
  TLINE=$(grep -a "^T= " "$RUN_LOG_CLEAN" | tail -n 1 || true)
  TIME_S=$(echo "$TLINE" | sed -nE 's/.*Time:\s*([0-9.]+)s.*/\1/p')
  if [[ -z "$TIME_S" ]]; then
    # Fallback: grab the last standalone Time: ...s if progress lines are not captured
    TIME_S=$(grep -a "Time:" "$RUN_LOG_CLEAN" | tail -n 1 | sed -nE 's/.*Time:\s*([0-9.]+)s.*/\1/p')
  fi

  # Extract RSS MB from the last RSS line
  RLINE=$(grep -a "RSS:" "$RUN_LOG_CLEAN" | tail -n 1 || true)
  RSS_MB=$(echo "$RLINE" | sed -nE 's/.*RSS:\s*([0-9.]+) MB.*/\1/p')

  [[ -z "$TIME_S" ]] && TIME_S=NA
  [[ -z "$RSS_MB" ]] && RSS_MB=NA

  echo "$B,$BACKEND_USED,$BUILD_STATUS,$TIME_S,$RSS_MB,$T,$S" >> "$RES_CSV"
done

# Emit a markdown table for convenience
{
  echo "| backend_requested | backend_used | build_status | time_s | rss_mb | tcount | stored_depth |"
  echo "|---|---|---:|---:|---:|---:|---:|"
  tail -n +2 "$RES_CSV" | while IFS=, read -r b bu bs ts rm tt ss; do
    echo "| $b | $bu | $bs | $ts | $rm | $tt | $ss |"
  done
} > "$RES_MD"

echo "\nResults written to:"
echo "  $RES_CSV"
echo "  $RES_MD"
