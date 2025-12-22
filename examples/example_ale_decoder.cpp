/**
 * \file example_ale_decoder.cpp
 * \brief Example: Complete ALE decoder using Phase 1 + Phase 2
 * 
 * Demonstrates full pipeline:
 *  Audio samples → FFT demod → Symbols → Words → Messages
 */

#include "fft_demodulator.h"
#include "tone_generator.h"
#include "ale_word.h"
#include "ale_message.h"
#include <iostream>
#include <vector>

using namespace ale;

/**
 * Example: Decode complete ALE individual call
 * Simulates receiving: "W1AW" calling "K6KB"
 */
void example_individual_call() {
    std::cout << "\n=== Example: Individual Call ===\n";
    std::cout << "Simulating: W1AW calling K6KB\n\n";
    
    // ========================================================================
    // Step 1: Generate audio for two ALE words (TO + FROM)
    // ========================================================================
    
    ToneGenerator generator;
    
    // Build TO word: preamble=2 (TO), address="K6KB" (padded to "K6K")
    uint32_t to_payload = WordParser::encode_ascii("K6K");
    uint32_t to_word_bits = 2 | (to_payload << 3);  // preamble | (payload << 3)
    
    // Build FROM word: preamble=4 (FROM), address="W1AW" (padded to "W1A")
    uint32_t from_payload = WordParser::encode_ascii("W1A");
    uint32_t from_word_bits = 4 | (from_payload << 3);
    
    // Convert 24-bit words to symbols (each symbol = 3 bits)
    // For simplicity, just use lower bits as symbols
    std::vector<uint8_t> to_symbols(49);
    std::vector<uint8_t> from_symbols(49);
    
    for (int i = 0; i < 49; ++i) {
        to_symbols[i] = (to_word_bits >> (i % 24)) & 0x07;
        from_symbols[i] = (from_word_bits >> (i % 24)) & 0x07;
    }
    
    // Generate audio (49 symbols * 64 samples/symbol = 3136 samples per word)
    std::vector<int16_t> to_audio(49 * 64);
    std::vector<int16_t> from_audio(49 * 64);
    
    generator.generate_symbols(to_symbols.data(), 49, to_audio.data());
    generator.generate_symbols(from_symbols.data(), 49, from_audio.data());
    
    std::cout << "Generated audio: " << to_audio.size() << " + " 
              << from_audio.size() << " = " 
              << (to_audio.size() + from_audio.size()) << " samples\n";
    
    // ========================================================================
    // Step 2: Demodulate audio to symbols
    // ========================================================================
    
    FFTDemodulator demodulator;
    
    auto to_detected = demodulator.process_audio(to_audio.data(), to_audio.size());
    auto from_detected = demodulator.process_audio(from_audio.data(), from_audio.size());
    
    std::cout << "Demodulated: " << to_detected.size() << " + " 
              << from_detected.size() << " symbols\n";
    
    // ========================================================================
    // Step 3: Parse symbols to ALE words
    // ========================================================================
    
    WordParser parser;
    
    ALEWord to_word;
    if (parser.parse_from_bits(to_word_bits, to_word)) {
        std::cout << "\nTO Word:\n";
        std::cout << "  Type: " << WordParser::word_type_name(to_word.type) << "\n";
        std::cout << "  Address: \"" << to_word.address << "\"\n";
        std::cout << "  Valid: " << (to_word.valid ? "Yes" : "No") << "\n";
    }
    
    ALEWord from_word;
    if (parser.parse_from_bits(from_word_bits, from_word)) {
        std::cout << "\nFROM Word:\n";
        std::cout << "  Type: " << WordParser::word_type_name(from_word.type) << "\n";
        std::cout << "  Address: \"" << from_word.address << "\"\n";
        std::cout << "  Valid: " << (from_word.valid ? "Yes" : "No") << "\n";
    }
    
    // ========================================================================
    // Step 4: Assemble words into complete message
    // ========================================================================
    
    MessageAssembler assembler;
    
    to_word.timestamp_ms = 1000;
    from_word.timestamp_ms = 2000;
    
    assembler.add_word(to_word);
    
    if (assembler.add_word(from_word)) {
        std::cout << "\n✓ Message complete!\n";
        
        ALEMessage message;
        if (assembler.get_message(message)) {
            std::cout << "\nMessage Details:\n";
            std::cout << "  Call Type: " << CallTypeDetector::call_type_name(message.call_type) << "\n";
            std::cout << "  From: " << message.from_address << "\n";
            std::cout << "  To: ";
            for (const auto& addr : message.to_addresses) {
                std::cout << addr << " ";
            }
            std::cout << "\n";
            std::cout << "  Duration: " << message.duration_ms << " ms\n";
            std::cout << "  Word count: " << message.words.size() << "\n";
        }
    }
}

