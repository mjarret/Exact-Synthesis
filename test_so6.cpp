// #include <gtest/gtest.h>      // Google Test framework
#include <algorithm>          // For std::shuffle
#include <random>             // For generating random numbers
#include <set>
#include <chrono>
#include <optional>           // For std::optional
#include "SO6.hpp"            // Include SO6 header
#include "Z2.hpp"             // Include Z2 header
#include "utils.hpp"
#include "pattern.hpp"
#include <iostream>           // For standard input/output
#include <cassert>            // For assert
#include "uint72_t.hpp"

// Utility function to print test case results
void print_test(const std::string& test_name, bool result) {
    std::cout << test_name << ": " << (result ? "PASSED" : "FAILED") << std::endl;
    assert(result && "Test failed! Check your implementation.");
}

void test_uint72_t() {
    std::cout << "Testing uint72_t...\n";

    // Test 1: Default Constructor
    uint72_t a;
    print_test("Default Constructor", a.low_bits == 0 && a.high_bits == 0);

    // Test 2: Parameterized Constructor
    uint72_t b(0xFFFFFFFFFFFFFFFFULL, 0xFF);
    print_test("Parameterized Constructor", b.low_bits == 0xFFFFFFFFFFFFFFFFULL && b.high_bits == 0xFF);

    // Test 3: Set and Get Single Bit
    a(0, true);  // Set bit 0 to 1
    a(71, true); // Set bit 71 to 1
    print_test("Set Single Bit", a[0] == true && a[71] == true);
    a(0, false); // Clear bit 0
    a(71, false); // Clear bit 71
    print_test("Clear Single Bit", a[0] == false && a[71] == false);

    // Test 4: Set and Get Bit Pair
    a.set_pair(62, 0b11);
    a.set_pair(70, 0b01);
    print_test("Set Bit Pair", a.get_pair(62) == 0b11 && a.get_pair(70) == 0b01);

    // Test 5: Shift Left
    uint72_t c(1, 0); // Start with 0b...0001
    bool pass = true;
    for(int shift = 0; shift < 71; ++shift) {
        if(shift < 64) {
            if((c.low_bits != 1ULL << shift) || (c.high_bits != 0)) {
                std::cout << "Failed at shift = " << shift << std::endl;
                std::cout << c << std::endl;
                std::cout << (1ULL << shift) << std::endl;
                pass = false;
                break;
            }
        } else {
            if((c.low_bits != 0) || (c.high_bits != (1 << (shift - 64)))) {
                pass = false;
                break;
            }
        }
        c = c << 1;
    }
    print_test("Shift Left (Beyond High Bits)", pass);

    // Test 6: Shift Right
    uint72_t d(0, 2); // Start with 0b10...0000
    d = d >> 1;
    print_test("Shift Right", d.low_bits == 0 && d.high_bits == 1);
    d = d >> 1;
    print_test("Shift Right (Beyond High Bits)", d.low_bits == (1ULL<<63) && d.high_bits == 0);

    // Test 7: AND Operator
    uint72_t e(0xAAAAAAAAAAAAAAAAULL, 0xAA);
    uint72_t f(0x5555555555555555ULL, 0x55);
    uint72_t g = e & f;
    print_test("AND Operator", g.low_bits == 0 && g.high_bits == 0);

    // Test 8: Negation Operator (~)
    uint72_t h(~(e.low_bits),~(e.high_bits));
    print_test("Negation Operator", h.low_bits == (~e).low_bits && h.high_bits == (~e).high_bits);

    // Test 9: Addition
    uint72_t i(0xFFFFFFFFFFFFFFFFULL, 0xFF);
    uint72_t j(1, 0);
    uint72_t k = i + j;
    print_test("Addition", k.low_bits == 0 && k.high_bits == 0);

    // Test 10: Subtraction
    uint72_t l(0, 1);
    uint72_t m(1, 0);
    uint72_t n = l - m;
    print_test("Subtraction", n.low_bits == (~0) && n.high_bits == 0);

    // Test 11: Get Bits
    uint72_t o(0x123456789ABCDEF0ULL, 0x12);
    uint16_t bits = o.get_bits(60);
    print_test("Get Bits", bits == 0b0000000100100001);

    // Test 12: Printing
    std::cout << "Printing Test: " << o << std::endl;

    std::cout << "All tests PASSED.\n";
}


Z2 rand_z2(bool flag = true) {
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<int> odd_dist(-14,14);
    int odd = 2*odd_dist(g)-1;
    return flag ? Z2(odd,odd_dist(g),odd_dist(g)) : Z2(odd,odd_dist(g),15);
}

