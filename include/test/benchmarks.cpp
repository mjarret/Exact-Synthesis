#include <iostream>
#include <vector>
#include <random>
#include <benchmark/benchmark.h>
#include <bit>
#include <cstdint>
#include <thread>
#include "../Z2.hpp"
#include "../Z2_Array.hpp"

// Option 1: Struct with a bitfield
struct BitfieldStruct {
    union {
        struct {
            uint8_t y : 8;
            uint8_t x : 8;
        } fields;
        uint16_t packed;
    } data;

    BitfieldStruct(uint16_t packed_ = 0) { data.packed = packed_; }

    void set(uint8_t x, uint8_t y) {
        data.fields.x = x;
        data.fields.y = y;
    }

    uint8_t getX() const {
        return data.fields.x;
    }

    uint8_t getY() const {
        return data.fields.y;
    }

    void swap() {
        data.packed = __builtin_bswap16(data.packed);
    }
};

// Option 2: Struct with Morton encoding using lookup tables
// struct MortonStruct {
//     uint16_t packed;

//     // Morton encoding using lookup table
//     void set(uint8_t x, uint8_t y) {
//         packed = mortonLookupTable[x] | (mortonLookupTable[y] << 1);
//     }

//     // Morton decoding for x using lookup table
//     uint8_t getX() const {
//         return decodeX[packed];
//     }

//     // Morton decoding for y using lookup table
//     uint8_t getY() const {
//         return decodeY[packed];
//     }
// };

std::vector<uint8_t> generateRandomData(size_t count) {
    std::vector<uint8_t> data(count);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint8_t> dist(0, 255);
    for (auto &val : data) {
        val = dist(rng);
    }
    return data;
}

std::vector<uint16_t> generateRandomData16(size_t count) {
    std::vector<uint16_t> data(count);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint16_t> dist(0, 65535);
    for (auto &val : data) {
        val = dist(rng);
    }
    return data;
}

std::vector<uint32_t> generateRandomData32(size_t count) {
    std::vector<uint32_t> data(count);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, -1); // Full 32-bit range
    for (auto &val : data) {
        val = dist(rng);
    }
    return data;
}

std::vector<Z2> generateRandomZ2(size_t count) {
    std::vector<Z2> data;
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint32_t> dist(0, -1); // Full 32-bit range
    for (size_t i = 0 ; i < count; i++) {data.push_back(Z2(dist(rng)));}
    return data;
}

std::vector<__uint128_t> generateRandom128(size_t count) {
    std::vector<__uint128_t> data(count);
    std::mt19937_64 rng(42); // Use 64-bit RNG for better range
    std::uniform_int_distribution<uint64_t> dist(0, -1); // Full 64-bit range

    for (auto &val : data) {
        uint64_t low = dist(rng);  // Generate lower 64 bits
        uint64_t high = dist(rng); // Generate upper 64 bits
        val = (static_cast<__uint128_t>(high) << 64) | low;
    }
    return data;
}

std::vector<BitfieldStruct> generateRandomBitfields(size_t count) {
    std::vector<BitfieldStruct> data(count);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint16_t> dist(0, 65535);
    for (auto &val : data) {
        val = BitfieldStruct(dist(rng));
    }
    return data;
}

static void Benchmark_BitfieldSet(benchmark::State& state) {
    BitfieldStruct bitfield;
    auto xValues = generateRandomData(state.range(0));
    auto yValues = generateRandomData(state.range(0));

    for (auto _ : state) {
        for (size_t i = 0; i < xValues.size(); ++i) {
            bitfield.set(xValues[i], yValues[i]);
        }
    }
}

static void Benchmark_BitfieldGet(benchmark::State& state) {
    BitfieldStruct bitfield;
    bitfield.set(123, 234);

    for (auto _ : state) {
        volatile uint8_t x = bitfield.getX();
        volatile uint8_t y = bitfield.getY();
        benchmark::DoNotOptimize(x);
        benchmark::DoNotOptimize(y);
    }
}

// static void Benchmark_MortonSet(benchmark::State& state) {
//     MortonStruct morton;
//     auto xValues = generateRandomData(state.range(0));
//     auto yValues = generateRandomData(state.range(0));

//     for (auto _ : state) {
//         for (size_t i = 0; i < xValues.size(); ++i) {
//             morton.set(xValues[i], yValues[i]);
//         }
//     }
// }

// static void Benchmark_MortonGet(benchmark::State& state) {
//     MortonStruct morton;
//     morton.set(123, 234);

