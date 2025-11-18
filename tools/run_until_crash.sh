#!/usr/bin/env bash
set -euo pipefail
RUNS=0
while true; do
  RUNS=$((RUNS+1))
  echo "run $RUNS"
  ../main.out -t 7 || { echo "crashed on run $RUNS"; exit 1; }
done

