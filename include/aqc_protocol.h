/**
 * \file aqc_protocol.h
 * \brief AQC-ALE (Advanced Quick Call ALE) protocol extensions
 * 
 * Implements MIL-STD-188-141B AQC-ALE enhancements:
 *  - Data Elements (DE1-DE9) extracted from 21-bit payload
 *  - CRC protection for orderwire messages
 *  - Slotted response mechanism (slots 0-7)
 *  - Transaction codes for link management
 *  - Traffic class identification
 * 
 * Key Finding: AQC-ALE uses the SAME 8-FSK modem as standard 2G ALE.
 * This is a PROTOCOL layer enhancement, not a different physical layer.
 * 
 * Specification: MIL-STD-188-141B Appendix C (AQC-ALE)
 */

#pragma once

#include "ale_word.h"
#include <cstdint>
#include <string>

namespace ale {
namespace aqc {

// ============================================================================
// Data Element (DE) Definitions per MIL-STD-188-141B
// ============================================================================

/**
 * \enum DE3_TrafficClass
 * DE3: Traffic class identifier (voice type, data mode)
 */
enum class DE3_TrafficClass : uint8_t {
    CLEAR_VOICE = 0,            ///< Clear analog voice
    DIGITAL_VOICE = 1,          ///< Digital voice (unencrypted)
    HFD_VOICE = 2,              ///< HF digital voice
    RESERVED_3 = 3,             ///< Reserved
    SECURE_DIGITAL_VOICE = 4,   ///< Encrypted digital voice
    RESERVED_5 = 5,             ///< Reserved
    RESERVED_6 = 6,             ///< Reserved
    RESERVED_7 = 7,             ///< Reserved
    ALE_MSG = 8,                ///< ALE messaging mode
    PSK_MSG = 9,                ///< PSK data mode
    TONE_39_MSG = 10,           ///< 39-tone data mode
    HF_EMAIL = 11,              ///< HF email mode
    KY100_ACTIVE = 12,          ///< KY-100 encryption active
    RESERVED_13 = 13,           ///< Reserved
    RESERVED_14 = 14,           ///< Reserved
    RESERVED_15 = 15            ///< Reserved
};

/**
 * \enum DE9_TransactionCode
 * DE9: Transaction code for link management
 */
enum class DE9_TransactionCode : uint8_t {
    RESERVED_0 = 0,             ///< Reserved
    MS_141A = 1,                ///< MIL-STD-188-141A messaging
    ACK_LAST = 2,               ///< Acknowledge last message
    NAK_LAST = 3,               ///< Negative acknowledge last
    TERMINATE = 4,              ///< Terminate link
    OP_ACKNAK = 5,              ///< Operator acknowledge/NAK
    AQC_CMD = 6,                ///< AQC command follows
    RESERVED_7 = 7              ///< Reserved
};

/**
 * \enum CRCStatus
 * CRC validation result for orderwire messages
 */
enum class CRCStatus : uint8_t {
    NOT_APPLICABLE = 0,         ///< No CRC in message
    CRC_OK = 1,                 ///< CRC valid
    CRC_ERROR = 2               ///< CRC mismatch
};

// ============================================================================
// Data Element Structure
// ============================================================================

/**
 * \struct DataElements
 * Parsed data elements from AQC-enhanced word payload
 * 
 * The 21-bit payload is structured differently in AQC mode:
 * - Standard 2G: 3 × 7-bit ASCII characters
 * - AQC mode: Data elements (DE1-DE9) with specific bit fields
 */
struct DataElements {
    uint8_t de1;                ///< DE1: Reserved (future use)
    uint8_t de2;                ///< DE2: Slot position (0-7)
    DE3_TrafficClass de3;       ///< DE3: Traffic class
    uint8_t de4;                ///< DE4: LQA/signal quality
    uint8_t de5;                ///< DE5: Link quality metric 1
    uint8_t de6;                ///< DE6: Link quality metric 2
    uint8_t de7;                ///< DE7: Reserved (future use)
    uint8_t de8;                ///< DE8: Number of orderwire commands
    DE9_TransactionCode de9;    ///< DE9: Transaction code
    
    DataElements() : de1(0), de2(0), de3(DE3_TrafficClass::CLEAR_VOICE), 
                     de4(0), de5(0), de6(0), de7(0), de8(0), 
                     de9(DE9_TransactionCode::RESERVED_0) {}
};

// ============================================================================
// AQC Message Structures
// ============================================================================

/**
 * \struct AQCCallProbe
 * AQC call probe message (enhanced TO call)
 */
struct AQCCallProbe {
    std::string to_address;     ///< Destination address
    std::string term_address;   ///< Terminator (calling station)
    DataElements de;            ///< Data elements
    uint32_t timestamp_ms;      ///< Reception time
    
    AQCCallProbe() : timestamp_ms(0) {}
};

/**
 * \struct AQCCallHandshake
 * AQC call handshake (enhanced response)
 */
struct AQCCallHandshake {
    std::string to_address;     ///< Original caller
    std::string from_address;   ///< Responding station
    DataElements de;            ///< Data elements
    CRCStatus crc_status;       ///< CRC validation result
    bool ack_this_flag;         ///< Acknowledge this call
    uint8_t slot_position;      ///< Assigned slot (0-7)
    uint32_t timestamp_ms;      ///< Reception time
    
    AQCCallHandshake() : crc_status(CRCStatus::NOT_APPLICABLE), 
                         ack_this_flag(false), slot_position(0), 
                         timestamp_ms(0) {}
};

/**
 * \struct AQCInlink
 * AQC inlink message (link established)
 */
struct AQCInlink {
    std::string to_address;     ///< Destination
    std::string term_address;   ///< Terminator
    DataElements de;            ///< Data elements
    CRCStatus crc_status;       ///< CRC validation result
    bool ack_this_flag;         ///< Acknowledge flag
    bool net_address_flag;      ///< Net call flag
    uint8_t slot_position;      ///< Response slot
    uint32_t timestamp_ms;      ///< Reception time
    
