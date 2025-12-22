/**
 * \file types.h
 * \brief Core type definitions for ALE 8-FSK modem
 * 
 * Specification Reference:
 *  - MIL-STD-188-141B Appendix A
 *  - 8-FSK modulation: 8 tones, 125 baud, 125 Hz spacing
 *  - Bandwidth: 1 kHz (tones 750-1750 Hz)
 */

#pragma once

#include <cstdint>
#include <complex>
#include <array>
#include <vector>

namespace ale {

// Physical layer constants per MIL-STD-188-141B
constexpr uint32_t SAMPLE_RATE_HZ = 8000;          ///< Audio sample rate
constexpr uint32_t SYMBOL_RATE_BAUD = 125;         ///< Symbol transmission rate
constexpr uint32_t TONE_SPACING_HZ = 125;          ///< Spacing between FSK tones
constexpr uint32_t NUM_TONES = 8;                  ///< Number of FSK tones
constexpr uint32_t BITS_PER_SYMBOL = 3;            ///< Each symbol encodes 3 bits
constexpr uint32_t BANDWIDTH_HZ = 1000;            ///< Total bandwidth 750-1750 Hz

// Tone frequencies (Hz)
constexpr std::array<uint32_t, NUM_TONES> TONE_FREQS_HZ = {
    750, 875, 1000, 1125, 1250, 1375, 1500, 1625
};

// FFT parameters
constexpr uint32_t FFT_SIZE = 64;                  ///< 64-point FFT
constexpr uint32_t FFT_BIN_OFFSET = 6;             ///< First ALE tone in bin 6
constexpr uint32_t FFT_BIN_SPAN = 16;              ///< 8 tones span 16 bins (every 2 bins)

// Word structure per spec
constexpr uint32_t SYMBOLS_PER_WORD = 49;          ///< Word = 49 symbols = 147 bits
constexpr uint32_t PREAMBLE_BITS = 3;              ///< Preamble field
constexpr uint32_t PAYLOAD_BITS = 21;              ///< Payload field (3x7-bit ASCII chars)
constexpr uint32_t WORD_BITS = PREAMBLE_BITS + PAYLOAD_BITS;

// Error correction
constexpr uint32_t GOLAY_CODEWORD_BITS = 24;       ///< Extended Golay code
constexpr uint32_t GOLAY_INFO_BITS = 12;           ///< Information bits per codeword
constexpr uint32_t GOLAY_PARITY_BITS = 12;         ///< Parity bits per codeword
constexpr uint32_t MAX_GOLAY_ERRORS = 3;           ///< Corrects up to 3 bits

// Redundancy
constexpr uint32_t SYMBOL_REPETITION = 3;          ///< Each data bit sent 3 times
constexpr uint32_t VOTE_BUFFER_LENGTH = 48;        ///< Symbols for voting buffer
constexpr uint32_t VOTE_THRESHOLD_BAD = 25;        ///< Threshold for bad symbol detection

// Types
using ComplexFloat = std::complex<float>;
using ComplexDouble = std::complex<double>;

/**
 * \enum PreambleType
 * Word preamble types per MIL-STD-188-141B
 */
enum class PreambleType : uint8_t {
    DATA = 0,
    THRU = 1,
    TO   = 2,
    TWS  = 3,    ///< To With Self
    FROM = 4,
    TIS  = 5,    ///< This Is Self
    CMD  = 6,
    REP  = 7,
    UNKNOWN = 0xFF
};

/**
 * \struct Symbol
 * Decoded FSK symbol with confidence metrics
 */
struct Symbol {
    uint8_t bits[BITS_PER_SYMBOL];      ///< 3-bit symbol value (0-7)
    float magnitude;                     ///< Peak magnitude from FFT
    float signal_to_noise;               ///< SNR estimate
    uint32_t sample_index;               ///< Sample number when detected
};

/**
 * \struct Word
 * Decoded ALE word with FEC
 */
struct Word {
    uint32_t raw_bits;                   ///< 24-bit raw word (before FEC)
    uint32_t corrected_bits;             ///< 24-bit corrected word (after FEC)
    PreambleType preamble;               ///< Preamble type (3 bits)
    uint32_t payload;                    ///< Payload (21 bits, 3x ASCII)
    uint8_t error_count;                 ///< Number of errors corrected
    bool crc_valid;                      ///< CRC check result
    uint32_t word_index;                 ///< Sequence number
};

/**
 * \class FFTBuffer
 * Circular buffer for sliding FFT analysis
 * 
 * Implements O(N) per-bin DFT computation for streaming audio
 */
class FFTBuffer {
public:
    FFTBuffer();
    
    /**
     * Add new sample and return updated FFT magnitudes
     * \param sample Audio sample value (-32768 to +32767)
     * \return Reference to magnitude array [FFT_SIZE]
     */
    const std::array<float, FFT_SIZE>& push_sample(int16_t sample);
    
    /**
     * Get current FFT magnitudes without advancing
     */
    const std::array<float, FFT_SIZE>& get_magnitudes() const;
    
    /**
     * Reset buffer to zero
     */
    void reset();
    
private:
    // Twiddle factors for DFT computation
    std::array<float, FFT_SIZE> fft_cs_twiddle;       // cos values
    std::array<float, FFT_SIZE> fft_ss_twiddle;       // sin values
    std::array<float, FFT_SIZE> s0, s1, s2;           // Goertzel state (deprecated)
    std::array<float, FFT_SIZE> coeff;                // Goertzel coeff (deprecated)
    std::array<float, FFT_SIZE> magnitude;            // Output magnitudes
    
    uint32_t sample_count;
    uint32_t fft_history_offset;
    
    void compute_magnitudes();
    
    /**
     * Compute DFT magnitudes from sample buffer
     */
    void compute_magnitudes_from_buffer(const std::array<float, FFT_SIZE>& samples);
};

} // namespace ale
