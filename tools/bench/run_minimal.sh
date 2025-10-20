#!/usr/bin/env bash
set -e

# Minimal, explicit, no loops. Builds each backend and runs the binary.
# Captures full logs and writes a tiny CSV summary.

T=12
S=9
mkdir -p bench/min

# Build + Run (logs)
make clean >/dev/null 2>&1 || true
make BACKEND=boost
./main.out -t "$T" -s "$S" > bench/min/boost.log 2>&1

make clean >/dev/null 2>&1 || true
make BACKEND=ankerl
./main.out -t "$T" -s "$S" > bench/min/ankerl.log 2>&1

## Removed other backends; only boost and ankerl are benchmarked.

# Results (explicit extracts)
echo "backend,time_s,rss_mb" > bench/min/results.csv
echo "boost,$(sed -nE 's/.*Time:\s*([0-9.]+)s.*/\1/p' bench/min/boost.log | tail -n 1),$(sed -nE 's/.*RSS:\s*([0-9.]+) MB.*/\1/p' bench/min/boost.log | tail -n 1)" >> bench/min/results.csv
echo "ankerl,$(sed -nE 's/.*Time:\s*([0-9.]+)s.*/\1/p' bench/min/ankerl.log | tail -n 1),$(sed -nE 's/.*RSS:\s*([0-9.]+) MB.*/\1/p' bench/min/ankerl.log | tail -n 1)" >> bench/min/results.csv

echo "\nLogs: bench/min/*.log"
echo "Results: bench/min/results.csv"
