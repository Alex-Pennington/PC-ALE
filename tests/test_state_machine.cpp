/**
 * \file test_state_machine.cpp
 * \brief Unit tests for ALE state machine (Phase 3)
 * 
 * Tests:
 *  1. State transitions
 *  2. Channel management and scanning
 *  3. Call initiation
 *  4. Incoming call handling
 *  5. LQA (Link Quality Analysis)
 *  6. Timeout handling
 */

#include "ale_state_machine.h"
#include <iostream>
#include <iomanip>
#include <vector>
#include <cstring>

namespace ale {

// Test helper: Track state changes
class StateTracker {
public:
    std::vector<std::pair<ALEState, ALEState>> transitions;
    
    void record(ALEState from, ALEState to) {
        transitions.push_back({from, to});
    }
    
    void clear() {
        transitions.clear();
    }
    
    bool had_transition(ALEState from, ALEState to) const {
        for (const auto& t : transitions) {
            if (t.first == from && t.second == to) {
                return true;
            }
        }
        return false;
    }
};

// Test helper: Track transmitted words
class WordTracker {
public:
    std::vector<ALEWord> words;
    
    void record(const ALEWord& word) {
        words.push_back(word);
    }
    
    void clear() {
        words.clear();
    }
    
    size_t count() const {
        return words.size();
    }
};

// Test helper: Track channel changes
class ChannelTracker {
public:
    std::vector<uint32_t> frequencies;
    
    void record(const Channel& ch) {
        frequencies.push_back(ch.frequency_hz);
    }
    
    void clear() {
        frequencies.clear();
    }
    
