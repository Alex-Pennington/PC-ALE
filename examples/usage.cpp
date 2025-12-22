/**
 * \example usage.cpp
 * \brief Example usage of PC-ALE 2.0 8-FSK modem core
 * 
 * Demonstrates:
 *  1. Tone generation
 *  2. Symbol detection
 *  3. Golay encoding/decoding
 *  4. End-to-end modulation/demodulation
 */

#include "ale_types.h"
#include "tone_generator.h"
#include "fft_demodulator.h"
#include "symbol_decoder.h"
#include "golay.h"

#include <iostream>
#include <vector>

using namespace ale;

// ============================================================================
// Example 1: Generate and detect a single 8-FSK symbol
// ============================================================================

void example_1_simple_symbol() {
    std::cout << "\n=== Example 1: Generate & Detect Single Symbol ===\n\n";
    
    // Create generator
    ToneGenerator gen;
    std::vector<int16_t> audio(64);
    
    // Generate symbol 3 (1125 Hz) for 64 samples
    gen.generate_tone(3, 64, audio.data());
    
    std::cout << "Generated 64 samples of symbol 3 (1125 Hz tone)\n";
    
    // Demodulate with FFT
    FFTDemodulator demod;
    auto symbols = demod.process_audio(audio.data(), 64);
    
    if (!symbols.empty()) {
        uint8_t detected = (symbols[0].bits[2] << 2) |
                           (symbols[0].bits[1] << 1) |
                            symbols[0].bits[0];
        
        std::cout << "Detected symbol: " << (int)detected << "\n";
        std::cout << "SNR: " << symbols[0].signal_to_noise << " dB\n";
    }
}

// ============================================================================
// Example 2: Modulate a sequence of symbols
// ============================================================================

