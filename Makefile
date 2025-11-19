# Compiler and Flags
CXX := g++  # default
CXXFLAGS := -std=c++20 -Ofast -pthread -funroll-loops -flto=auto -march=native -Wfatal-errors -DNDEBUG -fno-math-errno -fno-trapping-math

# Easy compiler switch: COMPILER=gcc|clang (defaults to clang)
COMPILER ?= clang
ifeq ($(COMPILER),clang)
  # Some systems only ship 'clang' (not clang++) — use clang driver
  CXX := clang
  # Prefer ThinLTO with Clang and adjust deprecated -Ofast
  CXXFLAGS := $(filter-out -flto=auto -Ofast,$(CXXFLAGS)) -flto=thin -O3 -ffast-math
else ifeq ($(COMPILER),gcc)
  CXX := g++
endif

# Optional user-provided additions without overriding defaults
EXTRA_CXXFLAGS ?=
CXXFLAGS += $(EXTRA_CXXFLAGS)

# Numeric backend selection: NUMERIC=z2 (default) or NUMERIC=dyadic
NUMERIC ?= z2
ifeq ($(NUMERIC),dyadic)
  CXXFLAGS += -DEXACT_USE_DYADIC_SQRT2=1
endif

# Enumerator selection: ENUM=cycle to enable OP6-style cycle caching
ENUM ?= lut
ifeq ($(ENUM),cycle)
  CXXFLAGS += -DDS_ENUM_CYCLE=1
endif



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
INCLUDE := -I. -Iinclude -Iinclude/indicators -Iinclude/indicators/details -Iinclude/third_party
# Allow caller to inject include paths (e.g., for phmap/absl/folly)
EXTRA_INCLUDE ?=
INCLUDE += $(EXTRA_INCLUDE)
LDFLAGS := -ltbb
ifeq ($(COMPILER),clang)
  LDFLAGS += -lstdc++
endif

# Source Files
SRC := src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp apps/main.cpp src/algo/Generate.cpp src/T_Operator.cpp src/MITM.cpp
OBJ := $(SRC:.cpp=.o)

# Standalone app object files (built on demand)
APP_OBJ := \
  apps/hash_recompute_tester.o \
  apps/mitm_match_tester.o \
  apps/lut_history_tester.o \
  apps/canonical_form_print.o \
  apps/self_inverse_tester.o \
  apps/pair_distance_tester.o

ALL_OBJ := $(OBJ) $(APP_OBJ)

# Output Executable
TARGET := main.out
DEBUG_CXXFLAGS := -g
DEBUG_LDFLAGS := -ltcmalloc

# Binaries produced by this workspace
BINARIES := $(TARGET) hash_tester mitm_tester lut_history_tester canonical_form_print self_tester pair_tester

# Benchmarks / helper binaries
BENCH_BINS := \
  lehmer_bench \
  row_ecs_bench \
  ord6_enum_bench \
  op6_enum_bench \
  linear_transform_bench \
  dyadic_bench \
  lut_build_bench \
  lut_sweep_bench \
  cycle_build_bench \
  mitm_bench \
  mitm_depth_bench \
  mitm_seed_gen \
  col_compare_bench \
  main_run_bench \
  dyadic_int_bench

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

# Link the Target
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(OBJ) -o $@ $(LDFLAGS)

# Compile Rules
%.o: %.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Stabilize T-move codegen if optimizer is too aggressive
src/T_Operator.o: CXXFLAGS += -fno-strict-aliasing -fwrapv

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

# Sanitizers (opt-in dev builds)
asan:
	$(MAKE) clean ; $(MAKE) CXXFLAGS='-std=c++20 -O1 -g -fsanitize=address -fno-omit-frame-pointer $(INCLUDE)'

ubsan:
	$(MAKE) clean ; $(MAKE) CXXFLAGS='-std=c++20 -O1 -g -fsanitize=undefined -fno-omit-frame-pointer $(INCLUDE)'

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
# Google Benchmark target for Lehmer6 (requires libbenchmark-dev)
.PHONY: lehmer_bench
lehmer_bench: benchmarks/lehmer6_bench.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) -I../Exact-Synthesis-bak-2/include $^ -o $@ -lbenchmark -lpthread $(LDFLAGS)

benchmarks/lehmer6_bench.o: benchmarks/lehmer6_bench.cpp include/ds/Lehmer6.hpp ../Exact-Synthesis-bak-2/include/ds/Lehmer6.hpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -I../Exact-Synthesis-bak-2/include -c $< -o $@

