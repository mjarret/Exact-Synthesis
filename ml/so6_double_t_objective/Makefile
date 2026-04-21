THIS_DIR := $(abspath $(dir $(lastword $(MAKEFILE_LIST))))
REPO_ROOT := $(abspath $(THIS_DIR)/../..)
PYTHON ?= $(REPO_ROOT)/.venv-ml/bin/python

SETUP_PY := setup.py
MODULE_GLOB := readonly_lut*.so
BUILD_DIR := build
PYCACHE_DIR := __pycache__

.PHONY: help build rebuild clean debug asan ubsan noind smoke

help:
	@printf '%s\n' \
	  'Targets:' \
	  '  make build    - build the readonly_lut extension in place' \
	  '  make rebuild  - remove build artifacts, then rebuild' \
	  '  make clean    - remove build artifacts produced by setup.py' \
	  '  make debug    - rebuild with READONLY_LUT_DEBUG=1' \
	  '  make asan     - rebuild with READONLY_LUT_ASAN=1' \
	  '  make ubsan    - rebuild with READONLY_LUT_UBSAN=1' \
	  '  make noind    - rebuild with READONLY_LUT_DISABLE_INDICATORS=1' \
	  '  make smoke    - run the Python smoke phase (override SMOKE_ARGS=...)' \
	  '' \
	  'Variables:' \
	  '  PYTHON=/path/to/python'

build:
	cd "$(THIS_DIR)" && env \
	  EXACT_SYNTHESIS_DIR="$(REPO_ROOT)" \
	  READONLY_LUT_DEBUG= \
	  READONLY_LUT_ASAN= \
	  READONLY_LUT_UBSAN= \
	  READONLY_LUT_DISABLE_INDICATORS= \
	  "$(PYTHON)" "$(SETUP_PY)" build_ext --inplace

rebuild: clean build

clean:
	cd "$(THIS_DIR)" && rm -rf "$(BUILD_DIR)" "$(PYCACHE_DIR)" $(MODULE_GLOB)

debug: clean
	cd "$(THIS_DIR)" && env \
	  EXACT_SYNTHESIS_DIR="$(REPO_ROOT)" \
	  READONLY_LUT_DEBUG=1 \
	  READONLY_LUT_ASAN= \
	  READONLY_LUT_UBSAN= \
	  READONLY_LUT_DISABLE_INDICATORS= \
	  "$(PYTHON)" "$(SETUP_PY)" build_ext --inplace

asan: clean
	cd "$(THIS_DIR)" && env \
	  EXACT_SYNTHESIS_DIR="$(REPO_ROOT)" \
	  READONLY_LUT_DEBUG= \
	  READONLY_LUT_ASAN=1 \
	  READONLY_LUT_UBSAN= \
	  READONLY_LUT_DISABLE_INDICATORS= \
	  "$(PYTHON)" "$(SETUP_PY)" build_ext --inplace

ubsan: clean
	cd "$(THIS_DIR)" && env \
	  EXACT_SYNTHESIS_DIR="$(REPO_ROOT)" \
	  READONLY_LUT_DEBUG= \
	  READONLY_LUT_ASAN= \
	  READONLY_LUT_UBSAN=1 \
	  READONLY_LUT_DISABLE_INDICATORS= \
	  "$(PYTHON)" "$(SETUP_PY)" build_ext --inplace

noind: clean
	cd "$(THIS_DIR)" && env \
	  EXACT_SYNTHESIS_DIR="$(REPO_ROOT)" \
	  READONLY_LUT_DEBUG= \
	  READONLY_LUT_ASAN= \
	  READONLY_LUT_UBSAN= \
	  READONLY_LUT_DISABLE_INDICATORS=1 \
	  "$(PYTHON)" "$(SETUP_PY)" build_ext --inplace

SMOKE_ARGS ?= --phase smoke --max-t-depth 12 --smoke-depth 12 --smoke-n 4 --smoke-seed 123 --threads 0 --lut-progress plain

smoke: build
	cd "$(THIS_DIR)" && env \
	  EXACT_SYNTHESIS_DIR="$(REPO_ROOT)" \
	  PYTHONPATH="$(THIS_DIR):$(PYTHONPATH)" \
	  "$(PYTHON)" train_so6_double_t_objective.py $(SMOKE_ARGS)
