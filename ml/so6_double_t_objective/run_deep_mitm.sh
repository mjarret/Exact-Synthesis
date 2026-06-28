#!/usr/bin/env bash
# Build the readonly_lut binding fresh and run the deep MITM structure probe.
# Run this ON the large-memory machine (the binding is compiled with -march=native, so a
# .so built elsewhere will SIGILL -- it must be compiled here).
#
# Tune via environment variables, e.g.:
#   LEFT_DEPTH=12 DEPTHS="16 18 20" M=16 NCHAINS=400 TIMEOUT=600 ./run_deep_mitm.sh   # fast, reaches depth 20
#   LEFT_DEPTH=13 DEPTHS="16 18 20 22 24" ./run_deep_mitm.sh                            # deeper, reaches depth 24
# If the deepest depth OOMs, you still keep the shallower results (written incrementally) and
# the pool cache (runs/deep_pool.npz); just re-run with smaller --depths or a larger LEFT_DEPTH.
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
export EXACT_SYNTHESIS_DIR="$REPO"
PY="${PYTHON:-python3}"

echo "== deep MITM probe =="
echo "repo            : $REPO"
echo "python          : $($PY --version 2>&1)"
echo "left-depth      : ${LEFT_DEPTH:-12}   depths: ${DEPTHS:-16 18 20}   (right capped at 8 -> reaches depth 20)"
echo "m/state-per-dep : ${M:-16}   n-chains: ${NCHAINS:-400}   timeout/call: ${TIMEOUT:-600}s"
echo

# --- dependencies (numpy + pybind11 in the active python; TBB as a system lib) ---
$PY -c "import pybind11" 2>/dev/null || $PY -m pip install pybind11
$PY -c "import numpy"    2>/dev/null || $PY -m pip install numpy
echo "  (system dependency: a C++20 compiler and TBB -- e.g. 'apt install libtbb-dev g++')"
echo

# --- fresh build (mandatory: stale -march=native objects can SIGILL on different hardware) ---
cd "$HERE"
rm -rf build
rm -f readonly_lut*.so
echo "building readonly_lut (this includes the MITM oracle additions) ..."
$PY setup.py build_ext --inplace
echo

# --- run ---
$PY deep_mitm_probe.py \
    --left-depth "${LEFT_DEPTH:-12}" \
    --depths ${DEPTHS:-16 18 20} \
    --m "${M:-16}" \
    --n-chains "${NCHAINS:-400}" \
    --timeout "${TIMEOUT:-600}" \
    ${MAXRD:+--max-right-depth "$MAXRD"} \
    --out runs/deep_results.json \
    --pool runs/deep_pool.npz

echo
echo "DONE.  Send back: ml/so6_double_t_objective/runs/deep_results.json"
