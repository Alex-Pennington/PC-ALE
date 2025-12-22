/**
 * \file test_aqc_parser.cpp
 * \brief Unit tests for AQC-ALE parser and data element extraction
 */

#include "aqc_protocol.h"
#include "ale_word.h"
#include <cassert>
#include <cstring>
#include <iostream>

using namespace ale;
using namespace ale::aqc;

// Test data element extraction
void test_extract_data_elements() {
    std::cout << "Test: Extract Data Elements from 21-bit payload...\n";
    
    // Create test payload with known DE values
    // Bit layout:
    // Bits 0-2:   DE2 = 5 (slot 5)
    // Bits 3-6:   DE3 = 9 (PSK_MSG)
    // Bits 7-11:  DE4 = 20 (LQA)
    // Bits 12-14: DE9 = 2 (ACK_LAST)
    // Bits 15-17: DE1 = 3
    // Bits 18-20: DE8 = 1 (1 orderwire command)
    
    uint32_t payload = 0;
    payload |= (5 << 0);   // DE2: slot 5
    payload |= (9 << 3);   // DE3: PSK_MSG
    payload |= (20 << 7);  // DE4: LQA 20
    payload |= (2 << 12);  // DE9: ACK_LAST
    payload |= (3 << 15);  // DE1: 3
    payload |= (1 << 18);  // DE8: 1 command
    
    DataElements de;
    bool result = AQCParser::extract_data_elements(payload, de);
    
    assert(result == true);
    assert(de.de2 == 5);
    assert(de.de3 == DE3_TrafficClass::PSK_MSG);
    assert(de.de4 == 20);
    assert(de.de9 == DE9_TransactionCode::ACK_LAST);
    assert(de.de1 == 3);
    assert(de.de8 == 1);
    
    std::cout << "  ✓ DE2 (slot): " << static_cast<int>(de.de2) << "\n";
    std::cout << "  ✓ DE3 (traffic): " << AQCParser::traffic_class_name(de.de3) << "\n";
    std::cout << "  ✓ DE4 (LQA): " << static_cast<int>(de.de4) << "\n";
    std::cout << "  ✓ DE9 (transaction): " << AQCParser::transaction_code_name(de.de9) << "\n";
    std::cout << "  PASSED\n\n";
}

// Test traffic class names
void test_traffic_class_names() {
    std::cout << "Test: Traffic Class Names...\n";
    
    assert(strcmp(AQCParser::traffic_class_name(DE3_TrafficClass::CLEAR_VOICE), "CLEAR_VOICE") == 0);
    assert(strcmp(AQCParser::traffic_class_name(DE3_TrafficClass::DIGITAL_VOICE), "DIGITAL_VOICE") == 0);
    assert(strcmp(AQCParser::traffic_class_name(DE3_TrafficClass::PSK_MSG), "PSK_MSG") == 0);
    assert(strcmp(AQCParser::traffic_class_name(DE3_TrafficClass::HF_EMAIL), "HF_EMAIL") == 0);
    
    std::cout << "  ✓ All traffic class names correct\n";
    std::cout << "  PASSED\n\n";
}

// Test transaction code names
void test_transaction_code_names() {
    std::cout << "Test: Transaction Code Names...\n";
    
    assert(strcmp(AQCParser::transaction_code_name(DE9_TransactionCode::MS_141A), "MS_141A") == 0);
    assert(strcmp(AQCParser::transaction_code_name(DE9_TransactionCode::ACK_LAST), "ACK_LAST") == 0);
    assert(strcmp(AQCParser::transaction_code_name(DE9_TransactionCode::NAK_LAST), "NAK_LAST") == 0);
    assert(strcmp(AQCParser::transaction_code_name(DE9_TransactionCode::TERMINATE), "TERMINATE") == 0);
    
    std::cout << "  ✓ All transaction code names correct\n";
    std::cout << "  PASSED\n\n";
}

