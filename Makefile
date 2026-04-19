# Compiler and Flags
CXX := g++  # default
CXXFLAGS := -std=c++20 -Ofast -pthread -funroll-loops -flto=auto -march=native -Wfatal-errors -DNDEBUG \
  -fdata-sections -ffunction-sections -fomit-frame-pointer

# Easy compiler switch: COMPILER=gcc|clang (defaults to clang)
COMPILER ?= clang
ifeq ($(COMPILER),clang)
  # Some systems only ship 'clang' (not clang++) — use clang driver
  CXX := clang
  # Prefer ThinLTO with Clang and adjust deprecated -Ofast
  CXXFLAGS := $(filter-out -flto=auto -Ofast,$(CXXFLAGS)) -flto=thin -O3 -fno-semantic-interposition
else ifeq ($(COMPILER),gcc)
  CXX := g++
  # GCC-specific perf tweaks
  CXXFLAGS += -fno-semantic-interposition -fno-plt
endif

# Optional user-provided additions without overriding defaults
EXTRA_CXXFLAGS ?=
CXXFLAGS += $(EXTRA_CXXFLAGS)

# Numeric backend: unified on Dyadic (no compile-time switch)

# Enumerator selection flag removed; OP6-style cycle caching is no longer compiled.



# Aggressive optimizations toggle: AGGRESSIVE=1 to add extra flags
AGGRESSIVE ?= 0
ifeq ($(AGGRESSIVE),1)
  ifeq ($(COMPILER),gcc)
    # GCC-specific aggressive flags (use at your own risk)
    CXXFLAGS += \
      -fno-semantic-interposition \
      -fno-plt \
      -frename-registers \
      -fomit-frame-pointer \
      -falign-functions=32 -falign-jumps=16 -falign-loops=16 \
      -fno-asynchronous-unwind-tables
  else ifeq ($(COMPILER),clang)
    # Clang-specific aggressive flags (use at your own risk)
    CXXFLAGS += \
      -fno-semantic-interposition \
      -fomit-frame-pointer
  endif
endif

# When using 'clang' driver, ensure C++ mode only for compilation, not link
CLANG_CXXMODE :=
ifeq ($(COMPILER),clang)
  CLANG_CXXMODE := -x c++
endif

# Opt-in warnings (export WARN=1 to enable)
ifeq ($(WARN),1)
  CXXFLAGS += -Wall -Wextra -Wconversion -Wshadow -Wpedantic
endif
#--target=x86_64-pc-linux-gnu 
INCLUDE := -I. -Iinclude -Iinclude/third_party
# Allow caller to inject include paths (e.g., for phmap/absl/folly)
EXTRA_INCLUDE ?=
INCLUDE += $(EXTRA_INCLUDE)
LDFLAGS := -ltbb
ifeq ($(COMPILER),clang)
  LDFLAGS += -lstdc++
endif
 # Link-time perf optimizations and dead code removal
LDFLAGS += -Wl,-O2 -Wl,--gc-sections -Wl,--as-needed

# Optional linker selection and ThinLTO cache for faster incremental links
LINKER ?=
ifeq ($(LINKER),lld)
  CXXFLAGS += -fuse-ld=lld
  # LLD-only: identical code folding can shrink code size and i-cache pressure
  LDFLAGS += -Wl,--icf=all
  # ThinLTO cache directory is also an LLD-only flag; only add it when we are
  # explicitly using lld as the linker.
  ifeq ($(COMPILER),clang)
    LDFLAGS += -Wl,--thinlto-cache-dir=.thinlto-cache
  endif
endif

# Source Files
SRC := src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp apps/main.cpp src/algo/Generate.cpp src/T_Operator.cpp src/MITM.cpp src/util/lut_export.cpp
OBJ := $(SRC:.cpp=.o)

# Standalone app object files (built on demand)
APP_OBJ := \
  apps/hash_recompute_tester.o \
  apps/mitm_match_tester.o \
  apps/lut_history_tester.o \
  apps/canonical_form_print.o \
  apps/self_inverse_tester.o \
  apps/pair_distance_tester.o \
  apps/root_string_tester.o

