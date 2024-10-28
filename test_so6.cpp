// #include <gtest/gtest.h>      // Google Test framework
#include <algorithm>          // For std::shuffle
#include <random>             // For generating random numbers
#include <set>
#include <chrono>
#include "SO6.hpp"            // Include SO6 header
#include "Z2.hpp"             // Include Z2 header
#include "utils.hpp"
#include <iostream>           // For standard input/output


// This is the smartest test SO6 :)
SO6 test_SO6() {
    SO6 ret;
    for(int row = 0; row < 6; row++) {
        for(int col = 0; col < 6; col++) {
            ret.get_element(row,col) = Z2(row,col,0);
        }
    }
    return ret;
}


// Main function for running tests
int main(int argc, char **argv) {
   SO6 test = test_SO6();
    // Optionally, you can print the unpermuted matrix
    // test.unpermuted_print();

    // Generate the initial permutation
    uint8_t row_perm[6] = {0, 1, 2, 3, 4, 5};

    int permutation_count = 0;
    int total_tests = 0;
    int passed_tests = 0;

    do {
        permutation_count++;
        bool permutation_passed = true;

        // Create custom iterators with the current permutation
        SO6::Iterator customBegin(test, 0, row_perm);
        SO6::Iterator customEnd(test, 36, row_perm);

        int index = 0;
        for (SO6::Iterator it = customBegin; it != customEnd; ++it, ++index) {
            const Z2& value = *it;

            int expected_row = row_perm[index % 6];
            int expected_col = test.Col[index / 6];  // Assuming Col is unpermuted

            Z2 expected_value(expected_row, expected_col, 0);

            if (value != expected_value) {
                permutation_passed = false;
                std::cout << "Mismatch in permutation #" << permutation_count << " at index " << index << std::endl;
                std::cout << "Expected: " << expected_value << ", Got: " << value << std::endl;
            }

            total_tests++;
        }

        if (permutation_passed) {
            passed_tests++;
            // Optionally, print success message
            // std::cout << "Permutation #" << permutation_count << " passed." << std::endl;
        }

    } while (std::next_permutation(row_perm, row_perm+6));

    std::cout << "Total permutations tested: " << permutation_count << std::endl;
    std::cout << "Total tests: " << total_tests << std::endl;
    std::cout << "Permutations passed: " << passed_tests << std::endl;

    if (passed_tests == permutation_count) {
        std::cout << "All permutations passed successfully!" << std::endl;
    } else {
        std::cout << (permutation_count - passed_tests) << " permutations failed." << std::endl;
    }

    for(int i = 0; i < 6; i++) {
        row_perm[i] = i;
    }
    uint8_t row_perm2[6] = {0, 1, 2, 3, 4, 5};
    SO6::Iterator fixedBegin(test, 0, row_perm2);
    SO6::Iterator fixedEnd(test, 6, row_perm2);
    do {
        std::cout << "Current row permutation: ";
        for(int i = 0; i < 6; i++) {
            std::cout << (int) row_perm[i] << " ";
        }
        std::cout << std::endl;
        for(uint8_t sign_perm = 0; sign_perm < 32; sign_perm++) {
            std::cout << "\tSign permutation: " << (int) sign_perm << " (";
            for (int bit = 0; bit < 5; ++bit) {
                std::cout << ((sign_perm >> bit) & 1);
            }
            std::cout << ")" << std::endl;

            SO6::Iterator customBegin(test, 0, row_perm);
            SO6::Iterator customEnd(test, 6, row_perm);
            utils::lex_order(customBegin, customEnd, fixedBegin, fixedEnd, sign_perm, 0);
        }
    } while (std::next_permutation(row_perm, row_perm+6));

    std::cout << "\n";
    for(int c = 0; c < 6; c++) {
        auto indices = test.get_column(c);
        for(SO6::Iterator it = indices.first; it != indices.second; ++it) {
            std::cout << *it << " ";
        }
        std::cout << std::endl;
    }

    std::cout << "Testing SO6::operator< function" << std::endl;
    SO6 test1 = test_SO6();
    SO6 test2 = test_SO6();
    std::cout << (test1 < test2) << std::endl;
    test2.get_element(5,4) = -test2.get_element(5,4);
    std::cout << (test1 < test2) << std::endl;
    for(int i = 0; i < 5; i++) {
        test2.get_element(i,4) = -test2.get_element(i,4);
    }
    std::cout << (test1 < test2) << std::endl;

    std::cout << "Testing get_column with permutations " << std::endl;
    for(int i = 0; i < 6; i++) {
        row_perm[i] = i;
    }
    do {
        for(int i = 0; i < 6; i++) {
            std::cout << (int) row_perm[i] << " ";
        }
        std::cout << std::endl;
        auto indices = test.get_column(0, row_perm);
        SO6::Iterator customBegin(test, 0, row_perm);
        for(SO6::Iterator it = indices.first; it != indices.second; ++it) {
            std::cout << *it << " ";
            std::cout << *customBegin << " " << std::endl;
            ++customBegin;
        }
        std::cout << std::endl;
    } while(std::next_permutation(row_perm, row_perm+6));
    
    test.get_element(0,3) = Z2(100,0,0);
    test.get_element(5,4) = Z2(70,0,0);
    test.get_element(0,1) = Z2(65,0,0);
    test.get_element(0,2) = Z2(-100,0,0);
    test.unpermuted_print();
    test.canonical_form();
    test.unpermuted_print();

    return 0;
}
