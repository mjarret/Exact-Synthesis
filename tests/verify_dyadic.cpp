/**
 * @file verify_dyadic.cpp
 * @brief Brute-force sanity checks for DyadicSqrt2 arithmetic mirroring verify_z2.
 *
 * This mirrors the Z2 exhaustive multiply identity on reduced ranges to
 * keep runtime reasonable under tests. It checks that for
 *   x = (a1 + b1*sqrt(2)) * 2^e1
 *   y = (a2 + b2*sqrt(2)) * 2^e2
 * we have
 *   x*y = ((a1*a2 + 2*b1*b2) + (a1*b2 + a2*b1)*sqrt(2)) * 2^(e1+e2)
 */

#include <tbb/parallel_for.h>
#include <tbb/blocked_range2d.h>
#include <atomic>
#include <mutex>
#include <iostream>
#include <cstdlib>

#include "DyadicSqrt2.hpp"

int main() {
    const int min_val = -15, max_val = 15;     // reduced vs Z2 mirror
    const int min_exp = -6,  max_exp = 6;

    std::atomic<size_t> failures{0};
    std::mutex log_mu;
    const int log_limit = 5; std::atomic<int> logged{0};

    tbb::parallel_for(
        tbb::blocked_range2d<int,int>(min_val, max_val+1, min_val, max_val+1),
        [&](const tbb::blocked_range2d<int,int>& range){
            for (int a1 = range.rows().begin(); a1 < range.rows().end(); ++a1) {
                for (int b1 = range.cols().begin(); b1 < range.cols().end(); ++b1) {
                    for (int a2 = min_val; a2 <= max_val; ++a2) {
                        for (int b2 = min_val; b2 <= max_val; ++b2) {
                            for (int e1 = min_exp; e1 <= max_exp; ++e1) {
                                for (int e2 = min_exp; e2 <= max_exp; ++e2) {
                                    DyadicSqrt2 x(static_cast<uint8_t>(a1), static_cast<uint8_t>(b1), static_cast<uint8_t>(e1));
                                    DyadicSqrt2 y(static_cast<uint8_t>(a2), static_cast<uint8_t>(b2), static_cast<uint8_t>(e2));
                                    DyadicSqrt2 prod = x * y;

                                    int exp_int   = a1*a2 + 2*b1*b2;
                                    int exp_s2    = a1*b2 + a2*b1;
                                    int exp_denom = e1 + e2;

                                    bool ok = (prod.int_c   == static_cast<int8_t>(exp_int)) &&
                                              (prod.sqrt2_c == static_cast<int8_t>(exp_s2)) &&
                                              (prod.denom_exp == static_cast<uint8_t>(exp_denom));
                                    if (!ok) {
                                        failures.fetch_add(1, std::memory_order_relaxed);
                                        int was = logged.fetch_add(1, std::memory_order_relaxed);
                                        if (was < log_limit) {
                                            std::lock_guard<std::mutex> lk(log_mu);
                                            std::cerr << "[dyadic-mul-mismatch] x=("<<a1<<","<<b1<<")*2^"<<e1
                                                      << " y=("<<a2<<","<<b2<<")*2^"<<e2
                                                      << " -> got ("<<int(prod.int_c)<<","<<int(prod.sqrt2_c)
                                                      << ")*2^"<<int(prod.denom_exp)
                                                      << " expected ("<<exp_int<<","<<exp_s2
                                                      << ")*2^"<<exp_denom << "\n";
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    );

    if (failures.load(std::memory_order_relaxed) != 0) {
        std::cerr << "[verify_dyadic] failures: " << failures.load() << "\n";
        return 1;
    }
    std::cout << "[verify_dyadic] OK" << std::endl;
    return 0;
}

