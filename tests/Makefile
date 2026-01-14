# Compiler and flags
CXX := g++
CXXFLAGS := -std=c++20 -Ofast -march=native -flto -mtune=native -funroll-loops -MMD -MP #-DDEBUG
LDFLAGS := -ltbb -lbenchmark -lpthread

# Include directories
INCLUDE := -I../include -I. -I../include/boost -I../include/tbb

# Targets
TARGETS := verify_dyadic.out verify_dyadic_vs_z2.out canon_regression.out

CANON_OBJS := \
	canon_regression.o \
	canon_regression_SO6.o \
	canon_regression_Canonicalizer.o \
	canon_regression_Globals.o \
	canon_regression_T_Operator.o

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
# Dyadic mirror verification (reduced ranges)
verify_dyadic.out: verify_dyadic.o verify_dyadic_SO6.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)

verify_dyadic_vs_z2.out: verify_dyadic_vs_z2.o
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)

canon_regression.out: $(CANON_OBJS)
	$(CXX) $(CXXFLAGS) $(INCLUDE) -o $@ $^ $(LDFLAGS)

.PHONY: canon_regression
canon_regression: canon_regression.out

canon_regression_SO6.o: ../src/SO6.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

canon_regression_Canonicalizer.o: ../src/algo/Canonicalizer.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

canon_regression_Globals.o: ../src/Globals.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

canon_regression_T_Operator.o: ../src/T_Operator.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

verify_dyadic_SO6.o: ../src/SO6.cpp
	$(CXX) $(CXXFLAGS) $(INCLUDE) -c $< -o $@

# Clean up build files
clean:
	rm -f $(TARGETS) $(OBJS) *.d
	rm -f *.o

.PHONY: all clean

# Include dependency files
-include $(OBJS:.o=.d)