// Test AQC call probe parsing
void test_parse_call_probe() {
    std::cout << "Test: Parse AQC Call Probe...\n";
    
    // Create mock ALE words
    ALEWord words[2];
    
    // Word 0: TO address
    words[0].type = WordType::TO;
    strcpy(words[0].address, "ABC");
    words[0].raw_payload = 0x012345;  // Some payload
    words[0].timestamp_ms = 1000;
    words[0].valid = true;
    
    // Word 1: FROM (terminator)
    words[1].type = WordType::FROM;
    strcpy(words[1].address, "XYZ");
    words[1].raw_payload = 0;
    words[1].timestamp_ms = 1100;
    words[1].valid = true;
    
    AQCParser parser;
    AQCCallProbe probe;
    bool result = parser.parse_call_probe(words, 2, probe);
    
    assert(result == true);
    assert(probe.to_address == "ABC");
    assert(probe.term_address == "XYZ");
    assert(probe.timestamp_ms == 1000);
    
    std::cout << "  ✓ TO address: " << probe.to_address << "\n";
    std::cout << "  ✓ TERM address: " << probe.term_address << "\n";
    std::cout << "  PASSED\n\n";
}

// Test AQC call handshake parsing
void test_parse_call_handshake() {
    std::cout << "Test: Parse AQC Call Handshake...\n";
    
    ALEWord words[2];
    
    // Word 0: TO (original caller)
    words[0].type = WordType::TO;
    strcpy(words[0].address, "ABC");
    words[0].raw_payload = 0;
    words[0].timestamp_ms = 2000;
    words[0].valid = true;
    
    // Word 1: FROM (responding station) with AQC DE
    words[1].type = WordType::FROM;
    strcpy(words[1].address, "XYZ");
    
    // Build payload with DE fields
    uint32_t payload = 0;
    payload |= (3 << 0);   // DE2: slot 3
    payload |= (1 << 3);   // DE3: DIGITAL_VOICE
    payload |= (15 << 7);  // DE4: LQA 15
    payload |= (2 << 12);  // DE9: ACK_LAST
    
    words[1].raw_payload = payload;
    words[1].timestamp_ms = 2100;
    words[1].valid = true;
    
    AQCParser parser;
    AQCCallHandshake handshake;
    bool result = parser.parse_call_handshake(words, 2, handshake);
    
    assert(result == true);
    assert(handshake.to_address == "ABC");
    assert(handshake.from_address == "XYZ");
    
    std::cout << "  ✓ TO address: " << handshake.to_address << "\n";
    std::cout << "  ✓ FROM address: " << handshake.from_address << "\n";
    std::cout << "  ✓ Slot: " << static_cast<int>(handshake.slot_position) << "\n";
    std::cout << "  PASSED\n\n";
}

// Test AQC inlink parsing
void test_parse_inlink() {
    std::cout << "Test: Parse AQC Inlink...\n";
    
    ALEWord words[2];
    
    // Word 0: TWS (net call)
    words[0].type = WordType::TWS;
    strcpy(words[0].address, "NET");
    
    // Payload with DE fields
    uint32_t payload = 0;
    payload |= (0 << 0);   // DE2: slot 0
    payload |= (8 << 3);   // DE3: ALE_MSG
    payload |= (25 << 7);  // DE4: LQA 25
    payload |= (1 << 12);  // DE9: MS_141A
    
    words[0].raw_payload = payload;
    words[0].timestamp_ms = 3000;
    words[0].valid = true;
    
    // Word 1: FROM (terminator)
    words[1].type = WordType::FROM;
    strcpy(words[1].address, "STA");
    words[1].raw_payload = 0;
    words[1].timestamp_ms = 3100;
    words[1].valid = true;
    
    AQCParser parser;
    AQCInlink inlink;
    bool result = parser.parse_inlink(words, 2, inlink);
    
    assert(result == true);
    assert(inlink.to_address == "NET");
    assert(inlink.term_address == "STA");
    assert(inlink.net_address_flag == true);
    
    std::cout << "  ✓ TO address: " << inlink.to_address << "\n";
    std::cout << "  ✓ TERM address: " << inlink.term_address << "\n";
    std::cout << "  ✓ Net call: " << (inlink.net_address_flag ? "YES" : "NO") << "\n";
    std::cout << "  PASSED\n\n";
}

