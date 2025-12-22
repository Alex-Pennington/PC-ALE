/**
 * \file ale_message.cpp
 * \brief Implementation of ALE message assembly
 */

#include "ale_message.h"
#include <algorithm>

namespace ale {

static const char* CALL_TYPE_NAMES[] = {
    "INDIVIDUAL", "NET", "GROUP", "ALL_CALL", "SOUNDING", 
    "AMD", "INDIVIDUAL_ACK", "NET_ACK", "UNKNOWN"
};

// ============================================================================
// MessageAssembler Implementation
// ============================================================================

MessageAssembler::MessageAssembler() 
    : active(false), last_word_time_ms(0), word_timeout_ms(5000) {}

bool MessageAssembler::add_word(const ALEWord& word) {
    if (!word.valid) {
        return false;  // Ignore invalid words
    }
    
    uint32_t current_time = word.timestamp_ms;
    
    // Check for timeout if assembly active
    if (active && check_timeout(current_time)) {
        reset();  // Timeout, start fresh
    }
    
    // Start new message or add to existing
    if (!active) {
        current_message.start_time_ms = current_time;
        active = true;
    }
    
    current_words.push_back(word);
    last_word_time_ms = current_time;
    
    // Check if sequence is complete
    if (is_sequence_complete(current_words)) {
        // Finalize message
        current_message.words = current_words;
        current_message.duration_ms = current_time - current_message.start_time_ms;
        current_message.call_type = determine_call_type(current_words);
        current_message.complete = true;
        
        extract_addresses(current_words, current_message);
        extract_data(current_words, current_message);
        
        return true;  // Message complete
    }
    
    return false;  // Still assembling
}

bool MessageAssembler::get_message(ALEMessage& output) {
    if (!current_message.complete) {
        return false;
    }
    
    output = current_message;
    reset();  // Clear for next message
    return true;
}

void MessageAssembler::reset() {
    current_words.clear();
    current_message = ALEMessage();
    active = false;
    last_word_time_ms = 0;
}

CallType MessageAssembler::determine_call_type(const std::vector<ALEWord>& words) {
    return CallTypeDetector::detect(words);
}

bool MessageAssembler::is_sequence_complete(const std::vector<ALEWord>& words) {
    if (words.empty()) {
        return false;
    }
    
    // Minimum complete sequence: TIS (sounding) or TO+FROM (individual call)
    if (words.size() < 1) {
        return false;
    }
    
    // Check for common complete patterns
    bool has_to = false;
    bool has_from = false;
    bool has_tis = false;
    
    for (const auto& word : words) {
        switch (word.type) {
            case WordType::TO:
            case WordType::TWS:
                has_to = true;
                break;
            case WordType::FROM:
                has_from = true;
                break;
            case WordType::TIS:
                has_tis = true;
                break;
            default:
                break;
        }
    }
    
    // Sounding is complete with just TIS
    if (has_tis) {
        return true;
    }
    
    // Individual/net call is complete with TO + FROM
    if (has_to && has_from) {
        return true;
    }
    
    // Otherwise, wait for more words
    return false;
}

void MessageAssembler::extract_addresses(const std::vector<ALEWord>& words, ALEMessage& msg) {
    for (const auto& word : words) {
        if (word.type == WordType::TO || word.type == WordType::TWS) {
            std::string addr(word.address, 3);
            // Trim trailing spaces
            addr.erase(addr.find_last_not_of(' ') + 1);
            if (!addr.empty()) {
                msg.to_addresses.push_back(addr);
            }
        } else if (word.type == WordType::FROM || word.type == WordType::TIS) {
            std::string addr(word.address, 3);
            addr.erase(addr.find_last_not_of(' ') + 1);
            if (!addr.empty()) {
                msg.from_address = addr;
            }
        }
    }
}

void MessageAssembler::extract_data(const std::vector<ALEWord>& words, ALEMessage& msg) {
    for (const auto& word : words) {
        if (word.type == WordType::DATA) {
            std::string data(word.address, 3);
            data.erase(data.find_last_not_of(' ') + 1);
            if (!data.empty()) {
                msg.data_content.push_back(data);
            }
        }
    }
}

bool MessageAssembler::check_timeout(uint32_t current_time_ms) {
    if (current_time_ms < last_word_time_ms) {
        return false;  // Time wrapped, ignore
    }
    
    return (current_time_ms - last_word_time_ms) > word_timeout_ms;
}

// ============================================================================
// CallTypeDetector Implementation
// ============================================================================

CallType CallTypeDetector::detect(const std::vector<ALEWord>& words) {
    if (words.empty()) {
        return CallType::UNKNOWN;
    }
    
    if (is_sounding(words)) {
        return CallType::SOUNDING;
    }
    
    if (is_amd(words)) {
        return CallType::AMD;
    }
    
    if (is_individual_call(words)) {
        return CallType::INDIVIDUAL;
    }
    
    if (is_net_call(words)) {
        return CallType::NET;
    }
    
    return CallType::UNKNOWN;
}

bool CallTypeDetector::is_individual_call(const std::vector<ALEWord>& words) {
    bool has_to = false;
    bool has_from = false;
    
    for (const auto& word : words) {
        if (word.type == WordType::TO) has_to = true;
        if (word.type == WordType::FROM) has_from = true;
    }
    
    return has_to && has_from;
}

bool CallTypeDetector::is_net_call(const std::vector<ALEWord>& words) {
    bool has_tws = false;
    bool has_from = false;
    
    for (const auto& word : words) {
        if (word.type == WordType::TWS) has_tws = true;
        if (word.type == WordType::FROM) has_from = true;
    }
    
    return has_tws && has_from;
}

bool CallTypeDetector::is_sounding(const std::vector<ALEWord>& words) {
    for (const auto& word : words) {
        if (word.type == WordType::TIS) {
            return true;
        }
    }
    return false;
}

bool CallTypeDetector::is_amd(const std::vector<ALEWord>& words) {
    bool has_to = false;
    bool has_from = false;
    bool has_data = false;
    
    for (const auto& word : words) {
        if (word.type == WordType::TO) has_to = true;
        if (word.type == WordType::FROM) has_from = true;
        if (word.type == WordType::DATA) has_data = true;
    }
    
    return has_to && has_from && has_data;
}

const char* CallTypeDetector::call_type_name(CallType type) {
    uint8_t index = static_cast<uint8_t>(type);
    if (index > 8) index = 8;  // UNKNOWN
    return CALL_TYPE_NAMES[index];
}

} // namespace ale
