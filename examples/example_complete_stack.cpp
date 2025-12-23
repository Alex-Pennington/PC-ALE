/**
 * \file example_complete_stack.cpp
 * \brief Complete ALE stack example (Phases 1-3)
 * 
 * Demonstrates full integration:
 *   - Phase 1: 8-FSK modem core
 *   - Phase 2: Protocol layer
 *   - Phase 3: Link state machine
 * 
 * Simulates:
 *   1. Scanning for calls
 *   2. Initiating a call
 *   3. Receiving incoming call
 *   4. Link establishment
 *   5. Sounding transmission
 */

#include "ale_state_machine.h"
#include "ale_message.h"
#include "tone_generator.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>

using namespace ale;

// ============================================================================
// Hardware Interface (using PC-ALE-PAL abstractions)
// ============================================================================

// NOTE: In a real implementation, you would:
// 1. Clone PC-ALE-PAL: git submodule add https://github.com/Alex-Pennington/PC-ALE-PAL.git extern/PAL
// 2. Include PAL headers: #include "pal/IRadio.h", #include "pal/IAudioDriver.h"
// 3. Create platform-specific implementations (ALSAAudioDriver, HamlibRadio, etc.)
// 4. Inject them into ALEStateMachine
//
// For this example, we use simple mock implementations.

// Mock implementation of PAL's IRadio interface
class MockRadio {
    // In production: class HamlibRadio : public pal::IRadio { ... }
public:
    void tune(uint32_t frequency_hz, const std::string& mode) {
        current_frequency = frequency_hz;
        current_mode = mode;
        std::cout << "  Radio: Tuned to " << frequency_hz / 1000.0 
                  << " kHz " << mode << "\n";
    }
    
    void set_ptt(bool transmit) {
        ptt_active = transmit;
        std::cout << "  Radio: PTT " << (transmit ? "ON" : "OFF") << "\n";
    }
    
    uint32_t get_frequency() const { return current_frequency; }
    
private:
    uint32_t current_frequency = 0;
    std::string current_mode = "USB";
    bool ptt_active = false;
};

// Mock implementation of PAL's IAudioDriver interface
class MockModem {
    // In production: class ALSAAudioDriver : public pal::IAudioDriver { ... }
public:
    MockModem() : generator() {}
    
    // Transmit ALE word
    void transmit_word(const ALEWord& word) {
        std::cout << "  Modem TX: " << WordParser::word_type_name(word.type)
                  << " [" << word.address << "]\n";
        
        // Convert word to symbols (normally done by protocol layer)
        // For demo, just show the concept
        uint8_t symbols[24];  // 24 symbols per word
        for (int i = 0; i < 24; ++i) {
            symbols[i] = i % 8;  // Placeholder
        }
        
        // Generate audio (Phase 1)
        // Each symbol = 64 samples, 24 symbols = 1536 samples
        std::vector<int16_t> audio(1536);  // Buffer for audio
        generator.generate_symbols(symbols, 24, audio.data(), 1.0f);
        std::cout << "    Generated " << audio.size() << " audio samples\n";
        
        // In real system: send audio to sound card
    }
    
    // Receive audio and decode word
    bool receive_word(ALEWord& word_out) {
        // In real system: get audio from sound card
        // For demo: return false (no word received)
        return false;
    }
    
private:
    ToneGenerator generator;
};

// ============================================================================
// ALE System Controller
// ============================================================================

class ALEController {
public:
    ALEController() {
        setup_callbacks();
        configure_channels();
    }
    
    void run_demo() {
        std::cout << "\n";
        std::cout << "╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  PC-ALE 2.0 Complete Stack Demo                          ║\n";
        std::cout << "║  Phases 1-3: Modem + Protocol + Link State Machine       ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
        
        // Demo 1: Start scanning
        demo_scanning();
        
        // Demo 2: Initiate call
        demo_outbound_call();
        
        // Demo 3: Receive call
        demo_inbound_call();
        
        // Demo 4: Send sounding
        demo_sounding();
        
        std::cout << "\n╔════════════════════════════════════════════════════════════╗\n";
        std::cout << "║  Demo Complete                                            ║\n";
        std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    }
    
private:
    void setup_callbacks() {
        // Set self address
        state_machine.set_self_address("W1AW");
        
        // State change callback
        state_machine.set_state_callback([this](ALEState from, ALEState to) {
            std::cout << "State: " << ALEStateMachine::state_name(from)
                      << " → " << ALEStateMachine::state_name(to) << "\n";
        });
        
        // Transmit callback
        state_machine.set_transmit_callback([this](const ALEWord& word) {
            modem.transmit_word(word);
        });
        
        // Channel change callback
        state_machine.set_channel_callback([this](const Channel& ch) {
            radio.tune(ch.frequency_hz, ch.mode);
        });
    }
    