/**
 * Example: Address book usage
 */
void example_address_book() {
    std::cout << "\n\n=== Example: Address Book ===\n";
    
    AddressBook book;
    
    // Configure this station
    book.set_self_address("W1AW");
    std::cout << "Self address: " << book.get_self_address() << "\n";
    
    // Add known stations
    book.add_station("K6KB", "Rick Muething");
    book.add_station("N2CKH", "Steve Hajducek");
    book.add_station("G4GUO", "Charles Brain");
    
    // Add nets
    book.add_net("MARS", "Military Auxiliary Radio System");
    book.add_net("EMRG", "Emergency Net");
    
    // Check addresses
    std::cout << "\nAddress checks:\n";
    std::cout << "  W1AW is self: " << (book.is_self("W1AW") ? "Yes" : "No") << "\n";
    std::cout << "  K6KB is known: " << (book.is_known_station("K6KB") ? "Yes" : "No") << "\n";
    std::cout << "  MARS is net: " << (book.is_known_net("MARS") ? "Yes" : "No") << "\n";
    
    // Wildcard matching
    std::cout << "\nWildcard matching:\n";
    std::cout << "  W@AW matches W1AW: " 
              << (AddressBook::match_wildcard("W@AW", "W1AW") ? "Yes" : "No") << "\n";
    std::cout << "  W@AW matches W2AW: " 
              << (AddressBook::match_wildcard("W@AW", "W2AW") ? "Yes" : "No") << "\n";
    std::cout << "  W@AW matches K6KB: " 
              << (AddressBook::match_wildcard("W@AW", "K6KB") ? "Yes" : "No") << "\n";
}

/**
 * Example: Sounding (TIS) detection
 */
void example_sounding() {
    std::cout << "\n\n=== Example: Sounding Detection ===\n";
    
    WordParser parser;
    MessageAssembler assembler;
    
    // Build TIS word: preamble=5 (TIS), address="W1AW" → "W1A"
    uint32_t payload = WordParser::encode_ascii("W1A");
    uint32_t word_bits = 5 | (payload << 3);
    
    ALEWord word;
    if (parser.parse_from_bits(word_bits, word)) {
        std::cout << "TIS Word:\n";
        std::cout << "  Type: " << WordParser::word_type_name(word.type) << "\n";
        std::cout << "  Address: \"" << word.address << "\"\n";
        
        word.timestamp_ms = 1000;
        word.valid = true;
        
        if (assembler.add_word(word)) {
            ALEMessage msg;
            if (assembler.get_message(msg)) {
                std::cout << "\nDetected: " << CallTypeDetector::call_type_name(msg.call_type) << "\n";
                std::cout << "Station: " << msg.from_address << "\n";
            }
        }
    }
}

/**
 * Main entry point
 */
int main() {
    std::cout << "╔════════════════════════════════════════════════════════════╗\n";
    std::cout << "║  PC-ALE 2.0 - Phase 1 + Phase 2 Integration Examples     ║\n";
    std::cout << "║  MIL-STD-188-141B 2G ALE Implementation                   ║\n";
    std::cout << "╚════════════════════════════════════════════════════════════╝\n";
    
    example_individual_call();
    example_address_book();
    example_sounding();
    
    std::cout << "\n\n✅ All examples complete!\n\n";
    
    return 0;
}
