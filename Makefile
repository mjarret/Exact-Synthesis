# Compiler and Flags
CXX := g++  # Use g++
CXXFLAGS := -std=c++20 -Ofast -pthread -fopenmp -funroll-loops -flto=auto -march=native -Wfatal-errors
#--target=x86_64-pc-linux-gnu 
INCLUDE := -Iinclude/boost -Iinclude/tbb -I. -Iinclude -Iinclude/indicators -Iinclude/indicators/details 
LDFLAGS := -lboost_program_options -ltbb -lstdc++ -llz4

# Source Files
SRC := src/SO6.cpp src/algo/Canonicalizer.cpp src/io/SO6_serialize.cpp src/Globals.cpp apps/main.cpp src/Z2.cpp
OBJ := $(SRC:.cpp=.o)

# Output Executable
TARGET := main.out
DEBUG_FLAGS := -g -ltcmalloc

# Default Rule
all: $(TARGET)

# Link the Target
$(TARGET): $(OBJ)
	$(CXX) $(CXXFLAGS) $(INCLUDE) $(OBJ) -o $@ $(LDFLAGS)

# Compile Rules
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Debug
debug: CXXFLAGS += $(DEBUG_FLAGS)
debug: clean $(TARGET)

# Clean Rule
clean:
	rm -f $(OBJ) $(TARGET)

# Standalone targets for Perm6Enum tests/benchmarks (do not link into main)
perm6_test: tests/perm6_enum_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -Iinclude -o $@ $<

perm6_bench: apps/perm6_bench.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -Iinclude -o $@ $<

perm6_packed_test: tests/perm6_packed_test.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -Iinclude -o $@ $<

perm6_packed_bench: apps/perm6_packed_bench.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -Iinclude -o $@ $<

.PHONY: docs
docs:
	doxygen Doxyfile

.PHONY: auto-add
auto-add:
	@files=$$(git status --porcelain | awk '{print $$2}'); \
	if [ -n "$$files" ]; then \
		git add $$files && echo "Added:" $$files; \
	else \
		echo "Nothing to add."; \
	fi
