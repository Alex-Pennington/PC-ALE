/**
 * \file test_fs1052_frames.cpp
 * \brief Unit tests for FS-1052 frame formatting and parsing
 */

#include "fs1052_protocol.h"
#include <cassert>
#include <cstring>
#include <iostream>

using namespace fs1052;

// Test CRC-32 calculation
void test_crc32_calculation() {
    std::cout << "Test: CRC-32 Calculation...\n";
    
    // Test with known data
    const uint8_t data1[] = "TEST DATA FOR CRC";
    uint32_t crc1 = FrameFormatter::calculate_crc32(data1, strlen((const char*)data1));
    
    // CRC should be deterministic
    uint32_t crc1_repeat = FrameFormatter::calculate_crc32(data1, strlen((const char*)data1));
    assert(crc1 == crc1_repeat);
    
    // Different data should give different CRC
    const uint8_t data2[] = "DIFFERENT DATA";
    uint32_t crc2 = FrameFormatter::calculate_crc32(data2, strlen((const char*)data2));
    assert(crc1 != crc2);
    
    std::cout << "  ✓ CRC-32 for '" << data1 << "': 0x" << std::hex << crc1 << std::dec << "\n";
    std::cout << "  ✓ CRC-32 for '" << data2 << "': 0x" << std::hex << crc2 << std::dec << "\n";
    std::cout << "  ✓ CRC values are deterministic\n";
    std::cout << "  PASSED\n\n";
}

// Test control frame formatting
void test_control_frame_format() {
    std::cout << "Test: Control Frame Formatting...\n";
    
    ControlFrame frame;
    frame.protocol_version = PROTOCOL_VERSION;
    frame.arq_mode = ARQMode::VARIABLE_ARQ;
    frame.neg_mode = NegotiationMode::CHANGES_ONLY;
    frame.address_mode = AddressMode::SHORT_2_BYTE;
    frame.frame_type = FrameType::T1_CONTROL;
    
    // Set addresses
    frame.src_address_length = 2;
    frame.src_address[0] = 'A';
    frame.src_address[1] = 'B';
    frame.des_address_length = 2;
    frame.des_address[0] = 'X';
    frame.des_address[1] = 'Y';
    
    // Link state
    frame.link_state = LinkState::LINK_UP;
    frame.link_timeout = 30;
    
    // ACK/NAK
    frame.ack_nak_type = AckNakType::NULL_ACK;
    
    uint8_t buffer[256];
    int length = FrameFormatter::format_control_frame(frame, buffer, sizeof(buffer));
    
    assert(length > 0);
    assert(length < 256);
    
    // Verify header byte
    assert((buffer[0] & 0x01) != 0);  // Sync mismatch bit
    assert((buffer[0] & 0x02) != 0);  // Control frame bit
    
    std::cout << "  ✓ Frame formatted: " << length << " bytes\n";
    std::cout << "  ✓ Header byte: 0x" << std::hex << (int)buffer[0] << std::dec << "\n";
    std::cout << "  PASSED\n\n";
}

// Test data frame formatting
void test_data_frame_format() {
    std::cout << "Test: Data Frame Formatting...\n";
    
    DataFrame frame;
    frame.data_rate_format = DataRateFormat::ABSOLUTE;
    frame.data_rate = static_cast<uint8_t>(DataRate::BPS_2400);
    frame.interleaver_length = InterleaverLength::LONG;
    frame.sequence_number = 42;
    frame.msg_byte_offset = 1024;
    
    // Add test data
    const char* test_data = "Hello, FS-1052!";
    frame.data_length = strlen(test_data);
    memcpy(frame.data, test_data, frame.data_length);
    
    uint8_t buffer[1200];
    int length = FrameFormatter::format_data_frame(frame, buffer, sizeof(buffer));
    
    assert(length > 0);
    assert(length == 9 + frame.data_length + 4);  // Header + data + CRC
    
    // Verify header
    assert((buffer[0] & 0x01) != 0);  // Sync mismatch bit
    assert((buffer[0] & 0x02) == 0);  // Data frame (not control)
    
    // Verify sequence number
    assert(buffer[2] == 42);
    
    std::cout << "  ✓ Frame formatted: " << length << " bytes\n";
    std::cout << "  ✓ Sequence number: " << (int)buffer[2] << "\n";
    std::cout << "  ✓ Payload: " << frame.data_length << " bytes\n";
    std::cout << "  PASSED\n\n";
}

