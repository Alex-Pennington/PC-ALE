/**
 * \file symbol_decoder.cpp
 * \brief Implementation of FSK symbol detection and decoding
 */

#include "symbol_decoder.h"
#include <algorithm>
#include <cmath>

namespace ale {

uint8_t SymbolDecoder::detect_symbol(const std::array<float, FFT_SIZE>& magnitudes) {
    // Find peak magnitude in ALE tone region (bins 6-13, one bin per tone)
    // Tone frequencies: 750, 875, 1000, 1125, 1250, 1375, 1500, 1625 Hz
    // At 8000 Hz sample rate with 64-point FFT (125 Hz/bin):
    // 750/125=6, 875/125=7, 1000/125=8, 1125/125=9, 1250/125=10, 1375/125=11, 1500/125=12, 1625/125=13
    
    float peak_mag = -1e6f;
    uint32_t peak_bin = 0xFF;
    
    // Check each ALE tone bin (consecutive from 6 to 13)
    for (uint32_t bin = 6; bin <= 13; ++bin) {
        if (bin < FFT_SIZE && magnitudes[bin] > peak_mag) {
            peak_mag = magnitudes[bin];
            peak_bin = bin;
        }
    }
    
    if (peak_bin == 0xFF) {
        return 0xFF;  // No valid tone detected
    }
    
    // Convert bin to symbol value (0-7)
    // bin 6 -> symbol 0, bin 7 -> symbol 1, etc.
    uint8_t symbol = peak_bin - 6;
    
    return symbol;
}

uint8_t SymbolDecoder::bin_to_symbol(uint32_t bin_index) {
    // ALE tones occupy bins 6-13 (consecutive) -> symbols 0-7
    // Mapping: bin 6->symbol 0, bin 7->symbol 1, ..., bin 13->symbol 7
    
    if (bin_index < 6 || bin_index > 13) {
        return 0xFF;  // Invalid bin
    }
    
    uint8_t symbol = bin_index - 6;  // 6->0, 7->1, ..., 13->7
    return symbol;
}

uint8_t SymbolDecoder::majority_vote(const uint8_t bits[3]) {
    // Sum the three bits: 0-3 is the count of 1s
    uint8_t sum = bits[0] + bits[1] + bits[2];
    // Majority voting: if 2 or more are 1, output 1, else 0
    return (sum >= 2) ? 1 : 0;
}

uint32_t SymbolDecoder::decode_word_with_voting(const uint8_t symbols[SYMBOLS_PER_WORD],
                                                uint32_t& output_word) {
    // MIL-STD-188-141B uses 3x redundancy:
    // Each data bit is transmitted at positions k, k+49, k+98 (mod 147)
    // Extract 24-bit word = 3-bit preamble + 21-bit payload
    
    uint32_t word = 0;
    uint32_t errors_corrected = 0;
    
    // Decode 24 data bits using triple voting
    for (uint32_t bit_idx = 0; bit_idx < WORD_BITS; ++bit_idx) {
        // Get three copies of this bit
        uint8_t bit_copies[3];
        
        for (uint32_t rep = 0; rep < SYMBOL_REPETITION; ++rep) {
            uint32_t sym_idx = bit_idx + rep * SYMBOLS_PER_WORD;
            if (sym_idx >= SYMBOLS_PER_WORD * SYMBOL_REPETITION) {
                break;  // Not enough symbols
            }
            
            uint8_t symbol = symbols[sym_idx];
            if (symbol >= 8) {
                bit_copies[rep] = 0xFF;  // Invalid
            } else {
                // Extract bit at position bit_idx from symbol
                // Need to convert symbol (0-7) to bit value
                // Symbols contain 3 bits each, so bit_idx within symbol
                uint8_t bit_in_symbol = bit_idx % BITS_PER_SYMBOL;
                bit_copies[rep] = (symbol >> bit_in_symbol) & 1;
            }
        }
        
        // Majority vote
        uint8_t final_bit = majority_vote(bit_copies);
        
        // Check for single-bit errors (when not unanimous)
        if ((bit_copies[0] != bit_copies[1]) ||
            (bit_copies[1] != bit_copies[2]) ||
            (bit_copies[0] != bit_copies[2])) {
            errors_corrected++;
        }
        
        // Set bit in output word
        word |= (final_bit << bit_idx);
    }
    
    output_word = word;
    return errors_corrected;
}

} // namespace ale