// This is the smartest test SO6 :)
SO6 case_1() {
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<int> odd_dist(-14,14);
    int odd; 

    SO6 ret;
    for(int row = 0; row < 2; row++) {
        for(int col = 0; col < 2; col++) {
            odd = 2*odd_dist(g)-1;
            ret.get_element(row,col) = rand_z2(false);
        }
    }
    for(int row = 2; row < 6; row++) {
        for(int col = 2; col < 6; col++) {
            odd = 2*odd_dist(g)-1;
            ret.get_element(row,col) = rand_z2();
        }
    }
    return ret;
}

SO6 case_2() {
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<int> odd_dist(-14,14);

    SO6 ret;
    for(int row = 0; row < 4; row++) {
        for(int col = 0; col < 2; col++) {
            int odd = 2*odd_dist(g)-1;
            ret.get_element(row,col) = rand_z2(false);
        }
    }

    for(int row = 4; row < 6; row++) {
        for(int col = 2; col < 6; col++) {
            int odd = 2*odd_dist(g)-1;
            ret.get_element(row,col) = rand_z2();
        }
    }
    return ret;
}

SO6 case_2_t() {
    std::random_device rd;
    std::mt19937 g(rd());
    std::uniform_int_distribution<int> odd_dist(-14,14);
    

    SO6 ret;
    for(int row = 0; row < 2; row++) {
        for(int col = 0; col < 4; col++) {
            int odd = 2*odd_dist(g)-1;
            ret.get_element(row,col) = rand_z2(false);
        }
    }
    for(int row = 2; row < 6; row++) {
        for(int col = 4; col < 6; col++) {
            int odd = 2*odd_dist(g)-1;
            ret.get_element(row,col) = rand_z2();
        }
    }
    return ret;
}

SO6 case_3() {
    SO6 ret;
    for(int row = 0; row < 4; row++) {
        for(int col = 0; col < 4; col++) {
            ret.get_element(row,col) = rand_z2(false);
        }
    }
    for(int row = 4; row < 6; row++) {
        for(int col = 4; col < 6; col++) {
            ret.get_element(row,col) = rand_z2();
        }
    }
    return ret;
}

SO6 case_4() {
    SO6 ret;
    for(int row = 0; row < 4; row++) {
        for(int col = 0; col < 4; col++) {
            ret.get_element(row,col) = rand_z2(false);
        }
    }
    for(int row = 2; row < 5; row++) {
        for(int col = 2; col < 5; col++) {
            ret.get_element(row,col) = rand_z2();
        }
    }
    for(int row = 4; row < 6; row++) {
        for(int col = 4; col < 6; col++) {
            ret.get_element(row,col) = rand_z2();
        }
    }
    return ret;
}

SO6 case_5() {
    SO6 ret;
    for(int row = 0; row < 2; row++) {
        for(int col = 0; col < 2; col++) {
            ret.get_element(row,col) = rand_z2(false);
        }
    }
    for(int row = 2; row < 4; row++) {
        for(int col = 2; col < 4; col++) {
            ret.get_element(row,col) = rand_z2(false);
        }
    }
    return ret;
}

SO6 case_6() {
    SO6 ret;
    for(int row = 0; row < 2; row++) {
        for(int col = 0; col < 4; col++) {
            ret.get_element(row,col) = Z2(1,col %2 , 15);
        }
    }
    for(int row = 2; row < 4; row++) {
        for(int col = 0; col < 6; col++) {
            ret.get_element(row,col) = Z2(1,col %2 , 15);
        }
    }
    for(int row = 2; row < 4; row++) {
        for(int col = 2; col < 4; col++) {
            ret.get_element(row,col) = Z2(0,0,0);
        }
    }
    return ret;
}

SO6 case_6_t() {
    SO6 ret;
    for(int row = 0; row < 4; row++) {
        for(int col = 0; col < 2; col++) {
            ret.get_element(row,col) = Z2(1,col %2 , 2);
        }
    }
    for(int row = 0; row < 6; row++) {
        for(int col = 2; col < 4; col++) {
            ret.get_element(row,col) = Z2(1,col % 2, 2);
        }
    }
    for(int row = 2; row < 4; row++) {
        for(int col = 2; col < 4; col++) {
            ret.get_element(row,col) = Z2(0,0,0);
        }
    }
    return ret;
}

