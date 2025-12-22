/**
 * \file ale_state_machine.cpp
 * \brief Implementation of ALE state machine
 */

#include "ale_state_machine.h"
#include <algorithm>
#include <cstring>

namespace ale {

// State names
static const char* STATE_NAMES[] = {
    "IDLE", "SCANNING", "CALLING", "HANDSHAKE", "LINKED", "SOUNDING", "ERROR"
};

// Event names
static const char* EVENT_NAMES[] = {
    "START_SCAN", "STOP_SCAN", "CALL_REQUEST", "CALL_DETECTED",
    "HANDSHAKE_COMPLETE", "LINK_TIMEOUT", "LINK_TERMINATED",
    "SOUNDING_REQUEST", "SOUNDING_COMPLETE", "ERROR_OCCURRED"
};

// ============================================================================
// ALEStateMachine Implementation
// ============================================================================

ALEStateMachine::ALEStateMachine()
    : current_state(ALEState::IDLE),
      previous_state(ALEState::IDLE),
      link_start_time_ms(0),
      last_word_time_ms(0),
      state_entry_time_ms(0),
      last_scan_hop_time_ms(0),
      current_time_ms(0) {
}

const char* ALEStateMachine::state_name(ALEState state) {
    uint8_t index = static_cast<uint8_t>(state);
    if (index > 6) index = 6;  // ERROR
    return STATE_NAMES[index];
}

const char* ALEStateMachine::event_name(ALEEvent event) {
    uint8_t index = static_cast<uint8_t>(event);
    if (index > 9) return "UNKNOWN";
    return EVENT_NAMES[index];
}

bool ALEStateMachine::process_event(ALEEvent event) {
    ALEState old_state = current_state;
    
    // State-specific event handling
    switch (current_state) {
        case ALEState::IDLE:
            if (event == ALEEvent::START_SCAN) {
                return transition_to(ALEState::SCANNING);
            }
            else if (event == ALEEvent::CALL_REQUEST) {
                return transition_to(ALEState::CALLING);
            }
            else if (event == ALEEvent::SOUNDING_REQUEST) {
                return transition_to(ALEState::SOUNDING);
            }
            break;
            
        case ALEState::SCANNING:
            if (event == ALEEvent::STOP_SCAN) {
                return transition_to(ALEState::IDLE);
            }
            else if (event == ALEEvent::CALL_DETECTED) {
                return transition_to(ALEState::HANDSHAKE);
            }
            else if (event == ALEEvent::CALL_REQUEST) {
                return transition_to(ALEState::CALLING);
            }
            break;
            
        case ALEState::CALLING:
            if (event == ALEEvent::HANDSHAKE_COMPLETE) {
                return transition_to(ALEState::LINKED);
            }
            else if (event == ALEEvent::LINK_TIMEOUT) {
                return transition_to(ALEState::IDLE);
            }
            break;
            
        case ALEState::HANDSHAKE:
            if (event == ALEEvent::HANDSHAKE_COMPLETE) {
                return transition_to(ALEState::LINKED);
            }
            else if (event == ALEEvent::LINK_TIMEOUT) {
                return transition_to(ALEState::SCANNING);
            }
            break;
            
        case ALEState::LINKED:
            if (event == ALEEvent::LINK_TERMINATED || event == ALEEvent::LINK_TIMEOUT) {
                return transition_to(ALEState::IDLE);
            }
            break;
            
        case ALEState::SOUNDING:
            if (event == ALEEvent::SOUNDING_COMPLETE) {
                return transition_to(ALEState::SCANNING);
            }
            break;
            
        case ALEState::ERROR:
            if (event == ALEEvent::START_SCAN) {
                return transition_to(ALEState::SCANNING);
            }
            else {
                return transition_to(ALEState::IDLE);
            }
            break;
    }
    
    // Error event always goes to ERROR state
    if (event == ALEEvent::ERROR_OCCURRED) {
        return transition_to(ALEState::ERROR);
    }
    
    return false;  // No state change
}

void ALEStateMachine::update(uint32_t current_time_ms) {
    this->current_time_ms = current_time_ms;
    
    // Check for timeouts
    if (check_link_timeout()) {
        process_event(ALEEvent::LINK_TIMEOUT);
    }
    
    // State-specific periodic processing
    switch (current_state) {
        case ALEState::SCANNING:
            handle_scanning();
            break;
            
        case ALEState::CALLING:
            handle_calling();
            break;
            
        case ALEState::HANDSHAKE:
            handle_handshake();
            break;
            
        case ALEState::LINKED:
            handle_linked();
            break;
            
        case ALEState::SOUNDING:
            handle_sounding();
            break;
            
        default:
            break;
    }
}

bool ALEStateMachine::transition_to(ALEState new_state) {
    if (current_state == new_state) {
        return false;
    }
    
    exit_state(current_state);
    
    previous_state = current_state;
    current_state = new_state;
    state_entry_time_ms = current_time_ms;
    
    enter_state(new_state);
    
    // Call state change callback
    if (state_callback) {
        state_callback(previous_state, current_state);
    }
    
    return true;
}

void ALEStateMachine::enter_state(ALEState new_state) {
    switch (new_state) {
        case ALEState::SCANNING:
            // Reset scan position
            scan_config.channel_index = 0;
            last_scan_hop_time_ms = current_time_ms;
            
            // Set first channel
            if (!scan_config.scan_list.empty()) {
                set_channel(0);
            }
            break;
            
        case ALEState::CALLING:
            link_start_time_ms = current_time_ms;
            break;
            
        case ALEState::HANDSHAKE:
            link_start_time_ms = current_time_ms;
            break;
            
        case ALEState::LINKED:
            link_start_time_ms = current_time_ms;
            last_word_time_ms = current_time_ms;
            break;
            
        case ALEState::SOUNDING:
            // Transmit TIS word
            if (!address_book.get_self_address().empty()) {
                ALEWord tis_word;
                tis_word.type = WordType::TIS;
                std::string self = address_book.get_self_address();
                strncpy(tis_word.address, self.c_str(), 3);
                tis_word.valid = true;
                tis_word.timestamp_ms = current_time_ms;
                
                transmit_word(tis_word);
            }
            break;
            
        default:
            break;
    }
}

void ALEStateMachine::exit_state(ALEState old_state) {
    switch (old_state) {
        case ALEState::LINKED:
            // Clear link state
            active_call_to.clear();
            active_call_from.clear();
            break;
            
        default:
            break;
    }
}

void ALEStateMachine::handle_idle() {
    // Nothing to do in IDLE
}

void ALEStateMachine::handle_scanning() {
    // Check if it's time to hop to next channel
    if (check_scan_dwell_timeout()) {
        hop_to_next_channel();
    }
}

void ALEStateMachine::handle_calling() {
    // Timeout handling done in update()
}

void ALEStateMachine::handle_handshake() {
    // Timeout handling done in update()
}

void ALEStateMachine::handle_linked() {
    // Monitor link health
}

void ALEStateMachine::handle_sounding() {
    // Check if sounding complete (after word transmission)
    uint32_t elapsed = current_time_ms - state_entry_time_ms;
    if (elapsed > ALETimingConstants::WORD_DURATION_MS) {
        process_event(ALEEvent::SOUNDING_COMPLETE);
    }
}

void ALEStateMachine::configure_scan(const ScanConfig& config) {
    scan_config = config;
}

void ALEStateMachine::add_scan_channel(const Channel& channel) {
    scan_config.scan_list.push_back(channel);
}

void ALEStateMachine::set_self_address(const std::string& address) {
    address_book.set_self_address(address);
}

const Channel* ALEStateMachine::get_current_channel() const {
    if (scan_config.scan_list.empty()) {
        return nullptr;
    }
    
    if (scan_config.channel_index >= scan_config.scan_list.size()) {
        return nullptr;
    }
    
    return &scan_config.scan_list[scan_config.channel_index];
}

bool ALEStateMachine::initiate_call(const std::string& to_address) {
    if (current_state != ALEState::IDLE && current_state != ALEState::SCANNING) {
        return false;  // Can't call now
    }
    
    active_call_to = to_address;
    active_call_from = address_book.get_self_address();
    
    // Transition to CALLING state first
    bool transitioned = process_event(ALEEvent::CALL_REQUEST);
    
    if (transitioned) {
        // Build and transmit TO + FROM words
        build_call_words(to_address, false);
    }
    
    return transitioned;
}

bool ALEStateMachine::initiate_net_call(const std::string& net_address) {
    if (current_state != ALEState::IDLE && current_state != ALEState::SCANNING) {
        return false;
    }
    
    active_call_to = net_address;
    active_call_from = address_book.get_self_address();
    
    // Transition to CALLING state first
    bool transitioned = process_event(ALEEvent::CALL_REQUEST);
    
    if (transitioned) {
        // Build and transmit TWS + FROM words
        build_call_words(net_address, true);
    }
    
    return transitioned;
}

bool ALEStateMachine::respond_to_call() {
    if (current_state != ALEState::HANDSHAKE) {
        return false;
    }
    
    // Send response (implementation depends on call type)
    // For now, just mark handshake complete
    process_event(ALEEvent::HANDSHAKE_COMPLETE);
    
    return true;
}

bool ALEStateMachine::send_sounding() {
    if (current_state != ALEState::IDLE && current_state != ALEState::SCANNING) {
        return false;
    }
    
    return process_event(ALEEvent::SOUNDING_REQUEST);
}

void ALEStateMachine::process_received_word(const ALEWord& word) {
    if (!word.valid) {
        return;
    }
    
    last_word_time_ms = current_time_ms;
    
    // Update LQA with word quality
    LinkQuality lq;
    lq.fec_errors = word.fec_errors;
    lq.total_words = 1;
    lq.timestamp_ms = current_time_ms;
    update_link_quality(lq);
    
    // Process word based on type and state
    if (current_state == ALEState::SCANNING) {
        // Check if this is a call to us
        std::string addr(word.address, 3);
        addr.erase(addr.find_last_not_of(' ') + 1);  // Trim
        
        if (word.type == WordType::TO || word.type == WordType::TWS) {
            if (address_book.is_self(addr)) {
                active_call_to = addr;
                process_event(ALEEvent::CALL_DETECTED);
            }
        }
    }
    
    // Feed to message assembler
    message_assembler.add_word(word);
}

void ALEStateMachine::update_link_quality(const LinkQuality& lq) {
    uint32_t ch_idx = scan_config.channel_index;
    
    // Ensure quality vector is large enough
    while (channel_quality.size() <= ch_idx) {
        channel_quality.push_back(LinkQuality());
    }
    
    // Update quality metrics
    channel_quality[ch_idx] = lq;
    
    // Update channel LQA score
    if (ch_idx < scan_config.scan_list.size()) {
        // Simple LQA score: 100 - (errors * 10)
        float score = 100.0f - (lq.fec_errors * 10.0f);
        score = std::max(0.0f, std::min(100.0f, score));
        
        scan_config.scan_list[ch_idx].lqa_score = score;
    }
}

const Channel* ALEStateMachine::select_best_channel() const {
    if (scan_config.scan_list.empty()) {
        return nullptr;
    }
    
    // Find channel with highest LQA score
    const Channel* best = &scan_config.scan_list[0];
    
    for (const auto& channel : scan_config.scan_list) {
        if (channel.lqa_score > best->lqa_score) {
            best = &channel;
        }
    }
    
    return best;
}

void ALEStateMachine::hop_to_next_channel() {
    if (scan_config.scan_list.empty()) {
        return;
    }
    
    scan_config.channel_index = (scan_config.channel_index + 1) % scan_config.scan_list.size();
    set_channel(scan_config.channel_index);
    last_scan_hop_time_ms = current_time_ms;
}

void ALEStateMachine::set_channel(uint32_t index) {
    if (index >= scan_config.scan_list.size()) {
        return;
    }
    
    scan_config.channel_index = index;
    
    // Update channel timestamp
    scan_config.scan_list[index].last_scan_time_ms = current_time_ms;
    
    // Call channel change callback
    if (channel_callback) {
        channel_callback(scan_config.scan_list[index]);
    }
}

bool ALEStateMachine::check_link_timeout() {
    uint32_t timeout_ms = 0;
    
    switch (current_state) {
        case ALEState::CALLING:
        case ALEState::HANDSHAKE:
            timeout_ms = ALETimingConstants::CALL_TIMEOUT_MS;
            break;
            
        case ALEState::LINKED:
            timeout_ms = ALETimingConstants::LINK_TIMEOUT_MS;
            break;
            
        default:
            return false;
    }
    
    uint32_t elapsed = current_time_ms - state_entry_time_ms;
    return elapsed > timeout_ms;
}

bool ALEStateMachine::check_scan_dwell_timeout() {
    if (current_state != ALEState::SCANNING) {
        return false;
    }
    
    uint32_t elapsed = current_time_ms - last_scan_hop_time_ms;
    return elapsed >= scan_config.dwell_time_ms;
}

void ALEStateMachine::build_call_words(const std::string& to_addr, bool is_net) {
    WordParser parser;
    
    // Build TO or TWS word
    ALEWord to_word = ALEWord();  // Use constructor
    to_word.type = is_net ? WordType::TWS : WordType::TO;
    strncpy(to_word.address, to_addr.c_str(), 3);
    to_word.address[3] = '\0';  // Ensure null termination
    to_word.valid = true;
    to_word.timestamp_ms = current_time_ms;
    
    transmit_word(to_word);
    
    // Build FROM word
    ALEWord from_word = ALEWord();  // Use constructor
    from_word.type = WordType::FROM;
    std::string self = address_book.get_self_address();
    strncpy(from_word.address, self.c_str(), 3);
    from_word.address[3] = '\0';  // Ensure null termination
    from_word.valid = true;
    from_word.timestamp_ms = current_time_ms + ALETimingConstants::WORD_DURATION_MS;
    
    transmit_word(from_word);
}

void ALEStateMachine::transmit_word(const ALEWord& word) {
    if (transmit_callback) {
        transmit_callback(word);
    }
}

} // namespace ale
