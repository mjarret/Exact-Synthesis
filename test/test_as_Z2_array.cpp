#include <iostream>
#include "../include/Z2_Array.hpp"

int main() {
    // Create a Z2_Array instance
    Z2_Array arr;

    // Set components for arr
    arr.set_component(0, Z2(1, 2, 3));
    arr.set_component(1, Z2(4, 5, 6));
    arr.set_component(2, Z2(7, 8, 9));
    arr.set_component(3, Z2(10, 11, 12));
    arr.set_component(4, Z2(13, 14, 15));
    arr.set_component(5, Z2(16, 17, 18));

    // Print initial array
    std::cout << "Initial arr: " << arr << std::endl;

    // Access the array data as a reinterpreted Z2 array
    Z2* z2_array = arr.as_Z2_array();

    // Print each element using the reinterpreted Z2 array
    std::cout << "Elements accessed via as_Z2_array:" << std::endl;
    for (int i = 0; i < 6; ++i) {
        std::cout << "Element " << i << ": " << z2_array[i] << std::endl;
    }

    return 0;
}
