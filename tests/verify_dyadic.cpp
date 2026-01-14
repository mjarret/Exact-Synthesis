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
#include <cstdlib>
#include <iostream>
#include <limits>
#include <mutex>
#include <random>
#include <sstream>

#include "DyadicSqrt2.hpp"
#include "so6/SO6.hpp"

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

    const int roundtrip_samples = 10000;
    int roundtrip_failures = 0;
    const int roundtrip_log_limit = 5;

    std::mt19937 rng(0xD1A4D1C5u);
    std::uniform_int_distribution<int> coeff_dist(
        static_cast<int>(std::numeric_limits<int_t>::min()),
        static_cast<int>(std::numeric_limits<int_t>::max()));
    std::uniform_int_distribution<int> denom_dist(
        0, static_cast<int>(std::numeric_limits<uint8_t>::max()));

    for (int i = 0; i < roundtrip_samples; ++i) {
        const int ic = coeff_dist(rng);
        const int sc = coeff_dist(rng);
        const int de = denom_dist(rng);
        DyadicSqrt2 z(static_cast<int_t>(ic), static_cast<int_t>(sc), static_cast<uint8_t>(de));

        std::ostringstream oss;
        oss << z;
        const std::string s = oss.str();
        DyadicSqrt2 parsed(s);

        const bool coeff_ok = (parsed.int_c == z.int_c) && (parsed.sqrt2_c == z.sqrt2_c);
        const bool denom_ok = (z.numerator_bits == 0)
            ? (parsed.denom_exp == 0)
            : (parsed.denom_exp == z.denom_exp);

        if (!coeff_ok || !denom_ok) {
            ++roundtrip_failures;
            if (roundtrip_failures <= roundtrip_log_limit) {
                std::cerr << "[dyadic-parse-mismatch] in=" << s
                          << " got (" << int(parsed.int_c) << "," << int(parsed.sqrt2_c)
                          << ")e" << int(parsed.denom_exp)
                          << " expected (" << int(z.int_c) << "," << int(z.sqrt2_c)
                          << ")e" << (z.numerator_bits == 0 ? 0 : int(z.denom_exp))
                          << "\n";
            }
        }
    }

    const int so6_samples = 300;
    int so6_failures = 0;
    const int so6_log_limit = 3;

    for (int i = 0; i < so6_samples; ++i) {
        SO6 m;
        for (int r = 0; r < 6; ++r) {
            for (int c = 0; c < 6; ++c) {
                const int ic = coeff_dist(rng);
                const int sc = coeff_dist(rng);
                const int de = denom_dist(rng);
                DyadicSqrt2 z(static_cast<int_t>(ic), static_cast<int_t>(sc),
                             static_cast<uint8_t>(de));
                m.set_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c), z);
            }
        }

        std::ostringstream oss;
        m.print_mathematica(oss);
        const std::string s = oss.str();
        SO6 parsed(s);

        for (int r = 0; r < 6; ++r) {
            for (int c = 0; c < 6; ++c) {
                const DyadicSqrt2 orig = m.get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c));
                const DyadicSqrt2 got = parsed.get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c));
                const uint8_t expect_denom = (orig.numerator_bits == 0) ? 0 : orig.denom_exp;
                const bool ok = (orig.int_c == got.int_c)
                             && (orig.sqrt2_c == got.sqrt2_c)
                             && (got.denom_exp == expect_denom);
                if (!ok) {
                    ++so6_failures;
                    if (so6_failures <= so6_log_limit) {
                        std::cerr << "[so6-parse-mismatch] r=" << r << " c=" << c
                                  << " in=" << s
                                  << " got (" << int(got.int_c) << "," << int(got.sqrt2_c)
                                  << ")e" << int(got.denom_exp)
                                  << " expected (" << int(orig.int_c) << "," << int(orig.sqrt2_c)
                                  << ")e" << int(expect_denom) << "\n";
                    }
                }
            }
        }
    }

    const auto mul_failures = failures.load(std::memory_order_relaxed);
    if (mul_failures != 0 || roundtrip_failures != 0 || so6_failures != 0) {
        if (mul_failures != 0) {
            std::cerr << "[verify_dyadic] mul failures: " << mul_failures << "\n";
        }
        if (roundtrip_failures != 0) {
            std::cerr << "[verify_dyadic] parse failures: " << roundtrip_failures << "\n";
        }
        if (so6_failures != 0) {
            std::cerr << "[verify_dyadic] SO6 parse failures: " << so6_failures << "\n";
        }
        return 1;
    }
    std::cout << "[verify_dyadic] OK" << std::endl;
    return 0;
}
