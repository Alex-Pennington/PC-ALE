/**
 * \file ale_state_machine.h
 * \brief ALE link state machine
 * 
 * Implements MIL-STD-188-141B Appendix A link establishment procedures:
 *  - Scanning for incoming calls
 *  - Initiating outbound calls
 *  - Link handshake and establishment
 *  - Sounding/LQA operations
 * 
 * Specification: MIL-STD-188-141B Appendix A
 */

#pragma once

#include "ale_message.h"
#include "ale_word.h"
#include <cstdint>
#include <vector>
#include <string>
#include <functional>

namespace ale {

/**
 * \enum ALEState
 * ALE state machine states per MIL-STD-188-141B
 */
enum class ALEState {
    IDLE,           ///< Not actively linking
    SCANNING,       ///< Monitoring channels for calls
    CALLING,        ///< Initiating an outbound call
    HANDSHAKE,      ///< Exchanging link setup (responding to call)
    LINKED,         ///< Active link established
    SOUNDING,       ///< Transmitting or receiving sounding
    ERROR           ///< Error state
};

/**
 * \enum ALEEvent
 * Events that trigger state transitions
 */
enum class ALEEvent {
    START_SCAN,         ///< Begin scanning
    STOP_SCAN,          ///< Stop scanning
    CALL_REQUEST,       ///< User requests to make a call
    CALL_DETECTED,      ///< Incoming call detected (TO matches self)
    HANDSHAKE_COMPLETE, ///< Link handshake successful
    LINK_TIMEOUT,       ///< Link timed out
    LINK_TERMINATED,    ///< Link ended normally
    SOUNDING_REQUEST,   ///< Request to send sounding
    SOUNDING_COMPLETE,  ///< Sounding transmission complete
    ERROR_OCCURRED      ///< Error condition
};

/**
 * \struct Channel
 * Radio channel definition
 */
struct Channel {
    uint32_t frequency_hz;      ///< Center frequency in Hz
    std::string mode;           ///< Mode (USB, LSB, etc.)
    float lqa_score;            ///< Link Quality Analysis score (0-100)
    uint32_t last_scan_time_ms; ///< Last time scanned
    uint32_t call_count;        ///< Number of calls heard on this channel
    
    Channel() : frequency_hz(0), mode("USB"), lqa_score(0.0f), 
                last_scan_time_ms(0), call_count(0) {}
    
    Channel(uint32_t freq, const std::string& m = "USB") 
        : frequency_hz(freq), mode(m), lqa_score(0.0f),
          last_scan_time_ms(0), call_count(0) {}
};

/**
 * \struct ScanConfig
 * Scanning configuration
 */
struct ScanConfig {
    std::vector<Channel> scan_list;     ///< Channels to scan
    uint32_t dwell_time_ms;             ///< Time to listen per channel
    uint32_t channel_index;             ///< Current scan channel index
    bool enabled;                       ///< Scanning enabled
    
    ScanConfig() : dwell_time_ms(200), channel_index(0), enabled(false) {}
};

/**
 * \struct LinkQuality
 * Link quality metrics per channel
 */
struct LinkQuality {
    float snr_db;                   ///< Signal-to-noise ratio (dB)
    float ber;                      ///< Bit error rate (0.0 - 1.0)
    uint32_t fec_errors;            ///< FEC errors corrected
    uint32_t total_words;           ///< Total words received
    uint32_t timestamp_ms;          ///< Measurement timestamp
    
    LinkQuality() : snr_db(0.0f), ber(0.0f), fec_errors(0), 
                    total_words(0), timestamp_ms(0) {}
};

/**
 * \class ALEStateMachine
 * Core ALE state machine implementing MIL-STD-188-141B procedures
 */
class ALEStateMachine {
public:
    ALEStateMachine();
    
    /**
     * Process state machine event
     * \param event Event to process
     * \return true if state changed
     */
    bool process_event(ALEEvent event);
    
    /**
     * Update state machine (periodic tick)
     * Call this regularly (e.g., every 10-50 ms)
     * \param current_time_ms Current time in milliseconds
     */
    void update(uint32_t current_time_ms);
    
    /**
     * Get current state
     */
    ALEState get_state() const { return current_state; }
    
    /**
     * Get state name
     */
    static const char* state_name(ALEState state);
    
    /**
     * Get event name
     */
    static const char* event_name(ALEEvent event);
    
