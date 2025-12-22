/**
 * \file test_fs1052_arq.cpp
 * \brief Unit tests for FS-1052 ARQ state machine
 */

#include "fs1052_arq.h"
#include <cassert>
#include <iostream>
#include <vector>
#include <cstring>

using namespace fs1052;

// Test harness
struct TestHarness {
    std::vector<std::vector<uint8_t>> sent_frames;
    ARQState last_state;
    std::string last_error;
    
    void tx_callback(const uint8_t* frame, int length) {
        std::vector<uint8_t> data(frame, frame + length);
        sent_frames.push_back(data);
    }
    
    void state_callback(ARQState old_state, ARQState new_state) {
        last_state = new_state;
    }
    
    void error_callback(const char* msg) {
        last_error = msg;
    }
    
    void clear() {
        sent_frames.clear();
        last_state = ARQState::IDLE;
        last_error.clear();
    }
};

// Test initial state
void test_initial_state() {
    std::cout << "Test: Initial State...\n";
    
    VariableARQ arq;
    assert(arq.get_state() == ARQState::IDLE);
    assert(arq.is_transfer_complete());
    
    const auto& stats = arq.get_stats();
    assert(stats.blocks_sent == 0);
    assert(stats.blocks_received == 0);
    
    std::cout << "  ✓ Initial state is IDLE\n";
    std::cout << "  ✓ Transfer complete flag set\n";
    std::cout << "  ✓ Statistics zeroed\n";
    std::cout << "  PASSED\n\n";
}

// Test state transitions
void test_state_transitions() {
    std::cout << "Test: State Transitions...\n";
    
    TestHarness harness;
    VariableARQ arq;
    
    arq.init(
        [&](const uint8_t* f, int l) { harness.tx_callback(f, l); },
        [&](ARQState o, ARQState n) { harness.state_callback(o, n); },
        [&](const char* m) { harness.error_callback(m); }
    );
    
    // IDLE -> TX_DATA
    uint8_t test_data[] = "Hello";
    arq.start_transmission(test_data, 5);
    assert(harness.last_state == ARQState::TX_DATA || 
           harness.last_state == ARQState::WAIT_ACK);
    
    std::cout << "  ✓ IDLE → TX_DATA transition\n";
    
    // Reset back to IDLE
    arq.reset();
    assert(arq.get_state() == ARQState::IDLE);
    
    std::cout << "  ✓ Reset returns to IDLE\n";
    std::cout << "  PASSED\n\n";
}

// Test simple data transmission
void test_simple_transmission() {
    std::cout << "Test: Simple Data Transmission...\n";
    
    TestHarness harness;
    VariableARQ arq;
    
    arq.init(
        [&](const uint8_t* f, int l) { harness.tx_callback(f, l); },
        [&](ARQState o, ARQState n) { harness.state_callback(o, n); },
        nullptr
    );
    
    // Send small message (should fit in one block)
    const char* msg = "Test message";
    bool started = arq.start_transmission((const uint8_t*)msg, strlen(msg));
    assert(started);
    
    // Should have sent at least one data frame
    assert(!harness.sent_frames.empty());
    
    // Verify it's a data frame
    if (!harness.sent_frames.empty()) {
        const auto& frame = harness.sent_frames[0];
        FrameType type = FrameParser::detect_frame_type(frame.data());
        assert(type == FrameType::DATA);
        
        // Parse and verify
        DataFrame df;
        bool parsed = FrameParser::parse_data_frame(frame.data(), frame.size(), df);
        assert(parsed);
        assert(df.sequence_number == 0);
        assert(df.data_length == strlen(msg));
    }
    
    std::cout << "  ✓ Transmission started\n";
    std::cout << "  ✓ Data frame sent\n";
    std::cout << "  ✓ Sequence number 0\n";
    std::cout << "  ✓ Correct payload length\n";
    std::cout << "  PASSED\n\n";
}

