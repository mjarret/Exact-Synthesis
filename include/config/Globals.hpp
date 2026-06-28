/**
 * @file Globals.hpp
 * @brief Global configuration and runtime parameters.
 */
#ifndef GLOBALS_HPP
#define GLOBALS_HPP

#include <cstdint>
#include <string>

// Threading and performance tracking
extern uint8_t THREADS;

// Configuration and state variables used outside Globals.cpp
extern uint8_t target_T_count;
extern uint8_t stored_depth_max;
extern bool verbose;
extern bool suppress_indicators;
extern bool mitm_bf_extension;

// LUT generator mode. T: one edge = one T operator, layer n = T-depth n (default).
// TT: one edge = two fused T operators, layer n = T-depth 2n. Depth CLI args stay in
// actual T-count units; pure TT requires an even target depth.
enum class GeneratorKind : uint8_t { T, TT };
extern GeneratorKind generator_kind;

/**
 * @brief Static helper for setting and validating global parameters.
 */
class Globals {
    public:
        static void setParameters(int argc, char *argv[]);
        static void configure();
        static const std::string& root_spec();
};
#endif // GLOBALS_HPP
