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
            ret.get_element(row,col) = Z2(row+1,col %3,0);
            if(!(col %2) && !(row %4)) ret.get_element(row,col).intPart = -ret.get_element(row,col).intPart;
        }
    }
    return ret;
}

// SO6 test_SO6() {
//     SO6 ret;
//     for(int row = 0; row < 6; row++) {
//         for(int col = 0; col < 6; col++) {
//             ret.get_element(row,col) = std::rand() % 2;
//             if (std::rand() % 2 == 0) {
//                 ret.get_element(row,col).negate();
//             }
//         }
//     }
//     return ret;
// }

SO6 test_SO6_cols() {
    SO6 ret;
    for(int row = 0; row < 6; row++) {
        for(int col = 0; col < 6; col++) {
            ret.get_element(row,col) = Z2(row,0,0);
        }
    }
    ret.get_element(5,0) = Z2(2,0,0);
    return ret;
}


// Main function for running tests
int main(int argc, char **argv) {
    // for(int i = 0; i < 20; i++) {
        SO6 id = SO6::identity();
        id = id.left_multiply_by_T(0);
        id.unpermuted_print();
        id=id.left_multiply_by_T(9);
        id.unpermuted_print();
        id=id.left_multiply_by_T(14);
        id.unpermuted_print();
    // }
        auto tmp = id.col_equivalence_classes();
        for(auto &[key, cols] : tmp) {
            std::cout << "(";
            for(auto col : cols) {
                std::cout << col;
            }
            std::cout << ")";
        }
        utils::print_sign_mask(id.sign_convention);
    return 0;
    // Initialize the SO6 object with test data
    SO6 obj = test_SO6();

    obj.unpermuted_print();
    auto c_eq_c = obj.col_equivalence_classes();



    // obj.canonical_form();

    std::pair sign_masks = utils::sign_masks(obj, obj.Row, c_eq_c);

    uint16_t row_sign_mask = sign_masks.first;
    uint16_t col_signs = sign_masks.second;

    std::cout << "row sign mask: ";
    utils::print_sign_mask(row_sign_mask);
    std::cout << std::endl;

    std::cout << "col sign mask: ";
    utils::print_sign_mask(col_signs);
    std::cout << std::endl;
    
    obj.unpermuted_print();

    obj = utils::apply_sign_mask(obj, row_sign_mask, col_signs, obj.Row);
    obj.unpermuted_print();

    // std::vector<uint8_t> row_signs = utils::all_row_masks(obj, obj.Row, c_eq_c);
    // for(auto v : row_signs) {
    //     utils::print_sign_mask(v);
    //     std::cout << std::endl;
    // }
    // // obj = utils::apply_sign_mask(obj, row_signs, col_signs, obj.Row, obj.Col);
    // // obj.unpermuted_print();

    // std::cout << "col equivalence classes: " << std::endl;
    // for(auto &[key, cols] : c_eq_c) {
    //     std::cout << "(";
    //     for(auto col : cols) {
    //         std::cout << col;
    //     }
    //     std::cout << ")";
    // }
    // std::cout << std::endl;


    // std::vector<uint8_t> sign_conventions = utils::all_row_masks(obj, obj.Row, c_eq_c);
    // obj.canonical_form();
    // for(auto sc : sign_conventions) {
    //     SO6 tmp = obj;
    //     tmp=utils::apply_sign_mask(tmp, sc, col_signs, obj.Row);
    //     tmp.unpermuted_print();
    // }
    return 0;
}