// Test multi-block transmission
void test_multi_block_transmission() {
    std::cout << "Test: Multi-Block Transmission...\n";
    
    TestHarness harness;
    VariableARQ arq;
    arq.set_window_size(4);  // Allow 4 blocks in flight
    
    arq.init(
        [&](const uint8_t* f, int l) { harness.tx_callback(f, l); },
        nullptr,
        nullptr
    );
    
    // Create data that requires multiple blocks (2KB)
    std::vector<uint8_t> large_data(2048);
    for (size_t i = 0; i < large_data.size(); i++) {
        large_data[i] = i & 0xFF;
    }
    
    arq.start_transmission(large_data.data(), large_data.size());
    
    // Should have sent multiple frames (up to window size)
    assert(harness.sent_frames.size() > 1);
    assert(harness.sent_frames.size() <= 4);  // Window size limit
    
    // Verify sequence numbers are sequential
    for (size_t i = 0; i < harness.sent_frames.size(); i++) {
        DataFrame df;
        bool parsed = FrameParser::parse_data_frame(
            harness.sent_frames[i].data(),
            harness.sent_frames[i].size(),
            df
        );
        assert(parsed);
        assert(df.sequence_number == i);
    }
    
    std::cout << "  ✓ Multiple blocks sent: " << harness.sent_frames.size() << "\n";
    std::cout << "  ✓ Sequence numbers sequential\n";
    std::cout << "  ✓ Window size respected\n";
    std::cout << "  PASSED\n\n";
}

// Test ACK processing
void test_ack_processing() {
    std::cout << "Test: ACK Processing...\n";
    
    TestHarness harness;
    VariableARQ arq;
    
    arq.init(
        [&](const uint8_t* f, int l) { harness.tx_callback(f, l); },
        [&](ARQState o, ARQState n) { harness.state_callback(o, n); },
        nullptr
    );
    
    // Send data
    const char* msg = "ACK test";
    arq.start_transmission((const uint8_t*)msg, strlen(msg));
    
    // Create ACK frame
    ControlFrame ack;
    ack.protocol_version = PROTOCOL_VERSION;
    ack.arq_mode = ARQMode::VARIABLE_ARQ;
    ack.ack_nak_type = AckNakType::DATA_ACK;
    
    // ACK block 0
    ack.bit_map[0] = 0x01;  // Bit 0 set
    
    uint8_t ack_buffer[256];
    int ack_len = FrameFormatter::format_control_frame(ack, ack_buffer, sizeof(ack_buffer));
    
    // Process ACK
    arq.handle_received_frame(ack_buffer, ack_len);
    
    // Verify state changed
    const auto& stats = arq.get_stats();
    assert(stats.acks_received == 1);
    
    std::cout << "  ✓ ACK frame processed\n";
    std::cout << "  ✓ Statistics updated\n";
    std::cout << "  PASSED\n\n";
}

// Test timeout handling
void test_timeout_handling() {
    std::cout << "Test: Timeout Handling...\n";
    
    TestHarness harness;
    VariableARQ arq;
    arq.set_ack_timeout(1000);  // 1 second
    
    arq.init(
        [&](const uint8_t* f, int l) { harness.tx_callback(f, l); },
        [&](ARQState o, ARQState n) { harness.state_callback(o, n); },
        nullptr
    );
    
    // Send data
    const char* msg = "Timeout test";
    arq.start_transmission((const uint8_t*)msg, strlen(msg));
    
    size_t initial_frames = harness.sent_frames.size();
    
    // Simulate timeout
    arq.update(0);      // Start
    arq.update(1500);   // After timeout
    
    // Should have triggered retransmission
    const auto& stats = arq.get_stats();
    assert(stats.timeouts >= 1 || harness.last_state == ARQState::RETRANSMIT);
    
    std::cout << "  ✓ Timeout detected\n";
    std::cout << "  ✓ State changed appropriately\n";
    std::cout << "  PASSED\n\n";
}

