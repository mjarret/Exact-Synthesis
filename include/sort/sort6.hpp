#ifndef SORT6_HPP
#define SORT6_HPP

#include <algorithm>
#include <functional>

struct sort6 {
public:
    
	template <typename Container, typename Comparator>
	static constexpr void sorting_network_dispatch(Container& data, Comparator comp) {
		const size_t size = data.size();

		// TODO: add SIMD-specialized networks for size 4–6 when vector intrinsics are available.
		auto compare_swap = [&](size_t i, size_t j) {
			if (comp(data[j], data[i])) std::swap(data[i], data[j]);
		};

		switch (size) {
			case 0:
			case 1:
				break;
			case 2:
				compare_swap(0, 1);
				break;
			case 3:
				compare_swap(0, 1);
				compare_swap(1, 2);
				compare_swap(0, 1);
				break;
			case 4:
				compare_swap(0, 1);
				compare_swap(2, 3);
				compare_swap(0, 2);
				compare_swap(1, 3);
				compare_swap(1, 2);
				break;
			case 5:
				compare_swap(0, 1);
				compare_swap(3, 4);
				compare_swap(2, 4);
				compare_swap(2, 3);
				compare_swap(1, 4);
				compare_swap(0, 3);
				compare_swap(0, 2);
				compare_swap(1, 3);
				compare_swap(1, 2);
				break;
			case 6:
				compare_swap(0, 1);
				compare_swap(2, 3);
				compare_swap(4, 5);
				compare_swap(0, 2);
				compare_swap(1, 3);
				compare_swap(4, 5);
				compare_swap(0, 4);
				compare_swap(1, 5);
				compare_swap(2, 4);
				compare_swap(3, 5);
				compare_swap(1, 2);
				compare_swap(3, 4);
				compare_swap(1, 4);
				compare_swap(2, 3);
				compare_swap(2, 4);
				break;
			default:
				// Fallback to generic insertion sort for unexpected sizes
				for (size_t i = 1; i < size; ++i) {
					size_t j = i;
					while (j > 0 && comp(data[j], data[j - 1])) {
						std::swap(data[j], data[j - 1]);
						--j;
					}
				}
				break;
		}
	}

	template <typename Container>
	static constexpr void sorting_network_dispatch(Container& data) {
		auto comp = [](const auto& lhs, const auto& rhs) { return lhs < rhs; };
		sorting_network_dispatch(data, comp);
	}
private:
    // Add any private members or methods here if needed
};

#endif // SORT6_HPP