//     for (auto _ : state) {
//         volatile uint8_t x = morton.getX();
//         volatile uint8_t y = morton.getY();
//         benchmark::DoNotOptimize(x);
//         benchmark::DoNotOptimize(y);
//     }
// }

static void Benchmark_BitfieldAdd(benchmark::State& state) {
    size_t dataSize = state.range(0);
    BitfieldStruct left, right;
    auto xValues = generateRandomData(dataSize);
    auto yValues = generateRandomData(dataSize);

    for (auto _ : state) {
        state.PauseTiming(); // Pause timing for setup
        left.set(xValues[state.iterations() % xValues.size()], yValues[state.iterations() % yValues.size()]);
        right.set(xValues[state.iterations() % xValues.size()], yValues[state.iterations() % yValues.size()]);
        state.ResumeTiming(); // Resume timing for the actual addition

        int16_t result = left.data.packed + right.data.packed;
        benchmark::DoNotOptimize(result);
    }
}

static void Benchmark_BitfieldAddSeparate(benchmark::State& state) {
    size_t dataSize = state.range(0);
    BitfieldStruct left, right;
    auto xValues = generateRandomData(dataSize);
    auto yValues = generateRandomData(dataSize);
    int8_t result1 =0;
    for (auto _ : state) {
        state.PauseTiming(); // Pause timing for setup
        left.set(xValues[state.iterations() % xValues.size()], yValues[state.iterations() % yValues.size()]);
        right.set(xValues[state.iterations() % xValues.size()], yValues[state.iterations() % yValues.size()]);
        state.ResumeTiming(); // Resume timing for the actual addition
        
        for(size_t i=0; i < 10000; ++i) {
            benchmark::DoNotOptimize(result1 += left.data.fields.x + right.data.fields.x);
            benchmark::DoNotOptimize(result1 += left.data.fields.y + right.data.fields.y);
        }
    }
}

// static void Benchmark_sqrt2Add(benchmark::State& state) {
//     size_t dataSize = state.range(0);
//     sqrt2_int left(0), right;
//     auto xValues = generateRandomData(dataSize);
//     auto yValues = generateRandomData(dataSize);

//     for (auto _ : state) {
//         state.PauseTiming(); // Pause timing for setup
//         right = sqrt2_int(xValues[state.iterations() % xValues.size()], yValues[state.iterations() % yValues.size()]);
//         state.ResumeTiming(); // Resume timing for the actual addition
//         #pragma unroll
//         for(size_t i = 0; i < 1000; ++i) {
//             benchmark::DoNotOptimize(left += right);
//         }
//     }
// }

// static void Benchmark_sqrt2Sub(benchmark::State& state) {
//     size_t dataSize = state.range(0);
//     sqrt2_int left(0), right;
//     auto xValues = generateRandomData(dataSize);
//     auto yValues = generateRandomData(dataSize);

//     for (auto _ : state) {
//         state.PauseTiming(); // Pause timing for setup
//         right = sqrt2_int(xValues[state.iterations() % xValues.size()], yValues[state.iterations() % yValues.size()]);
//         state.ResumeTiming(); // Resume timing for the actual addition
//         #pragma unroll
//         for(size_t i = 0; i < 1000; ++i) {
//             benchmark::DoNotOptimize(left -= right);
//         }
//     }
// }

static void Benchmark_Z2Add(benchmark::State& state) {
    size_t dataSize = state.range(0);
    Z2 left(static_cast<uint32_t>(0));
    Z2 right(static_cast<uint32_t>(0));
    auto xValues = generateRandomZ2(dataSize);

    for (auto _ : state) {
        state.PauseTiming(); // Pause timing for setup
            right = xValues[state.iterations() % xValues.size()];
        state.ResumeTiming(); // Resume timing for the actual addition
        #pragma unroll
        for(size_t i = 0; i < 1000; ++i)
            benchmark::DoNotOptimize(left += right);
    }
}

// static void Benchmark_Z2ArrayAdd(benchmark::State& state) {
//     size_t dataSize = state.range(0);
//     Z2_Array left(0), right(0);
//     auto xValues = generateRandom128(dataSize);
//     auto yValues = generateRandom128(dataSize);

//     for (auto _ : state) {
//         state.PauseTiming(); // Pause timing for setup
//             left = Z2_Array(xValues[state.iterations() % xValues.size()]);
//             right = Z2_Array(yValues[state.iterations() % yValues.size()]);
//         state.ResumeTiming(); // Resume timing for the actual addition
//         for(size_t i = 0; i < 1000; ++i)
//             benchmark::DoNotOptimize(left += right);
//     }
// }

