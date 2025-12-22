# Quick Start Guide - PC-ALE 2.0 8-FSK Modem

## Building the Project

### Prerequisites
- CMake 3.15 or later
- C++17 compatible compiler (GCC, Clang, or MSVC)
- For Windows: MinGW-w64 or Visual Studio 2019+

### Windows (GCC/MinGW)

```bash
cd ALE-Clean-Room
mkdir build
cd build
cmake .. -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build .
```

### Windows (Visual Studio 2022)

```bash
cd ALE-Clean-Room
mkdir build
cd build
cmake .. -G "Visual Studio 17 2022"
cmake --build . --config Release
```

### Linux / macOS

```bash
cd ALE-Clean-Room
mkdir build
cd build
cmake ..
make
```

### Raspberry Pi (ARM)

```bash
cd ALE-Clean-Room
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j4
```

## Running Tests

```bash
cd build

# Run all unit tests
ctest --verbose

# Or run directly
./test_fsk_core       # All 5 core tests (Windows: test_fsk_core.exe)
./debug_fft           # FFT bin debugging tool
```

Expected output:
```
Passed: 5  Failed: 0
```

## Using the Library

### Link Against Libraries

In your CMakeLists.txt:
```cmake
add_executable(my_app my_code.cpp)

# Link to PC-ALE libraries
target_link_libraries(my_app ale_fsk_core ale_fec)
target_include_directories(my_app PRIVATE <path>/ALE-Clean-Room/include)
```

### Basic Usage Example

```cpp
#include "ale_types.h"
#include "tone_generator.h"
#include "fft_demodulator.h"

using namespace ale;

// Generate 8 FSK tones
ToneGenerator gen;
uint8_t symbols[8] = {0, 1, 2, 3, 4, 5, 6, 7};
std::vector<int16_t> audio(512);

gen.generate_symbols(symbols, 8, audio.data());

// Demodulate and detect symbols
FFTDemodulator demod;
auto detected = demod.process_audio(audio.data(), 512);

// Process detected symbols
for (const auto& sym : detected) {
    uint8_t value = (sym.bits[2] << 2) | (sym.bits[1] << 1) | sym.bits[0];
    std::cout << "Symbol: " << (int)value 
              << ", SNR: " << sym.signal_to_noise << " dB\n";
}
```

### Core Components

#### 1. Tone Generation

```cpp
ToneGenerator gen;
std::vector<int16_t> samples(64);

// Generate symbol 3 (1125 Hz)
gen.generate_tone(3, 64, samples.data());

// Generate sequence of symbols
uint8_t sequence[8] = {0, 1, 2, 3, 4, 5, 6, 7};
gen.generate_symbols(sequence, 8, samples.data());
```

#### 2. FSK Demodulation

```cpp
FFTDemodulator demod;

// Process audio frames
std::vector<Symbol> symbols = demod.process_audio(audio_data, num_samples);

// Each symbol has:
//   - bits[3]: 3-bit symbol value
//   - magnitude: FFT peak magnitude
//   - signal_to_noise: SNR in dB
//   - sample_index: Sample number detected
```

#### 3. Golay Error Correction

```cpp
// Encode 12-bit data to 24-bit codeword
uint16_t data = 0x123;
uint32_t codeword = Golay::encode(data);

// Decode (with error correction)
uint16_t recovered = 0;
uint8_t errors = Golay::decode(codeword, recovered);

if (errors <= 3) {
    // Correctable error(s) or perfect reception
}
```

#### 4. Majority Voting

```cpp
uint8_t bit_copies[3] = {1, 1, 0};
uint8_t voted_bit = SymbolDecoder::majority_vote(bit_copies);
// Result: 1 (majority wins)
```

## File Organization

