# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20 -Ofast -march=native -flto -mtune=native -funroll-loops -MMD -MP #-DDEBUG
LDFLAGS := -ltbb -lbenchmark -lpthread

# Include directories
INCLUDE := -I../include -I../include/indicators -I. -I../include/indicators/details -I../include/boost -I../include/tbb

# Targets
TARGETS := verify_z2.out

# Source files
# SRCS_verify := verify_sqrt2_int.cpp verify_z2.cpp
SRCS_z2 :=   # Fixed path
SRCS := $(SRCS_benchmarks) $(SRCS_verify) $(SRCS_z2)

# Object files
OBJS := $(SRCS:.cpp=.o)

# Default target
all: $(TARGETS)

# Build object files
%.o: %.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Build individual targets
# verify_sqrt2_int.out: verify_sqrt2_int.o
# 	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)

verify_z2.out: verify_z2.o   # Now correctly links with Z2_old.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)

# Clean up build files
clean:
	rm -f $(TARGETS) $(OBJS) *.d
	rm -f *.o

.PHONY: all clean

# Include dependency files
-include $(OBJS:.o=.d)
