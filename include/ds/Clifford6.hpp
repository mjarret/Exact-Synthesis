/**
 * @file Clifford6.hpp
 * @brief Packed signed row permutation for SO6: 10-bit Lehmer index + 5-bit sign mask (rows 1..5).
 */
#pragma once

#include <array>
#include <cstdint>
#include <stdexcept>
#include "ds/Lehmer6.hpp"

class Clifford6 {
public:
    static constexpr uint16_t SENTINEL = 0xFFFF;

    Clifford6() = default;
    explicit constexpr Clifford6(uint16_t bits) : bits_(bits) {}

    static Clifford6 from_index(uint16_t idx, uint8_t sign_mask = 0) {
        return Clifford6(pack(idx, sign_mask));
    }

    static Clifford6 from_perm(const std::array<uint8_t, 6>& perm, uint8_t sign_mask = 0) {
        return from_index(Lehmer6::from_perm(perm).bits(), sign_mask);
    }

    static constexpr Clifford6 from_bits(uint16_t bits) {
        return Clifford6(bits);
    }

    static constexpr Clifford6 identity() {
        return Clifford6(pack(0, 0));
    }

    constexpr uint16_t bits() const { return bits_; }
    constexpr uint16_t perm_index() const { return bits_ & kPermMask; }
    constexpr uint8_t sign_mask() const { return static_cast<uint8_t>((bits_ >> kSignShift) & kSignMask); }

    Lehmer6 perm() const { return Lehmer6::from_index(perm_index()); }
    const std::array<uint8_t, 6>& decode_ref() const { return Lehmer6::decode_ref(perm_index()); }

    Clifford6 with_sign(uint8_t sign_mask) const {
        return Clifford6(pack(perm_index(), sign_mask));
    }

    Clifford6 with_perm_index(uint16_t idx) const {
        return from_index(idx, sign_mask());
    }

    bool operator==(const Clifford6& other) const = default;

private:
    static constexpr uint16_t kPermMask   = 0x03FFu; // 10 bits
    static constexpr uint8_t  kSignMask   = 0x1Fu;   // rows 1..5 (5 bits)
    static constexpr uint8_t  kSignShift  = 10;

    static constexpr uint16_t pack(uint16_t perm_idx, uint8_t sign_mask) {
        return static_cast<uint16_t>((perm_idx & kPermMask) |
                                     (static_cast<uint16_t>(sign_mask & kSignMask) << kSignShift));
    }

    uint16_t bits_{SENTINEL};
};

namespace std {
template <>
struct hash<Clifford6> {
    size_t operator()(const Clifford6& c) const noexcept { return c.bits(); }
};
}