static void Benchmark_BitFieldReduce(benchmark::State& state) {
    auto test_cases = generateRandomData(state.range(0));
    for (auto _ : state) {
        state.PauseTiming();
        BitfieldStruct num(test_cases[state.iterations() % test_cases.size()]);
        state.ResumeTiming();

        uint8_t tz = std::countr_zero(num.data.fields.y);
        if(std::countr_zero(num.data.fields.x) > tz) {
            tz = std::countr_zero(num.data.fields.x);
            num.swap();
        }
        num.data.packed = ((num.data.packed >> tz) & (((1 << (8 - tz + 1)) - 1) << tz) | (((1 << (8 - tz + 1)) - 1) << (9 + tz)));
        // Prevent compiler optimizations
        benchmark::DoNotOptimize(num);
        benchmark::DoNotOptimize(num.data.packed);
    }
}

static void Benchmark_BitFieldReduceExhaustive(benchmark::State& state) {
    std::vector<uint16_t> test_cases(65536);    // Exhaustive
    for (auto _ : state) {
        state.PauseTiming();
        BitfieldStruct num(test_cases[state.iterations() % test_cases.size()]);
        state.ResumeTiming();

        uint8_t tz = std::countr_zero(num.data.fields.y);
        if(std::countr_zero(num.data.fields.x) > tz) {
            tz = std::countr_zero(num.data.fields.x);
            num.swap();
        }
        num.data.packed = ((num.data.packed >> tz) & (((1 << (8 - tz + 1)) - 1) << tz) | (((1 << (8 - tz + 1)) - 1) << (9 + tz)));
        // Prevent compiler optimizations
        benchmark::DoNotOptimize(num);
        benchmark::DoNotOptimize(num.data.packed);
    }
}

// static void Benchmark_Sqrt2Reduce(benchmark::State& state) {
//     auto test_cases = generateRandomData(state.range(0));
//     for (auto _ : state) {
//         state.PauseTiming();
//         sqrt2_int num(test_cases[state.iterations() % test_cases.size()]);
//         state.ResumeTiming();

//         uint8_t exp = num.scientific_notation();
//          // Prevent compiler optimizations
//         benchmark::DoNotOptimize(num);
//         benchmark::DoNotOptimize(exp);
//     }
// }

static void Benchmark_ShiftSeparate(benchmark::State& state) {
    size_t dataSize = state.range(0);
    BitfieldStruct left, right;
    auto xValues = generateRandomData(dataSize);
    auto yValues = generateRandomData(dataSize);

    for (auto _ : state) {
        state.PauseTiming(); // Pause timing for setup
        left.set(xValues[state.iterations() % xValues.size()], yValues[state.iterations() % yValues.size()]);
        state.ResumeTiming(); // Resume timing for the actual addition

        int8_t result1 = left.data.fields.x << 4;
        benchmark::DoNotOptimize(result1);
        int8_t result2 = left.data.fields.y << 4;
        benchmark::DoNotOptimize(result2);
    }
}

static void Benchmark_ShiftTogether(benchmark::State& state) {
    size_t dataSize = state.range(0);
    BitfieldStruct left, right;
    auto xValues = generateRandomData(dataSize);
    auto yValues = generateRandomData(dataSize);

    uint16_t mask = static_cast<uint16_t>(0b1111) | (static_cast<uint16_t>(0b1111) << 9);

    for (auto _ : state) {
        state.PauseTiming(); // Pause timing for setup
        left.set(xValues[state.iterations() % xValues.size()], yValues[state.iterations() % yValues.size()]);
        state.ResumeTiming(); // Resume timing for the actual addition

        int16_t result1 = (left.data.packed << 4) & mask;
        benchmark::DoNotOptimize(result1);      
    }
}


static void BM_Empty(benchmark::State& state) {
  for (auto _ : state) {
    // literally do nothing
  }
}

// Function to calculate the number of iterations using Hoeffding's inequality
int CalculateHoeffdingIterations(float relative_error, float delta) {
    // Simplified formula for n
    return static_cast<int>(std::ceil(50 * std::log(2 / delta) / (relative_error * relative_error)));
}

float Chernoff_bound(float mean, int trials = 1, float relative_error = 0.05) {
    return 2*std::exp(-2*trials*mean*std::pow(relative_error, 2));
}

void BenchmarkPrime(benchmark::internal::Benchmark* b) {
    // Collect trial data
    int trials = CalculateHoeffdingIterations(.0005,.0005);
    const int mt = std::thread::hardware_concurrency()-2;
    b->Threads(mt)
    ->Arg(1+trials/mt)
    ->Repetitions(mt)
    ->ReportAggregatesOnly(true);
}

