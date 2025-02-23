#ifndef UINT72_T_HPP
#define UINT72_T_HPP

#include <bitset>
#include <cstdint>
#include <iostream>
#include <type_traits>
#include <cassert>
#include <climits>
#include <bit>
#include <array>

using low_t = uint64_t;
using high_t = uint8_t;

constexpr uint8_t low_size = 64;
constexpr uint8_t high_size = 8;

constexpr uint8_t low_mask = -1;
constexpr uint8_t high_mask = -1;
constexpr uint8_t total_size = low_size + high_size;

struct __attribute__((packed)) uint72_t {
public:
    low_t low = 0;   // First 64 bits
    high_t high = 0;   // Last 8 bits (bits 64–71)

    // Constructor
    constexpr uint72_t(low_t low_ = 0, high_t high_ = 0) : low(low_), high(high_) {}

    // Constructor to initialize from an arbitrary integer (e.g., 72-bit value)
    template <typename T>
    constexpr uint72_t(T value)
        : uint72_t(static_cast<low_t>(value), static_cast<high_t>(sizeof(T) > high_size ? value >> low_size : 0)) {
        static_assert(std::is_integral<T>::value, "Value must be an integral type.");
    }

    template <typename T>
    constexpr uint72_t& set_bits(int start, int end, T value) {
        static_assert(std::is_integral<T>::value, "T must be an integral type");

        int range = end - start;
        uint72_t to_set;
        uint72_t mask;
        switch (range) {
            case 1:
                mask = uint72_t(0b1,0) << start;
                to_set = uint72_t(value & 0b1,0) << start;
                break;
            case 2:
                mask = uint72_t(0b11,0) << start;
                to_set = uint72_t(value & 0b11,0) << start;
                break;
            case 12:
                mask = uint72_t(0xFFF,0) << start;
                to_set = uint72_t(value & 0xFFF,0) << start;
                break;
            case 64:
                mask = uint72_t(0xFFFFFFFFFFFFFFFFULL,0) << start;
                to_set = uint72_t(value & 0xFFFFFFFFFFFFFFFFULL);
                break;
            case 72:
                mask = ~uint72_t(0,0); // All 72 bits set
                to_set = uint72_t(value);
                break;
            default:
                mask = ((uint72_t(1,0) << range) - 1) << start;
                to_set = uint72_t(value & static_cast<T>(mask));
                break;
        }
        *this &= ~mask;
        *this |= to_set;
        return *this;
    }

    constexpr uint72_t(const std::string& binary_string) {
        low = 0;
        high = 0;
        for (size_t i = 0; i < 36; ++i) {
            high_t bits = (binary_string[2*i] == '1') << 1 | (binary_string[2*i+1] == '1');
            set_bits(2*i, 2*i+2, bits);
        }
    }

    // Get a single bit
    constexpr bool operator[](size_t bit_pos) const {
        return (bit_pos < low_size) ? (low >> bit_pos) & 1ULL : (high >> (bit_pos - low_size)) & 1U;
    }

    // Set a single bit
    constexpr uint72_t& operator()(size_t bit_pos, bool value) {
        if (bit_pos < low_size) {
            low = (low & ~(1ULL << bit_pos)) | (static_cast<low_t>(value) << bit_pos);
        } else {
            size_t high_bit_pos = bit_pos - low_size;
            high = (high & ~(1U << high_bit_pos)) | (static_cast<high_t>(value) << high_bit_pos);
        } 
        return *this;
    }

    constexpr uint72_t& operator=(const uint72_t& other) {
        low = other.low;
        high = other.high;
        return *this;
    }

    // Equality operators
    constexpr bool operator==(const uint72_t& other) const {
        return low == other.low && high == other.high;
    }

    constexpr bool operator!=(const uint72_t& other) const {
        return !(*this == other);
    }

    constexpr bool operator<(const uint72_t& other) const {
        return high < other.high || (high == other.high && low < other.low);
    }

    // Bitwise AND operator with general integral types
    template <typename T>
    constexpr uint72_t operator&(const T& other) const {
        static_assert(std::is_integral<T>::value, "Bitwise AND is only defined for integral types.");
        
        // Compute new low and high parts based on the size of T
        low_t new_low = low & static_cast<low_t>(other); // Mask with low part
        high_t new_high = high & (sizeof(T) > high_size ? static_cast<high_t>(other >> low_size) : 0);
        
        return uint72_t(new_low, new_high);
    }