    /**
     * Configure scanning
     * \param config Scan configuration
     */
    void configure_scan(const ScanConfig& config);
    
    /**
     * Add channel to scan list
     * \param channel Channel to add
     */
    void add_scan_channel(const Channel& channel);
    
    /**
     * Set self address
     * \param address Self station address
     */
    void set_self_address(const std::string& address);
    
    /**
     * Get current channel
     */
    const Channel* get_current_channel() const;
    
    /**
     * Initiate individual call
     * \param to_address Destination address
     * \return true if call initiated
     */
    bool initiate_call(const std::string& to_address);
    
    /**
     * Initiate net call
     * \param net_address Net address
     * \return true if call initiated
     */
    bool initiate_net_call(const std::string& net_address);
    
    /**
     * Respond to incoming call (send handshake)
     * \return true if response sent
     */
    bool respond_to_call();
    
    /**
     * Send sounding (TIS)
     * \return true if sounding initiated
     */
    bool send_sounding();
    
    /**
     * Process received word
     * \param word Received ALE word
     */
    void process_received_word(const ALEWord& word);
    
    /**
     * Update link quality for current channel
     * \param lq Link quality metrics
     */
    void update_link_quality(const LinkQuality& lq);
    
    /**
     * Get best channel based on LQA
     * \return Pointer to best channel, or nullptr if none
     */
    const Channel* select_best_channel() const;
    
    /**
     * Set state change callback
     * Called whenever state changes
     */
    void set_state_callback(std::function<void(ALEState, ALEState)> callback) {
        state_callback = callback;
    }
    
    /**
     * Set word transmit callback
     * Called when state machine needs to transmit a word
     */
    void set_transmit_callback(std::function<void(const ALEWord&)> callback) {
        transmit_callback = callback;
    }
    
    /**
     * Set channel change callback
     * Called when switching channels
     */
    void set_channel_callback(std::function<void(const Channel&)> callback) {
        channel_callback = callback;
    }
    
private:
    // State machine
    ALEState current_state;
    ALEState previous_state;
    
    // Configuration
    ScanConfig scan_config;
    AddressBook address_book;
    
    // Active link state
    std::string active_call_to;         ///< TO address for active call
    std::string active_call_from;       ///< FROM address for active call
    uint32_t link_start_time_ms;        ///< Link start timestamp
    uint32_t last_word_time_ms;         ///< Last word received timestamp
    MessageAssembler message_assembler; ///< Message assembly
    
    // Timing
    uint32_t state_entry_time_ms;       ///< Time entered current state
    uint32_t last_scan_hop_time_ms;     ///< Last channel hop time
    uint32_t current_time_ms;           ///< Current time
    
    // LQA tracking
    std::vector<LinkQuality> channel_quality; ///< Quality per channel
    
    // Callbacks
    std::function<void(ALEState, ALEState)> state_callback;
    std::function<void(const ALEWord&)> transmit_callback;
    std::function<void(const Channel&)> channel_callback;
    
    // State machine internals
    void enter_state(ALEState new_state);
    void exit_state(ALEState old_state);
    bool transition_to(ALEState new_state);
    
    // State handlers
    void handle_idle();
    void handle_scanning();
    void handle_calling();
    void handle_handshake();
    void handle_linked();
    void handle_sounding();
    
    // Channel management
    void hop_to_next_channel();
    void set_channel(uint32_t index);
    
    // Timeout checking
    bool check_link_timeout();
    bool check_scan_dwell_timeout();
    
    // Call management
    void build_call_words(const std::string& to_addr, bool is_net);
    void transmit_word(const ALEWord& word);
};

/**
 * \class ALETimingConstants
 * Timing constants per MIL-STD-188-141B
 */
class ALETimingConstants {
public:
    static constexpr uint32_t WORD_DURATION_MS = 392;       ///< 49 symbols * 8ms/symbol
    static constexpr uint32_t SYMBOL_DURATION_MS = 8;       ///< 125 baud = 8ms/symbol
    static constexpr uint32_t SCAN_DWELL_MS = 200;          ///< Default dwell time
    static constexpr uint32_t CALL_TIMEOUT_MS = 30000;      ///< 30 second call timeout
    static constexpr uint32_t LINK_TIMEOUT_MS = 120000;     ///< 2 minute link timeout
    static constexpr uint32_t SOUNDING_INTERVAL_MS = 60000; ///< 1 minute between soundings
};

} // namespace ale
