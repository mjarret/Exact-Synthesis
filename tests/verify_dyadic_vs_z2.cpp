/**
 * @file verify_dyadic_vs_z2.cpp
 * @brief Cross-check DyadicSqrt2 operations against Z2 across small ranges.
 *
 * Rationale: If Z2 has been validated independently, we can accept DyadicSqrt2
 * correctness by brute-forcing operations and comparing results byte-for-byte
 * (24-bit packed layout) with Z2 for the same inputs.
 */

#include <tbb/parallel_for.h>
#include <tbb/blocked_range2d.h>
#include <atomic>
#include <mutex>
#include <iostream>
#include <cstdlib>

#include "Z2.hpp"
#include "DyadicSqrt2.hpp"

static inline bool eq24(uint32_t a, uint32_t b) { return (a & 0xFFFFFFu) == (b & 0xFFFFFFu); }

int main() {
    // Ranges chosen to keep runtime modest while exercising diverse cases
    const int min_iv = -6,  max_iv = 6;   // int_c, sqrt2_c
    const int min_exp = -3, max_exp = 3;  // denom_exp

    std::atomic<size_t> mismatches{0};
    std::mutex log_mu; std::atomic<int> logged{0}; const int log_limit = 10;

    tbb::parallel_for(
        tbb::blocked_range2d<int,int>(min_iv, max_iv+1, min_iv, max_iv+1),
        [&](const tbb::blocked_range2d<int,int>& r){
            for (int a1 = r.rows().begin(); a1 < r.rows().end(); ++a1) {
                for (int b1 = r.cols().begin(); b1 < r.cols().end(); ++b1) {
                    for (int a2 = min_iv; a2 <= max_iv; ++a2) {
                        for (int b2 = min_iv; b2 <= max_iv; ++b2) {
                            for (int e1 = min_exp; e1 <= max_exp; ++e1) {
                                for (int e2 = min_exp; e2 <= max_exp; ++e2) {
                                    // Construct values (cast to uint8_t to match bitfield storage)
                                    Z2 z1(static_cast<uint8_t>(a1), static_cast<uint8_t>(b1), static_cast<uint8_t>(e1));
                                    Z2 z2(static_cast<uint8_t>(a2), static_cast<uint8_t>(b2), static_cast<uint8_t>(e2));
                                    DyadicSqrt2 d1(static_cast<uint8_t>(a1), static_cast<uint8_t>(b1), static_cast<uint8_t>(e1));
                                    DyadicSqrt2 d2(static_cast<uint8_t>(a2), static_cast<uint8_t>(b2), static_cast<uint8_t>(e2));

                                    // Addition
                                    if (!eq24((z1 + z2).data, (d1 + d2).data)) {
                                        size_t m = mismatches.fetch_add(1, std::memory_order_relaxed) + 1;
                                        int was = logged.fetch_add(1, std::memory_order_relaxed);
                                        if (was < log_limit) {
                                            std::lock_guard<std::mutex> lk(log_mu);
                                            std::cerr << "[add] mismatch z vs d for ("<<a1<<","<<b1<<","<<e1
                                                      << ") + ("<<a2<<","<<b2<<","<<e2<<")\n";
                                        }
                                    }
                                    // Subtraction
                                    if (!eq24((z1 - z2).data, (d1 - d2).data)) {
                                        mismatches.fetch_add(1, std::memory_order_relaxed);
                                        int was = logged.fetch_add(1, std::memory_order_relaxed);
                                        if (was < log_limit) {
                                            std::lock_guard<std::mutex> lk(log_mu);
                                            std::cerr << "[sub] mismatch\n";
                                        }
                                    }
                                    // Multiplication
                                    if (!eq24((z1 * z2).data, (d1 * d2).data)) {
                                        mismatches.fetch_add(1, std::memory_order_relaxed);
                                        int was = logged.fetch_add(1, std::memory_order_relaxed);
                                        if (was < log_limit) {
                                            std::lock_guard<std::mutex> lk(log_mu);
                                            std::cerr << "[mul] mismatch\n";
                                        }
                                    }
                                    // Negation
                                    if (!eq24((-z1).data, (-d1).data)) {
                                        mismatches.fetch_add(1, std::memory_order_relaxed);
                                        int was = logged.fetch_add(1, std::memory_order_relaxed);
                                        if (was < log_limit) {
                                            std::lock_guard<std::mutex> lk(log_mu);
                                            std::cerr << "[neg] mismatch\n";
                                        }
                                    }
                                    // Shifts: left then right (single step)
                                    {
                                        Z2 zl = z1; DyadicSqrt2 dl = d1;
                                        zl <<= 1; dl <<= 1;
                                        if (!eq24(zl.data, dl.data)) {
                                            mismatches.fetch_add(1, std::memory_order_relaxed);
                                            int was = logged.fetch_add(1, std::memory_order_relaxed);
                                            if (was < log_limit) {
                                                std::lock_guard<std::mutex> lk(log_mu);
                                                std::cerr << "[shl] mismatch\n";
                                            }
                                        }
                                    }
                                    {
                                        Z2 zr = z1; DyadicSqrt2 dr = d1;
                                        zr >>= 1; dr >>= 1;
                                        if (!eq24(zr.data, dr.data)) {
                                            mismatches.fetch_add(1, std::memory_order_relaxed);
                                            int was = logged.fetch_add(1, std::memory_order_relaxed);
                                            if (was < log_limit) {
                                                std::lock_guard<std::mutex> lk(log_mu);
                                                std::cerr << "[shr] mismatch\n";
                                            }
                                        }
                                    }
                                    // Equality
                                    {
                                        bool eqz = (z1.data == z2.data);
                                        bool eqd = (d1.data == d2.data);
                                        if (eqz != eqd) {
                                            mismatches.fetch_add(1, std::memory_order_relaxed);
                                            int was = logged.fetch_add(1, std::memory_order_relaxed);
                                            if (was < log_limit) {
                                                std::lock_guard<std::mutex> lk(log_mu);
                                                std::cerr << "[eq] mismatch\n";
                                            }
                                        }
                                    }
                                    // Ordering (if available)
                                    #if __cpp_impl_three_way_comparison
                                    {
                                        auto oz = (z1 <=> z2);
                                        auto od = (d1 <=> d2);
                                        if ((oz == std::strong_ordering::less) != (od == std::strong_ordering::less) ||
                                            (oz == std::strong_ordering::greater) != (od == std::strong_ordering::greater) ||
                                            (oz == std::strong_ordering::equal) != (od == std::strong_ordering::equal)) {
                                            mismatches.fetch_add(1, std::memory_order_relaxed);
                                            int was = logged.fetch_add(1, std::memory_order_relaxed);
                                            if (was < log_limit) {
                                                std::lock_guard<std::mutex> lk(log_mu);
                                                std::cerr << "[cmp] mismatch\n";
                                            }
                                        }
                                    }
                                    #endif
                                }
                            }
                        }
                    }
                }
            }
        }
    );

    if (mismatches.load(std::memory_order_relaxed) != 0) {
        std::cerr << "[verify_dyadic_vs_z2] mismatches: " << mismatches.load() << "\n";
        return 1;
    }
    std::cout << "[verify_dyadic_vs_z2] OK" << std::endl;
    return 0;
}

