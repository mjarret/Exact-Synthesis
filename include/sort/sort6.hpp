#ifndef SORT6_HPP
#define SORT6_HPP

#include <array>
#include <algorithm>
#include <stdexcept>
#include <iterator>
#include <functional>
#include "util/utils.hpp"
#include "so6/SO6.hpp"
#include "sort/sorting_networks.h"

struct sort6 {
public:
    
	template <typename Container, typename Comparator>
	static constexpr void sorting_network_dispatch(Container& data, Comparator& comp) {
		const size_t size = data.size();

		switch (size) {
			case 1: {
				// Nothing to sort for size 1
				break;
			}
			case 2: {
				if (comp(data[1], data[0])) std::swap(data[0], data[1]);
				break;
			}
			case 3: {
				std::array<typename Container::value_type, 3> array_data = {data[0], data[1], data[2]};
				sorting_networks::SortingNetwork<3>().operator()(array_data);
				// Sort manually using the comparator
				std::sort(array_data.begin(), array_data.end(), comp);
				std::copy(array_data.begin(), array_data.end(), data.begin());
				break;
			}
			case 4: {
				std::array<typename Container::value_type, 4> array_data = {data[0], data[1], data[2], data[3]};
				sorting_networks::SortingNetwork<4>().operator()(array_data);
				std::sort(array_data.begin(), array_data.end(), comp);
				std::copy(array_data.begin(), array_data.end(), data.begin());
				break;
			}
			case 5: {
				std::array<typename Container::value_type, 5> array_data = {data[0], data[1], data[2], data[3], data[4]};
				sorting_networks::SortingNetwork<5>().operator()(array_data);
				std::sort(array_data.begin(), array_data.end(), comp);
				std::copy(array_data.begin(), array_data.end(), data.begin());
				break;
			}
			case 6: {
				std::array<typename Container::value_type, 6> array_data = {data[0], data[1], data[2], data[3], data[4], data[5]};
				sorting_networks::SortingNetwork<6>().operator()(array_data);
				std::sort(array_data.begin(), array_data.end(), comp);
				std::copy(array_data.begin(), array_data.end(), data.begin());
				break;
			}
			default: {
				throw std::invalid_argument("Unsupported size for sorting network.");
			}
		}
	}
private:
    // Add any private members or methods here if needed
};

#endif // SORT6_HPP
