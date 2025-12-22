/**
 * \file tone_generator.cpp
 * \brief Implementation of NCO-based 8-FSK tone generator
 */

#include "tone_generator.h"
#include <cmath>
#include <algorithm>

namespace ale {

ToneGenerator::ToneGenerator() {
    init_sine_table();
    init_phase_increments();
    reset();
}

void ToneGenerator::init_sine_table() {
    // Pre-compute sine table for one complete cycle
    for (uint32_t i = 0; i < SINE_TABLE_SIZE; ++i) {
        double angle = 2.0 * M_PI * i / SINE_TABLE_SIZE;
        sine_table[i] = std::sin(angle);
    }
}

void ToneGenerator::init_phase_increments() {
    // Phase increment = (freq_hz / sample_rate) * 2^32
    // For fixed-point phase accumulation
    for (uint32_t tone_idx = 0; tone_idx < NUM_TONES; ++tone_idx) {
        uint32_t freq_hz = TONE_FREQS_HZ[tone_idx];
        
        // phase_increment = freq_hz * 2^32 / sample_rate
        double increment = static_cast<double>(freq_hz) * (1LL << 32) / SAMPLE_RATE_HZ;
        phase_increment[tone_idx] = static_cast<uint32_t>(increment);
    }
}

void ToneGenerator::reset() {
    std::fill(phase_accum.begin(), phase_accum.end(), 0);
}

float ToneGenerator::sine_interpolate(uint32_t phase) const {
    // phase is 32-bit, use upper bits as table index
    uint32_t index = (phase >> 24) & 0xFF;  // Use bits 24-31 for table index
    uint32_t frac = phase & 0xFFFFFF;       // Fractional part
    
    float frac_norm = frac / static_cast<float>(1 << 24);
    
    uint32_t next_index = (index + 1) & (SINE_TABLE_SIZE - 1);
    float s0 = sine_table[index];
    float s1 = sine_table[next_index];
    
    // Linear interpolation
    return s0 * (1.0f - frac_norm) + s1 * frac_norm;
}

uint32_t ToneGenerator::generate_symbols(const uint8_t* symbols, uint32_t num_symbols,
                                         int16_t* output, float amplitude) {
    uint32_t samples_written = 0;
    
    for (uint32_t sym_idx = 0; sym_idx < num_symbols; ++sym_idx) {
        uint8_t symbol = symbols[sym_idx];
        if (symbol >= NUM_TONES) {
            symbol = NUM_TONES - 1;  // Clamp invalid symbols
        }
        
        uint32_t phase_inc = phase_increment[symbol];
        
        // Generate one symbol = 64 samples at this frequency
        for (uint32_t sample_idx = 0; sample_idx < SAMPLE_RATE_HZ / SYMBOL_RATE_BAUD; ++sample_idx) {
            float sine_val = sine_interpolate(phase_accum[symbol]);
            
            // Convert to int16 with amplitude scaling
            int32_t sample = static_cast<int32_t>(sine_val * amplitude * 32767.0f);
            sample = std::max(-32768, std::min(32767, sample));
            
            output[samples_written++] = static_cast<int16_t>(sample);
            
            // Update phase accumulator
            phase_accum[symbol] += phase_inc;
        }
    }
    
    return samples_written;
}

uint32_t ToneGenerator::generate_tone(uint8_t symbol_value, uint32_t num_samples,
                                      int16_t* output, float amplitude) {
    if (symbol_value >= NUM_TONES) {
        symbol_value = NUM_TONES - 1;
    }
    
    uint32_t phase_inc = phase_increment[symbol_value];
    
    for (uint32_t i = 0; i < num_samples; ++i) {
        float sine_val = sine_interpolate(phase_accum[symbol_value]);
        
        int32_t sample = static_cast<int32_t>(sine_val * amplitude * 32767.0f);
        sample = std::max(-32768, std::min(32767, sample));
        
        output[i] = static_cast<int16_t>(sample);
        
        phase_accum[symbol_value] += phase_inc;
    }
    
    return num_samples;
}

} // namespace ale
