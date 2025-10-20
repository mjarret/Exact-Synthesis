# Compiler and Flags
CXX := g++  # Use g++
CXXFLAGS := -std=c++20 -Ofast -pthread -funroll-loops -flto=auto -march=native -Wfatal-errors

# Opt-in warnings (export WARN=1 to enable)
ifeq ($(WARN),1)
  CXXFLAGS += -Wall -Wextra -Wconversion -Wshadow -Wpedantic
endif
#--target=x86_64-pc-linux-gnu 
INCLUDE := -I. -Iinclude -Iinclude/indicators -Iinclude/indicators/details -Iinclude/third_party
LDFLAGS := -ltbb

# Source Files
SRC := src/SO6.cpp src/algo/Canonicalizer.cpp src/Globals.cpp apps/main.cpp src/Z2.cpp src/algo/Generate.cpp
OBJ := $(SRC:.cpp=.o)

# Output Executable
TARGET := main.out
DEBUG_CXXFLAGS := -g
DEBUG_LDFLAGS := -ltcmalloc

# Default Rule
all: $(TARGET)

# Link the Target
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(OBJ) -o $@ $(LDFLAGS)

# Compile Rules
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Debug
debug: CXXFLAGS += $(DEBUG_CXXFLAGS)
debug: LDFLAGS += $(DEBUG_LDFLAGS)
debug: clean $(TARGET)

# Clean Rule
clean:
	rm -f $(OBJ) $(TARGET)

.PHONY: distclean
distclean: clean
	# Remove local binaries and prior outputs
	sh tools/cleanup/clean.sh || true

.PHONY: ultraclean
ultraclean: distclean
	# Aggressive removal including benchmark artifacts and logs
	sh tools/cleanup/clean.sh || true
		perm6_bench perm6_compact_bench perm6_compact_bench.out perm6_compact_test \
		perm6_packed_bench perm6_packed_test perm6_test || true
	# Benchmarks artifacts
	rm -f benchmarks/*.o benchmarks/*.out benchmarks/*.d || true
	# Logs and large prior outputs
	rm -f *.log comparison_log.txt || true

# Standalone targets for Perm6Enum tests/benchmarks (do not link into main)
perm6_test: tests/perm6_enum_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -Iinclude -o $@ $<

perm6_bench: apps/perm6_bench.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -Iinclude -o $@ $<

perm6_packed_test: tests/perm6_packed_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -Iinclude -o $@ $<

perm6_packed_bench: apps/perm6_packed_bench.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -Iinclude -o $@ $<

perm6_compact_test: tests/perm6_compact_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -Iinclude -o $@ $<

perm6_compact_bench: apps/perm6_compact_bench.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -Iinclude -o $@ $<

.PHONY: docs all clean distclean ultraclean debug asan ubsan
docs:
	doxygen Doxyfile

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