// Test control frame round-trip (format + parse)
void test_control_frame_roundtrip() {
    std::cout << "Test: Control Frame Round-Trip...\n";
    
    ControlFrame original;
    original.protocol_version = PROTOCOL_VERSION;
    original.arq_mode = ARQMode::VARIABLE_ARQ;
    original.neg_mode = NegotiationMode::EVERY_TIME;
    original.address_mode = AddressMode::SHORT_2_BYTE;
    
    original.src_address_length = 2;
    original.src_address[0] = 'S';
    original.src_address[1] = 'T';
    original.des_address_length = 2;
    original.des_address[0] = 'D';
    original.des_address[1] = 'E';
    
    original.link_state = LinkState::CALL_ACK;
    original.link_timeout = 60;
    original.ack_nak_type = AckNakType::DATA_ACK;
    
    // Format
    uint8_t buffer[256];
    int length = FrameFormatter::format_control_frame(original, buffer, sizeof(buffer));
    assert(length > 0);
    
    // Parse
    ControlFrame parsed;
    bool success = FrameParser::parse_control_frame(buffer, length, parsed);
    assert(success);
    
    // Verify
    assert(parsed.protocol_version == original.protocol_version);
    assert(parsed.arq_mode == original.arq_mode);
    assert(parsed.neg_mode == original.neg_mode);
    assert(parsed.address_mode == original.address_mode);
    assert(parsed.src_address[0] == original.src_address[0]);
    assert(parsed.src_address[1] == original.src_address[1]);
    assert(parsed.des_address[0] == original.des_address[0]);
    assert(parsed.des_address[1] == original.des_address[1]);
    assert(parsed.link_state == original.link_state);
    assert(parsed.link_timeout == original.link_timeout);
    
    std::cout << "  ✓ Format + Parse successful\n";
    std::cout << "  ✓ All fields match\n";
    std::cout << "  PASSED\n\n";
}

// Test data frame round-trip
void test_data_frame_roundtrip() {
    std::cout << "Test: Data Frame Round-Trip...\n";
    
    DataFrame original;
    original.data_rate_format = DataRateFormat::ABSOLUTE;
    original.data_rate = static_cast<uint8_t>(DataRate::BPS_1200);
    original.interleaver_length = InterleaverLength::SHORT;
    original.sequence_number = 123;
    original.msg_byte_offset = 4096;
    
    const char* test_data = "Round-trip test data for FS-1052 protocol";
    original.data_length = strlen(test_data);
    memcpy(original.data, test_data, original.data_length);
    
    // Format
    uint8_t buffer[1200];
    int length = FrameFormatter::format_data_frame(original, buffer, sizeof(buffer));
    assert(length > 0);
    
    // Parse
    DataFrame parsed;
    bool success = FrameParser::parse_data_frame(buffer, length, parsed);
    assert(success);
    
    // Verify
    assert(parsed.data_rate_format == original.data_rate_format);
    assert(parsed.data_rate == original.data_rate);
    assert(parsed.interleaver_length == original.interleaver_length);
    assert(parsed.sequence_number == original.sequence_number);
    assert(parsed.msg_byte_offset == original.msg_byte_offset);
    assert(parsed.data_length == original.data_length);
    assert(memcmp(parsed.data, original.data, original.data_length) == 0);
    
    std::cout << "  ✓ Format + Parse successful\n";
    std::cout << "  ✓ Sequence: " << (int)parsed.sequence_number << "\n";
    std::cout << "  ✓ Offset: " << parsed.msg_byte_offset << "\n";
    std::cout << "  ✓ Data matches (" << parsed.data_length << " bytes)\n";
    std::cout << "  PASSED\n\n";
}

// Test CRC validation (corrupted frame)
void test_crc_corruption_detection() {
    std::cout << "Test: CRC Corruption Detection...\n";
    
    DataFrame frame;
    frame.sequence_number = 1;
    frame.data_length = 10;
    memcpy(frame.data, "TEST DATA!", 10);
    
    uint8_t buffer[1200];
    int length = FrameFormatter::format_data_frame(frame, buffer, sizeof(buffer));
    
    // Verify it parses correctly first
    DataFrame parsed1;
    assert(FrameParser::parse_data_frame(buffer, length, parsed1));
    
    // Corrupt one byte in the middle
    buffer[5] ^= 0xFF;
    
    // Should fail CRC validation
    DataFrame parsed2;
    bool corrupted_parse = FrameParser::parse_data_frame(buffer, length, parsed2);
    assert(!corrupted_parse);
    
    std::cout << "  ✓ Valid frame parses successfully\n";
    std::cout << "  ✓ Corrupted frame rejected\n";
    std::cout << "  PASSED\n\n";
}

