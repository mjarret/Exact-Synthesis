# Makefile for the C++/CUDA SO6 BFS LUT builder.
#
# Usage:  make        # build bfs_gpu
#         make run    # build and run on depth 6
#         make clean

NVCC      ?= /usr/local/cuda/bin/nvcc
STD       ?= c++17
OPT       ?= -O3
# Build fat binary covering A100 (sm_80), A2/A30 (sm_86), H100 (sm_90) so the
# same binary runs across Hopper cluster GPUs.  Override with ARCH=sm_XX to
# target a single arch.
ifneq ($(ARCH),)
GENCODE := -arch=$(ARCH)
else
GENCODE := -gencode=arch=compute_80,code=sm_80 \
           -gencode=arch=compute_86,code=sm_86 \
           -gencode=arch=compute_90,code=sm_90 \
           -gencode=arch=compute_90,code=compute_90
endif
CUFLAGS   := -std=$(STD) $(OPT) $(GENCODE) --expt-relaxed-constexpr \
             --extended-lambda -Xcompiler "-Wall -Wextra -Wno-unused-parameter" \
             -lineinfo

TARGET    := bfs_gpu
SRC       := bfs_gpu.cu
HEADERS   := z2_device.cuh apply_T.cuh canonical.cuh dedup.cuh

all: $(TARGET)

$(TARGET): $(SRC) $(HEADERS)
	$(NVCC) $(CUFLAGS) $< -o $@

run: $(TARGET)
	./$(TARGET) --depth 6

clean:
	rm -f $(TARGET)

.PHONY: all run clean
