/**
 * \file fft_demodulator.h
 * \brief FFT-based 8-FSK demodulator
 * 
 * Implements sliding 64-point FFT for efficient tone detection.
 * One new sample per symbol (8000 Hz / 125 baud = 64 samples/symbol)
 * 
 * Specification: MIL-STD-188-141B
 *  - 8 tones: 750-1750 Hz, 125 Hz spacing
 *  - FFT bins 6-22 (every 2 bins) contain ALE tones
 *  - Peak detection with noise floor estimation
 */

#pragma once

#include "ale_types.h"
#include <array>
#include <cstdint>

namespace ale {

class FFTDemodulator {
public:
    FFTDemodulator();
    
    /**
     * Process audio frame and detect symbols
     * \param samples Audio buffer (8-bit samples)
     * \param num_samples Number of samples
     * \return Vector of detected symbols
     */
    std::vector<Symbol> process_audio(const int16_t* samples, uint32_t num_samples);
    
    /**
     * Process single sample for symbol detection
     * \param sample 16-bit audio sample
     * \return Non-null Symbol if symbol boundary detected, else nullptr
     */
    Symbol* process_sample(int16_t sample);
    
    /**
     * Reset demodulator state
     */
    void reset();
    
    /**
     * Get current FFT magnitude array
     * \return Reference to magnitudes [FFT_SIZE]
     */
    const std::array<float, FFT_SIZE>& get_fft_magnitudes() const;
    
private:
    FFTBuffer fft_buffer;
    uint32_t sample_count;
    uint32_t samples_per_symbol;        // = 8000 / 125 = 64
    
    std::array<float, FFT_SIZE> mag_history[SYMBOLS_PER_WORD];
    uint32_t mag_history_offset;
    
    /**
     * Detect peak tone from FFT bins
     * Returns symbol value (0-7) or 0xFF if detection failed
     */
    uint8_t detect_symbol(const std::array<float, FFT_SIZE>& magnitudes);
    
    /**
     * Estimate noise floor from magnitude array
     */
    float estimate_noise_floor(const std::array<float, FFT_SIZE>& magnitudes);
    
    /**
     * Compute signal-to-noise ratio
     */
    float compute_snr(float signal, float noise);
};

} // namespace ale
