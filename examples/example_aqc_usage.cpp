/**
 * \file example_aqc_usage.cpp
 * \brief Example demonstrating AQC-ALE protocol extensions
 * 
 * Shows how to:
 * - Extract data elements from ALE words
 * - Parse AQC-enhanced messages
 * - Calculate and validate CRC for orderwire
 * - Use slotted response mechanism
 */

#include "aqc_protocol.h"
#include "ale_word.h"
#include <iostream>
#include <iomanip>
#include <cstring>

using namespace ale;
using namespace ale::aqc;

void print_separator() {
    std::cout << "----------------------------------------\n";
}

// Example 1: Parse AQC call with data elements
void example_aqc_call() {
    std::cout << "Example 1: AQC-Enhanced Call\n";
    print_separator();
    
    // Simulate received ALE words
    ALEWord words[2];
    
    // Word 0: TO address with AQC data elements
    words[0].type = WordType::TO;
    strcpy(words[0].address, "ABC");
    
    // Build payload with DE fields
    uint32_t payload = 0;
    payload |= (3 << 0);   // DE2: Slot 3
    payload |= (9 << 3);   // DE3: PSK_MSG (PSK data mode)
    payload |= (25 << 7);  // DE4: LQA 25 (good signal)
    payload |= (2 << 12);  // DE9: ACK_LAST
    payload |= (1 << 15);  // DE1: Reserved
    payload |= (0 << 18);  // DE8: No orderwire commands
    
    words[0].raw_payload = payload;
    words[0].timestamp_ms = 1000;
    words[0].valid = true;
    
    // Word 1: FROM address (calling station)
    words[1].type = WordType::FROM;
    strcpy(words[1].address, "XYZ");
    words[1].raw_payload = 0;
    words[1].timestamp_ms = 1100;
    words[1].valid = true;
    
    // Parse the call
    AQCParser parser;
    AQCCallProbe probe;
    
    if (parser.parse_call_probe(words, 2, probe)) {
        std::cout << "Call detected:\n";
        std::cout << "  TO: " << probe.to_address << "\n";
        std::cout << "  FROM: " << probe.term_address << "\n";
        std::cout << "  Slot: " << (int)probe.de.de2 << "\n";
        std::cout << "  Traffic: " << 
            AQCParser::traffic_class_name(probe.de.de3) << "\n";
        std::cout << "  LQA: " << (int)probe.de.de4 << " (0-31 scale)\n";
        std::cout << "  Transaction: " << 
            AQCParser::transaction_code_name(probe.de.de9) << "\n";
    }
    
    std::cout << "\n";
}

// Example 2: Data element extraction
void example_data_elements() {
    std::cout << "Example 2: Data Element Extraction\n";
    print_separator();
    
    // Create a payload with known values
    uint32_t payload = 0;
    payload |= (5 << 0);   // DE2: Slot 5
    payload |= (11 << 3);  // DE3: HF_EMAIL
    payload |= (18 << 7);  // DE4: LQA 18
    payload |= (1 << 12);  // DE9: MS_141A
    
    DataElements de;
    AQCParser::extract_data_elements(payload, de);
    
    std::cout << "Extracted from 21-bit payload (0x" << std::hex 
              << payload << std::dec << "):\n";
    std::cout << "  DE2 (Slot): " << (int)de.de2 << "\n";
    std::cout << "  DE3 (Traffic): " << 
        AQCParser::traffic_class_name(de.de3) << "\n";
    std::cout << "  DE4 (LQA): " << (int)de.de4 << "\n";
    std::cout << "  DE9 (Transaction): " << 
        AQCParser::transaction_code_name(de.de9) << "\n";
    
    std::cout << "\n";
}

