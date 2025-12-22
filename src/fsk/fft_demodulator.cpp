/**
 * \file fft_demodulator.cpp
 * \brief Implementation of FFT-based 8-FSK demodulator
 */

#include "fft_demodulator.h"
#include "symbol_decoder.h"
#include <algorithm>
#include <cmath>

namespace ale {

FFTDemodulator::FFTDemodulator()
    : sample_count(0),
      samples_per_symbol(SAMPLE_RATE_HZ / SYMBOL_RATE_BAUD),
      mag_history_offset(0) {
    
    // Initialize magnitude history
    for (uint32_t i = 0; i < SYMBOLS_PER_WORD; ++i) {
        std::fill(mag_history[i].begin(), mag_history[i].end(), 0.0f);
    }
}

void FFTDemodulator::reset() {
    sample_count = 0;
    mag_history_offset = 0;
    fft_buffer.reset();
    
    for (uint32_t i = 0; i < SYMBOLS_PER_WORD; ++i) {
        std::fill(mag_history[i].begin(), mag_history[i].end(), 0.0f);
    }
}

const std::array<float, FFT_SIZE>& FFTDemodulator::get_fft_magnitudes() const {
    return fft_buffer.get_magnitudes();
}

std::vector<Symbol> FFTDemodulator::process_audio(const int16_t* samples, uint32_t num_samples) {
    std::vector<Symbol> symbols;
    
    for (uint32_t i = 0; i < num_samples; ++i) {
        Symbol* sym = process_sample(samples[i]);
        if (sym) {
            symbols.push_back(*sym);
        }
    }
    
    return symbols;
}

Symbol* FFTDemodulator::process_sample(int16_t sample) {
    // Push sample to FFT buffer
    const auto& magnitudes = fft_buffer.push_sample(sample);
    
    ++sample_count;
    
    // Check if we've collected enough samples for one symbol (64 samples = 8000/125)
    if ((sample_count % samples_per_symbol) != 0) {
        return nullptr;
    }
    
    // Create symbol structure
    static Symbol current_symbol;
    
    // Detect symbol from FFT magnitudes
    uint8_t symbol_bits = SymbolDecoder::detect_symbol(magnitudes);
    
    if (symbol_bits == 0xFF) {
        return nullptr;  // Detection failed
    }
    
    // Extract bit values
    current_symbol.bits[0] = (symbol_bits >> 0) & 1;
    current_symbol.bits[1] = (symbol_bits >> 1) & 1;
    current_symbol.bits[2] = (symbol_bits >> 2) & 1;
    
    // Find peak for magnitude and SNR calculation
    float peak_mag = 0.0f;
    float noise_floor = estimate_noise_floor(magnitudes);
    
    for (uint32_t bin = FFT_BIN_OFFSET; bin < FFT_BIN_OFFSET + FFT_BIN_SPAN; ++bin) {
        if (magnitudes[bin] > peak_mag) {
            peak_mag = magnitudes[bin];
        }
    }
    
    current_symbol.magnitude = peak_mag;
    current_symbol.signal_to_noise = compute_snr(peak_mag, noise_floor);
    current_symbol.sample_index = sample_count - 1;
    
    // Store magnitude history for later word decoding
    mag_history[mag_history_offset] = magnitudes;
    mag_history_offset = (mag_history_offset + 1) % SYMBOLS_PER_WORD;
    
    return &current_symbol;
}

uint8_t FFTDemodulator::detect_symbol(const std::array<float, FFT_SIZE>& magnitudes) {
    return SymbolDecoder::detect_symbol(magnitudes);
}

float FFTDemodulator::estimate_noise_floor(const std::array<float, FFT_SIZE>& magnitudes) {
    // Estimate noise floor from minimum magnitudes in non-ALE regions
    float min_mag = magnitudes[0];
    
    // Check bins outside ALE region
    for (uint32_t i = 0; i < FFT_BIN_OFFSET; ++i) {
        if (magnitudes[i] < min_mag) {
            min_mag = magnitudes[i];
        }
    }
    
    for (uint32_t i = FFT_BIN_OFFSET + FFT_BIN_SPAN; i < FFT_SIZE; ++i) {
        if (magnitudes[i] < min_mag) {
            min_mag = magnitudes[i];
        }
    }
    
    return std::max(min_mag, 0.001f);  // Avoid division by zero
}

float FFTDemodulator::compute_snr(float signal, float noise) {
    if (noise < 0.001f) noise = 0.001f;
    return 20.0f * std::log10(signal / noise + 1e-6f);
}

} // namespace ale