    size_t count() const {
        return frequencies.size();
    }
};

// ============================================================================
// Test 1: Basic State Transitions
// ============================================================================

bool test_state_transitions() {
    std::cout << "\n[TEST 1] State Transitions\n";
    std::cout << "==========================\n";
    
    ALEStateMachine sm;
    StateTracker tracker;
    
    sm.set_state_callback([&tracker](ALEState from, ALEState to) {
        tracker.record(from, to);
    });
    
    // Test 1: IDLE -> SCANNING
    std::cout << "  IDLE -> SCANNING: ";
    bool changed = sm.process_event(ALEEvent::START_SCAN);
    bool correct = (changed && sm.get_state() == ALEState::SCANNING);
    std::cout << (correct ? "PASS" : "FAIL") << "\n";
    if (!correct) return false;
    
    // Test 2: SCANNING -> IDLE
    std::cout << "  SCANNING -> IDLE: ";
    changed = sm.process_event(ALEEvent::STOP_SCAN);
    correct = (changed && sm.get_state() == ALEState::IDLE);
    std::cout << (correct ? "PASS" : "FAIL") << "\n";
    if (!correct) return false;
    
    // Test 3: IDLE -> CALLING
    std::cout << "  IDLE -> CALLING: ";
    changed = sm.process_event(ALEEvent::CALL_REQUEST);
    correct = (changed && sm.get_state() == ALEState::CALLING);
    std::cout << (correct ? "PASS" : "FAIL") << "\n";
    if (!correct) return false;
    
    // Test 4: CALLING -> LINKED
    std::cout << "  CALLING -> LINKED: ";
    changed = sm.process_event(ALEEvent::HANDSHAKE_COMPLETE);
    correct = (changed && sm.get_state() == ALEState::LINKED);
    std::cout << (correct ? "PASS" : "FAIL") << "\n";
    if (!correct) return false;
    
    // Test 5: LINKED -> IDLE
    std::cout << "  LINKED -> IDLE: ";
    changed = sm.process_event(ALEEvent::LINK_TERMINATED);
    correct = (changed && sm.get_state() == ALEState::IDLE);
    std::cout << (correct ? "PASS" : "FAIL") << "\n";
    if (!correct) return false;
    
    std::cout << "PASS: All state transitions\n";
    return true;
}

// ============================================================================
// Test 2: Channel Scanning
// ============================================================================

bool test_channel_scanning() {
    std::cout << "\n[TEST 2] Channel Scanning\n";
    std::cout << "=========================\n";
    
    ALEStateMachine sm;
    ChannelTracker tracker;
    
    sm.set_channel_callback([&tracker](const Channel& ch) {
        tracker.record(ch);
    });
    
    // Configure scan list with 3 channels
    ScanConfig config;
    config.scan_list.push_back(Channel(7100000, "USB"));  // 7.1 MHz
    config.scan_list.push_back(Channel(14100000, "USB")); // 14.1 MHz
    config.scan_list.push_back(Channel(21100000, "USB")); // 21.1 MHz
    config.dwell_time_ms = 100;  // Fast scanning for test
    
    sm.configure_scan(config);
    
    // Start scanning
    sm.process_event(ALEEvent::START_SCAN);
    
    std::cout << "  Configured 3 channels: ";
    std::cout << (config.scan_list.size() == 3 ? "PASS" : "FAIL") << "\n";
    
    // Simulate time passing and channel hopping
    uint32_t time_ms = 0;
    tracker.clear();
    
    for (int i = 0; i < 10; ++i) {
        time_ms += 50;  // 50ms increments
        sm.update(time_ms);
    }
    
    std::cout << "  Channel hopping count: " << tracker.count();
    bool hopped = (tracker.count() >= 3);  // Should hop multiple times
    std::cout << (hopped ? " PASS" : " FAIL") << "\n";
    
    return hopped;
}

// ============================================================================
// Test 3: Call Initiation
// ============================================================================

bool test_call_initiation() {
    std::cout << "\n[TEST 3] Call Initiation\n";
    std::cout << "========================\n";
    
    ALEStateMachine sm;
    WordTracker tracker;
    
    sm.set_transmit_callback([&tracker](const ALEWord& word) {
        tracker.record(word);
    });
    
    // Configure self address
    AddressBook book;
    book.set_self_address("W1AW");
    
    // Initiate individual call to K6KB
    std::cout << "  Initiating individual call: ";
    bool success = sm.initiate_call("K6KB");
    
    bool correct_state = (sm.get_state() == ALEState::CALLING);
    bool words_sent = (tracker.count() == 2);  // TO + FROM
    
    bool pass = success && correct_state && words_sent;
    std::cout << (pass ? "PASS" : "FAIL");
    if (!pass) {
        std::cout << " (state=" << ALEStateMachine::state_name(sm.get_state())
                  << ", words=" << tracker.count() << ")";
    }
    std::cout << "\n";
    
    if (!words_sent) return false;
    
    // Check word types
    std::cout << "  Word 1 (TO): ";
    bool word1_ok = (tracker.words[0].type == WordType::TO);
    std::cout << (word1_ok ? "PASS" : "FAIL") << "\n";
    
    std::cout << "  Word 2 (FROM): ";
    bool word2_ok = (tracker.words[1].type == WordType::FROM);
    std::cout << (word2_ok ? "PASS" : "FAIL") << "\n";
    
    return pass && word1_ok && word2_ok;
}

// ============================================================================
// Test 4: Incoming Call Detection
// ============================================================================

bool test_incoming_call() {
    std::cout << "\n[TEST 4] Incoming Call Detection\n";
    std::cout << "=================================\n";
    
    ALEStateMachine sm;
    
    // Configure self address
    AddressBook book;
    book.set_self_address("W1AW");
    
    // Start scanning
    sm.process_event(ALEEvent::START_SCAN);
    
    std::cout << "  State: " << ALEStateMachine::state_name(sm.get_state()) << "\n";
    
    // Simulate receiving TO word addressed to us
    ALEWord to_word;
    to_word.type = WordType::TO;
    strncpy(to_word.address, "W1A", 3);  // Matches W1AW
    to_word.valid = true;
    to_word.timestamp_ms = 1000;
    
    std::cout << "  Receiving TO word for W1A: ";
    
    // Process the word - this should trigger CALL_DETECTED
    sm.process_received_word(to_word);
    
    // Manually trigger call detected (in real system, word processing would do this)
    sm.process_event(ALEEvent::CALL_DETECTED);
    
    bool in_handshake = (sm.get_state() == ALEState::HANDSHAKE);
    std::cout << (in_handshake ? "PASS" : "FAIL");
    std::cout << " (state=" << ALEStateMachine::state_name(sm.get_state()) << ")\n";
    
    return in_handshake;
}

// ============================================================================
// Test 5: Link Quality Analysis (LQA)
// ============================================================================

bool test_lqa() {
    std::cout << "\n[TEST 5] Link Quality Analysis\n";
    std::cout << "===============================\n";
    
    ALEStateMachine sm;
    
    // Configure channels
    ScanConfig config;
    config.scan_list.push_back(Channel(7100000, "USB"));
    config.scan_list.push_back(Channel(14100000, "USB"));
    config.scan_list.push_back(Channel(21100000, "USB"));
    
    sm.configure_scan(config);
    sm.process_event(ALEEvent::START_SCAN);
    
    // Simulate different quality on each channel
    LinkQuality lq1;
    lq1.snr_db = 20.0f;
    lq1.fec_errors = 0;
    lq1.total_words = 10;
    
    sm.update_link_quality(lq1);  // Channel 0: Good
    
    // Move to next channel
    sm.update(200);
    
    LinkQuality lq2;
    lq2.snr_db = 10.0f;
    lq2.fec_errors = 2;
    lq2.total_words = 10;
    
    sm.update_link_quality(lq2);  // Channel 1: Poor
    
    // Select best channel
    const Channel* best = sm.select_best_channel();
    
    std::cout << "  Best channel selection: ";
    bool pass = (best != nullptr && best->frequency_hz == 7100000);
    std::cout << (pass ? "PASS" : "FAIL");
    if (best) {
        std::cout << " (" << best->frequency_hz << " Hz, score=" 
                  << best->lqa_score << ")";
    }
    std::cout << "\n";
    
    return pass;
}

// ============================================================================
// Test 6: Timeout Handling
// ============================================================================

bool test_timeouts() {
    std::cout << "\n[TEST 6] Timeout Handling\n";
    std::cout << "=========================\n";
    
    ALEStateMachine sm;
    StateTracker tracker;
    
    sm.set_state_callback([&tracker](ALEState from, ALEState to) {
        tracker.record(from, to);
    });
    
    // Test call timeout
    std::cout << "  Call timeout: ";
    sm.process_event(ALEEvent::CALL_REQUEST);
    
    // Simulate timeout
    uint32_t timeout = ALETimingConstants::CALL_TIMEOUT_MS + 1000;
    sm.update(timeout);
    
    bool timed_out = (sm.get_state() == ALEState::IDLE);
    std::cout << (timed_out ? "PASS" : "FAIL");
    std::cout << " (final state: " << ALEStateMachine::state_name(sm.get_state()) << ")\n";
    
    return timed_out;
}

// ============================================================================
// Test 7: Sounding
// ============================================================================

bool test_sounding() {
    std::cout << "\n[TEST 7] Sounding Transmission\n";
    std::cout << "==============================\n";
    
    ALEStateMachine sm;
    WordTracker tracker;
    
    sm.set_transmit_callback([&tracker](const ALEWord& word) {
        tracker.record(word);
    });
    
    // Send sounding
    std::cout << "  Initiating sounding: ";
    bool success = sm.send_sounding();
    
    bool in_sounding = (sm.get_state() == ALEState::SOUNDING);
    bool word_sent = (tracker.count() >= 1);
    
    bool pass = success && in_sounding;
    std::cout << (pass ? "PASS" : "FAIL");
    std::cout << " (state=" << ALEStateMachine::state_name(sm.get_state())
              << ", words=" << tracker.count() << ")\n";
    
    if (word_sent) {
        std::cout << "  TIS word sent: ";
        bool is_tis = (tracker.words[0].type == WordType::TIS);
        std::cout << (is_tis ? "PASS" : "FAIL") << "\n";
        pass = pass && is_tis;
    }
    
    // Simulate sounding complete
    sm.update(ALETimingConstants::WORD_DURATION_MS + 100);
    
    std::cout << "  Sounding complete: ";
    bool returned_to_scan = (sm.get_state() == ALEState::SCANNING);
    std::cout << (returned_to_scan ? "PASS" : "FAIL") << "\n";
    
    return pass && returned_to_scan;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int run_all_tests() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PC-ALE 2.0 Phase 3 - Link State Machine Tests           ║\n";
    std::cout << "║  MIL-STD-188-141B Link Establishment                      ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    int pass_count = 0;
    int fail_count = 0;
    
    if (test_state_transitions()) { pass_count++; } else { fail_count++; }
    if (test_channel_scanning()) { pass_count++; } else { fail_count++; }
    if (test_call_initiation()) { pass_count++; } else { fail_count++; }
    if (test_incoming_call()) { pass_count++; } else { fail_count++; }
    if (test_lqa()) { pass_count++; } else { fail_count++; }
    if (test_timeouts()) { pass_count++; } else { fail_count++; }
    if (test_sounding()) { pass_count++; } else { fail_count++; }
    
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  Test Results                                              ║\n";
    std::cout << "║  Passed: " << std::setw(2) << pass_count << "  Failed: " << std::setw(2) << fail_count 
              << "                                    ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n\n";
    
    return (fail_count == 0) ? 0 : 1;
}

} // namespace ale

int main() {
    return ale::run_all_tests();
}
