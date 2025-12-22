/**
 * \file example_fs1052_transfer.cpp
 * \brief Example: Complete FS-1052 data transfer
 * 
 * Demonstrates a complete reliable data transfer using FS-1052 Variable ARQ.
 * Shows transmit side, receive side, and bidirectional communication.
 */

#include "fs1052_arq.h"
#include <iostream>
#include <vector>
#include <cstring>
#include <chrono>

using namespace fs1052;

/**
 * Simulated channel - introduces delays and optional errors
 */
class SimulatedChannel {
public:
    SimulatedChannel() : m_error_rate(0.0) {}
    
    void set_error_rate(double rate) { m_error_rate = rate; }
    
    // Transmit from A to B
    void transmit_a_to_b(const uint8_t* frame, int length) {
        if (should_drop()) {
            std::cout << "  [Channel] Dropped frame A→B\n";
            return;
        }
        m_b_queue.push_back(std::vector<uint8_t>(frame, frame + length));
    }
    
    // Transmit from B to A
    void transmit_b_to_a(const uint8_t* frame, int length) {
        if (should_drop()) {
            std::cout << "  [Channel] Dropped frame B→A\n";
            return;
        }
        m_a_queue.push_back(std::vector<uint8_t>(frame, frame + length));
    }
    
    // Deliver queued frames to A
    std::vector<std::vector<uint8_t>> receive_at_a() {
        auto frames = m_a_queue;
        m_a_queue.clear();
        return frames;
    }
    
    // Deliver queued frames to B
    std::vector<std::vector<uint8_t>> receive_at_b() {
        auto frames = m_b_queue;
        m_b_queue.clear();
        return frames;
    }
    
private:
    bool should_drop() {
        return (rand() / (double)RAND_MAX) < m_error_rate;
    }
    
    std::vector<std::vector<uint8_t>> m_a_queue;
    std::vector<std::vector<uint8_t>> m_b_queue;
    double m_error_rate;
};

/**
 * Example 1: Simple one-way transfer
 */
void example_one_way_transfer() {
    std::cout << "========================================\n";
    std::cout << "Example 1: One-Way Data Transfer\n";
    std::cout << "========================================\n\n";
    
    SimulatedChannel channel;
    
    // Sender (Station A)
    VariableARQ sender;
    sender.init(
        [&](const uint8_t* f, int l) {
            std::cout << "  [TX] Sending frame (" << l << " bytes)\n";
            channel.transmit_a_to_b(f, l);
        },
        [](ARQState old_state, ARQState new_state) {
            std::cout << "  [TX] " << arq_state_name(old_state) 
                     << " → " << arq_state_name(new_state) << "\n";
        }
    );
    
    // Receiver (Station B)
    VariableARQ receiver;
    receiver.init(
        [&](const uint8_t* f, int l) {
            std::cout << "  [RX] Sending ACK (" << l << " bytes)\n";
            channel.transmit_b_to_a(f, l);
        },
        [](ARQState old_state, ARQState new_state) {
            std::cout << "  [RX] " << arq_state_name(old_state) 
                     << " → " << arq_state_name(new_state) << "\n";
        }
    );
    
    // Start receiver
    receiver.process_event(ARQEvent::START_RX);
    
    // Send message
    const char* message = "The quick brown fox jumps over the lazy dog. "
                         "This is a test of the FS-1052 ARQ protocol.";
    std::cout << "Transmitting: \"" << message << "\"\n\n";
    
    sender.start_transmission((const uint8_t*)message, strlen(message));
    
    // Simulate transfer
    uint32_t time_ms = 0;
    int iterations = 0;
    
    while (!sender.is_transfer_complete() && iterations < 50) {
        // Deliver frames to receiver
        auto rx_frames = channel.receive_at_b();
        for (const auto& frame : rx_frames) {
            receiver.handle_received_frame(frame.data(), frame.size());
        }
        
        // Deliver ACKs to sender
        auto ack_frames = channel.receive_at_a();
        for (const auto& frame : ack_frames) {
            sender.handle_received_frame(frame.data(), frame.size());
        }
        
        // Update timers
        time_ms += 100;
        sender.update(time_ms);
        receiver.update(time_ms);
        
        iterations++;
    }
    
    // Display results
    std::cout << "\nTransfer complete!\n";
    std::cout << "Iterations: " << iterations << "\n";
    
    const auto& tx_stats = sender.get_stats();
    std::cout << "\nSender statistics:\n";
    std::cout << "  Blocks sent: " << tx_stats.blocks_sent << "\n";
    std::cout << "  Retransmissions: " << tx_stats.blocks_retransmitted << "\n";
    std::cout << "  ACKs received: " << tx_stats.acks_received << "\n";
    
    const auto& rx_stats = receiver.get_stats();
    std::cout << "\nReceiver statistics:\n";
    std::cout << "  Blocks received: " << rx_stats.blocks_received << "\n";
    std::cout << "  ACKs sent: " << rx_stats.acks_sent << "\n";
    
    // Verify received data
    const auto& received_data = receiver.get_received_data();
    std::string received_msg((char*)received_data.data(), received_data.size());
    
    if (received_msg == message) {
        std::cout << "\n✓ Data integrity verified!\n";
    } else {
        std::cout << "\n✗ Data mismatch!\n";
    }
    
    std::cout << "\n";
}

