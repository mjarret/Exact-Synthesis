#ifndef PERMUTATION_CODER_HPP
#define PERMUTATION_CODER_HPP

#include <cstdint>

/// **PermutationCoder**
/// Temporary direct 15-bit storage version.
class PermutationCoder {
public:
    /// **Encode using 15 bits (first 5 numbers stored directly in 3-bit fields)**
    static constexpr uint16_t encode(uint8_t a, uint8_t b, uint8_t c, uint8_t d, uint8_t e, uint8_t f) {
        return (a & 0x7) << 12 | (b & 0x7) << 9 | (c & 0x7) << 6 | (d & 0x7) << 3 | (e & 0x7);
    }

    /// **Decode a specific element of the permutation**
    static constexpr inline __attribute__((always_inline)) uint8_t decode(uint16_t rank, uint8_t i) {
        switch (i) {
            case 0: return (rank >> 12) & 0x7;
            case 1: return (rank >> 9)  & 0x7;
            case 2: return (rank >> 6)  & 0x7;
            case 3: return (rank >> 3)  & 0x7;
            case 4: return rank & 0x7;
            default: return 15 - (rank >> 12) & 0x7 - (rank >> 9) & 0x7 - (rank >> 6) & 0x7 - (rank >> 3) & 0x7 - rank & 0x7;
        }
    }

    /// **Decode the full 6-element permutation**
    static void inline __attribute__((always_inline)) decode(uint16_t rank, uint8_t &a, uint8_t &b, uint8_t &c, uint8_t &d, uint8_t &e, uint8_t &f) {
        a = decode(rank, 0);
        b = decode(rank, 1);
        c = decode(rank, 2);
        d = decode(rank, 3);
        e = decode(rank, 4);

        // **Compute the last number `f` as the missing one from {0,1,2,3,4,5}**
        uint8_t used = (1 << a) | (1 << b) | (1 << c) | (1 << d) | (1 << e);
        for (uint8_t i = 0; i < 8; ++i) {
            if (!(used & (1 << i))) {
                f = i;
                return;
            }
        }
    }

    
};

#endif // PERMUTATION_CODER_HPP
