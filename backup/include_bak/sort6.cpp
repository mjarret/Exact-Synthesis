#include "sort6.hpp"

    // pattern& sort6::insertion(pattern& d)
    // {
    //     for (int i = 1; i < 6; i++) {
    //         // Extract current 12-bit object to be sorted
    //         auto tmp = d.get_column(i);
    //         int j = i;

    //         // Shift larger objects right
    //         while (j > 0 && tmp < d.get_column(j - 1)) {
    //             auto prev = d.get_column(j - 1);
    //             d.set_column(j, prev);
    //             j--;
    //         }

    //         // Place tmp in the correct position
    //         d.set_column(j, tmp);
    //     }
    //     return d;
    // }

    // pattern& sort6::network(pattern& d)
    // {
    //     // Comparison-based network sort for 6 columns
    //     // Comparison-based network sort for 6 columns

    //     // Optimal sort network
    //     if (d.get_column(1) < d.get_column(2)) d.swap_columns(1, 2);
    //     if (d.get_column(0) < d.get_column(2)) d.swap_columns(0, 2);
    //     if (d.get_column(0) < d.get_column(1)) d.swap_columns(0, 1);
    //     if (d.get_column(4) < d.get_column(5)) d.swap_columns(4, 5);
    //     if (d.get_column(3) < d.get_column(5)) d.swap_columns(3, 5);
    //     if (d.get_column(3) < d.get_column(4)) d.swap_columns(3, 4);
    //     if (d.get_column(0) < d.get_column(3)) d.swap_columns(0, 3);
    //     if (d.get_column(1) < d.get_column(4)) d.swap_columns(1, 4);
    //     if (d.get_column(2) < d.get_column(5)) d.swap_columns(2, 5);
    //     if (d.get_column(2) < d.get_column(4)) d.swap_columns(2, 4);
    //     if (d.get_column(1) < d.get_column(3)) d.swap_columns(1, 3);
    //     if (d.get_column(2) < d.get_column(3)) d.swap_columns(2, 3);

    //     return d;
    // }

    // pattern& sort6::counting(pattern& d)
    // {
    //     std::map<uint16_t, int> count;
    //     for(int col =0; col< 6; col++) {
    //         count[d.get_column(col)]++;
    //     }
    //     int j =0;
    //     for(auto col : count) {
    //         for(int i = 0; i < col.second; i++) {
    //             d.set_column(j, col.first);
    //             j++;
    //         }
    //     }
    //     return d;
    // }