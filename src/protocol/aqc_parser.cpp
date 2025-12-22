/**
 * \file aqc_parser.cpp
 * \brief Implementation of AQC-ALE parser and data element extraction
 */

#include "aqc_protocol.h"
#include <cstring>
#include <algorithm>

namespace ale {
namespace aqc {

// Traffic class names
static const char* TRAFFIC_CLASS_NAMES[] = {
    "CLEAR_VOICE",          // 0
    "DIGITAL_VOICE",        // 1
    "HFD_VOICE",            // 2
    "RESERVED_3",           // 3
    "SECURE_DIGITAL_VOICE", // 4
    "RESERVED_5",           // 5
    "RESERVED_6",           // 6
    "RESERVED_7",           // 7
    "ALE_MSG",              // 8
    "PSK_MSG",              // 9
    "TONE_39_MSG",          // 10
    "HF_EMAIL",             // 11
    "KY100_ACTIVE",         // 12
    "RESERVED_13",          // 13
    "RESERVED_14",          // 14
    "RESERVED_15"           // 15
};

// Transaction code names
static const char* TRANSACTION_CODE_NAMES[] = {
    "RESERVED_0",           // 0
    "MS_141A",              // 1
    "ACK_LAST",             // 2
    "NAK_LAST",             // 3
    "TERMINATE",            // 4
    "OP_ACKNAK",            // 5
    "AQC_CMD",              // 6
    "RESERVED_7"            // 7
};

AQCParser::AQCParser() {}

bool AQCParser::is_aqc_format(const ALEWord& word) {
    // AQC format detection heuristics:
    // 1. CMD word type with specific payload patterns
    // 2. DATA words with non-ASCII patterns (DE fields instead)
    // 3. Specific bit patterns indicating AQC structure
    
    // For now, detect based on word type and payload analysis
    // Full implementation would use more sophisticated detection
    
    if (word.type == WordType::CMD) {
        // CMD words are often used for AQC signaling
        return true;
    }
    
    // Check if payload contains valid ASCII or DE pattern
    // If all 3 characters are non-printable, likely DE format
    if (word.address[0] < 0x20 || word.address[0] > 0x7E) {
        // Non-ASCII, could be AQC
        return true;
    }
    
    return false;
}

bool AQCParser::extract_data_elements(uint32_t payload, DataElements& de) {
    // Extract data elements from 21-bit payload
    // Bit mapping per MIL-STD-188-141B AQC specification
    
    // NOTE: The exact bit mapping depends on the specific AQC word type
    // and message structure. This is a general implementation based on
    // common AQC DE field layouts observed in MARS-ALE reference code.
    
    // Example bit allocation (21 bits total):
    // Bits 0-2:   DE2 (slot position, 3 bits = 0-7)
    // Bits 3-6:   DE3 (traffic class, 4 bits = 0-15)
    // Bits 7-11:  DE4 (LQA, 5 bits = 0-31)
    // Bits 12-14: DE9 (transaction code, 3 bits = 0-7)
    // Bits 15-17: DE1 (reserved, 3 bits)
    // Bits 18-20: DE8 (orderwire count, 3 bits)
    
    de.de2 = (payload >> 0) & 0x07;         // Bits 0-2: Slot (0-7)
    de.de3 = static_cast<DE3_TrafficClass>((payload >> 3) & 0x0F);  // Bits 3-6: Traffic class
    de.de4 = (payload >> 7) & 0x1F;         // Bits 7-11: LQA (0-31)
    de.de9 = static_cast<DE9_TransactionCode>((payload >> 12) & 0x07); // Bits 12-14: Transaction
    de.de1 = (payload >> 15) & 0x07;        // Bits 15-17: Reserved
    de.de8 = (payload >> 18) & 0x07;        // Bits 18-20: Orderwire count
    
    // DE5, DE6, DE7 would be in additional words or different message types
    de.de5 = 0;
    de.de6 = 0;
    de.de7 = 0;
    
    return true;
}

bool AQCParser::parse_call_probe(const ALEWord* words, size_t count, AQCCallProbe& probe) {
    if (count < 2) {
        return false;  // Need at least TO + TERM words
    }
    
    // Expected structure:
    // Word 0: TO address (could be AQC-enhanced)
    // Word 1: Terminator (FROM)
    
    // Extract TO address
    if (words[0].type == WordType::TO || words[0].type == WordType::TWS) {
        probe.to_address = words[0].address;
        
        // Check if AQC-enhanced
        if (is_aqc_format(words[0])) {
            extract_data_elements(words[0].raw_payload, probe.de);
        }
    } else {
        return false;
    }
    
    // Extract terminator
    if (words[1].type == WordType::FROM || words[1].type == WordType::TIS) {
        probe.term_address = words[1].address;
    } else {
        return false;
    }
    
    // Set timestamp from first word
    probe.timestamp_ms = words[0].timestamp_ms;
    
    return true;
}

bool AQCParser::parse_call_handshake(const ALEWord* words, size_t count, AQCCallHandshake& handshake) {
    if (count < 2) {
        return false;
    }
    
    // Expected structure:
    // Word 0: TO address (original caller)
    // Word 1: FROM address (responding station)
    // Optional: CMD word with DE fields
    
    // Extract addresses
    if (words[0].type == WordType::TO) {
        handshake.to_address = words[0].address;
    } else {
        return false;
    }
    
    if (words[1].type == WordType::FROM || words[1].type == WordType::TIS) {
        handshake.from_address = words[1].address;
        
        // Check for AQC enhancements
        if (is_aqc_format(words[1])) {
            extract_data_elements(words[1].raw_payload, handshake.de);
            handshake.slot_position = handshake.de.de2;
            handshake.ack_this_flag = (handshake.de.de9 == DE9_TransactionCode::ACK_LAST);
        }
    } else {
        return false;
    }
    
    // Check for additional CMD word with CRC
    if (count >= 3 && words[2].type == WordType::CMD) {
        // CRC would be validated here
        handshake.crc_status = CRCStatus::NOT_APPLICABLE;  // Placeholder
    }
    
    handshake.timestamp_ms = words[0].timestamp_ms;
    
    return true;
}

bool AQCParser::parse_inlink(const ALEWord* words, size_t count, AQCInlink& inlink) {
    if (count < 2) {
        return false;
    }
    
    // Expected structure:
    // Word 0: TO address
    // Word 1: Terminator
    // Optional: Additional AQC control words
    
    // Extract TO address
    if (words[0].type == WordType::TO || words[0].type == WordType::TWS) {
        inlink.to_address = words[0].address;
        inlink.net_address_flag = (words[0].type == WordType::TWS);
        
        // Extract AQC data elements
        if (is_aqc_format(words[0])) {
            extract_data_elements(words[0].raw_payload, inlink.de);
            inlink.slot_position = inlink.de.de2;
            inlink.ack_this_flag = (inlink.de.de9 == DE9_TransactionCode::ACK_LAST);
        }
    } else {
        return false;
    }
    
    // Extract terminator
    if (words[1].type == WordType::FROM || words[1].type == WordType::TIS) {
        inlink.term_address = words[1].address;
    } else {
        return false;
    }
    
    // Check for CRC in additional words
    if (count >= 3 && words[2].type == WordType::CMD) {
        inlink.crc_status = CRCStatus::NOT_APPLICABLE;  // Placeholder for CRC validation
    }
    
    inlink.timestamp_ms = words[0].timestamp_ms;
    
    return true;
}

bool AQCParser::parse_orderwire(const ALEWord* words, size_t count, AQCOrderwire& orderwire) {
    if (count < 1) {
        return false;
    }
    
    // Orderwire (AMD) messages use DATA words
    // Concatenate message text from multiple words
    std::string message;
    
    for (size_t i = 0; i < count; i++) {
        if (words[i].type == WordType::DATA) {
            // Append 3 characters from each word
            message.append(words[i].address, 3);
        } else if (words[i].type == WordType::CMD) {
            // CMD word may contain CRC
            // Extract CRC from payload
            uint16_t crc = words[i].raw_payload & 0xFFFF;
            orderwire.calculated_crc = crc;
            
            // Validate CRC against accumulated message
            // (Actual validation would be done by AQCCRC class)
            orderwire.crc_status = CRCStatus::NOT_APPLICABLE;  // Placeholder
        }
    }
    
    // Trim trailing spaces/nulls
    while (!message.empty() && (message.back() == ' ' || message.back() == '\0')) {
        message.pop_back();
    }
    
    orderwire.message = message;
    orderwire.timestamp_ms = words[0].timestamp_ms;
    
    return !message.empty();
}

const char* AQCParser::traffic_class_name(DE3_TrafficClass tc) {
    uint8_t index = static_cast<uint8_t>(tc);
    if (index > 15) index = 15;
    return TRAFFIC_CLASS_NAMES[index];
}

const char* AQCParser::transaction_code_name(DE9_TransactionCode code) {
    uint8_t index = static_cast<uint8_t>(code);
    if (index > 7) index = 7;
    return TRANSACTION_CODE_NAMES[index];
}

// ============================================================================
// CRC Implementation
// ============================================================================

uint8_t AQCCRC::calculate_crc8(const uint8_t* data, size_t length) {
    // CRC-8 polynomial: 0x07 (x^8 + x^2 + x + 1)
    uint8_t crc = 0x00;  // Initial value
    
    for (size_t i = 0; i < length; i++) {
        crc ^= data[i];
        
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x80) {
                crc = (crc << 1) ^ 0x07;
            } else {
                crc = (crc << 1);
            }
        }
    }
    
