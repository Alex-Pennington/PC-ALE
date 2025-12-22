/**
 * \file golay.h
 * \brief Extended Golay (24,12) FEC encoder/decoder
 * 
 * Table-driven implementation of Extended Golay error-correcting code.
 * Corrects up to 3 bit errors per 24-bit codeword.
 * 
 * Specification: MIL-STD-188-141B
 *  - Code: Extended Golay (24,12)
 *  - Information bits: 12
 *  - Parity bits: 12
 *  - Error correction capability: 3 bits
 */

#pragma once

#include <cstdint>
#include <array>

namespace ale {

class Golay {
public:
    /**
     * Encode 12-bit information word to 24-bit codeword
     * Codeword = [information (12 bits) | parity (12 bits)]
     * 
     * \param info 12-bit information word
     * \return 24-bit encoded codeword
     */
    static uint32_t encode(uint16_t info);
    
    /**
     * Decode and correct 24-bit codeword
     * Corrects up to 3 random bit errors.
     * 
     * \param codeword 24-bit received codeword
     * \param output [out] 12-bit decoded information
     * \return Number of bit errors corrected (0-3), or 0xFF if uncorrectable
     */
    static uint8_t decode(uint32_t codeword, uint16_t& output);
    
    /**
     * Extract information bits from codeword (no error correction)
     * 
     * \param codeword 24-bit codeword
     * \return 12-bit information field
     */
    static uint16_t extract_info(uint32_t codeword);
    
    /**
     * Extract parity bits from codeword
     * 
     * \param codeword 24-bit codeword
     * \return 12-bit parity field
     */
    static uint16_t extract_parity(uint32_t codeword);
    
private:
    // Syndrome lookup table for error correction
    // Maps syndrome (12 bits) to error pattern (24 bits)
    // Computed statically at compile-time or load-time
    static constexpr uint32_t SYNDROME_TABLE_SIZE = 1 << 12;
    static std::array<uint32_t, SYNDROME_TABLE_SIZE> syndrome_table;
    
    /**
     * Compute syndrome for error detection
     * \param codeword 24-bit codeword
     * \return 12-bit syndrome
     */
    static uint16_t compute_syndrome(uint32_t codeword);
    
    /**
     * Compute parity of bit field (count set bits mod 2)
     */
    static uint8_t compute_parity(uint32_t value);
    
    /**
     * Initialize syndrome table once at startup
     */
    static bool init_syndrome_table();
    
    static bool syndrome_table_initialized;
};

} // namespace ale