    AQCInlink() : crc_status(CRCStatus::NOT_APPLICABLE), 
                  ack_this_flag(false), net_address_flag(false), 
                  slot_position(0), timestamp_ms(0) {}
};

/**
 * \struct AQCOrderwire
 * AQC orderwire message (AMD - Automatic Message Display)
 */
struct AQCOrderwire {
    std::string message;        ///< Orderwire text
    CRCStatus crc_status;       ///< CRC validation result
    uint16_t calculated_crc;    ///< CRC value
    uint32_t timestamp_ms;      ///< Reception time
    
    AQCOrderwire() : crc_status(CRCStatus::NOT_APPLICABLE), 
                     calculated_crc(0), timestamp_ms(0) {}
};

// ============================================================================
// AQC Parser
// ============================================================================

/**
 * \class AQCParser
 * Parse AQC-enhanced ALE words and extract data elements
 */
class AQCParser {
public:
    AQCParser();
    
    /**
     * Detect if word is AQC-enhanced format
     * \param word Decoded ALE word
     * \return true if AQC format detected
     */
    static bool is_aqc_format(const ALEWord& word);
    
    /**
     * Extract data elements from 21-bit payload
     * Bit mapping per MIL-STD-188-141B AQC spec
     * 
     * \param payload 21-bit payload from word
     * \param de [out] Extracted data elements
     * \return true if extraction successful
     */
    static bool extract_data_elements(uint32_t payload, DataElements& de);
    
    /**
     * Parse AQC call probe message
     * \param words Array of ALE words
     * \param count Number of words
     * \param probe [out] Parsed call probe
     * \return true if parsing successful
     */
    bool parse_call_probe(const ALEWord* words, size_t count, AQCCallProbe& probe);
    
    /**
     * Parse AQC call handshake message
     * \param words Array of ALE words
     * \param count Number of words
     * \param handshake [out] Parsed handshake
     * \return true if parsing successful
     */
    bool parse_call_handshake(const ALEWord* words, size_t count, AQCCallHandshake& handshake);
    
    /**
     * Parse AQC inlink message
     * \param words Array of ALE words
     * \param count Number of words
     * \param inlink [out] Parsed inlink
     * \return true if parsing successful
     */
    bool parse_inlink(const ALEWord* words, size_t count, AQCInlink& inlink);
    
    /**
     * Parse AQC orderwire (AMD) message
     * \param words Array of ALE words
     * \param count Number of words
     * \param orderwire [out] Parsed orderwire
     * \return true if parsing successful
     */
    bool parse_orderwire(const ALEWord* words, size_t count, AQCOrderwire& orderwire);
    
    /**
     * Get traffic class name
     * \param tc Traffic class enum
     * \return Human-readable name
     */
    static const char* traffic_class_name(DE3_TrafficClass tc);
    
    /**
     * Get transaction code name
     * \param code Transaction code enum
     * \return Human-readable name
     */
    static const char* transaction_code_name(DE9_TransactionCode code);
};

// ============================================================================
// CRC Calculation
// ============================================================================

/**
 * \class AQCCRC
 * CRC calculation for AQC orderwire messages
 * Uses CRC-8 or CRC-16 per MIL-STD-188-141B
 */
class AQCCRC {
public:
    /**
     * Calculate CRC-8 for orderwire message
     * Polynomial: 0x07 (x^8 + x^2 + x + 1)
     * 
     * \param data Message data
     * \param length Data length in bytes
     * \return 8-bit CRC value
     */
    static uint8_t calculate_crc8(const uint8_t* data, size_t length);
    
    /**
     * Calculate CRC-16 for orderwire message
     * Polynomial: 0x1021 (CCITT CRC-16)
     * 
     * \param data Message data
     * \param length Data length in bytes
     * \return 16-bit CRC value
     */
    static uint16_t calculate_crc16(const uint8_t* data, size_t length);
    
    /**
     * Validate CRC-8 in message
     * \param data Message with CRC appended
     * \param length Total length (data + CRC)
     * \return true if CRC valid
     */
    static bool validate_crc8(const uint8_t* data, size_t length);
    
    /**
     * Validate CRC-16 in message
     * \param data Message with CRC appended
     * \param length Total length (data + CRC)
     * \return true if CRC valid
     */
    static bool validate_crc16(const uint8_t* data, size_t length);
};

// ============================================================================
// Slot Management
// ============================================================================

/**
 * \class SlotManager
 * Manage slotted response timing for AQC (slots 0-7)
 */
class SlotManager {
public:
    SlotManager();
    
    /**
     * Calculate slot timing
     * \param slot_number Slot position (0-7)
     * \param base_time_ms Base time reference
     * \return Transmission time in milliseconds
     */
    static uint32_t calculate_slot_time(uint8_t slot_number, uint32_t base_time_ms);
    
    /**
     * Assign slot based on address hash
     * Distributes stations across slots to reduce collisions
     * 
     * \param address Station address
     * \return Assigned slot (0-7)
     */
    static uint8_t assign_slot(const std::string& address);
    
    /**
     * Get slot duration in milliseconds
     * \return Slot duration (per MIL-STD-188-141B)
     */
    static uint32_t get_slot_duration_ms();
    
private:
    static constexpr uint32_t SLOT_DURATION_MS = 200;  ///< 200ms per slot
    static constexpr uint8_t NUM_SLOTS = 8;            ///< 8 slots total
};

} // namespace aqc
} // namespace ale
