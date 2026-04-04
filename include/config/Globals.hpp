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
extern bool use_tt;

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