SO6 case_7() {
    SO6 ret;
    for(int row = 0; row < 2; row++) {
        for(int col = 0; col < 2; col++) {
            ret.get_element(row,col) = Z2(1,col %2 ,2);
        }
    }
    for(int row = 2; row < 4; row++) {
        for(int col = 2; col < 4; col++) {
            ret.get_element(row,col) = Z2(1,col % 2,2);
        }
    }
    for(int row = 4; row < 6; row++) {
        for(int col = 4; col < 6; col++) {
            ret.get_element(row,col) = Z2(1,col % 2,2);
        }
    }
    return ret;
}

std::vector<SO6> get_all_permutations(const SO6& original) {
    std::vector<SO6> permutations;
    std::vector<int> rows = {0, 1, 2, 3, 4, 5};
    std::vector<int> cols = {0, 1, 2, 3, 4, 5};

    do {
        do {
            // Create a new permuted matrix
            SO6 permutedMatrix;
            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < 6; ++j) {
                    permutedMatrix.get_element(i, j) = original.get_element(rows.at(i), cols.at(j));
                }
            }
            // Add the permuted matrix to the vector
            permutations.push_back(permutedMatrix);
        } while (std::next_permutation(cols.begin(), cols.end()));
    } while (std::next_permutation(rows.begin(), rows.end()));

    return permutations;
}


// Main function for running tests
int main(int argc, char **argv) {
    test_uint72_t(); // Run tests for uint72_t

    pattern pat;
    for(int row = 0; row < 6; row ++) for(int col = 0; col < 6; col++) {
        for(int k = 0 ; k < 4; k++) {
            pat.set(row,col,k);
            auto val = pat.get_val(row,col);
            if(val != k) {
                std::cerr << "Error setting value at " << row << "," << col << std::endl;
                return 1;
            }
            pat.set(row,col,0b00);
            val = pat.get_val(row,col);
            if(val != 0) {
                std::cerr << "Error resetting value at " << row << "," << col << std::endl;
                return 1;
            }
            pat.set(row,col,{k>>1,k&1});
            std::pair<bool,bool> boolpair = pat.get(row,col);
            if(boolpair.first != (k>>1) || boolpair.second != (k&1)) {
                std::cerr << "Error setting value at " << row << "," << col << std::endl;
                return 1;
            }
            pat.set(row,col,0b00);
            val = pat.get_val(row,col);
            if(val != 0) {
                std::cerr << "Error resetting value at " << row << "," << col << std::endl;
                return 1;
            }
        }
    }

    // Define the cases and their expected results
    struct TestCase {
        std::string name;           // Name of the case
        SO6 (*case_func)();         // Function to generate the case
        uint8_t expected_case_num;  // Expected case number
    };

    // List of test cases
    std::vector<TestCase> test_cases = {
        {"Case 1", case_1, 1},
        {"Case 2", case_2, 2},
        {"Case 2 Transposed", case_2_t, 2},
        {"Case 3", case_3, 3},
        {"Case 4", case_4, 4},
        {"Case 5", case_5, 5},
        {"Case 6", case_6, 6},
        {"Case 6 Transposed", case_6_t, 6},
        {"Case 7", case_7, 7}
    };

    // Run all test cases
    for (const auto& test_case : test_cases) {
        std::cout << "Testing " << test_case.name << "..." << std::endl;

        // Generate the SO6 instance
        SO6 test_matrix = test_case.case_func();
        test_matrix.unpermuted_print(); // Print the unpermuted matrix for debugging
        std::vector<SO6> permutations = get_all_permutations(test_matrix);

        bool passed = true;
        for (const auto& permuted_matrix : permutations) {
            pattern pat = permuted_matrix.to_pattern(); // Convert permutation to a pattern
            uint8_t case_num = pat.case_num();          // Get the detected case number
            if (case_num != test_case.expected_case_num) {
                passed = false;
                permuted_matrix.unpermuted_print(); // Print the unpermuted matrix for debugging
                std::cout << pat << std::endl;
                std::cout << "Expected case number: " << (int)test_case.expected_case_num << std::endl;
                std::cout << "Detected case number: " << (int)case_num << std::endl;
                std::cout << pat.pattern_data << std::endl;
                std::cin.get();
                break; // No need to check further
            }
        }

        if (passed) {
            std::cout << "Test PASSED for " << test_case.name << std::endl;
        } else {
            std::cerr << "Test FAILED for " << test_case.name << std::endl;
        }

        std::cout << "---------------------------------" << std::endl;
    }

    return 0;
}

