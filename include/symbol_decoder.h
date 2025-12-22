/**
 * \file symbol_decoder.h
 * \brief FSK symbol detection and decoding
 * 
 * Extracts 3-bit symbol values from FFT magnitude peaks.
 * Uses majority voting for triple-redundancy error correction.
 * 
 * Specification: MIL-STD-188-141B
 *  - Each data bit transmitted 3 times (symbols at positions k, k+49, k+98)
 *  - Symbol detection: find peak magnitude in bins 6-22
 *  - Bin-to-symbol mapping: bins 6,8,10,12,14,16,18,20 -> symbols 0-7
 */

#pragma once

#include "ale_types.h"
#include <array>
#include <cstdint>

namespace ale {

class SymbolDecoder {
public:
    SymbolDecoder();
    
    /**
     * Detect FSK symbol from FFT magnitudes
     * Finds peak in bins 6-22 and maps to symbol value
     * 
     * \param magnitudes FFT magnitude array [FFT_SIZE]
     * \return Symbol value 0-7, or 0xFF if detection failed
     */
    static uint8_t detect_symbol(const std::array<float, FFT_SIZE>& magnitudes);
    
    /**
     * Extract 3-bit symbol value from peak position
     * Maps FFT bin position to 0-7 symbol
     * 
     * \param bin_index FFT bin (6-22, every 2 bins for ALE)
     * \return Symbol value 0-7
     */
    static uint8_t bin_to_symbol(uint32_t bin_index);
    
    /**
     * Majority voting for triple-redundant bit
     * Combines 3 copies of same bit for error correction
     * 
     * \param bits Array of 3 bit values
     * \return Final bit value (0 or 1)
     */
    static uint8_t majority_vote(const uint8_t bits[3]);
    
    /**
     * Decode word using triple redundancy voting
     * 
     * \param symbols Array of 49 detected symbols
     * \param output_word [out] 24-bit decoded word
     * \return Number of bit errors corrected
     */
    static uint32_t decode_word_with_voting(const uint8_t symbols[SYMBOLS_PER_WORD],
                                            uint32_t& output_word);
    
private:
    // Lookup table: FFT bin -> symbol value
    // Bins 6-22 (every 2): 6->0, 8->1, 10->2, 12->3, 14->4, 16->5, 18->6, 20->7, 22->0xFF
    static constexpr std::array<uint8_t, 32> BIN_TO_SYMBOL_TABLE = {
        0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0, 0xFF, 1, 0xFF,
        2, 0xFF, 3, 0xFF, 4, 0xFF, 5, 0xFF, 6, 0xFF,
        7, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
        0xFF, 0xFF
    };
};

} // namespace ale