void BenchmarkConfig(benchmark::internal::Benchmark* b) {
    int trials = CalculateHoeffdingIterations(.05,.005);
    const int mt = std::thread::hardware_concurrency()-2;
    b->Threads(mt)
    ->Arg(1+trials/mt)
    ->Repetitions(mt)
    ->ReportAggregatesOnly(true);
}


std::vector<uint16_t> generateRandomUint16(size_t count) {
    std::vector<uint16_t> data(count);
    std::mt19937 rng(42);
    std::uniform_int_distribution<uint16_t> dist(0, 65535);
    for (auto &val : data) {
        val = dist(rng);
    }
    return data;
}

static void Benchmark_RawAdd(benchmark::State& state) {
    size_t dataSize = state.range(0);
    auto yValues = generateRandomUint16(dataSize);   
    uint16_t left = 0;
    for (auto _ : state) {
        state.PauseTiming(); // Pause timing for setup
        uint8_t right = yValues[state.iterations() % yValues.size()];
        state.ResumeTiming(); // Resume timing for the actual addition
        #pragma unroll 
        for (size_t i = 0; i < 1000; ++i) {
            benchmark::DoNotOptimize(left += right);
        }
    }
}

// inline uint16_t& add_to(uint16_t& a, uint16_t b) {
//     asm (
//         "1:\n\t"                      // Label for the start of the loop
//         "movw %0, %%cx\n\t"           // Copy A to CX (for first iteration carry calculation)
//         "xorw %1, %0\n\t"             // Compute sum without carry: A XOR B -> A
//         "andw %1, %%cx\n\t"           // Compute carry: A & B -> CX
//         "shlw $2, %%cx\n\t"           // Shift carry left by 2
//         "testw %%cx, %%cx\n\t"        // Check if carry from first iteration is zero
//         "jz 2f\n\t"                   // If carry is zero, skip the second iteration

//         // Second iteration
//         "xorw %%cx, %1\n\t"           // Compute sum without carry: B XOR carry -> B
//         "andw %0, %%cx\n\t"           // Compute carry: A (new) & carry (from CX) -> DX
//         "shlw $2, %%cx\n\t"           // Shift carry left by 2
//         "testw %%cx, %%cx\n\t"        // Check if carry from first iteration is zero
//         "jz 2f\n\t"                   // If carry is zero, skip the second iteration

//         "2:\n\t"                      // Label for loop exit

//         : "+r"(a), "+r"(b)            // Output: a and b are modified in place
//         :                            // No additional inputs
//         : "cx", "cc"           // Clobbers
//     );
//     return a;
// }



// static void Benchmark_MortonAdd(benchmark::State& state) {
//     size_t dataSize = state.range(0);
//     sqrt2_int_morton left, right;
//     auto xValues = generateRandomData16(dataSize);
//     auto yValues = generateRandomData16(dataSize);
//     sqrt2_int_morton result(0);

//     for (auto _ : state) {
//         state.PauseTiming(); // Pause timing for setup
//         left = sqrt2_int_morton(xValues[state.iterations() % xValues.size()]);
//         state.ResumeTiming(); // Resume timing for the actual addition
//         #pragma unroll
//         for(size_t i = 0; i < 1000; ++i)
//             benchmark::DoNotOptimize(result += left);
//     }
// }

BENCHMARK(Benchmark_RawAdd)->Apply(BenchmarkConfig);
BENCHMARK(Benchmark_Z2Add)->Apply(BenchmarkConfig);

// BENCHMARK(Benchmark_MortonAddOld)->Apply(BenchmarkConfig);

// BENCHMARK(Benchmark_Z2Add)->Apply(BenchmarkConfig);
// BENCHMARK(Benchmark_Z2ArrayAdd)->Apply(BenchmarkConfig);
// BENCHMARK(Benchmark_BitfieldAdd)->Apply(BenchmarkConfig);
// BENCHMARK(Benchmark_BitFieldReduceExhaustive)->Apply(BenchmarkConfig);
// BENCHMARK(Benchmark_ShiftSeparate)->Apply(BenchmarkConfig);
// BENCHMARK(Benchmark_ShiftTogether)->Apply(BenchmarkConfig);
// BENCHMARK(Benchmark_BitFieldReduce)->Apply(BenchmarkConfig);
// BENCHMARK(Benchmark_Sqrt2Reduce)->Apply(BenchmarkConfig);
BENCHMARK_MAIN();