ALL_OBJ := $(OBJ) $(APP_OBJ)

# Output Executable
TARGET := main.out
DEBUG_CXXFLAGS := -g
DEBUG_LDFLAGS := -ltcmalloc

# Binaries produced by this workspace
BINARIES := $(TARGET) hash_tester mitm_tester lut_history_tester canonical_form_print self_tester pair_tester root_tester

# Benchmarks / helper binaries we actively support
BENCH_BINS := \
  dyadic_bench \
  lut_vs_T_depth_bench \
  lut_build_bench \
  mitm_bench

# Default Rule
all: $(TARGET)

# (GPU/OpenCL accelerator removed; CPU-only build)

# Hash recompute tester (standalone)
.PHONY: hash_tester
hash_tester: apps/hash_recompute_tester.o src/SO6.o src/algo/Generate.o src/T_Operator.o src/Globals.o src/algo/Canonicalizer.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o $@ $(LDFLAGS)

apps/hash_recompute_tester.o: apps/hash_recompute_tester.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# MITM match tester (standalone)
.PHONY: mitm_tester
mitm_tester: apps/mitm_match_tester.o src/SO6.o src/algo/Generate.o src/T_Operator.o src/Globals.o src/algo/Canonicalizer.o src/MITM.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o $@ $(LDFLAGS)

apps/mitm_match_tester.o: apps/mitm_match_tester.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

apps/lut_history_tester.o: apps/lut_history_tester.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Self-inverse tester (standalone)
.PHONY: self_tester
self_tester: apps/self_inverse_tester.o src/SO6.o src/algo/Canonicalizer.o src/Globals.o src/T_Operator.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o $@ $(LDFLAGS)

apps/self_inverse_tester.o: apps/self_inverse_tester.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Root string tester (standalone)
.PHONY: root_tester
root_tester: apps/root_string_tester.o src/SO6.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o $@ $(LDFLAGS)

apps/root_string_tester.o: apps/root_string_tester.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Benchmark LUT (T vs TT comparison)
.PHONY: benchmark_lut
benchmark_lut: apps/benchmark_lut.o src/SO6.o src/algo/Generate.o src/T_Operator.o src/Globals.o src/algo/Canonicalizer.o src/util/lut_export.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o benchmark_lut.out $(LDFLAGS)

apps/benchmark_lut.o: apps/benchmark_lut.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# CPU BFS bench — per-layer timing, output matches bfs_gpu (CUDA version).
.PHONY: bfs_cpu_bench
bfs_cpu_bench: apps/bfs_cpu_bench.o src/SO6.o src/algo/Generate.o src/T_Operator.o src/Globals.o src/algo/Canonicalizer.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o bfs_cpu_bench.out $(LDFLAGS)

apps/bfs_cpu_bench.o: apps/bfs_cpu_bench.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Link the Target
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(OBJ) -o $@ $(LDFLAGS)

# Compile Rules
%.o: %.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Optional hardening for T_Operator TU (aliasing/wrap semantics)
# Enable with T_OP_SAFE=1 to add: -fno-strict-aliasing -fwrapv
T_OP_SAFE ?= 0
ifeq ($(T_OP_SAFE),1)
src/T_Operator.o: CXXFLAGS += -fno-strict-aliasing -fwrapv
endif

# Debug
debug: CXXFLAGS := $(filter-out -DNDEBUG,$(CXXFLAGS)) $(DEBUG_CXXFLAGS)
debug: LDFLAGS += $(DEBUG_LDFLAGS)
debug: clean $(TARGET)

# Clean Rule

