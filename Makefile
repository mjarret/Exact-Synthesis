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

# Compile-time hash backend selection
# Valid values: boost, ankerl
BACKEND ?= ankerl

# Frequency map storage selection (default: none)
# Usage: make FREQ=baseline|cols|none
FREQ ?= none
ifeq ($(FREQ),baseline)
  CXXFLAGS += -DEXACT_FREQ_NONE=0 -DEXACT_FREQ_COLS_ONLY=0
else ifeq ($(FREQ),cols)
  CXXFLAGS += -DEXACT_FREQ_NONE=0 -DEXACT_FREQ_COLS_ONLY=1
else ifeq ($(FREQ),none)
  CXXFLAGS += -DEXACT_FREQ_NONE=1 -DEXACT_FREQ_COLS_ONLY=0
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
SRC := src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp apps/main.cpp src/Z2.cpp src/algo/Generate.cpp
OBJ := $(SRC:.cpp=.o)

# Output Executable
TARGET := main.out
DEBUG_CXXFLAGS := -g
DEBUG_LDFLAGS := -ltcmalloc

# Default Rule
all: $(TARGET)

# (GPU/OpenCL accelerator removed; CPU-only build)

# Mapping tester (standalone)
.PHONY: perm_tester
perm_tester: apps/perm_mapping_tester.o src/SO6.o src/Z2.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) $^ -o $@ $(LDFLAGS)

apps/perm_mapping_tester.o: apps/perm_mapping_tester.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Link the Target
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(OBJ) -o $@ $(LDFLAGS)

# Compile Rules
%.o: %.cpp
	$(CXX) $(CLANG_CXXMODE) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Debug
debug: CXXFLAGS := $(filter-out -DNDEBUG,$(CXXFLAGS)) $(DEBUG_CXXFLAGS)
debug: LDFLAGS += $(DEBUG_LDFLAGS)
debug: clean $(TARGET)

# Clean Rule

ifeq ($(BACKEND),boost)
  CXXFLAGS += -DEXACT_USE_BOOST
endif
ifeq ($(BACKEND),ankerl)
  CXXFLAGS += -DEXACT_USE_ANKERL
endif
clean:
	rm -f $(OBJ) $(TARGET)

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