/**
 * Example 2: Transfer with packet loss
 */
void example_with_errors() {
    std::cout << "========================================\n";
    std::cout << "Example 2: Transfer with 10% Loss\n";
    std::cout << "========================================\n\n";
    
    SimulatedChannel channel;
    channel.set_error_rate(0.1);  // 10% packet loss
    
    VariableARQ sender;
    sender.set_ack_timeout(1000);  // 1 second timeout
    
    int tx_count = 0;
    sender.init(
        [&](const uint8_t* f, int l) {
            tx_count++;
            channel.transmit_a_to_b(f, l);
        },
        nullptr
    );
    
    VariableARQ receiver;
    int ack_count = 0;
    receiver.init(
        [&](const uint8_t* f, int l) {
            ack_count++;
            channel.transmit_b_to_a(f, l);
        },
        nullptr
    );
    
    receiver.process_event(ARQEvent::START_RX);
    
    // Send larger message to increase chance of packet loss
    std::vector<uint8_t> large_message(2000);
    for (size_t i = 0; i < large_message.size(); i++) {
        large_message[i] = i & 0xFF;
    }
    
    std::cout << "Transmitting " << large_message.size() << " bytes...\n\n";
    sender.start_transmission(large_message.data(), large_message.size());
    
    uint32_t time_ms = 0;
    int iterations = 0;
    
    while (!sender.is_transfer_complete() && iterations < 100) {
        auto rx_frames = channel.receive_at_b();
        for (const auto& frame : rx_frames) {
            receiver.handle_received_frame(frame.data(), frame.size());
        }
        
        auto ack_frames = channel.receive_at_a();
        for (const auto& frame : ack_frames) {
            sender.handle_received_frame(frame.data(), frame.size());
        }
        
        time_ms += 100;
        sender.update(time_ms);
        receiver.update(time_ms);
        
        iterations++;
    }
    
    std::cout << "Transfer complete!\n";
    std::cout << "Total TX attempts: " << tx_count << "\n";
    std::cout << "Total ACKs: " << ack_count << "\n";
    
    const auto& stats = sender.get_stats();
    std::cout << "\nStatistics:\n";
    std::cout << "  Blocks sent: " << stats.blocks_sent << "\n";
    std::cout << "  Retransmissions: " << stats.blocks_retransmitted << "\n";
    std::cout << "  Timeouts: " << stats.timeouts << "\n";
    std::cout << "  ACKs received: " << stats.acks_received << "\n";
    
    // Verify data
    const auto& received = receiver.get_received_data();
    if (received.size() == large_message.size()) {
        bool match = true;
        for (size_t i = 0; i < received.size(); i++) {
            if (received[i] != large_message[i]) {
                match = false;
                break;
            }
        }
        
        if (match) {
            std::cout << "\n✓ Data integrity verified despite packet loss!\n";
        } else {
            std::cout << "\n✗ Data corruption detected!\n";
        }
    } else {
        std::cout << "\n✗ Size mismatch: expected " << large_message.size() 
                 << ", got " << received.size() << "\n";
    }
    
    std::cout << "\n";
}

/**
 * Example 3: Different data rates
 */
void example_data_rates() {
    std::cout << "========================================\n";
    std::cout << "Example 3: Different Data Rates\n";
    std::cout << "========================================\n\n";
    
    DataRate rates[] = {
        DataRate::BPS_75,
        DataRate::BPS_300,
        DataRate::BPS_1200,
        DataRate::BPS_2400
    };
    
    for (auto rate : rates) {
        std::cout << "Testing at " << data_rate_name(rate) << "...\n";
        
        SimulatedChannel channel;
        VariableARQ sender;
        VariableARQ receiver;
        
        sender.set_data_rate(rate);
        sender.init([&](const uint8_t* f, int l) { channel.transmit_a_to_b(f, l); });
        receiver.init([&](const uint8_t* f, int l) { channel.transmit_b_to_a(f, l); });
        
        receiver.process_event(ARQEvent::START_RX);
        
        const char* msg = "Rate test message";
        sender.start_transmission((const uint8_t*)msg, strlen(msg));
        
        uint32_t time_ms = 0;
        for (int i = 0; i < 20 && !sender.is_transfer_complete(); i++) {
            for (const auto& f : channel.receive_at_b()) {
                receiver.handle_received_frame(f.data(), f.size());
            }
            for (const auto& f : channel.receive_at_a()) {
                sender.handle_received_frame(f.data(), f.size());
            }
            time_ms += 100;
            sender.update(time_ms);
        }
        
        if (sender.is_transfer_complete()) {
            std::cout << "  ✓ Transfer successful\n";
        } else {
            std::cout << "  ✗ Transfer incomplete\n";
        }
    }
    
    std::cout << "\n";
}

int main() {
    std::cout << "FS-1052 ARQ Protocol Examples\n";
    std::cout << "==============================\n\n";
    
    // Example 1: Simple transfer
    example_one_way_transfer();
    
    // Example 2: Transfer with errors
    example_with_errors();
    
    // Example 3: Different rates
    example_data_rates();
    
    std::cout << "========================================\n";
    std::cout << "All examples complete!\n";
    std::cout << "========================================\n";
    
    return 0;
}
