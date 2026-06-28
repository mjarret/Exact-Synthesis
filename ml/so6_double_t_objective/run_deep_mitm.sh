#!/usr/bin/env bash
# Build the readonly_lut binding fresh and run the deep MITM structure probe.
# Run this ON the large-memory machine (the binding is compiled with -march=native, so a
# .so built elsewhere will SIGILL -- it must be compiled here).
#
# Defaults: generator=tt (even depths only), left-depth 12, depths 14/16/18 (capped at 18 so every
# right build is <=6 -> bounded and fast; no more runaway right-8 builds hitting the timeout).
# Tune via environment variables, e.g.:
#   DEPTHS="16 18" NCHAINS=800 ./run_deep_mitm.sh        # more samples at the high end
#   GENERATOR=t ./run_deep_mitm.sh                       # single-T (every depth) instead of even-only
# Results/pool: runs/deep_results_tt.json, runs/deep_pool_tt.npz (written incrementally + resumable).
set -euo pipefail

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO="$(cd "$HERE/../.." && pwd)"
export EXACT_SYNTHESIS_DIR="$REPO"

# --- resolve a python that has numpy + pybind11 (auto-create a venv if needed) ---
# Debian/Ubuntu mark system python as "externally managed" (PEP 668), so we never
# pip-install into it; we use the caller's $PYTHON if it already has the deps, else
# build a local venv at .venv-deep. Override with PYTHON=/path/to/python.
PY="${PYTHON:-python3}"
# setuptools is required to run setup.py and is NOT in a default py3.13 venv.
if ! "$PY" -c "import numpy, pybind11, setuptools" 2>/dev/null; then
    VENV="$HERE/.venv-deep"
    if [ ! -x "$VENV/bin/python" ]; then
        echo "deps missing in '$PY'; creating venv at $VENV ..."
        "$PY" -m venv "$VENV" 2>/dev/null || python3 -m venv "$VENV" || {
            echo "ERROR: 'python3 -m venv' failed. Run:  sudo apt install -y python3-venv python3-full"; exit 1; }
    fi
    "$VENV/bin/python" -m pip install -q --upgrade pip >/dev/null 2>&1 || true
    "$VENV/bin/python" -m pip install -q numpy pybind11 setuptools wheel || {
        echo "ERROR: pip install into venv failed"; exit 1; }
    PY="$VENV/bin/python"
fi

echo "== deep MITM probe =="
echo "repo            : $REPO"
echo "python          : $("$PY" --version 2>&1)  ($PY)"
echo "generator       : ${GENERATOR:-tt} (even depths only)   left-depth: ${LEFT_DEPTH:-12}"
echo "depths          : ${DEPTHS:-14 16 18}   (capped at 18 -> right<=6, bounded/fast)"
echo "m/state-per-dep : ${M:-20}   n-chains: ${NCHAINS:-500}   timeout/call: ${TIMEOUT:-120}s"
echo "  (system dependency: a C++20 compiler and TBB -- e.g. 'apt install libtbb-dev g++')"
echo

# --- fresh build (mandatory: stale -march=native objects can SIGILL on different hardware) ---
cd "$HERE"
rm -rf build
rm -f readonly_lut*.so
echo "building readonly_lut (this includes the MITM oracle additions) ..."
"$PY" setup.py build_ext --inplace
echo

# --- run ---
"$PY" deep_mitm_probe.py \
    --generator "${GENERATOR:-tt}" \
    --left-depth "${LEFT_DEPTH:-12}" \
    --depths ${DEPTHS:-14 16 18} \
    --m "${M:-20}" \
    --n-chains "${NCHAINS:-500}" \
    --timeout "${TIMEOUT:-120}" \
    ${MAXRD:+--max-right-depth "$MAXRD"} \
    --out runs/deep_results_tt.json \
    --pool runs/deep_pool_tt.npz

echo
echo "DONE.  Send back: ml/so6_double_t_objective/runs/deep_results.json"
