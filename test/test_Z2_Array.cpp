#include <iostream>
#include "../include/Z2_Array.hpp"

int main() {
    // Create two Z2_Array instances
    Z2_Array arr1;
    Z2_Array arr2;

    // Set components for arr1
    arr1.set_component(0, Z2(1, 2, 3));
    arr1.set_component(1, Z2(4, 5, 6));
    arr1.set_component(2, Z2(7, 8, 9));
    arr1.set_component(3, Z2(10, 11, 12));
    arr1.set_component(4, Z2(13, 14, 15));
    arr1.set_component(5, Z2(16, 17, 18));

    // Set components for arr2
    arr2.set_component(0, Z2(2, 3, 4));
    arr2.set_component(1, Z2(5, 6, 7));
    arr2.set_component(2, Z2(8, 9, 10));
    arr2.set_component(3, Z2(11, 12, 13));
    arr2.set_component(4, Z2(14, 15, 16));
    arr2.set_component(5, Z2(17, 18, 19));

    // Print initial arrays
    std::cout << "Initial arr1: " << arr1 << std::endl;
    std::cout << "Initial arr2: " << arr2 << std::endl;

    // Perform addition
    Z2_Array arr_add = arr1 + arr2;
    std::cout << "arr1 + arr2: " << arr_add << std::endl;

    // Perform subtraction
    Z2_Array arr_sub = arr1 - arr2;
    std::cout << "arr1 - arr2: " << arr_sub << std::endl;

    // Perform multiplication
    Z2_Array arr_mul = arr1 * arr2;
    std::cout << "arr1 * arr2: " << arr_mul << std::endl;

    // Perform negation
    Z2_Array arr_neg = -arr1;
    std::cout << "-arr1: " << arr_neg << std::endl;

    // Check equality
    bool is_equal = (arr1 == arr2);
    std::cout << "arr1 == arr2: " << std::boolalpha << is_equal << std::endl;

    // Check three-way comparison
    auto cmp = (arr1 <=> arr2);
    std::cout << "arr1 <=> arr2: " << (cmp == 0 ? "equal" : (cmp < 0 ? "less" : "greater")) << std::endl;

    return 0;
}