clean:
	# Object and dep files (anywhere)
	find . -type f \( -name '*.o' -o -name '*.d' \) -delete || true
	# Top-level binaries and outputs
	rm -f $(BINARIES) $(BENCH_BINS) *.out || true
	# Benchmark objects explicitly (for portability on systems without 'find -delete')
	rm -f benchmarks/*.o apps/*.o src/*.o || true

.PHONY: distclean
distclean: clean
	# Remove local binaries and prior outputs
	sh tools/cleanup/clean.sh || true

.PHONY: ultraclean
ultraclean: distclean
	# Aggressive removal
	sh tools/cleanup/clean.sh || true
	# Logs and large prior outputs
	rm -f *.log comparison_log.txt || true

# (removed experimental Perm6 benchmarks/tests targets)

.PHONY: docs all clean distclean ultraclean debug asan ubsan
docs:
	doxygen Doxyfile

.PHONY: print-vars
print-vars:
	@echo "INCLUDE=$(INCLUDE)"
	@echo "LDFLAGS=$(LDFLAGS)"
	@echo "CXXFLAGS=$(CXXFLAGS)"
	@echo "COMPILER=$(COMPILER) LINKER=$(LINKER)"

# Sanitizers (opt-in dev builds)
asan:
	$(MAKE) clean ; $(MAKE) CXXFLAGS='-std=c++20 -O1 -g -fsanitize=address -fno-omit-frame-pointer $(INCLUDE)'

ubsan:
	$(MAKE) clean ; $(MAKE) CXXFLAGS='-std=c++20 -O1 -g -fsanitize=undefined -fno-omit-frame-pointer $(INCLUDE)'

# Clang PGO helpers (instrument/run/use). PROFILE points to .profdata
.PHONY: pgo-gen pgo-use
PROFILE ?= code.profdata
pgo-gen:
	$(MAKE) clean ; \
	$(MAKE) EXTRA_CXXFLAGS='-fprofile-instr-generate -fcoverage-mapping' LDFLAGS='$(LDFLAGS) -fprofile-instr-generate' ; \
	cp $(TARGET) main_pgo_gen

pgo-use:
	@if [ ! -f "$(PROFILE)" ]; then echo "Missing PROFILE=$(PROFILE). Run the instrumented binary to generate .profraw then llvm-profdata merge -output=$(PROFILE) *.profraw"; exit 1; fi
	$(MAKE) clean ; \
	$(MAKE) EXTRA_CXXFLAGS='-fprofile-instr-use=$(PROFILE)'

.PHONY: auto-add
auto-add:
	@files=$$(git status --porcelain | awk '{print $$2}'); \
	if [ -n "$$files" ]; then \
		git add $$files && echo "Added:" $$files; \
	else \
		echo "Nothing to add."; \
	fi
# Pair distance tester (standalone)
.PHONY: pair_tester
pair_tester: apps/pair_distance_tester.o src/SO6.o src/algo/Generate.o src/T_Operator.o src/Globals.o src/algo/Canonicalizer.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o $@ $(LDFLAGS)

apps/pair_distance_tester.o: apps/pair_distance_tester.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Canonical form print tester (standalone)
.PHONY: canonical_form_print
canonical_form_print: apps/canonical_form_print.o src/SO6.o src/algo/Canonicalizer.o src/Globals.o src/T_Operator.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o $@ $(LDFLAGS)

apps/canonical_form_print.o: apps/canonical_form_print.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# LUT path/history tester (standalone)
.PHONY: lut_history_tester
lut_history_tester: apps/lut_history_tester.o src/SO6.o src/algo/Generate.o src/T_Operator.o src/Globals.o src/algo/Canonicalizer.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o $@ $(LDFLAGS)
# Build LUT (T=8) end-to-end microbenchmarks
.PHONY: lut_build_bench
lut_build_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp src/algo/Generate.cpp src/T_Operator.cpp \
		benchmarks/build_lut_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

.PHONY: dyadic_bench
dyadic_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		benchmarks/dyadic_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

# Dyadic add benchmark (generic implementation)
# MITM meet-in-the-middle benchmark on random targets
.PHONY: mitm_bench
mitm_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp src/algo/Generate.cpp src/T_Operator.cpp src/MITM.cpp \
		benchmarks/mitm_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

.PHONY: lut_vs_T_depth_bench
lut_vs_T_depth_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp src/algo/Generate.cpp src/T_Operator.cpp \
		benchmarks/lut_vs_T_depth_bench.cpp -o $@ -lbenchmark -O3 -lpthread $(LDFLAGS)

# Core benchmark bundle for day-to-day profiling.
.PHONY: core_bench
core_bench: dyadic_bench lut_build_bench lut_vs_T_depth_bench mitm_bench