// Test orderwire message parsing
void test_parse_orderwire() {
    std::cout << "Test: Parse AQC Orderwire (AMD)...\n";
    
    // Create multi-word orderwire message
    ALEWord words[3];
    
    // Word 0: DATA "HEL"
    words[0].type = WordType::DATA;
    strcpy(words[0].address, "HEL");
    words[0].timestamp_ms = 4000;
    words[0].valid = true;
    
    // Word 1: DATA "LO "
    words[1].type = WordType::DATA;
    strcpy(words[1].address, "LO ");
    words[1].timestamp_ms = 4100;
    words[1].valid = true;
    
    // Word 2: CMD with CRC
    words[2].type = WordType::CMD;
    strcpy(words[2].address, "CRC");
    words[2].raw_payload = 0xABCD;  // Mock CRC
    words[2].timestamp_ms = 4200;
    words[2].valid = true;
    
    AQCParser parser;
    AQCOrderwire orderwire;
    bool result = parser.parse_orderwire(words, 3, orderwire);
    
    assert(result == true);
    assert(orderwire.message == "HELLO");
    assert(orderwire.calculated_crc == 0xABCD);
    
    std::cout << "  ✓ Message: \"" << orderwire.message << "\"\n";
    std::cout << "  ✓ CRC: 0x" << std::hex << orderwire.calculated_crc << std::dec << "\n";
    std::cout << "  PASSED\n\n";
}

// Test slot assignment
void test_slot_assignment() {
    std::cout << "Test: Slot Assignment...\n";
    
    // Test consistent slot assignment
    uint8_t slot1 = SlotManager::assign_slot("ABC");
    uint8_t slot2 = SlotManager::assign_slot("ABC");
    assert(slot1 == slot2);  // Same address gets same slot
    
    // Test slot range
    uint8_t slot3 = SlotManager::assign_slot("XYZ123");
    assert(slot3 < 8);  // Slot in valid range
    
    std::cout << "  ✓ Slot for 'ABC': " << static_cast<int>(slot1) << "\n";
    std::cout << "  ✓ Slot for 'XYZ123': " << static_cast<int>(slot3) << "\n";
    std::cout << "  PASSED\n\n";
}

// Test slot timing calculation
void test_slot_timing() {
    std::cout << "Test: Slot Timing Calculation...\n";
    
    uint32_t base_time = 1000;
    uint32_t slot_duration = SlotManager::get_slot_duration_ms();
    
    assert(slot_duration == 200);  // 200ms per slot
    
    uint32_t time_slot0 = SlotManager::calculate_slot_time(0, base_time);
    uint32_t time_slot3 = SlotManager::calculate_slot_time(3, base_time);
    uint32_t time_slot7 = SlotManager::calculate_slot_time(7, base_time);
    
    assert(time_slot0 == 1000);  // Base + 0*200
    assert(time_slot3 == 1600);  // Base + 3*200
    assert(time_slot7 == 2400);  // Base + 7*200
    
    std::cout << "  ✓ Slot 0 time: " << time_slot0 << " ms\n";
    std::cout << "  ✓ Slot 3 time: " << time_slot3 << " ms\n";
    std::cout << "  ✓ Slot 7 time: " << time_slot7 << " ms\n";
    std::cout << "  PASSED\n\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "AQC-ALE Parser Tests\n";
    std::cout << "========================================\n\n";
    
    try {
        test_extract_data_elements();
        test_traffic_class_names();
        test_transaction_code_names();
        test_parse_call_probe();
        test_parse_call_handshake();
        test_parse_inlink();
        test_parse_orderwire();
        test_slot_assignment();
        test_slot_timing();
        
        std::cout << "========================================\n";
        std::cout << "All AQC Parser tests PASSED! ✓\n";
        std::cout << "========================================\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test FAILED with exception: " << e.what() << "\n";
        return 1;
    }
}