    return crc;
}

uint16_t AQCCRC::calculate_crc16(const uint8_t* data, size_t length) {
    // CRC-16 CCITT polynomial: 0x1021 (x^16 + x^12 + x^5 + 1)
    uint16_t crc = 0xFFFF;  // Initial value
    
    for (size_t i = 0; i < length; i++) {
        crc ^= (static_cast<uint16_t>(data[i]) << 8);
        
        for (int bit = 0; bit < 8; bit++) {
            if (crc & 0x8000) {
                crc = (crc << 1) ^ 0x1021;
            } else {
                crc = (crc << 1);
            }
        }
    }
    
    return crc;
}

bool AQCCRC::validate_crc8(const uint8_t* data, size_t length) {
    if (length < 1) {
        return false;
    }
    
    // Last byte is CRC
    uint8_t received_crc = data[length - 1];
    uint8_t calculated_crc = calculate_crc8(data, length - 1);
    
    return (received_crc == calculated_crc);
}

bool AQCCRC::validate_crc16(const uint8_t* data, size_t length) {
    if (length < 2) {
        return false;
    }
    
    // Last 2 bytes are CRC (big-endian)
    uint16_t received_crc = (static_cast<uint16_t>(data[length - 2]) << 8) | data[length - 1];
    uint16_t calculated_crc = calculate_crc16(data, length - 2);
    
    return (received_crc == calculated_crc);
}

// ============================================================================
// Slot Manager Implementation
// ============================================================================

SlotManager::SlotManager() {}

uint32_t SlotManager::calculate_slot_time(uint8_t slot_number, uint32_t base_time_ms) {
    if (slot_number >= NUM_SLOTS) {
        slot_number = NUM_SLOTS - 1;
    }
    
    return base_time_ms + (slot_number * SLOT_DURATION_MS);
}

uint8_t SlotManager::assign_slot(const std::string& address) {
    // Hash address to assign slot
    // Simple hash: sum ASCII values mod 8
    uint32_t hash = 0;
    
    for (char ch : address) {
        hash += static_cast<uint32_t>(ch);
    }
    
    return hash % NUM_SLOTS;
}

uint32_t SlotManager::get_slot_duration_ms() {
    return SLOT_DURATION_MS;
}

} // namespace aqc
} // namespace ale