    void configure_channels() {
        ScanConfig config;
        config.scan_list.push_back(Channel(7100000, "USB"));   // 7.1 MHz
        config.scan_list.push_back(Channel(14100000, "USB"));  // 14.1 MHz
        config.scan_list.push_back(Channel(21100000, "USB"));  // 21.1 MHz
        config.dwell_time_ms = 500;
        
        state_machine.configure_scan(config);
        
        std::cout << "Configured scan list:\n";
        for (const auto& ch : config.scan_list) {
            std::cout << "  - " << ch.frequency_hz / 1000.0 << " kHz " 
                      << ch.mode << "\n";
        }
        std::cout << "\n";
    }
    
    void demo_scanning() {
        std::cout << "═══════════════════════════════════════════════\n";
        std::cout << "Demo 1: Channel Scanning\n";
        std::cout << "═══════════════════════════════════════════════\n\n";
        
        std::cout << "Starting scan...\n";
        state_machine.process_event(ALEEvent::START_SCAN);
        
        // Simulate time passing
        for (int i = 0; i < 3; ++i) {
            state_machine.update(i * 600);
            
            // Simulate receiving signal quality data
            LinkQuality lq;
            lq.snr_db = 10.0f + (i * 5.0f);
            lq.fec_errors = 0;
            lq.total_words = 10;
            state_machine.update_link_quality(lq);
        }
        
        // Select best channel
        const Channel* best = state_machine.select_best_channel();
        if (best) {
            std::cout << "\nBest channel: " << best->frequency_hz / 1000.0 
                      << " kHz (LQA score: " << best->lqa_score << ")\n";
        }
        
        std::cout << "\n";
    }
    
    void demo_outbound_call() {
        std::cout << "═══════════════════════════════════════════════\n";
        std::cout << "Demo 2: Outbound Call (W1AW calling K6KB)\n";
        std::cout << "═══════════════════════════════════════════════\n\n";
        
        // Stop scanning first
        state_machine.process_event(ALEEvent::STOP_SCAN);
        
        // Initiate call
        std::cout << "Initiating call to K6KB...\n";
        bool success = state_machine.initiate_call("K6KB");
        
        if (success) {
            std::cout << "Call initiated successfully\n";
            
            // Simulate handshake completion
            std::cout << "\nSimulating handshake...\n";
            state_machine.process_event(ALEEvent::HANDSHAKE_COMPLETE);
            
            std::cout << "Link established!\n";
        }
        
        std::cout << "\n";
    }
    
    void demo_inbound_call() {
        std::cout << "═══════════════════════════════════════════════\n";
        std::cout << "Demo 3: Inbound Call (Receiving call for W1AW)\n";
        std::cout << "═══════════════════════════════════════════════\n\n";
        
        // Terminate previous link
        state_machine.process_event(ALEEvent::LINK_TERMINATED);
        
        // Start scanning again
        state_machine.process_event(ALEEvent::START_SCAN);
        
        // Simulate receiving TO word
        std::cout << "Receiving incoming call...\n";
        ALEWord to_word;
        to_word.type = WordType::TO;
        strncpy(to_word.address, "W1A", 3);
        to_word.valid = true;
        to_word.timestamp_ms = 1000;
        
        std::cout << "  Received: TO W1A\n";
        state_machine.process_received_word(to_word);
        
        // Trigger call detected
        state_machine.process_event(ALEEvent::CALL_DETECTED);
        
        std::cout << "Call detected, entering handshake\n";
        
        std::cout << "\n";
    }
    
    void demo_sounding() {
        std::cout << "═══════════════════════════════════════════════\n";
        std::cout << "Demo 4: Sounding Transmission\n";
        std::cout << "═══════════════════════════════════════════════\n\n";
        
        // Return to scanning
        state_machine.process_event(ALEEvent::LINK_TERMINATED);
        state_machine.process_event(ALEEvent::START_SCAN);
        
        // Send sounding
        std::cout << "Sending sounding...\n";
        bool success = state_machine.send_sounding();
        
        if (success) {
            std::cout << "Sounding transmitted\n";
            
            // Simulate completion
            state_machine.update(ALETimingConstants::WORD_DURATION_MS + 100);
            std::cout << "Returned to scanning\n";
        }
        
        std::cout << "\n";
    }
    
    ALEStateMachine state_machine;
    MockRadio radio;
    MockModem modem;
};

// ============================================================================
// Main
// ============================================================================

int main() {
    try {
        ALEController controller;
        controller.run_demo();
        return 0;
    }
    catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }
}
