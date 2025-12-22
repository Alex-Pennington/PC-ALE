/**
 * \file tone_generator.h
 * \brief 8-FSK tone generator using NCO
 * 
 * Generates 8 FSK tones using numerically-controlled oscillator (NCO) approach
 * with sine lookup table for efficiency.
 * 
 * Specification: MIL-STD-188-141B
 *  - Frequencies: 750, 875, 1000, 1125, 1250, 1375, 1500, 1625 Hz
 *  - Sample rate: 8000 Hz
 *  - Symbol rate: 125 baud (64 samples per symbol)
 */

#pragma once

#include "ale_types.h"
#include <array>
#include <cstdint>

namespace ale {

class ToneGenerator {
public:
    ToneGenerator();
    
    /**
     * Generate tone samples for given symbols
     * \param symbols Array of symbol values (0-7)
     * \param num_symbols Number of symbols to generate
     * \param output Pre-allocated output buffer [num_symbols * 64]
     * \param amplitude Output amplitude (typically 0.5 to 1.0)
     * \return Number of samples written
     */
    uint32_t generate_symbols(const uint8_t* symbols, uint32_t num_symbols,
                              int16_t* output, float amplitude = 0.7f);
    
    /**
     * Generate continuous tone (no modulation switching)
     * \param symbol_value FSK symbol (0-7)
     * \param num_samples Number of samples to generate
     * \param output Output buffer [num_samples]
     * \param amplitude Output amplitude
     * \return Number of samples written
     */
    uint32_t generate_tone(uint8_t symbol_value, uint32_t num_samples,
                           int16_t* output, float amplitude = 0.7f);
    
    /**
     * Reset generator state
     */
    void reset();
    
private:
    // Sine table for 256 samples per cycle
    static constexpr uint32_t SINE_TABLE_SIZE = 256;
    std::array<float, SINE_TABLE_SIZE> sine_table;
    
    // Phase accumulators for each tone
    std::array<uint32_t, NUM_TONES> phase_accum;
    
    // Phase increment per sample for each tone (fixed-point Q16)
    std::array<uint32_t, NUM_TONES> phase_increment;
    
    /**
     * Initialize sine lookup table
     */
    void init_sine_table();
    
    /**
     * Initialize phase increments for all tones
     */
    void init_phase_increments();
    
    /**
     * Get sine value using table lookup with interpolation
     * \param phase Phase value (0 to 2^32, wraps at 2^32)
     * \return Sine value (-1.0 to 1.0)
     */
    float sine_interpolate(uint32_t phase) const;
};

} // namespace ale