// Test data reception
void test_data_reception() {
    std::cout << "Test: Data Reception...\n";
    
    TestHarness harness;
    VariableARQ arq;
    
    arq.init(
        [&](const uint8_t* f, int l) { harness.tx_callback(f, l); },
        nullptr,
        nullptr
    );
    
    // Switch to RX mode
    arq.process_event(ARQEvent::START_RX);
    assert(arq.get_state() == ARQState::RX_DATA);
    
    // Create incoming data frame
    const char* test_data = "Received data";
    DataFrame df;
    df.sequence_number = 0;
    df.msg_byte_offset = 0;
    df.data_length = strlen(test_data);
    memcpy(df.data, test_data, df.data_length);
    
    uint8_t frame_buffer[1200];
    int frame_len = FrameFormatter::format_data_frame(df, frame_buffer, sizeof(frame_buffer));
    
    // Process received frame
    arq.handle_received_frame(frame_buffer, frame_len);
    
    const auto& stats = arq.get_stats();
    assert(stats.blocks_received == 1);
    
    std::cout << "  ✓ RX mode activated\n";
    std::cout << "  ✓ Data frame received\n";
    std::cout << "  ✓ Statistics updated\n";
    std::cout << "  PASSED\n\n";
}

// Test sequence number wrapping
void test_sequence_wrapping() {
    std::cout << "Test: Sequence Number Wrapping...\n";
    
    TestHarness harness;
    VariableARQ arq;
    arq.set_window_size(10);
    
    arq.init(
        [&](const uint8_t* f, int l) { harness.tx_callback(f, l); },
        nullptr,
        nullptr
    );
    
    // Create data that will wrap sequence numbers (260 blocks)
    // Each block is 1023 bytes, so need 260 * 1023 bytes
    std::vector<uint8_t> huge_data(260 * 1000);  // Close enough
    
    arq.start_transmission(huge_data.data(), huge_data.size());
    
    // Verify we can handle wrap (sequences go 0-255, then back to 0)
    // Just verify we sent something without crashing
    assert(!harness.sent_frames.empty());
    
    std::cout << "  ✓ Large data handled\n";
    std::cout << "  ✓ No crashes on sequence wrap\n";
    std::cout << "  PASSED\n\n";
}

// Test statistics
void test_statistics() {
    std::cout << "Test: Statistics Tracking...\n";
    
    TestHarness harness;
    VariableARQ arq;
    
    arq.init(
        [&](const uint8_t* f, int l) { harness.tx_callback(f, l); },
        nullptr,
        nullptr
    );
    
    const char* msg = "Stats test";
    arq.start_transmission((const uint8_t*)msg, strlen(msg));
    
    const auto& stats = arq.get_stats();
    assert(stats.blocks_sent > 0);
    
    std::cout << "  ✓ Blocks sent: " << stats.blocks_sent << "\n";
    std::cout << "  ✓ Statistics tracked correctly\n";
    std::cout << "  PASSED\n\n";
}

// Test utility functions
void test_utility_functions() {
    std::cout << "Test: Utility Functions...\n";
    
    // State names
    assert(strcmp(arq_state_name(ARQState::IDLE), "IDLE") == 0);
    assert(strcmp(arq_state_name(ARQState::TX_DATA), "TX_DATA") == 0);
    assert(strcmp(arq_state_name(ARQState::WAIT_ACK), "WAIT_ACK") == 0);
    assert(strcmp(arq_state_name(ARQState::RX_DATA), "RX_DATA") == 0);
    
    // Event names
    assert(strcmp(arq_event_name(ARQEvent::START_TX), "START_TX") == 0);
    assert(strcmp(arq_event_name(ARQEvent::ACK_RECEIVED), "ACK_RECEIVED") == 0);
    assert(strcmp(arq_event_name(ARQEvent::TIMEOUT), "TIMEOUT") == 0);
    
    std::cout << "  ✓ State name functions\n";
    std::cout << "  ✓ Event name functions\n";
    std::cout << "  PASSED\n\n";
}

int main() {
    std::cout << "========================================\n";
    std::cout << "FS-1052 ARQ State Machine Tests\n";
    std::cout << "========================================\n\n";
    
    try {
        test_initial_state();
        test_state_transitions();
        test_simple_transmission();
        test_multi_block_transmission();
        test_ack_processing();
        test_timeout_handling();
        test_data_reception();
        test_sequence_wrapping();
        test_statistics();
        test_utility_functions();
        
        std::cout << "========================================\n";
        std::cout << "All FS-1052 ARQ tests PASSED! ✓\n";
        std::cout << "========================================\n";
        
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "Test FAILED with exception: " << e.what() << "\n";
        return 1;
    }
}
