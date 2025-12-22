/**
 * \file fs1052_arq.h
 * \brief FS-1052 ARQ State Machine - Variable ARQ Mode
 * 
 * Implements the Automatic Repeat Request (ARQ) protocol per FED-STD-1052.
 * Provides reliable data transfer with acknowledgments, retransmission,
 * and automatic rate adaptation.
 * 
 * ARQ States:
 * - IDLE: No active transfer
 * - TX_DATA: Transmitting data blocks
 * - WAIT_ACK: Waiting for acknowledgment
 * - RX_DATA: Receiving data blocks
 * - SEND_ACK: Sending acknowledgment
 * - RETRANSMIT: Retransmitting failed blocks
 */

#ifndef FS1052_ARQ_H
#define FS1052_ARQ_H

#include "fs1052_protocol.h"
#include <vector>
#include <queue>
#include <cstdint>
#include <functional>

namespace fs1052 {

/**
 * ARQ State Machine States
 */
enum class ARQState {
    IDLE,           ///< No active transfer
    TX_DATA,        ///< Transmitting data blocks
    WAIT_ACK,       ///< Waiting for acknowledgment
    RX_DATA,        ///< Receiving data blocks
    SEND_ACK,       ///< Sending acknowledgment
    RETRANSMIT,     ///< Retransmitting failed blocks
    ERROR           ///< Error state
};

/**
 * ARQ Events
 */
enum class ARQEvent {
    START_TX,           ///< Start transmission
    DATA_READY,         ///< Data ready to send
    FRAME_SENT,         ///< Frame transmitted
    ACK_RECEIVED,       ///< Acknowledgment received
    NAK_RECEIVED,       ///< Negative acknowledgment
    TIMEOUT,            ///< Timeout waiting for ACK
    START_RX,           ///< Start reception
    FRAME_RECEIVED,     ///< Valid frame received
    TRANSFER_COMPLETE,  ///< All data transferred
    ERROR_EVENT,        ///< Error occurred
    RESET               ///< Reset state machine
};

/**
 * Data block for transmission/reception
 */
struct DataBlock {
    uint8_t sequence;           ///< Sequence number (0-255)
    uint32_t offset;            ///< Byte offset in message
    uint16_t length;            ///< Data length
    uint8_t data[MAX_DATA_BLOCK_LENGTH];  ///< Data payload
    bool acknowledged;          ///< Has been ACKed
    uint8_t retransmit_count;   ///< Number of retransmissions
    uint32_t timestamp;         ///< When block was sent (ms)
};

/**
 * ARQ Statistics
 */
struct ARQStats {
    uint32_t blocks_sent;
    uint32_t blocks_received;
    uint32_t blocks_retransmitted;
    uint32_t acks_sent;
    uint32_t acks_received;
    uint32_t naks_received;
    uint32_t timeouts;
    uint32_t crc_errors;
    uint32_t sequence_errors;
};

/**
 * Callback function types
 */
using FrameCallback = std::function<void(const uint8_t* frame, int length)>;
using StateCallback = std::function<void(ARQState old_state, ARQState new_state)>;
using ErrorCallback = std::function<void(const char* error_msg)>;

/**
 * Variable ARQ State Machine
 * 
 * Implements FED-STD-1052 Variable ARQ mode with:
 * - Selective repeat ARQ
 * - Automatic retransmission
 * - Timeout handling
 * - Flow control
 * - Rate adaptation
 */
class VariableARQ {
public:
    /**
     * Constructor
     */
    VariableARQ();
    
    /**
     * Initialize ARQ with callbacks
     */
    void init(FrameCallback tx_callback, 
              StateCallback state_callback = nullptr,
              ErrorCallback error_callback = nullptr);
    
    /**
     * Reset state machine to IDLE
     */
    void reset();
    
    /**
     * Process ARQ event
     */
    void process_event(ARQEvent event);
    
