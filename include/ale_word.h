/**
 * \file ale_word.h
 * \brief ALE word structure and parser
 * 
 * Implements MIL-STD-188-141B word structure:
 *  - 24 bits total after Golay decoding
 *  - 3-bit preamble (word type)
 *  - 21-bit payload (3 × 7-bit ASCII characters)
 * 
 * Specification: MIL-STD-188-141B Appendix A
 */

#pragma once

#include "ale_types.h"
#include <string>
#include <cstdint>

namespace ale {

/**
 * \enum WordType
 * Preamble types per MIL-STD-188-141B Table A-II
 */
enum class WordType : uint8_t {
    DATA = 0,    ///< Data word (content)
    THRU = 1,    ///< Through word (repeater)
    TO   = 2,    ///< To address
    TWS  = 3,    ///< To With Self (group call including caller)
    FROM = 4,    ///< From address (calling station)
    TIS  = 5,    ///< This Is Self (station identification)
    CMD  = 6,    ///< Command word
    REP  = 7,    ///< Repeat request
    UNKNOWN = 0xFF
};

/**
 * \struct ALEWord
 * Decoded ALE word with preamble and payload
 */
struct ALEWord {
    WordType type;                    ///< Preamble type (3 bits)
    char address[4];                  ///< 3 ASCII characters + null terminator
    uint32_t raw_payload;             ///< Raw 21-bit payload
    uint8_t fec_errors;               ///< Golay errors corrected
    bool valid;                       ///< Word passed FEC and validation
    uint32_t timestamp_ms;            ///< Reception timestamp
    
    ALEWord() : type(WordType::UNKNOWN), raw_payload(0), fec_errors(0), 
                valid(false), timestamp_ms(0) {
        address[0] = address[1] = address[2] = address[3] = '\0';
    }
};

/**
 * \class WordParser
 * Parse ALE words from decoded symbols
 */
class WordParser {
public:
    WordParser();
    
    /**
     * Parse 49 symbols into ALE word
     * Applies Golay FEC and extracts preamble + payload
     * 
     * \param symbols Array of 49 detected symbols (0-7)
     * \param output [out] Decoded ALE word
     * \return true if parsing successful, false on error
     */
    bool parse_word(const uint8_t symbols[SYMBOLS_PER_WORD], ALEWord& output);
    
    /**
     * Parse from raw 24-bit word (after FEC)
     * \param word_bits 24-bit decoded word
     * \param output [out] Decoded ALE word
     * \return true if parsing successful
     */
    bool parse_from_bits(uint32_t word_bits, ALEWord& output);
    
    /**
     * Extract preamble type from 24-bit word
     * \param word_bits 24-bit word
     * \return WordType enum
     */
    static WordType extract_preamble(uint32_t word_bits);
    
    /**
     * Extract 21-bit payload from 24-bit word
     * \param word_bits 24-bit word
     * \return 21-bit payload
     */
    static uint32_t extract_payload(uint32_t word_bits);
    
    /**
     * Decode 21-bit payload to 3 ASCII characters
     * Each character is 7 bits (ASCII-64 character set)
     * 
     * \param payload 21-bit payload
     * \param output [out] 4-byte buffer (3 chars + null)
     * \return true if all characters valid
     */
    static bool decode_ascii(uint32_t payload, char output[4]);
    
    /**
     * Encode 3 ASCII characters to 21-bit payload
     * \param chars 3-character string
     * \return 21-bit payload, or 0xFFFFFFFF on error
     */
    static uint32_t encode_ascii(const char chars[3]);
    
    /**
     * Validate ASCII character for ALE transmission
     * MIL-STD-188-141B uses ASCII-64 character set
     * 
     * \param ch Character to validate
     * \return true if valid for ALE
     */
    static bool is_valid_ale_char(char ch);
    
    /**
     * Get string name for word type
     */
    static const char* word_type_name(WordType type);
    
private:
    uint32_t last_timestamp_ms;
};

/**
 * \class AddressBook
 * Manage ALE addresses (self, other stations, nets)
 */
class AddressBook {
public:
    AddressBook();
    
    /**
     * Set self address (this station's call sign)
     * \param address 3-15 character address
     * \return true if valid and set
     */
    bool set_self_address(const std::string& address);
    
    /**
     * Get self address
     */
    std::string get_self_address() const { return self_address; }
    
    /**
     * Add other station address
     * \param address Station address
     * \param name Optional friendly name
     */
    void add_station(const std::string& address, const std::string& name = "");
    
    /**
     * Add net address (group)
     * \param net_address Net/group address
     * \param description Optional description
     */
    void add_net(const std::string& net_address, const std::string& description = "");
    
    /**
     * Check if address matches self
     * \param address Address to check
     * \return true if matches self address
     */
    bool is_self(const std::string& address) const;
    
    /**
     * Check if address is in known stations
     */
    bool is_known_station(const std::string& address) const;
    
    /**
     * Check if address is a known net
     */
    bool is_known_net(const std::string& address) const;
    
    /**
     * Match address with wildcards
     * Supports '@' wildcard per MIL-STD-188-141B
     * 
     * \param pattern Pattern with wildcards
     * \param address Address to match
     * \return true if matches
     */
    static bool match_wildcard(const std::string& pattern, const std::string& address);
    
private:
    std::string self_address;
    std::vector<std::pair<std::string, std::string>> stations;  // address, name
    std::vector<std::pair<std::string, std::string>> nets;      // net, description
};

} // namespace ale
