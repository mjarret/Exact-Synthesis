// Packed 6-permutation using 18 bits (6 * 3 bits). Fast bit-twiddling operator[].
#pragma once

#include <array>
#include <cstdint>

namespace ds {

class Perm6Packed {
public:
    Perm6Packed() : bits_(0x0001'0020) { /* default to identity 0,1,2,3,4,5 */ }
    explicit Perm6Packed(uint32_t bits) : bits_(bits & mask_all()) {}

    static Perm6Packed from_array(const uint8_t p[6]) {
        uint32_t v = 0; uint32_t sh = 0;
        for (int i=0;i<6;++i, sh+=3) v |= (static_cast<uint32_t>(p[i] & 7u) << sh);
        return Perm6Packed(v);
    }
    static Perm6Packed from_array(const std::array<uint8_t,6>& p) {
        return from_array(p.data());
    }

    uint8_t operator[](int i) const {
        return static_cast<uint8_t>((bits_ >> (3 * (i & 7))) & 7u);
    }

    void to_array(uint8_t out[6]) const {
        uint32_t sh = 0; for (int i=0;i<6;++i, sh+=3) out[i] = static_cast<uint8_t>((bits_ >> sh) & 7u);
    }

    void set(int i, uint8_t v) {
        const uint32_t sh = 3u * static_cast<uint32_t>(i & 7);
        bits_ = (bits_ & ~(7u << sh)) | (static_cast<uint32_t>(v & 7u) << sh);
    }

    uint32_t bits() const { return bits_; }

private:
    static constexpr uint32_t mask_all() {
        return (1u << 18) - 1u; // lower 18 bits
    }
    uint32_t bits_;
};

} // namespace ds