    /**
     * Start transmission of message
     */
    bool start_transmission(const uint8_t* data, uint32_t length);
    
    /**
     * Handle received frame
     */
    void handle_received_frame(const uint8_t* frame, int length);
    
    /**
     * Periodic update (call from main loop)
     * Checks timeouts and retransmissions
     */
    void update(uint32_t current_time_ms);
    
    /**
     * Get current state
     */
    ARQState get_state() const { return m_state; }
    
    /**
     * Get statistics
     */
    const ARQStats& get_stats() const { return m_stats; }
    
    /**
     * Set ACK timeout (ms)
     */
    void set_ack_timeout(uint32_t timeout_ms) { m_ack_timeout = timeout_ms; }
    
    /**
     * Set maximum retransmissions
     */
    void set_max_retransmissions(uint8_t max) { m_max_retransmits = max; }
    
    /**
     * Set transmission window size
     */
    void set_window_size(uint8_t size) { m_window_size = size; }
    
    /**
     * Set data rate
     */
    void set_data_rate(DataRate rate);
    
    /**
     * Get current data rate
     */
    DataRate get_data_rate() const { return m_data_rate; }
    
    /**
     * Check if transfer is complete
     */
    bool is_transfer_complete() const;
    
    /**
     * Get received data (for RX side)
     */
    const std::vector<uint8_t>& get_received_data() const { return m_rx_buffer; }

private:
    // State machine
    ARQState m_state;
    ARQState m_prev_state;
    
    // Callbacks
    FrameCallback m_tx_callback;
    StateCallback m_state_callback;
    ErrorCallback m_error_callback;
    
    // Transmission
    std::vector<DataBlock> m_tx_blocks;     ///< Blocks to transmit
    std::queue<uint8_t> m_retransmit_queue; ///< Blocks needing retransmit
    uint8_t m_next_tx_sequence;             ///< Next sequence to send
    uint8_t m_window_base;                  ///< First unacknowledged sequence
    uint8_t m_window_size;                  ///< Max outstanding blocks
    
    // Reception
    std::vector<uint8_t> m_rx_buffer;       ///< Reassembly buffer
    bool m_rx_bitmap[256];                  ///< Received sequence bitmap
    uint8_t m_expected_sequence;            ///< Next expected sequence
    uint32_t m_rx_msg_length;               ///< Expected message length
    
    // Timing
    uint32_t m_last_tx_time;                ///< Last transmission time
    uint32_t m_ack_timeout;                 ///< ACK timeout (ms)
    uint32_t m_wait_start_time;             ///< When WAIT_ACK started
    
    // Parameters
    DataRate m_data_rate;                   ///< Current data rate
    uint8_t m_max_retransmits;              ///< Max retransmission attempts
    
    // Statistics
    ARQStats m_stats;
    
    // State handlers
    void handle_idle(ARQEvent event);
    void handle_tx_data(ARQEvent event);
    void handle_wait_ack(ARQEvent event);
    void handle_rx_data(ARQEvent event);
    void handle_send_ack(ARQEvent event);
    void handle_retransmit(ARQEvent event);
    
    // Helper functions
    void transition_to(ARQState new_state);
    void send_next_blocks();
    void send_block(uint8_t sequence);
    void send_ack();
    void send_nak(uint8_t sequence);
    void process_ack(const ControlFrame& frame);
    void process_data_frame(const DataFrame& frame);
    void check_timeouts(uint32_t current_time);
    bool all_blocks_acked() const;
    void report_error(const char* msg);
    
    // Block management
    void create_blocks(const uint8_t* data, uint32_t length);
    void mark_block_acked(uint8_t sequence);
    DataBlock* find_block(uint8_t sequence);
    void reassemble_data();
};

/**
 * Get ARQ state name
 */
const char* arq_state_name(ARQState state);

/**
 * Get ARQ event name
 */
const char* arq_event_name(ARQEvent event);

} // namespace fs1052

#endif // FS1052_ARQ_H
