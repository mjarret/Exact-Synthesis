#ifndef VERIFY_Z2_HPP
#define VERIFY_Z2_HPP

#include <iostream>
#include <sstream>
#include <bitset>
#include <mutex>
#include <atomic>
#include <cassert>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range2d.h>
#include <tbb/task.h>
#include <tbb/task_group.h>
#include "indicators/progress_bar.hpp"
#include "Z2.hpp"

#ifdef DEBUG
    #include <tbb/global_control.h>
    tbb::global_control c(tbb::global_control::max_allowed_parallelism, 1);
#endif

// Function to get the natural representation of Z2
std::string natural_repr(const Z2& z) {
    int8_t int_c_signed = static_cast<int8_t>(z.int_c);
    int8_t sqrt2_c_signed = static_cast<int8_t>(z.sqrt2_c);
    int8_t denom_exp_signed = static_cast<int8_t>(z.denom_exp);

    std::ostringstream repr;
    bool has_value = false;

    if (int_c_signed != 0) {
        repr << int_c_signed;
        has_value = true;
    }
    if (sqrt2_c_signed != 0) {
        if (has_value) repr << " + ";
        repr << sqrt2_c_signed << "√2";
        has_value = true;
    }
    if (!has_value) repr << "0";  

    repr << " * 2^" << static_cast<int>(denom_exp_signed);
    return repr.str();
}

// Diagnostic function to check invalid `int_c` values
void check_int_c_validity(const Z2& z, const std::string& label) {
    if (z.int_c % 2 == 0 && z.int_c != 0) {
        std::cerr << "❌ ERROR: Found an even int_c (" << static_cast<int>(z.int_c) 
                  << ") in " << label << "\n";
        std::cerr << "  Full bit representation: ("
                  << std::bitset<8>(z.int_c) << ", "
                  << std::bitset<8>(z.sqrt2_c) << ", "
                  << std::bitset<8>(z.denom_exp) << ")\n";
        std::cerr << "  Natural representation: " << natural_repr(z) << "\n";
        std::exit(1);
    }
}

void verify_z2_shifts() {
    std::cout << "Starting exhaustive bitwise shift verification...\n";
    std::atomic<bool> failure_detected(false);
    std::mutex print_mutex;

    tbb::parallel_for(
        tbb::blocked_range<uint16_t>(0, 65535), // Iterate over all 16-bit bitstrings
        [&](const tbb::blocked_range<uint16_t>& range) {
            for (uint16_t num_bits = range.begin(); num_bits < range.end(); ++num_bits) {
                if (failure_detected.load(std::memory_order_relaxed)) return;

                for (uint8_t shift = 1; shift < 8; ++shift) {  // Test shifts from 1 to 7
                    Z2 z;
                    z.numerator_bits = num_bits;

                    // Store original values before shift
                    int8_t original_int_c = z.int_c;
                    int8_t original_sqrt2_c = z.sqrt2_c;
                    
                    // **First method: Shift `int_c` and `sqrt2_c` separately**
                    int8_t separate_int_c = original_int_c >> shift;
                    int8_t separate_sqrt2_c = original_sqrt2_c >> shift;

                    // **Second method: Shift the full `numerator_bits` and extract components**
                    z >>= shift;
                    int8_t combined_int_c = z.int_c;
                    int8_t combined_sqrt2_c = z.sqrt2_c;

                    // **Verify right shift (`>>=`)**
                    if (separate_int_c != combined_int_c || separate_sqrt2_c != combined_sqrt2_c) {
                        std::lock_guard<std::mutex> lock(print_mutex);
                        std::cerr << "❌ ERROR: Right shift mismatch at num_bits = "
                                  << std::bitset<16>(num_bits) << " shift = " << static_cast<int>(shift) << "\n";
                        std::cerr << "  Input: int_c = " << static_cast<int>(original_int_c)
                                  << ", sqrt2_c = " << static_cast<int>(original_sqrt2_c) << "\n";
                        std::cerr << "  Separate: int_c = " << static_cast<int>(separate_int_c)
                                  << ", sqrt2_c = " << static_cast<int>(separate_sqrt2_c) << "\n";
                        std::cerr << "  Combined: int_c = " << static_cast<int>(combined_int_c)
                                  << ", sqrt2_c = " << static_cast<int>(combined_sqrt2_c) << "\n";
                        failure_detected.store(true, std::memory_order_relaxed);
                        return;
                    }

                    // Reset `Z2` object for left shift test
                    z.numerator_bits = num_bits;

                    // **First method: Shift `int_c` and `sqrt2_c` separately**
                    separate_int_c = (original_int_c << shift) & 0xFF;
                    separate_sqrt2_c = (original_sqrt2_c << shift) & 0xFF;

                    // **Second method: Shift the full `numerator_bits`**
                    z <<= shift;
                    combined_int_c = z.int_c;
                    combined_sqrt2_c = z.sqrt2_c;

                    // **Verify left shift (`<<=`)**
                    if (separate_int_c != combined_int_c || separate_sqrt2_c != combined_sqrt2_c) {
                        std::lock_guard<std::mutex> lock(print_mutex);
                        std::cerr << "❌ ERROR: Left shift mismatch at num_bits = "
                                  << std::bitset<16>(num_bits) << " shift = " << static_cast<int>(shift) << "\n";
                        std::cerr << "  Separate: int_c = " << static_cast<int>(separate_int_c)
                                  << ", sqrt2_c = " << static_cast<int>(separate_sqrt2_c) << "\n";
                        std::cerr << "  Combined: int_c = " << static_cast<int>(combined_int_c)
                                  << ", sqrt2_c = " << static_cast<int>(combined_sqrt2_c) << "\n";
                        failure_detected.store(true, std::memory_order_relaxed);
                        return;
                    }
                }
            }
        }
    );

    if (!failure_detected.load()) {
        std::cout << "✅ Shift verification passed for all cases!\n";
    } else {
        std::cerr << "❌ Shift verification failed. Check errors above.\n";
    }
}


