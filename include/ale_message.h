/**
 * \file ale_message.h
 * \brief ALE message assembly and call type recognition
 * 
 * Assembles ALE words into complete messages and determines call type.
 * 
 * Specification: MIL-STD-188-141B Appendix A
 */

#pragma once

#include "ale_word.h"
#include <vector>
#include <string>
#include <cstdint>

namespace ale {

/**
 * \enum CallType
 * Types of ALE calls per MIL-STD-188-141B
 */
enum class CallType {
    INDIVIDUAL,      ///< Point-to-point call (TO + FROM)
    NET,            ///< Net call (TO net address)
    GROUP,          ///< Group call (multiple TOs)
    ALL_CALL,       ///< Broadcast to all stations
    SOUNDING,       ///< Channel sounding/LQA
    AMD,            ///< Automatic Message Display
    INDIVIDUAL_ACK, ///< Acknowledgment of individual call
    NET_ACK,        ///< Acknowledgment of net call
    UNKNOWN
};

/**
 * \struct ALEMessage
 * Complete ALE message assembled from words
 */
struct ALEMessage {
    CallType call_type;
    std::vector<std::string> to_addresses;      ///< Destination addresses
    std::string from_address;                   ///< Source address
    std::vector<std::string> data_content;      ///< Data words
    std::vector<ALEWord> words;                 ///< All received words
    uint32_t start_time_ms;                     ///< First word timestamp
    uint32_t duration_ms;                       ///< Message duration
    bool complete;                              ///< Message fully received
    
    ALEMessage() : call_type(CallType::UNKNOWN), start_time_ms(0), 
                   duration_ms(0), complete(false) {}
};

/**
 * \class MessageAssembler
 * Assemble ALE words into complete messages
 */
class MessageAssembler {
public:
    MessageAssembler();
    
    /**
     * Add received word to assembler
     * Automatically assembles into messages when sequence complete
     * 
     * \param word Decoded ALE word
     * \return true if message complete (available via get_message())
     */
    bool add_word(const ALEWord& word);
    
    /**
     * Get completed message
     * \param output [out] Completed message
     * \return true if message available
     */
    bool get_message(ALEMessage& output);
    
    /**
     * Check if message assembly is in progress
     */
    bool is_active() const { return active; }
    
    /**
     * Reset assembler state
     */
    void reset();
    
    /**
     * Set maximum time between words (ms)
     * Default: 5000 ms per MIL-STD-188-141B
     */
    void set_timeout(uint32_t timeout_ms) { word_timeout_ms = timeout_ms; }
    
private:
    std::vector<ALEWord> current_words;
    ALEMessage current_message;
    bool active;
    uint32_t last_word_time_ms;
    uint32_t word_timeout_ms;
    
    /**
     * Determine call type from word sequence
     */
    CallType determine_call_type(const std::vector<ALEWord>& words);
    
    /**
     * Check if word sequence is complete
     */
    bool is_sequence_complete(const std::vector<ALEWord>& words);
    
    /**
     * Extract addresses from words
     */
    void extract_addresses(const std::vector<ALEWord>& words, ALEMessage& msg);
    
    /**
     * Extract data content from words
     */
    void extract_data(const std::vector<ALEWord>& words, ALEMessage& msg);
    
    /**
     * Check for timeout
     */
    bool check_timeout(uint32_t current_time_ms);
};

/**
 * \class CallTypeDetector
 * Determine ALE call type from word sequence
 */
class CallTypeDetector {
public:
    /**
     * Detect call type from word sequence
     * \param words Vector of ALE words
     * \return Detected call type
     */
    static CallType detect(const std::vector<ALEWord>& words);
    
    /**
     * Check if sequence is individual call
     * Pattern: TO + FROM (+ optional DATA)
     */
    static bool is_individual_call(const std::vector<ALEWord>& words);
    
    /**
     * Check if sequence is net call
     * Pattern: TO (net address) + FROM (+ optional DATA)
     */
    static bool is_net_call(const std::vector<ALEWord>& words);
    
    /**
     * Check if sequence is sounding
     * Pattern: TIS (+ optional channel info)
     */
    static bool is_sounding(const std::vector<ALEWord>& words);
    
    /**
     * Check if sequence is AMD
     * Pattern: TO + FROM + DATA
     */
    static bool is_amd(const std::vector<ALEWord>& words);
    
    /**
     * Get human-readable call type name
     */
    static const char* call_type_name(CallType type);
};

} // namespace ale
