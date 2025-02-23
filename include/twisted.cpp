#include <iostream>
#include <cstdint>
#include <cassert>

// A TwistedComplex number is stored as a 16-bit unsigned integer:
//   data = (imaginary << 8) | real,
// where real and imaginary are 8-bit values (0..255).
class TwistedComplex {
public:
    uint16_t data;  // Internal representation

    // Constructor from two 8-bit values (real and imag).
    TwistedComplex(uint8_t real, uint8_t imag)
        : data((static_cast<uint16_t>(imag) << 8) | real) { }

    // Constructor from raw 16-bit data.
    explicit TwistedComplex(uint16_t raw) : data(raw) { }

    // Accessors for the real (low) and imaginary (high) parts.
    uint8_t real() const { return data & 0xFF; }
    uint8_t imag() const { return data >> 8; }

    // Twisted addition.
    // In our safe domain we assume (this->real() + other.real()) < 256,
    // so we can simply add the raw data.
    TwistedComplex operator+(const TwistedComplex& other) const {
        return TwistedComplex(data + other.data);
    }

    // Twisted subtraction.
    // Standard 16-bit subtraction causes a borrow from the low byte if
    // (this->real() < other.real()). We correct for this by adding 256.
    TwistedComplex operator-(const TwistedComplex& other) const {
        return TwistedComplex(data - other.data + 256*((data & 0xFF) < (other.data & 0xFF)));
    }

    // Overload << for printing.
    friend std::ostream& operator<<(std::ostream& os, const TwistedComplex& tc) {
        os << int(tc.real()) << " + " << int(tc.imag()) << "√2";
        return os;
    }
};

// Helper function: compute expected twisted subtraction componentwise.
uint16_t expectedTwistedSubtract(uint16_t x, uint16_t y) {
    int realX = x & 0xFF;
    int imagX = x >> 8;
    int realY = y & 0xFF;
    int imagY = y >> 8;
    int realDiff = realX - realY;
    if (realDiff < 0)
        realDiff += 256;
    int imagDiff = imagX - imagY;
    if (imagDiff < 0)
        imagDiff += 256;
    return (static_cast<uint16_t>(imagDiff) << 8) | static_cast<uint16_t>(realDiff);
}

void testTwistedComplex() {
    int safeCount = 0;
    // We'll test over safe values:
    // Let the real parts be in [0, 128) so that (real1 + real2) < 256 (no overflow in addition).
    // Imag parts vary over [0,256). For subtraction, we handle the borrow.
    for (int imag1 = 0; imag1 < 256; imag1 += 17) {
        for (int real1 = 0; real1 < 128; real1 += 17) {
            // Use brace initialization to avoid vexing parse.
            TwistedComplex x{uint8_t(real1), uint8_t(imag1)};
            for (int imag2 = 0; imag2 < 256; imag2 += 17) {
                for (int real2 = 0; real2 < 128; real2 += 17) {
                    TwistedComplex y{uint8_t(real2), uint8_t(imag2)};
                    
                    // Test twisted addition.
                    // Under our safe assumption, addition is just raw addition.
                    TwistedComplex sum = x + y;
                    uint8_t expectedReal = ((x.real() + y.real()) & 0xFF);
                    uint8_t expectedImag = ((x.imag() + y.imag()) & 0xFF);
                    TwistedComplex expectedAdd{expectedReal, expectedImag};
                    if (sum.data != expectedAdd.data) {
                        std::cerr << "Addition error: x = " << x << ", y = " << y 
                                  << ", got " << sum << ", expected " << expectedAdd << "\n";
                        assert(false);
                    }
                    
                    // Test twisted subtraction.
                    TwistedComplex diff = x - y;
                    uint16_t expectedDiff = expectedTwistedSubtract(x.data, y.data);
                    if (diff.data != expectedDiff) {
                        std::cerr << "Subtraction error: x = " << x << ", y = " << y 
                                  << ", got " << diff.data << ", expected " << expectedDiff << "\n";
                        assert(false);
                    }
                    
                    safeCount++;
                }
            }
        }
    }
    std::cout << "Tested " << safeCount << " safe twisted arithmetic pairs. All tests passed.\n";
}

int main() {
    testTwistedComplex();
    return 0;
}
