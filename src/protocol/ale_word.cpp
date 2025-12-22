/**
 * \file ale_word.cpp
 * \brief Implementation of ALE word parser
 */

#include "ale_word.h"
#include "symbol_decoder.h"
#include "golay.h"
#include <cstring>
#include <cctype>
#include <algorithm>

namespace ale {

// Word type names per MIL-STD-188-141B
static const char* WORD_TYPE_NAMES[] = {
    "DATA", "THRU", "TO", "TWS", "FROM", "TIS", "CMD", "REP", "UNKNOWN"
};

// ASCII-64 character set for ALE (per MIL-STD-188-141B)
// Valid characters: A-Z, 0-9, space, and limited punctuation
static const char ALE_ASCII_64[] = 
    " 0123456789@ABCDEFGHIJKLMNOPQRSTUVWXYZ?.-/";

WordParser::WordParser() : last_timestamp_ms(0) {}

bool WordParser::parse_word(const uint8_t symbols[SYMBOLS_PER_WORD], ALEWord& output) {
    // Step 1: Decode symbols with majority voting
    uint32_t raw_word = 0;
    uint32_t errors = SymbolDecoder::decode_word_with_voting(symbols, raw_word);
    
    // Step 2: Apply Golay FEC to get 12-bit information
    // The 24-bit raw word should be treated as a Golay codeword
    uint16_t decoded_info = 0;
    uint8_t fec_errors = Golay::decode(raw_word, decoded_info);
    
    if (fec_errors == 0xFF) {
        // Uncorrectable FEC error
        output.valid = false;
        return false;
    }
    
    output.fec_errors = fec_errors;
    
    // Step 3: Extract preamble and payload from decoded information
    // 12-bit information contains compressed word data
    // Need to reconstruct 24-bit word: expand back or use different approach
    
    // Actually, per MIL-STD-188-141B, the word structure is:
    // - 24 data bits transmitted via 49 symbols (3 bits/symbol, 3x redundancy)
    // - After voting, we have 24 bits
    // - These 24 bits ARE the word (no Golay at word level, Golay is symbol-level)
    
    // So raw_word from voting IS our 24-bit word
    return parse_from_bits(raw_word & 0xFFFFFF, output);
}

bool WordParser::parse_from_bits(uint32_t word_bits, ALEWord& output) {
    // Extract preamble (bits 0-2)
    output.type = extract_preamble(word_bits);
    
    // Extract payload (bits 3-23)
    output.raw_payload = extract_payload(word_bits);
    
    // Decode ASCII characters
    bool ascii_valid = decode_ascii(output.raw_payload, output.address);
    
    output.valid = ascii_valid;
    return ascii_valid;
}

WordType WordParser::extract_preamble(uint32_t word_bits) {
    uint8_t preamble = word_bits & 0x07;  // Bits 0-2
    
    if (preamble > 7) {
        return WordType::UNKNOWN;
    }
    
    return static_cast<WordType>(preamble);
}

uint32_t WordParser::extract_payload(uint32_t word_bits) {
    return (word_bits >> 3) & 0x1FFFFF;  // Bits 3-23 (21 bits)
}

bool WordParser::decode_ascii(uint32_t payload, char output[4]) {
    // 21 bits = 3 × 7-bit characters
    char char0 = (payload >> 0) & 0x7F;   // Bits 0-6
    char char1 = (payload >> 7) & 0x7F;   // Bits 7-13
    char char2 = (payload >> 14) & 0x7F;  // Bits 14-20
    
    // Validate characters
    if (!is_valid_ale_char(char0) || 
        !is_valid_ale_char(char1) || 
        !is_valid_ale_char(char2)) {
        output[0] = output[1] = output[2] = '?';
        output[3] = '\0';
        return false;
    }
    
    output[0] = char0;
    output[1] = char1;
    output[2] = char2;
    output[3] = '\0';
    
    return true;
}

uint32_t WordParser::encode_ascii(const char chars[3]) {
    // Validate and encode 3 characters to 21 bits
    if (!is_valid_ale_char(chars[0]) || 
        !is_valid_ale_char(chars[1]) || 
        !is_valid_ale_char(chars[2])) {
        return 0xFFFFFFFF;
    }
    
    uint32_t payload = 0;
    payload |= (chars[0] & 0x7F) << 0;
    payload |= (chars[1] & 0x7F) << 7;
    payload |= (chars[2] & 0x7F) << 14;
    
    return payload & 0x1FFFFF;
}

bool WordParser::is_valid_ale_char(char ch) {
    // ALE uses 7-bit ASCII with restricted character set
    // Per MIL-STD-188-141B: A-Z, 0-9, space, and limited punctuation
    
    if (ch >= 'A' && ch <= 'Z') return true;
    if (ch >= '0' && ch <= '9') return true;
    if (ch == ' ' || ch == '@' || ch == '?' || ch == '.' || 
        ch == '-' || ch == '/') return true;
    
    return false;
}

const char* WordParser::word_type_name(WordType type) {
    uint8_t index = static_cast<uint8_t>(type);
    if (index > 7) index = 8;  // UNKNOWN
    return WORD_TYPE_NAMES[index];
}

// ============================================================================
// AddressBook Implementation
// ============================================================================

AddressBook::AddressBook() {}

bool AddressBook::set_self_address(const std::string& address) {
    // Validate address length (3-15 characters per MIL-STD-188-141B)
    if (address.length() < 3 || address.length() > 15) {
        return false;
    }
    
    // Validate characters
    for (char ch : address) {
        if (!WordParser::is_valid_ale_char(ch)) {
            return false;
        }
    }
    
    self_address = address;
    return true;
}

void AddressBook::add_station(const std::string& address, const std::string& name) {
    // Check if already exists
    for (const auto& station : stations) {
        if (station.first == address) {
            return;  // Already in list
        }
    }
    
    stations.push_back({address, name});
}

void AddressBook::add_net(const std::string& net_address, const std::string& description) {
    // Check if already exists
    for (const auto& net : nets) {
        if (net.first == net_address) {
            return;
        }
    }
    
    nets.push_back({net_address, description});
}

bool AddressBook::is_self(const std::string& address) const {
    return address == self_address;
}

bool AddressBook::is_known_station(const std::string& address) const {
    for (const auto& station : stations) {
        if (station.first == address) {
            return true;
        }
    }
    return false;
}

bool AddressBook::is_known_net(const std::string& address) const {
    for (const auto& net : nets) {
        if (net.first == address) {
            return true;
        }
    }
    return false;
}

bool AddressBook::match_wildcard(const std::string& pattern, const std::string& address) {
    // Simple wildcard matching with '@' as wildcard character
    // Per MIL-STD-188-141B, '@' matches any single character
    
    if (pattern.length() != address.length()) {
        return false;
    }
    
    for (size_t i = 0; i < pattern.length(); ++i) {
        if (pattern[i] == '@') {
            continue;  // Wildcard matches anything
        }
        if (pattern[i] != address[i]) {
            return false;
        }
    }
    
    return true;
}

} // namespace ale