// Example 3: CRC calculation and validation
void example_crc() {
    std::cout << "Example 3: CRC Protection for Orderwire\n";
    print_separator();
    
    // Orderwire message
    const char* message = "HELLO STATION ABC";
    size_t msg_len = strlen(message);
    
    // Calculate CRC-8
    uint8_t crc8 = AQCCRC::calculate_crc8(
        (const uint8_t*)message, 
        msg_len
    );
    
    // Calculate CRC-16
    uint16_t crc16 = AQCCRC::calculate_crc16(
        (const uint8_t*)message, 
        msg_len
    );
    
    std::cout << "Message: \"" << message << "\"\n";
    std::cout << "  CRC-8:  0x" << std::hex << (int)crc8 << std::dec << "\n";
    std::cout << "  CRC-16: 0x" << std::hex << crc16 << std::dec << "\n";
    
    // Create message with CRC appended
    uint8_t msg_with_crc8[64];
    memcpy(msg_with_crc8, message, msg_len);
    msg_with_crc8[msg_len] = crc8;
    
    uint8_t msg_with_crc16[64];
    memcpy(msg_with_crc16, message, msg_len);
    msg_with_crc16[msg_len] = (crc16 >> 8) & 0xFF;
    msg_with_crc16[msg_len + 1] = crc16 & 0xFF;
    
    // Validate
    bool valid8 = AQCCRC::validate_crc8(msg_with_crc8, msg_len + 1);
    bool valid16 = AQCCRC::validate_crc16(msg_with_crc16, msg_len + 2);
    
    std::cout << "  CRC-8 validation: " << (valid8 ? "PASS" : "FAIL") << "\n";
    std::cout << "  CRC-16 validation: " << (valid16 ? "PASS" : "FAIL") << "\n";
    
    // Simulate corruption
    msg_with_crc16[5] ^= 0x01;  // Flip 1 bit
    bool corrupted = AQCCRC::validate_crc16(msg_with_crc16, msg_len + 2);
    std::cout << "  After corruption: " << (corrupted ? "PASS" : "FAIL (detected!)") << "\n";
    
    std::cout << "\n";
}

// Example 4: Slotted response
void example_slots() {
    std::cout << "Example 4: Slotted Response Mechanism\n";
    print_separator();
    
    // Simulate net call with multiple stations
    const char* stations[] = { "STA1", "STA2", "STA3", "ABC", "XYZ", "NET1", "NET2", "BASE" };
    size_t num_stations = sizeof(stations) / sizeof(stations[0]);
    
    std::cout << "Net call response slots (200ms per slot):\n";
    
    uint32_t base_time = 10000;  // Call received at 10000 ms
    
    for (size_t i = 0; i < num_stations; i++) {
        // Assign slot based on address
        uint8_t slot = SlotManager::assign_slot(stations[i]);
        
        // Calculate response time
        uint32_t response_time = SlotManager::calculate_slot_time(slot, base_time);
        
        std::cout << "  " << stations[i] << " -> Slot " << (int)slot 
                  << " @ " << response_time << " ms\n";
    }
    
    std::cout << "\nSlot timing reduces collision probability!\n";
    std::cout << "\n";
}

// Example 5: Traffic class identification
void example_traffic_classes() {
    std::cout << "Example 5: Traffic Class Identification\n";
    print_separator();
    
    std::cout << "Supported traffic classes:\n";
    
    DE3_TrafficClass classes[] = {
        DE3_TrafficClass::CLEAR_VOICE,
        DE3_TrafficClass::DIGITAL_VOICE,
        DE3_TrafficClass::SECURE_DIGITAL_VOICE,
        DE3_TrafficClass::ALE_MSG,
        DE3_TrafficClass::PSK_MSG,
        DE3_TrafficClass::TONE_39_MSG,
        DE3_TrafficClass::HF_EMAIL
    };
    
    for (size_t i = 0; i < sizeof(classes) / sizeof(classes[0]); i++) {
        std::cout << "  " << std::setw(2) << (int)classes[i] 
                  << ": " << AQCParser::traffic_class_name(classes[i]) << "\n";
    }
    
    std::cout << "\n";
}

// Example 6: Transaction codes
void example_transaction_codes() {
    std::cout << "Example 6: Transaction Codes\n";
    print_separator();
    
    std::cout << "Available transaction codes:\n";
    
    DE9_TransactionCode codes[] = {
        DE9_TransactionCode::MS_141A,
        DE9_TransactionCode::ACK_LAST,
        DE9_TransactionCode::NAK_LAST,
        DE9_TransactionCode::TERMINATE,
        DE9_TransactionCode::OP_ACKNAK,
        DE9_TransactionCode::AQC_CMD
    };
    
    for (size_t i = 0; i < sizeof(codes) / sizeof(codes[0]); i++) {
        std::cout << "  " << std::setw(2) << (int)codes[i] 
                  << ": " << AQCParser::transaction_code_name(codes[i]) << "\n";
    }
    
    std::cout << "\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "AQC-ALE Protocol Extensions - Examples\n";
    std::cout << "========================================\n\n";
    
    std::cout << "KEY FINDING: AQC-ALE uses the SAME 8-FSK modem as standard 2G ALE.\n";
    std::cout << "This is a PROTOCOL layer enhancement, not a different physical layer.\n\n";
    
    example_aqc_call();
    example_data_elements();
    example_crc();
    example_slots();
    example_traffic_classes();
    example_transaction_codes();
    
    std::cout << "========================================\n";
    std::cout << "All examples complete!\n";
    std::cout << "========================================\n";
    
    return 0;
}
