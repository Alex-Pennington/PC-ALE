/**
 * \file types.cpp
 * \brief Implementation of core ALE types
 */

#include "ale_types.h"
#include <cmath>
#include <algorithm>

namespace ale {

FFTBuffer::FFTBuffer()
    : sample_count(0), fft_history_offset(0) {
    
    // Initialize sample history buffer (64 samples)
    // We'll use a simple DFT for each bin rather than full FFT
    
    // Initialize state variables
    std::fill(s0.begin(), s0.end(), 0.0f);
    std::fill(s1.begin(), s1.end(), 0.0f);
    std::fill(s2.begin(), s2.end(), 0.0f);
    std::fill(magnitude.begin(), magnitude.end(), 0.0f);
    
    // Pre-compute cosine/sine twiddle factors for 64-point DFT
    for (uint32_t k = 0; k < FFT_SIZE; ++k) {
        double angle = 2.0 * M_PI * k / FFT_SIZE;
        fft_cs_twiddle[k] = std::cos(angle);
        fft_ss_twiddle[k] = std::sin(angle);
    }
}

const std::array<float, FFT_SIZE>& FFTBuffer::push_sample(int16_t sample) {
    // Keep circular buffer of 64 samples
    static std::array<float, FFT_SIZE> sample_buffer = {};
    static uint32_t buffer_index = 0;
    
    // Normalize and store sample
    float normalized = static_cast<float>(sample) / 32768.0f;
    sample_buffer[buffer_index] = normalized;
    buffer_index = (buffer_index + 1) % FFT_SIZE;
    
    // Every 64 samples, compute FFT magnitudes
    if ((++sample_count % FFT_SIZE) == 0) {
        compute_magnitudes_from_buffer(sample_buffer);
    }
    
    return magnitude;
}

void FFTBuffer::compute_magnitudes_from_buffer(const std::array<float, FFT_SIZE>& samples) {
    // Compute DFT magnitude at each bin
    for (uint32_t k = 0; k < FFT_SIZE; ++k) {
        float real_part = 0.0f;
        float imag_part = 0.0f;
        
        // DFT: X[k] = sum(x[n] * exp(-j*2*pi*k*n/N))
        for (uint32_t n = 0; n < FFT_SIZE; ++n) {
            float cos_val = fft_cs_twiddle[((k * n) % FFT_SIZE)];
            float sin_val = fft_ss_twiddle[((k * n) % FFT_SIZE)];
            
            real_part += samples[n] * cos_val;
            imag_part -= samples[n] * sin_val;
        }
        
        // Compute magnitude with smoothing
        float mag = std::sqrt(real_part * real_part + imag_part * imag_part) / FFT_SIZE;
        magnitude[k] = 0.8f * magnitude[k] + 0.2f * mag;
    }
}

const std::array<float, FFT_SIZE>& FFTBuffer::get_magnitudes() const {
    return magnitude;
}

void FFTBuffer::reset() {
    sample_count = 0;
    fft_history_offset = 0;
    std::fill(s0.begin(), s0.end(), 0.0f);
    std::fill(s1.begin(), s1.end(), 0.0f);
    std::fill(s2.begin(), s2.end(), 0.0f);
    std::fill(magnitude.begin(), magnitude.end(), 0.0f);
}

void FFTBuffer::compute_magnitudes() {
    // This function is now deprecated in favor of compute_magnitudes_from_buffer
    // kept for compatibility
}

} // namespace ale
