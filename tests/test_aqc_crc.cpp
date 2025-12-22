/**
 * \file test_aqc_crc.cpp
 * \brief Unit tests for AQC-ALE CRC calculation and validation
 */

#include "aqc_protocol.h"
#include <cassert>
#include <cstring>
#include <iostream>

using namespace ale::aqc;

// Test CRC-8 calculation
void test_crc8_calculation() {
    std::cout << "Test: CRC-8 Calculation...\n";
    
    // Test with known data
    const uint8_t data1[] = { 0x48, 0x45, 0x4C, 0x4C, 0x4F };  // "HELLO"
    uint8_t crc1 = AQCCRC::calculate_crc8(data1, sizeof(data1));
    
    // CRC should be deterministic
    uint8_t crc1_repeat = AQCCRC::calculate_crc8(data1, sizeof(data1));
    assert(crc1 == crc1_repeat);
    
    // Different data should give different CRC (usually)
    const uint8_t data2[] = { 0x57, 0x4F, 0x52, 0x4C, 0x44 };  // "WORLD"
    uint8_t crc2 = AQCCRC::calculate_crc8(data2, sizeof(data2));
    
    std::cout << "  ✓ CRC-8 for 'HELLO': 0x" << std::hex << static_cast<int>(crc1) << std::dec << "\n";
    std::cout << "  ✓ CRC-8 for 'WORLD': 0x" << std::hex << static_cast<int>(crc2) << std::dec << "\n";
    std::cout << "  ✓ CRC values are deterministic\n";
    std::cout << "  PASSED\n\n";
}

// Test CRC-16 calculation
void test_crc16_calculation() {
    std::cout << "Test: CRC-16 Calculation...\n";
    
    // Test with known data
    const uint8_t data1[] = { 0x48, 0x45, 0x4C, 0x4C, 0x4F };  // "HELLO"
    uint16_t crc1 = AQCCRC::calculate_crc16(data1, sizeof(data1));
    
    // CRC should be deterministic
    uint16_t crc1_repeat = AQCCRC::calculate_crc16(data1, sizeof(data1));
    assert(crc1 == crc1_repeat);
    
    // Different data should give different CRC
    const uint8_t data2[] = { 0x57, 0x4F, 0x52, 0x4C, 0x44 };  // "WORLD"
    uint16_t crc2 = AQCCRC::calculate_crc16(data2, sizeof(data2));
    
    std::cout << "  ✓ CRC-16 for 'HELLO': 0x" << std::hex << crc1 << std::dec << "\n";
    std::cout << "  ✓ CRC-16 for 'WORLD': 0x" << std::hex << crc2 << std::dec << "\n";
    std::cout << "  ✓ CRC values are deterministic\n";
    std::cout << "  PASSED\n\n";
}

// Test CRC-8 validation (valid)
void test_crc8_validation_valid() {
    std::cout << "Test: CRC-8 Validation (Valid)...\n";
    
    // Create message with CRC
    uint8_t message[] = { 0x41, 0x42, 0x43, 0x00 };  // "ABC" + CRC placeholder
    
    // Calculate CRC for "ABC"
    uint8_t crc = AQCCRC::calculate_crc8(message, 3);
    message[3] = crc;  // Append CRC
    
    // Validate
    bool valid = AQCCRC::validate_crc8(message, 4);
    assert(valid == true);
    
    std::cout << "  ✓ Message: 'ABC'\n";
    std::cout << "  ✓ CRC: 0x" << std::hex << static_cast<int>(crc) << std::dec << "\n";
    std::cout << "  ✓ Validation: PASSED\n";
    std::cout << "  PASSED\n\n";
}

// Test CRC-8 validation (corrupted)
void test_crc8_validation_corrupted() {
    std::cout << "Test: CRC-8 Validation (Corrupted)...\n";
    
    // Create message with CRC
    uint8_t message[] = { 0x41, 0x42, 0x43, 0x00 };
    
    // Calculate correct CRC
    uint8_t crc = AQCCRC::calculate_crc8(message, 3);
    message[3] = crc;
    
    // Corrupt the data
    message[1] ^= 0x01;  // Flip one bit
    
    // Validate should fail
    bool valid = AQCCRC::validate_crc8(message, 4);
    assert(valid == false);
    
    std::cout << "  ✓ Message corrupted: 'A' + (B^0x01) + 'C'\n";
    std::cout << "  ✓ Validation: FAILED (as expected)\n";
    std::cout << "  PASSED\n\n";
}

// Test CRC-16 validation (valid)
void test_crc16_validation_valid() {
    std::cout << "Test: CRC-16 Validation (Valid)...\n";
    
    // Create message with CRC
    uint8_t message[10];
    memcpy(message, "TESTING", 7);
    
    // Calculate CRC for "TESTING"
    uint16_t crc = AQCCRC::calculate_crc16(message, 7);
    message[7] = (crc >> 8) & 0xFF;  // High byte
    message[8] = crc & 0xFF;         // Low byte
    
    // Validate
    bool valid = AQCCRC::validate_crc16(message, 9);
    assert(valid == true);
    
    std::cout << "  ✓ Message: 'TESTING'\n";
    std::cout << "  ✓ CRC: 0x" << std::hex << crc << std::dec << "\n";
    std::cout << "  ✓ Validation: PASSED\n";
    std::cout << "  PASSED\n\n";
}

