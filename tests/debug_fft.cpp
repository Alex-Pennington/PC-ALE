/**
 * \file debug_fft.cpp
 * \brief Debug FFT bin detection
 */

#include "ale_types.h"
#include "tone_generator.h"
#include "fft_demodulator.h"
#include <iostream>
#include <iomanip>
#include <vector>

using namespace ale;

int main() {
    ToneGenerator gen;
    FFTDemodulator demod;
    
    std::cout << "\nDEBUG: FFT Bin Detection\n";
    std::cout << "========================\n\n";
    
    // Generate tone 0 (750 Hz) 
    // FFT bin should be 6: 750 Hz / 125 Hz/bin = 6
    std::cout << "Tone 0 (750 Hz) -> should be bin 6\n";
    
    demod.reset();
    gen.reset();
    
    std::vector<int16_t> samples(64);
    gen.generate_tone(0, 64, samples.data());
    
    auto symbols = demod.process_audio(samples.data(), 64);
    
    auto mags = demod.get_fft_magnitudes();
    
    std::cout << "FFT Magnitudes:\n";
    for (uint32_t i = 4; i <= 10; ++i) {
        std::cout << "  Bin " << std::setw(2) << i << " (" << std::setw(4) << (i*125) 
                  << " Hz): " << std::fixed << std::setprecision(4) << mags[i] << "\n";
    }
    
    if (!symbols.empty()) {
        std::cout << "Detected symbol: " << (int)(symbols[0].bits[2] << 2 | 
                                                  symbols[0].bits[1] << 1 | 
                                                  symbols[0].bits[0]) << "\n";
    }
    
    // Test all 8 tones
    std::cout << "\n\nAll Tones FFT Mapping:\n";
    for (uint8_t tone = 0; tone < 8; ++tone) {
        demod.reset();
        gen.reset();
        
        std::vector<int16_t> audio(64);
        gen.generate_tone(tone, 64, audio.data());
        auto syms = demod.process_audio(audio.data(), 64);
        
        std::cout << "Tone " << (int)tone << " (freq " << TONE_FREQS_HZ[tone] 
                  << " Hz, bin " << (TONE_FREQS_HZ[tone] / 125) << "): ";
        
        if (!syms.empty()) {
            auto mags2 = demod.get_fft_magnitudes();
            float peak_mag = 0;
            int peak_bin = -1;
            for (uint32_t b = 0; b < FFT_SIZE; ++b) {
                if (mags2[b] > peak_mag) {
                    peak_mag = mags2[b];
                    peak_bin = b;
                }
            }
            std::cout << " peak at bin " << peak_bin << " (mag " << peak_mag << ")\n";
        }
    }
    
    return 0;
}