void parallel_exhaustive_test_z2() {
    Z2 a(1, 0, 3), b(-1, 0, 1);
    Z2 c = a + b;
    std::cout << c << std::endl;
    std::exit(0);

    std::cout << "Starting parallel exhaustive addition test...\n";
    std::mutex print_mutex;
    std::atomic<bool> failure_detected(false);

    const uint16_t min_val = 0; // Start from 0x010000 to exclude cases where the top byte is 0
    const uint16_t max_val = 65535;

    std::mutex progress_mutex;
    std::atomic<size_t> progress_counter{0};
    
    const size_t total_tests = static_cast<size_t>(max_val - min_val + 1) *
                               static_cast<size_t>(max_val - min_val + 1);
    
    std::cout << "perform " << total_tests << " tests" << std::endl;

    // Progress bar setup
    indicators::ProgressBar bar{
        indicators::option::BarWidth{50},
        indicators::option::Start{"["},
        indicators::option::Fill{"■"},
        indicators::option::Lead{"■"},
        indicators::option::Remainder{"-"},
        indicators::option::End{"]"},
        indicators::option::ForegroundColor{indicators::Color::green},
        indicators::option::ShowElapsedTime{true},
        indicators::option::ShowRemainingTime{true},
        indicators::option::ShowPercentage{true},
        indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}
    };

    bar.set_progress(static_cast<float>(0));

    tbb::parallel_for(
        tbb::blocked_range2d<uint16_t, uint16_t>(min_val, max_val, min_val, max_val),
        [&](const tbb::blocked_range2d<uint16_t, uint16_t>& range) {
            size_t local_progress = 0;

            for (uint16_t a1 = range.rows().begin(); a1 < range.rows().end(); ++a1) {
                if((a1 % 2) == 0 || (a1 > 15872)) {
                    local_progress++;
                    continue;
                }
                for (uint16_t b1 = range.cols().begin(); b1 < a1; ++b1) {
                    if((b1 % 2) == 0 || (b1 > 15872)) {
                        local_progress++;
                        continue;
                    }

                    if (failure_detected.load(std::memory_order_relaxed)) return;
                    for(int8_t i = 17; i < 19; i++) {
                        Z2 test1(a1,i), test2(b1,17);

                        // Z2_old test1_old(static_cast<int8_t>(test1.int_c), 
                        //                 static_cast<int8_t>(test1.sqrt2_c), 
                        //                 static_cast<int8_t>(test1.denom_exp));
                        // Z2_old test2_old(static_cast<int8_t>(test2.int_c), 
                        //                 static_cast<int8_t>(test2.sqrt2_c), 
                        //                 static_cast<int8_t>(test2.denom_exp));

                        Z2 result_add = test1 + test2;
                       
                    }
                    local_progress++;
                }
                progress_counter.fetch_add(local_progress, std::memory_order_relaxed);
                local_progress = 0;
                bar.set_progress(static_cast<float>(static_cast<double>(progress_counter) / static_cast<double>(total_tests) * 10000));    
            }
        }
    );

    std::cout << "\n✅ Parallel exhaustive addition test completed successfully!\n";
}



int main() {
    verify_z2_shifts();
    parallel_exhaustive_test_z2();
    return 0;
}

#endif // VERIFY_Z2_HPP
