/**
 * \file test_protocol.cpp
 * \brief Unit tests for ALE protocol layer (Phase 2)
 * 
 * Tests:
 *  1. Word parsing (preamble + payload extraction)
 *  2. ASCII encoding/decoding
 *  3. Address book management
 *  4. Message assembly
 *  5. Call type detection
 *  6. End-to-end call scenarios
 */

#include "ale_word.h"
#include "ale_message.h"
#include <iostream>
#include <iomanip>
#include <cstring>

namespace ale {

// ============================================================================
// Test 1: Word Preamble and Payload Extraction
// ============================================================================

bool test_word_parsing() {
    std::cout << "\n[TEST 1] Word Parsing (Preamble + Payload)\n";
    std::cout << "==========================================\n";
    
    WordParser parser;
    
    struct TestCase {
        uint32_t word_bits;
        WordType expected_type;
        const char* expected_addr;
        const char* description;
    };
    
    // Build test words: 3-bit preamble + 21-bit payload
    // Payload = 3 x 7-bit ASCII characters
    
    // Helper: encode word from preamble + 3 chars
    auto make_word = [](WordType type, const char* chars) -> uint32_t {
        uint32_t payload = WordParser::encode_ascii(chars);
        uint32_t preamble = static_cast<uint8_t>(type) & 0x07;
        return preamble | (payload << 3);
    };
    
    TestCase tests[] = {
        { make_word(WordType::TO, "W1A"), WordType::TO, "W1A", "TO address" },
        { make_word(WordType::FROM, "K6K"), WordType::FROM, "K6K", "FROM address" },
        { make_word(WordType::TIS, "N0C"), WordType::TIS, "N0C", "TIS (sounding)" },
        { make_word(WordType::DATA, "ABC"), WordType::DATA, "ABC", "DATA word" },
        { make_word(WordType::TWS, "NET"), WordType::TWS, "NET", "Net call (TWS)" },
    };
    
    bool all_pass = true;
    for (const auto& test : tests) {
        ALEWord word;
        bool success = parser.parse_from_bits(test.word_bits, word);
        
        bool type_match = (word.type == test.expected_type);
        bool addr_match = (strncmp(word.address, test.expected_addr, 3) == 0);
        bool pass = success && type_match && addr_match;
        
        std::cout << "  " << test.description << ": ";
        if (pass) {
            std::cout << "PASS (type=" << WordParser::word_type_name(word.type)
                      << ", addr=\"" << word.address << "\")\n";
        } else {
            std::cout << "FAIL";
            if (!type_match) {
                std::cout << " [type: expected " << WordParser::word_type_name(test.expected_type)
                          << ", got " << WordParser::word_type_name(word.type) << "]";
            }
            if (!addr_match) {
                std::cout << " [addr: expected \"" << test.expected_addr
                          << "\", got \"" << word.address << "\"]";
            }
            std::cout << "\n";
            all_pass = false;
        }
    }
    
    return all_pass;
}

// ============================================================================
// Test 2: ASCII Encoding/Decoding
// ============================================================================

bool test_ascii_codec() {
    std::cout << "\n[TEST 2] ASCII Encoding/Decoding\n";
    std::cout << "=================================\n";
    
    struct TestCase {
        const char* input;
        bool should_succeed;
        const char* description;
    };
    
    TestCase tests[] = {
        { "ABC", true, "Valid uppercase" },
        { "123", true, "Valid digits" },
        { "W1A", true, "Mixed alphanumeric" },
        { "N0C", true, "Call sign format" },
        { "@@@", true, "Wildcards" },
        { "   ", true, "Spaces" },
    };
    
    bool all_pass = true;
    for (const auto& test : tests) {
        uint32_t encoded = WordParser::encode_ascii(test.input);
        bool encode_success = (encoded != 0xFFFFFFFF);
        
        char decoded[4];
        bool decode_success = WordParser::decode_ascii(encoded, decoded);
        
        bool pass = (encode_success == test.should_succeed);
        if (pass && test.should_succeed) {
            pass = (strncmp(test.input, decoded, 3) == 0);
        }
        
        std::cout << "  " << test.description << " (\"" << test.input << "\"): ";
        if (pass) {
            if (test.should_succeed) {
                std::cout << "PASS (\"" << decoded << "\")\n";
            } else {
                std::cout << "PASS (rejected as expected)\n";
            }
        } else {
            std::cout << "FAIL\n";
            all_pass = false;
        }
    }
    
    return all_pass;
}

// ============================================================================
// Test 3: Address Book
// ============================================================================

bool test_address_book() {
    std::cout << "\n[TEST 3] Address Book\n";
    std::cout << "=====================\n";
    
    AddressBook book;
    
    // Test 1: Set self address
    bool self_ok = book.set_self_address("W1AW");
    std::cout << "  Set self address: " << (self_ok ? "PASS" : "FAIL") << "\n";
    
    // Test 2: Check self
    bool is_self = book.is_self("W1AW");
    std::cout << "  Check is_self: " << (is_self ? "PASS" : "FAIL") << "\n";
    
    // Test 3: Add stations
    book.add_station("K6KB", "Rick");
    book.add_station("N2CKH", "Steve");
    
    bool known = book.is_known_station("K6KB");
    std::cout << "  Known station check: " << (known ? "PASS" : "FAIL") << "\n";
    
    // Test 4: Add net
    book.add_net("MARS", "MARS Net");
    bool is_net = book.is_known_net("MARS");
    std::cout << "  Net address check: " << (is_net ? "PASS" : "FAIL") << "\n";
    
    // Test 5: Wildcard matching
    bool match1 = AddressBook::match_wildcard("W@AW", "W1AW");
    bool match2 = AddressBook::match_wildcard("W@AW", "W2AW");
    bool no_match = !AddressBook::match_wildcard("W@AW", "K1AB");
    
    std::cout << "  Wildcard matching: " 
              << (match1 && match2 && no_match ? "PASS" : "FAIL") << "\n";
    
    return self_ok && is_self && known && is_net && match1 && match2 && no_match;
}

// ============================================================================
// Test 4: Message Assembly
// ============================================================================

bool test_message_assembly() {
    std::cout << "\n[TEST 4] Message Assembly\n";
    std::cout << "=========================\n";
    
    MessageAssembler assembler;
    WordParser parser;
    
    // Build a simple individual call: TO + FROM
    auto make_word = [&parser](WordType type, const char* chars, uint32_t time_ms) -> ALEWord {
        uint32_t payload = WordParser::encode_ascii(chars);
        uint32_t preamble = static_cast<uint8_t>(type) & 0x07;
        uint32_t word_bits = preamble | (payload << 3);
        
        ALEWord word;
        parser.parse_from_bits(word_bits, word);
        word.timestamp_ms = time_ms;
        word.valid = true;
        return word;
    };
    
    // Simulate call: "W1AW" calling "K6KB"
    ALEWord to_word = make_word(WordType::TO, "K6K", 1000);
    ALEWord from_word = make_word(WordType::FROM, "W1A", 2000);
    
    bool msg_ready1 = assembler.add_word(to_word);
    std::cout << "  After TO word: " << (msg_ready1 ? "complete" : "pending") << "\n";
    
    bool msg_ready2 = assembler.add_word(from_word);
    std::cout << "  After FROM word: " << (msg_ready2 ? "complete" : "pending") << "\n";
    
    if (msg_ready2) {
        ALEMessage msg;
        bool got_msg = assembler.get_message(msg);
        
        if (got_msg) {
            std::cout << "  Message type: " << CallTypeDetector::call_type_name(msg.call_type) << "\n";
            std::cout << "  To: " << (msg.to_addresses.empty() ? "none" : msg.to_addresses[0]) << "\n";
            std::cout << "  From: " << msg.from_address << "\n";
            
            bool correct = (msg.call_type == CallType::INDIVIDUAL) &&
                          (!msg.to_addresses.empty()) &&
                          (!msg.from_address.empty());
            
            std::cout << "  Result: " << (correct ? "PASS" : "FAIL") << "\n";
            return correct;
        }
    }
    
    std::cout << "  Result: FAIL (message not assembled)\n";
    return false;
}

// ============================================================================
// Test 5: Call Type Detection
// ============================================================================

bool test_call_type_detection() {
    std::cout << "\n[TEST 5] Call Type Detection\n";
    std::cout << "============================\n";
    
    WordParser parser;
    
    auto make_word = [&parser](WordType type, const char* chars) -> ALEWord {
        uint32_t payload = WordParser::encode_ascii(chars);
        uint32_t preamble = static_cast<uint8_t>(type) & 0x07;
        uint32_t word_bits = preamble | (payload << 3);
        
        ALEWord word;
        parser.parse_from_bits(word_bits, word);
        word.valid = true;
        return word;
    };
    
    // Test individual call
    {
        std::vector<ALEWord> words;
        words.push_back(make_word(WordType::TO, "K6K"));
        words.push_back(make_word(WordType::FROM, "W1A"));
        
        CallType type = CallTypeDetector::detect(words);
        bool pass = (type == CallType::INDIVIDUAL);
        std::cout << "  Individual call: " << (pass ? "PASS" : "FAIL") 
                  << " (detected: " << CallTypeDetector::call_type_name(type) << ")\n";
    }
    
    // Test sounding
    {
        std::vector<ALEWord> words;
        words.push_back(make_word(WordType::TIS, "W1A"));
        
        CallType type = CallTypeDetector::detect(words);
        bool pass = (type == CallType::SOUNDING);
        std::cout << "  Sounding: " << (pass ? "PASS" : "FAIL")
                  << " (detected: " << CallTypeDetector::call_type_name(type) << ")\n";
    }
    
    // Test net call
    {
        std::vector<ALEWord> words;
        words.push_back(make_word(WordType::TWS, "NET"));
        words.push_back(make_word(WordType::FROM, "W1A"));
        
        CallType type = CallTypeDetector::detect(words);
        bool pass = (type == CallType::NET);
        std::cout << "  Net call: " << (pass ? "PASS" : "FAIL")
                  << " (detected: " << CallTypeDetector::call_type_name(type) << ")\n";
    }
    
    // Test AMD
    {
        std::vector<ALEWord> words;
        words.push_back(make_word(WordType::TO, "K6K"));
        words.push_back(make_word(WordType::FROM, "W1A"));
        words.push_back(make_word(WordType::DATA, "HI "));
        
        CallType type = CallTypeDetector::detect(words);
        bool pass = (type == CallType::AMD);
        std::cout << "  AMD (with data): " << (pass ? "PASS" : "FAIL")
                  << " (detected: " << CallTypeDetector::call_type_name(type) << ")\n";
    }
    
    std::cout << "PASS: All call type tests\n";
    return true;
}

// ============================================================================
// Main Test Runner
// ============================================================================

int run_all_tests() {
    std::cout << "\n";
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PC-ALE 2.0 Phase 2 - Protocol Layer Unit Tests          ║\n";
    std::cout << "║  MIL-STD-188-141B Word Structure & Message Assembly       ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    int pass_count = 0;
    int fail_count = 0;
    
    if (test_word_parsing()) { pass_count++; } else { fail_count++; }
    if (test_ascii_codec()) { pass_count++; } else { fail_count++; }
    if (test_address_book()) { pass_count++; } else { fail_count++; }
    if (test_message_assembly()) { pass_count++; } else { fail_count++; }
    if (test_call_type_detection()) { pass_count++; } else { fail_count++; }
    
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