```
ALE-Clean-Room/
├── include/              # Public API headers
│   ├── ale_types.h
│   ├── tone_generator.h
│   ├── fft_demodulator.h
│   ├── symbol_decoder.h
│   └── golay.h
├── src/                  # Implementation
│   ├── core/types.cpp
│   ├── fsk/*.cpp
│   └── fec/*.cpp
├── tests/                # Unit tests
│   ├── test_fsk_core.cpp (ALL PASSING ✓)
│   ├── debug_fft.cpp
│   └── usage.cpp
├── build/                # Build artifacts (after cmake --build)
│   ├── libale_fsk_core.a
│   ├── libale_fec.a
│   ├── test_fsk_core.exe
│   └── debug_fft.exe
└── examples/
    └── usage.cpp         # Usage examples
```

## API Reference

### ToneGenerator

```cpp
class ToneGenerator {
    // Generate symbols at 125 baud
    uint32_t generate_symbols(const uint8_t* symbols, uint32_t num_symbols,
                              int16_t* output, float amplitude = 0.7f);
    
    // Generate single continuous tone
    uint32_t generate_tone(uint8_t symbol_value, uint32_t num_samples,
                           int16_t* output, float amplitude = 0.7f);
    
    void reset();
};
```

### FFTDemodulator

```cpp
class FFTDemodulator {
    // Process audio and return detected symbols
    std::vector<Symbol> process_audio(const int16_t* samples, uint32_t num_samples);
    
    // Get current FFT magnitudes
    const std::array<float, FFT_SIZE>& get_fft_magnitudes() const;
    
    void reset();
};
```

### SymbolDecoder

```cpp
class SymbolDecoder {
    // Detect symbol from FFT magnitudes
    static uint8_t detect_symbol(const std::array<float, FFT_SIZE>& magnitudes);
    
    // Majority voting (3x redundancy)
    static uint8_t majority_vote(const uint8_t bits[3]);
};
```

### Golay

```cpp
class Golay {
    // Encode 12 bits to 24 bits
    static uint32_t encode(uint16_t info);
    
    // Decode with error correction (up to 3 bits)
    static uint8_t decode(uint32_t codeword, uint16_t& output);
};
```

## Troubleshooting

### Build Issues

**CMake not found:**
```bash
# Install CMake
# Windows: Use chocolatey or download from cmake.org
# Linux: sudo apt-get install cmake
# macOS: brew install cmake
```

**Compiler not found:**
```bash
# Windows with MinGW
# Download from winlibs.com or use package manager

# Linux
sudo apt-get install build-essential

# macOS
xcode-select --install
```

### Runtime Issues

**All symbols detected as 0:**
- Check that audio sample rate is 8000 Hz
- Verify tone generator is producing correct frequencies
- Use debug_fft tool to inspect FFT magnitudes

**No symbols detected (empty result):**
- Ensure audio buffer has at least 64 samples
- Check signal amplitude is not too low
- Verify no integer overflow in sample values

## Performance

| Operation | Time | Notes |
|-----------|------|-------|
| Tone Generation | ~1 µs per sample | Real-time at 8 kHz |
| FFT (64-point) | ~1 µs per bin | 64 µs per symbol |
| Symbol Detection | ~5 µs | Peak finding + mapping |
| Golay Decode | <1 µs | O(1) syndrome lookup |
| Complete Symbol | ~70 µs | Full modulation pipeline |

## Next Steps

1. **Run examples:** Build and run the unit tests to verify setup
2. **Explore examples:** Check `tests/usage.cpp` for integration patterns
3. **Try demodulation:** Feed real ALE audio to FFTDemodulator
4. **Phase 2 work:** Implement word structure and preamble detection

## References

- [BUILD_REPORT.md](BUILD_REPORT.md) - Detailed technical architecture
- [README.md](README.md) - Project overview and philosophy
- MIL-STD-188-141B - Official ALE specification

## Support

For issues or questions:
1. Check BUILD_REPORT.md technical documentation
2. Review test examples in tests/test_fsk_core.cpp
3. Run debug_fft.exe to inspect FFT behavior
4. Verify audio samples are 16-bit signed at 8000 Hz

---

**Status:** All unit tests passing ✅  
**License:** MIT  
**Clean-Room Implementation:** December 2024
