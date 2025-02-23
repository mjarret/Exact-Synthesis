#ifndef SQRT2_INT_TESTS_HPP
#define SQRT2_INT_TESTS_HPP

#include <iostream>
#include <fstream>
#include <iomanip>
#include <utility>
#include <cassert>
#include <atomic>
#include <mutex>
#include <bit>
#include <limits>
#include <bitset>
#include <tbb/parallel_for.h>
#include <tbb/blocked_range2d.h>
#include <tbb/global_control.h>
#include "../sqrt2_int.hpp"
// #include "../sqrt2_int_MORTON.hpp"
#include "../indicators/progress_bar.hpp"

// using sqrt2_int = sqrt2_int_morton;

void parallel_exhaustive_test_sqrt2_int() {
    std::cout << "Starting parallel exhaustive tests with progress indicator...\n";
    std::mutex progress_mutex;

    const uint16_t min_val = std::numeric_limits<uint16_t>::min(), max_val = std::numeric_limits<uint16_t>::max();
    const size_t total_tests = static_cast<size_t>(max_val - min_val + 1) *
                               static_cast<size_t>(max_val - min_val + 1) ;
    std::atomic<size_t> progress_counter{0};

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
        indicators::option::FontStyles{std::vector<indicators::FontStyle>{indicators::FontStyle::bold}}
    };

    tbb::parallel_for(
        tbb::blocked_range2d<int, int>(min_val,max_val,min_val,max_val),

        [&](const tbb::blocked_range2d<int, int>& range) {
            size_t local_progress = 0;

            for (size_t x = range.rows().begin(); x < range.rows().end(); ++x) {
                for (size_t y = range.cols().begin(); y < range.cols().end(); ++y) {
                    sqrt2_int num1(x), num2(y);
                    if(num1.int_c == 0) continue;
                    // if(num1.int_c == 0 && num1.sqrt2_c != 0) {
                    //     std::cout << "num1: " << num1 << static_cast<int>(num1.data % 256) << std::endl;
                    //     continue;
                    // }
                    sqrt2_int result = num1;
                    result += num2;

                    // Assume no overflow
                    if((num1.int_c + num2.int_c)> 256) {
                        continue;
                    }

                    // Test addition
                    sqrt2_int result_add = num1 + num2;
                    if(result_add.int_c != (num1.int_c + num2.int_c) % 256 || result_add.sqrt2_c != (num1.sqrt2_c + num2.sqrt2_c) % 256) if(!(static_cast<int>(num2.int_c)+static_cast<int>(num1.int_c)) > 255) {
                        std::cout << "num1: " << num1 << " num2: " << num2 << " result: " << result_add << std::endl;
                        std::exit(EXIT_FAILURE);
                    }

                    // Test negation
                    uint8_t a1 = num1.int_c, b1 = num1.sqrt2_c;
                    sqrt2_int negated = -num1;
                    // assert(negated.int_c == -a1 && "Negation failed");
                    // assert(negated.sqrt2_c == -b1 && "Negation failed");
                    if(negated.int_c != (256-a1)%256 || negated.sqrt2_c != (256-b1)%256) {
                        std::cout << "num1: " << num1 << " negated: " << negated << std::endl;
                        std::cout << "-a1: " << static_cast<int>((256-a1)%256) << " -b1: " << static_cast<int>((256-b1)%256) << std::endl;
                        std::exit(EXIT_FAILURE);
                    }

                    // Test substraction
                    sqrt2_int result_sub = num1 - num2;
                    if( (result_sub.int_c != static_cast<uint8_t>((int(num1.int_c) - int(num2.int_c) + 256) % 256)) ||
    (result_sub.sqrt2_c != static_cast<uint8_t>((int(num1.sqrt2_c) - int(num2.sqrt2_c) + 256) % 256)) ) {
                        std::cout << "subtraction failure: " << std::endl;
                        std::cout << "(" << num1 << ") - (" << num2 << ") = " << result_sub << std::endl;
                        std::cout << "(" << num1.int_c - num2.int_c << ") - (" << num1.sqrt2_c - num2.sqrt2_c << ") = " << static_cast<int>(result_sub.int_c) << " - " << static_cast<int>(result_sub.sqrt2_c) << std::endl;
                        std::cout << "num1 bits: " << std::bitset<8>(num1.int_c) << " " << std::bitset<8>(num1.sqrt2_c) << std::endl;
                        std::cout << "num2 bits: " << std::bitset<8>(num2.int_c) << " " << std::bitset<8>(num2.sqrt2_c) << std::endl;
                        std::cout << "result bits: " << std::bitset<8>(result_sub.int_c) << " " << std::bitset<8>(result_sub.sqrt2_c) << std::endl;
                        std::cout << "num1: " << num1 << " num2: " << num2 << " result: " << result_sub << std::endl;
                        std::exit(EXIT_FAILURE);
                    }
                    if((num1.data + (-num1).data - 256)%65536 != 0) std::exit(0);
                    // // Test multiplication
                    // sqrt2_int result_mul = num1 * num2;
                    // assert(result_mul.int_c == static_cast<int8_t>(a1 * a2 + 2 * b1 * b2) && "Multiplication failed");
                    // assert(result_mul.sqrt2_c == static_cast<int8_t>(a1 * b2 + b1 * a2) && "Multiplication failed");

                    // sqrt2_int expected = num1;
                    // num2 = num1;
                    // // Test scientific notation
                    // uint8_t expected_exponent = 0;
                    // while(!(expected.int_c & 1) && (expected != 0)) {
                    //     expected.int_c >>= 1;
                    //     expected.swap();
                    //     expected_exponent++;
                    // }
                    // uint8_t actual_exponent = num2.scientific_notation();
                    // if(expected == 0 && num2 == 0) continue;
                    // if (expected_exponent != actual_exponent || expected.int_c != num2.int_c) {
                    //     num1.scientific_notation();
                    //     std::exit(EXIT_FAILURE);
                    // }
                    // // Update progress
                    ++local_progress;
                }
            }

            progress_counter.fetch_add(local_progress);
            std::lock_guard<std::mutex> lock(progress_mutex);
            bar.set_progress(static_cast<float>(static_cast<double>(progress_counter) / static_cast<double>(total_tests) * 100));
        }
    );

    bar.set_progress(100);
    bar.set_option(indicators::option::PostfixText{"Complete!"});
    std::cout << "\nParallel exhaustive tests completed successfully!\n";
}

int main() {
    tbb::global_control c(tbb::global_control::max_allowed_parallelism, 1);

    parallel_exhaustive_test_sqrt2_int();
    return 0;
}

#endif // SQRT2_INT_TESTS_HPP