// Test frame type detection
void test_frame_type_detection() {
    std::cout << "Test: Frame Type Detection...\n";
    
    // Control frame
    ControlFrame ctrl_frame;
    uint8_t ctrl_buffer[256];
    int ctrl_len = FrameFormatter::format_control_frame(ctrl_frame, ctrl_buffer, sizeof(ctrl_buffer));
    FrameType ctrl_type = FrameParser::detect_frame_type(ctrl_buffer);
    assert(ctrl_type == FrameType::T1_CONTROL || 
           ctrl_type == FrameType::T2_CONTROL ||
           ctrl_type == FrameType::T3_CONTROL ||
           ctrl_type == FrameType::T4_CONTROL);
    
    // Data frame
    DataFrame data_frame;
    data_frame.data_length = 5;
    memcpy(data_frame.data, "TEST", 4);
    uint8_t data_buffer[1200];
    int data_len = FrameFormatter::format_data_frame(data_frame, data_buffer, sizeof(data_buffer));
    FrameType data_type = FrameParser::detect_frame_type(data_buffer);
    assert(data_type == FrameType::DATA);
    
    std::cout << "  ✓ Control frame detected\n";
    std::cout << "  ✓ Data frame detected\n";
    std::cout << "  PASSED\n\n";
}

// Test utility functions
void test_utility_functions() {
    std::cout << "Test: Utility Functions...\n";
    
    // ARQ mode names
    assert(strcmp(arq_mode_name(ARQMode::VARIABLE_ARQ), "Variable ARQ") == 0);
    assert(strcmp(arq_mode_name(ARQMode::BROADCAST), "Broadcast") == 0);
    assert(strcmp(arq_mode_name(ARQMode::CIRCUIT), "Circuit") == 0);
    assert(strcmp(arq_mode_name(ARQMode::FIXED_ARQ), "Fixed ARQ") == 0);
    
    // Data rate names
    assert(strcmp(data_rate_name(DataRate::BPS_75), "75 bps") == 0);
    assert(strcmp(data_rate_name(DataRate::BPS_2400), "2400 bps") == 0);
    
    // Data rate conversions
    assert(data_rate_to_bps(DataRate::BPS_75) == 75);
    assert(data_rate_to_bps(DataRate::BPS_150) == 150);
    assert(data_rate_to_bps(DataRate::BPS_2400) == 2400);
    
    assert(bps_to_data_rate(75) == DataRate::BPS_75);
    assert(bps_to_data_rate(1200) == DataRate::BPS_1200);
    assert(bps_to_data_rate(2500) == DataRate::BPS_4800);  // Rounds up
    
    std::cout << "  ✓ ARQ mode names correct\n";
    std::cout << "  ✓ Data rate names correct\n";
    std::cout << "  ✓ Rate conversions correct\n";
    std::cout << "  PASSED\n\n";
}

// Test sequence number wrapping
void test_sequence_wrapping() {
    std::cout << "Test: Sequence Number Wrapping...\n";
    
    // Test frames with sequence numbers near wrap point
    for (uint16_t seq = 253; seq <= 257; seq++) {
        DataFrame frame;
        frame.sequence_number = seq & 0xFF;  // Wrap at 256
        frame.data_length = 1;
        frame.data[0] = 'X';
        
        uint8_t buffer[1200];
        int length = FrameFormatter::format_data_frame(frame, buffer, sizeof(buffer));
        
        DataFrame parsed;
        assert(FrameParser::parse_data_frame(buffer, length, parsed));
        assert(parsed.sequence_number == (seq & 0xFF));
    }
    
    std::cout << "  ✓ Sequence 253, 254, 255 OK\n";
    std::cout << "  ✓ Sequence 0, 1 (after wrap) OK\n";
    std::cout << "  PASSED\n\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "FS-1052 Frame Format Tests\n";
    std::cout << "========================================\n\n";
    
    try {
        test_crc32_calculation();
        test_control_frame_format();
        test_data_frame_format();
        test_control_frame_roundtrip();
        test_data_frame_roundtrip();
        test_crc_corruption_detection();
        test_frame_type_detection();
        test_utility_functions();
        test_sequence_wrapping();
        
        std::cout << "========================================\n";
        std::cout << "All FS-1052 Frame tests PASSED! ✓\n";
        std::cout << "========================================\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test FAILED with exception: " << e.what() << "\n";
        return 1;
    }
}
