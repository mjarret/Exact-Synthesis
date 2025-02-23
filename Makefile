# Compiler and Flags
CXX := g++  # Use g++
CXXFLAGS := -std=c++20 -Ofast -pthread -fopenmp -funroll-loops -flto=auto -march=native -Wfatal-errors
#--target=x86_64-pc-linux-gnu 
INCLUDE := -Iinclude/boost -Iinclude/tbb -I. -Iinclude -Iinclude/indicators -Iinclude/indicators/details 
LDFLAGS := -lboost_program_options -ltbb -lstdc++

# Source Files
SRC := Globals.cpp SO6.cpp main.cpp 
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

.PHONY: auto-add
auto-add:
	@files=$$(git status --porcelain | awk '{print $$2}'); \
	if [ -n "$$files" ]; then \
		git add $$files && echo "Added:" $$files; \
	else \
		echo "Nothing to add."; \
	fi
