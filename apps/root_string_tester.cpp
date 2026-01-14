/**
 * @file root_string_tester.cpp
 * @brief Parse a Mathematica-style SO6 matrix from --root and round-trip print.
 */

#include <iostream>
#include <sstream>
#include <string>

#include "so6/SO6.hpp"

namespace {

struct Args {
    std::string root;
};

Args parse_args(int argc, char** argv) {
    Args a;
    for (int i = 1; i < argc; ++i) {
        std::string s(argv[i]);
        if (s == "-h" || s == "--help") {
            std::cout << "Usage: root_tester --root='{{a,b,...},{...}}'\n";
            std::exit(0);
        }
        const char* key = "--root=";
        if (s.rfind(key, 0) == 0) {
            a.root = s.substr(std::string(key).size());
        }
    }
    return a;
}

bool matrices_equal(const SO6& a, const SO6& b) {
    for (int r = 0; r < 6; ++r) {
        for (int c = 0; c < 6; ++c) {
            const auto va = a.get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c));
            const auto vb = b.get_element(static_cast<uint8_t>(r), static_cast<uint8_t>(c));
            if (va.data != vb.data) return false;
        }
    }
    return true;
}

} // namespace

int main(int argc, char** argv) {
    Args args = parse_args(argc, argv);
    if (args.root.empty()) {
        std::cerr << "Missing --root. Try --root='{{1,0e0,...},{...}}'\n";
        return 2;
    }

    SO6 root(args.root);

    std::ostringstream oss;
    root.print_mathematica(oss);
    const std::string roundtrip = oss.str();
    SO6 parsed(roundtrip);

    std::cout << roundtrip << "\n";
    if (!matrices_equal(root, parsed)) {
        std::cerr << "[root_tester] round-trip mismatch\n";
        return 1;
    }
    std::cout << "[root_tester] OK\n";
    return 0;
}