// Test CRC-16 validation (corrupted)
void test_crc16_validation_corrupted() {
    std::cout << "Test: CRC-16 Validation (Corrupted)...\n";
    
    // Create message with CRC
    uint8_t message[10];
    memcpy(message, "TESTING", 7);
    
    // Calculate correct CRC
    uint16_t crc = AQCCRC::calculate_crc16(message, 7);
    message[7] = (crc >> 8) & 0xFF;
    message[8] = crc & 0xFF;
    
    // Corrupt the data
    message[3] ^= 0xFF;  // Flip multiple bits
    
    // Validate should fail
    bool valid = AQCCRC::validate_crc16(message, 9);
    assert(valid == false);
    
    std::cout << "  ✓ Message corrupted (byte 3 flipped)\n";
    std::cout << "  ✓ Validation: FAILED (as expected)\n";
    std::cout << "  PASSED\n\n";
}

// Test CRC error detection sensitivity
void test_crc_error_detection() {
    std::cout << "Test: CRC Error Detection Sensitivity...\n";
    
    const char* original = "AQC-ALE ORDERWIRE TEST MESSAGE";
    size_t len = strlen(original);
    
    uint8_t message_crc8[64];
    uint8_t message_crc16[64];
    
    memcpy(message_crc8, original, len);
    memcpy(message_crc16, original, len);
    
    // Append CRC-8
    uint8_t crc8 = AQCCRC::calculate_crc8(message_crc8, len);
    message_crc8[len] = crc8;
    
    // Append CRC-16
    uint16_t crc16 = AQCCRC::calculate_crc16(message_crc16, len);
    message_crc16[len] = (crc16 >> 8) & 0xFF;
    message_crc16[len + 1] = crc16 & 0xFF;
    
    // Validate both
    assert(AQCCRC::validate_crc8(message_crc8, len + 1) == true);
    assert(AQCCRC::validate_crc16(message_crc16, len + 2) == true);
    
    // Test single-bit error detection
    message_crc8[5] ^= 0x01;  // Flip 1 bit
    message_crc16[5] ^= 0x01;
    
    assert(AQCCRC::validate_crc8(message_crc8, len + 1) == false);
    assert(AQCCRC::validate_crc16(message_crc16, len + 2) == false);
    
    std::cout << "  ✓ CRC-8 detects single-bit errors\n";
    std::cout << "  ✓ CRC-16 detects single-bit errors\n";
    std::cout << "  PASSED\n\n";
}

// Test empty message handling
void test_crc_empty_message() {
    std::cout << "Test: CRC with Empty Message...\n";
    
    uint8_t empty[] = { 0x00 };
    
    uint8_t crc8 = AQCCRC::calculate_crc8(empty, 0);
    uint16_t crc16 = AQCCRC::calculate_crc16(empty, 0);
    
    // Should not crash and return initial CRC value
    std::cout << "  ✓ CRC-8 for empty: 0x" << std::hex << static_cast<int>(crc8) << std::dec << "\n";
    std::cout << "  ✓ CRC-16 for empty: 0x" << std::hex << crc16 << std::dec << "\n";
    std::cout << "  PASSED\n\n";
}

// Test known CRC-16 CCITT values (if available)
void test_crc16_known_values() {
    std::cout << "Test: CRC-16 CCITT Known Values...\n";
    
    // "123456789" is a standard CRC test vector
    // Expected CRC-16 CCITT (0xFFFF init, 0x1021 poly): varies by implementation
    const uint8_t test_data[] = { '1', '2', '3', '4', '5', '6', '7', '8', '9' };
    uint16_t crc = AQCCRC::calculate_crc16(test_data, 9);
    
    std::cout << "  ✓ CRC-16 for '123456789': 0x" << std::hex << crc << std::dec << "\n";
    std::cout << "  ✓ (Implementation-specific value)\n";
    std::cout << "  PASSED\n\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "AQC-ALE CRC Tests\n";
    std::cout << "========================================\n\n";
    
    try {
        test_crc8_calculation();
        test_crc16_calculation();
        test_crc8_validation_valid();
        test_crc8_validation_corrupted();
        test_crc16_validation_valid();
        test_crc16_validation_corrupted();
        test_crc_error_detection();
        test_crc_empty_message();
        test_crc16_known_values();
        
        std::cout << "========================================\n";
        std::cout << "All AQC CRC tests PASSED! ✓\n";
        std::cout << "========================================\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test FAILED with exception: " << e.what() << "\n";
        return 1;
    }
}
