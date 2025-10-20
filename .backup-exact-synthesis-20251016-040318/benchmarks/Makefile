# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20 -Ofast -march=native -flto -mtune=native -funroll-loops -pthread -MMD -MP -DNDEBUG -DCMAKE_BUILD_TYPE=Release

# Prefer release libbenchmark from the distro (override BENCHMARK_LIB_DIR if needed)
BENCHMARK_LIB_DIR ?= /usr/lib/x86_64-linux-gnu
LDFLAGS := -L$(BENCHMARK_LIB_DIR) -Wl,-rpath,$(BENCHMARK_LIB_DIR) -lbenchmark -ltbb -lpthread

# Include directories
INCLUDE := -I../include -I../include/indicators -I../include/indicators/details -I../include/boost -I../include/tbb

# Targets
TARGET := benchmarks.out
SETS_TARGET := set_bench.out

# Sources and objects
SRCS := benchmarks.cpp
OBJS := $(SRCS:.cpp=.o)
SRCS_SETS := set_bench.cpp
OBJS_SETS := $(SRCS_SETS:.cpp=.o)
SRCS_HASH := hash_bench.cpp
OBJS_HASH := $(SRCS_HASH:.cpp=.o)
CORE_OBJS := ../src/SO6.o ../src/Z2.o ../src/Globals.o

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)

$(SETS_TARGET): $(OBJS_SETS) $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)

.PHONY: hash
hash: $(OBJS_HASH) $(CORE_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o hash_bench.out $^ $(LDFLAGS)

%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

clean:
	rm -f $(TARGET) $(OBJS) $(SETS_TARGET) $(OBJS_SETS) hash_bench.out $(OBJS_HASH) *.d

# Build core objects needed for set_bench
../src/%.o: ../src/%.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

.PHONY: all clean

-include $(OBJS:.o=.d)