# Row equivalence classes benchmark
.PHONY: row_ecs_bench
row_ecs_bench: benchmarks/row_ecs_bench.o src/SO6.o src/algo/Canonicalizer.o src/Globals.o src/algo/Generate.o src/T_Operator.o
	$(CXX) $(CXXFLAGS) -I../Exact-Synthesis-bak-2/include $(INCLUDE) $^ -o $@ -lbenchmark -lpthread $(LDFLAGS)

benchmarks/row_ecs_bench.o: benchmarks/row_ecs_bench.cpp ../Exact-Synthesis-bak-2/include/ds/OrderedPartition6.hpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) -I../Exact-Synthesis-bak-2/include $(INCLUDE) -c $< -o $@

# Order6 enumeration microbench
.PHONY: ord6_enum_bench
ord6_enum_bench: benchmarks/ord6_enum_bench.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o $@ -lbenchmark -lpthread $(LDFLAGS)

benchmarks/ord6_enum_bench.o: benchmarks/ord6_enum_bench.cpp include/ds/FlatFrequencyTable.hpp include/ds/Order6.hpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# OrderedPartition6 enumeration microbench (uses bak-2 includes exclusively first)
.PHONY: op6_enum_bench
op6_enum_bench: benchmarks/op6_enum_bench.o
	$(CXX) $(CXXFLAGS) -I../Exact-Synthesis-bak-2/include $(INCLUDE) $^ -o $@ -lbenchmark -lpthread $(LDFLAGS)

benchmarks/op6_enum_bench.o: benchmarks/op6_enum_bench.cpp ../Exact-Synthesis-bak-2/include/ds/OrderedPartition6.hpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) -I../Exact-Synthesis-bak-2/include $(INCLUDE) -c $< -o $@
# Linear transform vs T operator benchmarks
.PHONY: linear_transform_bench
linear_transform_bench: benchmarks/linear_transform_bench.o src/SO6.o src/algo/Canonicalizer.o src/Globals.o src/algo/Generate.o src/T_Operator.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o $@ -lbenchmark -lpthread $(LDFLAGS)

benchmarks/linear_transform_bench.o: benchmarks/linear_transform_bench.cpp include/so6/T_Operator.hpp include/ds/LinearTransform.hpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@
# Build LUT (T=8) end-to-end microbenchmarks
.PHONY: lut_build_bench cycle_build_bench
lut_build_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp src/algo/Generate.cpp src/T_Operator.cpp \
		benchmarks/build_lut_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

.PHONY: lut_sweep_bench
lut_sweep_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp src/algo/Generate.cpp src/T_Operator.cpp \
		benchmarks/lut_sweep_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

cycle_build_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS -DDS_ENUM_CYCLE=1 \
		src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp src/algo/Generate.cpp src/T_Operator.cpp \
		benchmarks/build_lut_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

.PHONY: dyadic_bench
dyadic_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		benchmarks/dyadic_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

# MITM meet-in-the-middle benchmark on random targets
.PHONY: mitm_bench
mitm_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp src/algo/Generate.cpp src/T_Operator.cpp src/MITM.cpp \
		benchmarks/mitm_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

# MITM runtime by optimal depth benchmark
.PHONY: mitm_depth_bench
mitm_depth_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp src/algo/Generate.cpp src/T_Operator.cpp src/MITM.cpp \
		benchmarks/mitm_depth_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

# MITM seed generator (offline script to precompute benchmark seeds)
.PHONY: mitm_seed_gen
mitm_seed_gen:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp src/algo/Generate.cpp src/T_Operator.cpp src/MITM.cpp \
		benchmarks/mitm_seed_gen.cpp -o $@ $(LDFLAGS)

# Column comparison microbenchmarks (current vs raw)
.PHONY: col_compare_bench
col_compare_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp src/algo/Generate.cpp src/T_Operator.cpp src/MITM.cpp \
		benchmarks/col_compare_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

# Dyadic vs Int32 microbenchmarks
.PHONY: dyadic_int_bench
dyadic_int_bench:
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		benchmarks/dyadic_int_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)

# Build two main variants (Z2 vs Dyadic) and a wrapper benchmark that runs them.
.PHONY: main_z2 main_dyadic main_run_bench
main_z2:
	$(MAKE) clean
	$(MAKE) NUMERIC=z2 $(TARGET)
	cp $(TARGET) main_z2

main_dyadic:
	$(MAKE) clean
	$(MAKE) NUMERIC=dyadic $(TARGET)
	cp $(TARGET) main_dyadic

main_run_bench: main_z2 main_dyadic
	$(CXX) $(CXXFLAGS) $(INCLUDE) -DEXACT_DISABLE_INDICATORS \
		benchmarks/main_run_bench.cpp -o $@ -lbenchmark -lpthread $(LDFLAGS)
