#ifndef UINT72_T_HPP
#define UINT72_T_HPP

#include <cstdint>
#include <cstddef>
#include <stdexcept>
#include <bitset>
#include <string>
#include <algorithm>

class uint72_t {
public:
    uint64_t low_bits;   // First 64 bits
    uint8_t high_bits;   // Last 8 bits (bits 64–71)

    // Constructor
    constexpr uint72_t(uint64_t low = 0, uint8_t high = 0) : low_bits(low), high_bits(high) {}

    // Constructor to initialize from an arbitrary integer (e.g., 72-bit value)
    template <typename T>
    constexpr uint72_t(T value)
        : uint72_t(static_cast<uint64_t>(value), static_cast<uint8_t>(sizeof(T) > 8 ? value >> 64 : 0)) {
        static_assert(std::is_integral<T>::value, "Value must be an integral type.");
    }

    constexpr uint72_t(const std::string& binary_string) {
        low_bits = 0;
        high_bits = 0;
        for (size_t i = 0; i < 36; ++i) {
            set_pair(2*i, (binary_string[2*i] == '1') << 1 | (binary_string[2*i+1] == '1'));
        }
    }

    // Get a single bit
    constexpr bool operator[](size_t bit_pos) const {
        if (bit_pos < 64) {
            return (low_bits >> bit_pos) & 1ULL;
        } else {
            return (high_bits >> (bit_pos - 64)) & 1U;
        }
    }

    // Set a single bit
    constexpr uint72_t& operator()(size_t bit_pos, bool value) {
        if (bit_pos < 64) {
            low_bits = (low_bits & ~(1ULL << bit_pos)) | (static_cast<uint64_t>(value) << bit_pos);
        } else {
            size_t high_bit_pos = bit_pos - 64;
            high_bits = (high_bits & ~(1U << high_bit_pos)) | (static_cast<uint8_t>(value) << high_bit_pos);
        } 
        return *this;
    }

    constexpr uint72_t& operator=(const uint72_t& other) {
        low_bits = other.low_bits;
        high_bits = other.high_bits;
        return *this;
    }

    // Get a pair of bits
    constexpr uint8_t get_pair(size_t bit_pos) const {
        if (bit_pos < 63) {
            return static_cast<uint8_t>((low_bits >> bit_pos) & 0b11);
        } else if (bit_pos == 63) {
            // The first bit is in low_bits[63], and the second bit is in high_bits[0]
            return static_cast<uint8_t>((low_bits >> 63) & 1ULL) | static_cast<uint8_t>((high_bits & 1U) << 1);
        } else {
            // Both bits are in high_bits
            size_t high_bit_pos = bit_pos - 64;
            return static_cast<uint8_t>((high_bits >> high_bit_pos) & 0b11);
        }
    }

    constexpr uint72_t& set_pair(size_t bit_pos, uint8_t value) {
        value &= 0b11;  // Ensure only two bits are used
        if (bit_pos < 63) {
            // Both bits are in low_bits
            low_bits = (low_bits & ~(0b11ULL << bit_pos)) | (static_cast<uint64_t>(value) << bit_pos);
        } else if (bit_pos == 63) {
            // The first bit is in low_bits[63], and the second bit is in high_bits[0]
            low_bits = (low_bits & ~(1ULL << 63)) | (static_cast<uint64_t>(value & 1) << 63);
            high_bits = (high_bits & ~1U) | static_cast<uint8_t>((value >> 1) & 1U);
        } else {
            // Both bits are in high_bits
            size_t high_bit_pos = bit_pos - 64;
            high_bits = (high_bits & ~(0b11U << high_bit_pos)) | (static_cast<uint8_t>(value) << high_bit_pos);
        }
        return *this;
    }

    // Equality operators
    constexpr bool operator==(const uint72_t& other) const {
        return low_bits == other.low_bits && high_bits == other.high_bits;
    }

    constexpr bool operator!=(const uint72_t& other) const {
        return !(*this == other);
    }

    constexpr uint72_t operator&(const uint72_t& other) const {
        return uint72_t(low_bits & other.low_bits, high_bits & other.high_bits);
    }

    constexpr uint72_t operator|(const uint72_t& other) const {
        return uint72_t(low_bits | other.low_bits, high_bits | other.high_bits);
    }

    constexpr uint72_t operator<<(size_t shift) const {
        if (shift >= 72) {
            return uint72_t(0, 0); // All bits shifted out of range
        } else if (shift >= 64) {
            // Shift bits from low_bits into high_bits
            return uint72_t(0, static_cast<uint8_t>(low_bits >> (shift - 64)));
        } else {
            // Shift within low_bits and propagate into high_bits
            uint64_t new_low_bits = low_bits << shift;
            uint8_t new_high_bits = static_cast<uint8_t>((low_bits >> (64 - shift)) & 0xFF) | (high_bits << shift);
            return uint72_t(new_low_bits, new_high_bits);
        }
    }

    constexpr uint72_t operator>>(size_t shift) const {
        if (shift >= 72) {
            return uint72_t(0, 0); // All bits shifted out of range
        } else if (shift >= 64) {
            // Shift bits from high_bits into low_bits
            return uint72_t(high_bits << (shift - 64), 0);
        } else {
            // Shift within high_bits and propagate into low_bits
            uint64_t new_low_bits = (low_bits >> shift) | (static_cast<uint64_t>(high_bits & ((1 << shift) - 1)) << (64 - shift));
            uint8_t new_high_bits = high_bits >> shift;
            return uint72_t(new_low_bits, new_high_bits);
        }
    }

    constexpr uint72_t operator~() const {
        return uint72_t(~low_bits, ~high_bits);
    }

    constexpr uint72_t operator+(const uint72_t& other) const {
        uint64_t sum_low = low_bits + other.low_bits;
        uint8_t carry = sum_low < low_bits;
        return uint72_t(sum_low, high_bits + other.high_bits + carry);
    }

    constexpr uint72_t operator-(const uint72_t& other) const {
        uint64_t diff_low = low_bits - other.low_bits;
        uint8_t borrow = diff_low > low_bits;
        return uint72_t(diff_low, high_bits - other.high_bits - borrow);
    }

    constexpr uint16_t get_bits(size_t bit_pos) const {
        constexpr size_t total_bits = 72;
        constexpr uint72_t mask = uint72_t((1ULL << 16) - 1, 0);

        // Apply mask and shift into position
        uint72_t masked = (*this >> bit_pos) & mask;
        return static_cast<uint16_t>(masked.low_bits);
    }

    friend std::ostream& operator<<(std::ostream& os, const uint72_t& value) {
            os << std::bitset<8>(value.high_bits) << std::bitset<64>(value.low_bits);
        return os;
    }

    constexpr int popcount() const {
        return __builtin_popcountll(low_bits) + __builtin_popcount(high_bits);
    }
};

#endif // UINT72_T_HPP