void example_2_symbol_sequence() {
    std::cout << "\n=== Example 2: Modulate Symbol Sequence ===\n\n";
    
    // Define a sequence: 0, 1, 2, 3, 4, 5, 6, 7
    uint8_t symbols[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    
    // Generate audio
    ToneGenerator gen;
    std::vector<int16_t> audio(8 * 64);  // 8 symbols × 64 samples each
    
    uint32_t samples = gen.generate_symbols(symbols, 8, audio.data());
    
    std::cout << "Generated " << samples << " audio samples ";
    std::cout << "(8 symbols × 64 samples/symbol)\n";
    
    // Demodulate
    FFTDemodulator demod;
    auto detected = demod.process_audio(audio.data(), samples);
    
    std::cout << "Detected " << detected.size() << " symbols\n";
    
    // Verify
    for (size_t i = 0; i < detected.size(); ++i) {
        uint8_t sym = (detected[i].bits[2] << 2) |
                      (detected[i].bits[1] << 1) |
                       detected[i].bits[0];
        std::cout << "  Symbol " << i << ": expected " << (int)symbols[i]
                  << ", detected " << (int)sym;
        if (sym == symbols[i]) {
            std::cout << " ✓\n";
        } else {
            std::cout << " ✗\n";
        }
    }
}

// ============================================================================
// Example 3: Golay FEC Encoding & Decoding
// ============================================================================

void example_3_golay_fec() {
    std::cout << "\n=== Example 3: Golay Error Correction ===\n\n";
    
    // Information word (12 bits)
    uint16_t info = 0xABC;  // 10101011110 in binary
    
    // Encode to 24-bit codeword
    uint32_t codeword = Golay::encode(info);
    std::cout << "Info word: 0x" << std::hex << info << std::dec << "\n";
    std::cout << "Codeword: 0x" << std::hex << codeword << std::dec << "\n";
    
    // Simulate single-bit error
    uint32_t corrupted = codeword ^ (1U << 5);
    std::cout << "\nCorrupted (1 bit flipped): 0x" << std::hex << corrupted << std::dec << "\n";
    
    // Decode and correct
    uint16_t decoded = 0;
    uint8_t errors = Golay::decode(corrupted, decoded);
    
    std::cout << "Decoded: 0x" << std::hex << decoded << std::dec << "\n";
    std::cout << "Errors corrected: " << (int)errors << "\n";
    
    if (decoded == info) {
        std::cout << "✓ Successfully recovered original data\n";
    } else {
        std::cout << "✗ Failed to recover data\n";
    }
}

// ============================================================================
// Example 4: Majority Voting
// ============================================================================

void example_4_majority_voting() {
    std::cout << "\n=== Example 4: Majority Voting ===\n\n";
    
    // Simulate 3 copies of a data bit (with possible error)
    uint8_t bit_copies[3] = {1, 1, 0};  // 2 out of 3 are 1, one is corrupted 0
    
    uint8_t corrected = SymbolDecoder::majority_vote(bit_copies);
    
    std::cout << "Three copies of bit: " << (int)bit_copies[0]
              << " " << (int)bit_copies[1]
              << " " << (int)bit_copies[2] << "\n";
    std::cout << "Majority vote result: " << (int)corrected << "\n";
    std::cout << "(Majority is 1, so despite one error, we recover correct bit)\n";
}

// ============================================================================
// Example 5: Complete Modulation Pipeline
// ============================================================================

void example_5_complete_pipeline() {
    std::cout << "\n=== Example 5: Complete Modulation Pipeline ===\n\n";
    
    // 1. Create test message (sequence of 5 symbols)
    uint8_t message[5] = {2, 5, 0, 7, 3};
    
    std::cout << "Step 1: Message\n";
    std::cout << "  Symbols: ";
    for (auto sym : message) {
        std::cout << (int)sym << " ";
    }
    std::cout << "\n";
    
    // 2. Encode with Golay
    std::cout << "\nStep 2: Golay FEC Encoding\n";
    
    // For demonstration, take first 2 symbols as data (6 bits)
    // and encode to Golay codeword
    uint16_t data = (message[0] << 3) | (message[1] >> 1);  // 12 bits
    uint32_t codeword = Golay::encode(data);
    
    std::cout << "  Data: 0x" << std::hex << data << std::dec << "\n";
    std::cout << "  Codeword: 0x" << std::hex << codeword << std::dec << "\n";
    
    // 3. Generate audio
    std::cout << "\nStep 3: FSK Modulation\n";
    
    ToneGenerator gen;
    std::vector<int16_t> audio(5 * 64);
    uint32_t samples = gen.generate_symbols(message, 5, audio.data());
    
    std::cout << "  Generated " << samples << " audio samples\n";
    
    // 4. Demodulate
    std::cout << "\nStep 4: FSK Demodulation\n";
    
    FFTDemodulator demod;
    auto detected = demod.process_audio(audio.data(), samples);
    
    std::cout << "  Detected " << detected.size() << " symbols\n";
    
    // 5. Verify
    std::cout << "\nStep 5: Verification\n";
    
    bool all_correct = true;
    for (size_t i = 0; i < detected.size() && i < 5; ++i) {
        uint8_t sym = (detected[i].bits[2] << 2) |
                      (detected[i].bits[1] << 1) |
                       detected[i].bits[0];
        bool match = (sym == message[i]);
        std::cout << "  Symbol " << i << ": " << (match ? "✓" : "✗") << "\n";
        if (!match) all_correct = false;
    }
    
    if (all_correct) {
        std::cout << "\n✓ All symbols recovered successfully!\n";
    }
}

// ============================================================================
// Main
// ============================================================================

int main() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║    PC-ALE 2.0 8-FSK Modem Core - Usage Examples           ║\n";
    std::cout << "║    MIL-STD-188-141B Automatic Link Establishment          ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    example_1_simple_symbol();
    example_2_symbol_sequence();
    example_3_golay_fec();
    example_4_majority_voting();
    example_5_complete_pipeline();
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║    Examples complete!                                      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    return 0;
}