    constexpr uint72_t operator&=(const uint72_t& other) {
        low &= other.low;
        high &= other.high;
        return *this;
    }

    constexpr uint72_t operator|(const uint72_t& other) const {
        return uint72_t(low | other.low, high | other.high);
    }

    constexpr uint72_t operator|=(const uint72_t& other) {
        low |= other.low;
        high |= other.high;
        return *this;
    }

    constexpr int fast_max(int x, int y) const {
        return x - ((x - y) & ((x - y) >> (sizeof(int) * CHAR_BIT - 1)));     
    }

    constexpr int fast_min(int x, int y) const {
        return y + ((x - y) & ((x - y) >> (sizeof(int) * CHAR_BIT - 1)));     
    }

    constexpr std::strong_ordering operator<=>(const uint72_t& other) const {
        std::strong_ordering comp = high <=> other.high; 
        if (comp != std::strong_ordering::equal) return comp;
        return low <=> other.low;
    }

    constexpr low_t fast_mask(int zeros) const {
        return static_cast<low_t>(-(zeros < low_size)) << zeros;
    }

    constexpr uint72_t operator<<(const int s) const {
        const int carry_bits_start = low_size - s;

        const low_t new_low = (low << s) & (-(s < low_size));
        const high_t new_high = (carry_bits_start <= 0) ? static_cast<high_t>(low << -carry_bits_start)
            : (high << s) | ((low >> carry_bits_start) & ((static_cast<high_t>(1) << s) - 1));

        return uint72_t(new_low, new_high);
    }

    constexpr uint72_t operator>>(const int s) const {       
        const int remaining_low = low_size - s;      // Number of bits that remain in low register

        const high_t new_high = (high >> s) & -(high_size > s);
        const low_t new_low = (remaining_low <= 0) ? high >> (-remaining_low) 
            : (low >> s) | (static_cast<low_t>(high & ((static_cast<high_t>(1) << s) - 1)) << remaining_low);
        
        return uint72_t(new_low, new_high);
    }

    constexpr uint72_t operator~() const {
        return uint72_t(~low, ~high);
    }

    constexpr uint72_t operator+(const uint72_t& other) const {
        low_t sum_low = low + other.low;
        high_t carry = sum_low < low;
        return uint72_t(sum_low, high + other.high + carry);
    }

    constexpr uint72_t operator-(const uint72_t& other) const {
        low_t diff_low = low - other.low;
        high_t borrow = diff_low > low;
        return uint72_t(diff_low, high - other.high - borrow);
    }

    constexpr uint72_t operator^(const uint72_t& other) const {
        return uint72_t(low ^ other.low, high ^ other.high);
    }

    constexpr uint72_t operator^=(const uint72_t& other) {
        low ^= other.low;
        high ^= other.high;
        return *this;
    }

    template <typename T>
    constexpr uint72_t& operator^=(const T& other) {
        static_assert(std::is_integral<T>::value, "T must be an integral type");

        // Apply XOR to low
        low ^= static_cast<low_t>(other);

        // If other is wider than 64 bits, extract the upper part
        if constexpr (sizeof(T) > low_size) {
            high ^= static_cast<high_t>(other >> low_size);
        }

        return *this;
    }

    friend std::ostream& operator<<(std::ostream& os, const uint72_t& value) {
            os << std::bitset<high_size>(value.high) << std::bitset<low_size>(value.low);
        return os;
    }

    constexpr int popcount() const {
        return __builtin_popcountll(low) + __builtin_popcount(high);
    }

    constexpr uint72_t& clear_bits(int start, int end) {
        if (start < low_size) {
            low &= ~((1ULL << start) - 1);
            low &= (1ULL << end) - 1;
        } else {
            high &= ~((1U << (start - low_size)) - 1);
            high &= (1U << (end - low_size)) - 1;
        }
        return *this;
    }

    template <typename T, typename = std::enable_if_t<std::is_integral_v<T>>>
    explicit operator T() const {
        if(sizeof(T) <= low_size) {
            return static_cast<T>(low);
        } else {
            return static_cast<T>(low) | (static_cast<T>(high) << low_size);
        }
    }
};

namespace std {
    template <>
    struct is_integral<uint72_t> : std::true_type {};
}

#endif // UINT72_T_HPP
