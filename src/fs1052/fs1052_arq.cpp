/**
 * \file fs1052_arq.cpp
 * \brief FS-1052 Variable ARQ Implementation
 */

#include "fs1052_arq.h"
#include <cstring>
#include <algorithm>

namespace fs1052 {

// Default parameters
static const uint32_t DEFAULT_ACK_TIMEOUT = 5000;  // 5 seconds
static const uint8_t DEFAULT_MAX_RETRANSMITS = 3;
static const uint8_t DEFAULT_WINDOW_SIZE = 16;

VariableARQ::VariableARQ()
    : m_state(ARQState::IDLE)
    , m_prev_state(ARQState::IDLE)
    , m_next_tx_sequence(0)
    , m_window_base(0)
    , m_window_size(DEFAULT_WINDOW_SIZE)
    , m_expected_sequence(0)
    , m_rx_msg_length(0)
    , m_last_tx_time(0)
    , m_ack_timeout(DEFAULT_ACK_TIMEOUT)
    , m_wait_start_time(0)
    , m_data_rate(DataRate::BPS_2400)
    , m_max_retransmits(DEFAULT_MAX_RETRANSMITS)
{
    memset(&m_stats, 0, sizeof(m_stats));
    memset(m_rx_bitmap, 0, sizeof(m_rx_bitmap));
}

void VariableARQ::init(FrameCallback tx_callback,
                       StateCallback state_callback,
                       ErrorCallback error_callback)
{
    m_tx_callback = tx_callback;
    m_state_callback = state_callback;
    m_error_callback = error_callback;
}

void VariableARQ::reset()
{
    transition_to(ARQState::IDLE);
    m_tx_blocks.clear();
    while (!m_retransmit_queue.empty()) {
        m_retransmit_queue.pop();
    }
    m_rx_buffer.clear();
    m_next_tx_sequence = 0;
    m_window_base = 0;
    m_expected_sequence = 0;
    memset(m_rx_bitmap, 0, sizeof(m_rx_bitmap));
    memset(&m_stats, 0, sizeof(m_stats));
}

void VariableARQ::process_event(ARQEvent event)
{
    switch (m_state) {
        case ARQState::IDLE:
            handle_idle(event);
            break;
        case ARQState::TX_DATA:
            handle_tx_data(event);
            break;
        case ARQState::WAIT_ACK:
            handle_wait_ack(event);
            break;
        case ARQState::RX_DATA:
            handle_rx_data(event);
            break;
        case ARQState::SEND_ACK:
            handle_send_ack(event);
            break;
        case ARQState::RETRANSMIT:
            handle_retransmit(event);
            break;
        case ARQState::ERROR:
            if (event == ARQEvent::RESET) {
                reset();
            }
            break;
    }
}

void VariableARQ::handle_idle(ARQEvent event)
{
    switch (event) {
        case ARQEvent::START_TX:
            if (!m_tx_blocks.empty()) {
                transition_to(ARQState::TX_DATA);
                send_next_blocks();
            }
            break;
        case ARQEvent::START_RX:
            transition_to(ARQState::RX_DATA);
            break;
        case ARQEvent::FRAME_RECEIVED:
            // Unexpected frame, ignore
            break;
        default:
            break;
    }
}

void VariableARQ::handle_tx_data(ARQEvent event)
{
    switch (event) {
        case ARQEvent::FRAME_SENT:
            if (all_blocks_acked()) {
                process_event(ARQEvent::TRANSFER_COMPLETE);
            } else {
                transition_to(ARQState::WAIT_ACK);
                m_wait_start_time = m_last_tx_time;
            }
            break;
        case ARQEvent::TRANSFER_COMPLETE:
            transition_to(ARQState::IDLE);
            break;
        case ARQEvent::ERROR_EVENT:
            transition_to(ARQState::ERROR);
            break;
        default:
            break;
    }
}

void VariableARQ::handle_wait_ack(ARQEvent event)
{
    switch (event) {
        case ARQEvent::ACK_RECEIVED:
            if (all_blocks_acked()) {
                transition_to(ARQState::IDLE);
            } else if (!m_retransmit_queue.empty()) {
                transition_to(ARQState::RETRANSMIT);
            } else {
                transition_to(ARQState::TX_DATA);
                send_next_blocks();
            }
            break;
        case ARQEvent::NAK_RECEIVED:
            m_stats.naks_received++;
            transition_to(ARQState::RETRANSMIT);
            break;
        case ARQEvent::TIMEOUT:
            m_stats.timeouts++;
            transition_to(ARQState::RETRANSMIT);
            break;
        case ARQEvent::ERROR_EVENT:
            transition_to(ARQState::ERROR);
            break;
        default:
            break;
    }
}

void VariableARQ::handle_rx_data(ARQEvent event)
{
    switch (event) {
        case ARQEvent::FRAME_RECEIVED:
            transition_to(ARQState::SEND_ACK);
            break;
        case ARQEvent::TRANSFER_COMPLETE:
            reassemble_data();
            transition_to(ARQState::IDLE);
            break;
        case ARQEvent::ERROR_EVENT:
            transition_to(ARQState::ERROR);
            break;
        default:
            break;
    }
}

void VariableARQ::handle_send_ack(ARQEvent event)
{
    switch (event) {
        case ARQEvent::FRAME_SENT:
            transition_to(ARQState::RX_DATA);
            break;
        default:
            break;
    }
}

void VariableARQ::handle_retransmit(ARQEvent event)
{
    switch (event) {
        case ARQEvent::DATA_READY:
            // Send retransmission blocks
            while (!m_retransmit_queue.empty()) {
                uint8_t seq = m_retransmit_queue.front();
                m_retransmit_queue.pop();
                
                DataBlock* block = find_block(seq);
                if (block && !block->acknowledged) {
                    if (block->retransmit_count >= m_max_retransmits) {
                        report_error("Max retransmissions exceeded");
                        transition_to(ARQState::ERROR);
                        return;
                    }
                    send_block(seq);
                    block->retransmit_count++;
                    m_stats.blocks_retransmitted++;
                }
            }
            transition_to(ARQState::WAIT_ACK);
            m_wait_start_time = m_last_tx_time;
            break;
        default:
            break;
    }
}

bool VariableARQ::start_transmission(const uint8_t* data, uint32_t length)
{
    if (m_state != ARQState::IDLE) {
        report_error("Cannot start transmission: not in IDLE state");
        return false;
    }
    
    if (!m_tx_callback) {
        report_error("No TX callback configured");
        return false;
    }
    
    create_blocks(data, length);
    process_event(ARQEvent::START_TX);
    return true;
}

void VariableARQ::handle_received_frame(const uint8_t* frame, int length)
{
    if (length < 1) {
        return;
    }
    
    FrameType type = FrameParser::detect_frame_type(frame);
    
    if (type == FrameType::DATA) {
        // Data frame
        DataFrame df;
        if (FrameParser::parse_data_frame(frame, length, df)) {
            process_data_frame(df);
            m_stats.blocks_received++;
            process_event(ARQEvent::FRAME_RECEIVED);
        } else {
            m_stats.crc_errors++;
        }
    } else if (type != FrameType::NO_FRAME) {
        // Control frame (ACK/NAK)
        ControlFrame cf;
        if (FrameParser::parse_control_frame(frame, length, cf)) {
            process_ack(cf);
            m_stats.acks_received++;
            process_event(ARQEvent::ACK_RECEIVED);
        }
    }
}

void VariableARQ::update(uint32_t current_time_ms)
{
    m_last_tx_time = current_time_ms;
    
    if (m_state == ARQState::WAIT_ACK) {
        check_timeouts(current_time_ms);
    }
}

void VariableARQ::set_data_rate(DataRate rate)
{
    m_data_rate = rate;
}

bool VariableARQ::is_transfer_complete() const
{
    if (m_state == ARQState::IDLE && !m_tx_blocks.empty()) {
        return all_blocks_acked();
    }
    return m_state == ARQState::IDLE;
}

// Private methods

void VariableARQ::transition_to(ARQState new_state)
{
    if (new_state != m_state) {
        ARQState old = m_state;
        m_prev_state = old;
        m_state = new_state;
        
        if (m_state_callback) {
            m_state_callback(old, new_state);
        }
    }
}

void VariableARQ::send_next_blocks()
{
    int sent = 0;
    
    // Send up to window_size blocks
    while (sent < m_window_size && m_next_tx_sequence < m_tx_blocks.size()) {
        uint8_t seq = m_next_tx_sequence;
        DataBlock* block = find_block(seq);
        
        if (block && !block->acknowledged) {
            send_block(seq);
            sent++;
        }
        
        m_next_tx_sequence++;
        if (m_next_tx_sequence >= 256) {
            m_next_tx_sequence = 0;
        }
    }
    
    if (sent > 0) {
        process_event(ARQEvent::FRAME_SENT);
    }
}

void VariableARQ::send_block(uint8_t sequence)
{
    DataBlock* block = find_block(sequence);
    if (!block || !m_tx_callback) {
        return;
    }
    
    // Build data frame
    DataFrame frame;
    frame.data_rate_format = DataRateFormat::ABSOLUTE;
    frame.data_rate = static_cast<uint8_t>(m_data_rate);
    frame.interleaver_length = InterleaverLength::SHORT;
    frame.sequence_number = block->sequence;
    frame.msg_byte_offset = block->offset;
    frame.data_length = block->length;
    memcpy(frame.data, block->data, block->length);
    
    // Format and send
    uint8_t buffer[1200];
    int length = FrameFormatter::format_data_frame(frame, buffer, sizeof(buffer));
    
    if (length > 0) {
        m_tx_callback(buffer, length);
        block->timestamp = m_last_tx_time;
        m_stats.blocks_sent++;
    }
}

void VariableARQ::send_ack()
{
    if (!m_tx_callback) {
        return;
    }
    
    ControlFrame frame;
    frame.protocol_version = PROTOCOL_VERSION;
    frame.arq_mode = ARQMode::VARIABLE_ARQ;
    frame.ack_nak_type = AckNakType::DATA_ACK;
    
    // Build ACK bitmap
    for (int i = 0; i < 256; i++) {
        if (m_rx_bitmap[i]) {
            int byte_idx = i / 8;
            int bit_idx = i % 8;
            frame.bit_map[byte_idx] |= (1 << bit_idx);
        }
    }
    
    uint8_t buffer[256];
    int length = FrameFormatter::format_control_frame(frame, buffer, sizeof(buffer));
    
    if (length > 0) {
        m_tx_callback(buffer, length);
        m_stats.acks_sent++;
        process_event(ARQEvent::FRAME_SENT);
    }
}

void VariableARQ::send_nak(uint8_t sequence)
{
    // Queue for retransmission
    m_retransmit_queue.push(sequence);
}

void VariableARQ::process_ack(const ControlFrame& frame)
{
    if (frame.ack_nak_type != AckNakType::DATA_ACK) {
        return;
    }
    
    // Process ACK bitmap
    for (int i = 0; i < 256; i++) {
        int byte_idx = i / 8;
        int bit_idx = i % 8;
        bool acked = (frame.bit_map[byte_idx] & (1 << bit_idx)) != 0;
        
        if (acked) {
            mark_block_acked(i);
        }
    }
}

void VariableARQ::process_data_frame(const DataFrame& frame)
{
    uint8_t seq = frame.sequence_number;
    
    // Check for duplicates
    if (m_rx_bitmap[seq]) {
        // Already received, ignore
        return;
    }
    
    // Mark as received
    m_rx_bitmap[seq] = true;
    
    // Store data
    uint32_t offset = frame.msg_byte_offset;
    if (offset + frame.data_length > m_rx_buffer.size()) {
        m_rx_buffer.resize(offset + frame.data_length);
    }
    
    memcpy(&m_rx_buffer[offset], frame.data, frame.data_length);
    
    // Check if all blocks received
    bool all_received = true;
    for (int i = 0; i < 256; i++) {
        if (!m_rx_bitmap[i]) {
            // Check if this block exists in the message
            // For now, assume sequential
            if (i < m_next_tx_sequence) {
                all_received = false;
                break;
            }
        }
    }
    
    if (all_received) {
        process_event(ARQEvent::TRANSFER_COMPLETE);
    }
}

void VariableARQ::check_timeouts(uint32_t current_time)
{
    if (current_time - m_wait_start_time > m_ack_timeout) {
        // Timeout - queue all unacked blocks for retransmit
        for (const auto& block : m_tx_blocks) {
            if (!block.acknowledged) {
                m_retransmit_queue.push(block.sequence);
            }
        }
        process_event(ARQEvent::TIMEOUT);
    }
}

bool VariableARQ::all_blocks_acked() const
{
    for (const auto& block : m_tx_blocks) {
        if (!block.acknowledged) {
            return false;
        }
    }
    return true;
}

void VariableARQ::report_error(const char* msg)
{
    if (m_error_callback) {
        m_error_callback(msg);
    }
}

void VariableARQ::create_blocks(const uint8_t* data, uint32_t length)
{
    m_tx_blocks.clear();
    m_next_tx_sequence = 0;
    m_window_base = 0;
    
    uint32_t offset = 0;
    uint8_t seq = 0;
    
    while (offset < length) {
        DataBlock block;
        block.sequence = seq;
        block.offset = offset;
        block.length = std::min(static_cast<uint32_t>(MAX_DATA_BLOCK_LENGTH), 
                                length - offset);
        memcpy(block.data, data + offset, block.length);
        block.acknowledged = false;
        block.retransmit_count = 0;
        block.timestamp = 0;
        
        m_tx_blocks.push_back(block);
        
        offset += block.length;
        seq++;
        if (seq >= 256) {
            seq = 0;
        }
    }
}

void VariableARQ::mark_block_acked(uint8_t sequence)
{
    DataBlock* block = find_block(sequence);
    if (block) {
        block->acknowledged = true;
    }
}

DataBlock* VariableARQ::find_block(uint8_t sequence)
{
    for (auto& block : m_tx_blocks) {
        if (block.sequence == sequence) {
            return &block;
        }
    }
    return nullptr;
}

void VariableARQ::reassemble_data()
{
    // Data is already reassembled in m_rx_buffer
    // Just verify integrity
}

// Utility functions

const char* arq_state_name(ARQState state)
{
    switch (state) {
        case ARQState::IDLE: return "IDLE";
        case ARQState::TX_DATA: return "TX_DATA";
        case ARQState::WAIT_ACK: return "WAIT_ACK";
        case ARQState::RX_DATA: return "RX_DATA";
        case ARQState::SEND_ACK: return "SEND_ACK";
        case ARQState::RETRANSMIT: return "RETRANSMIT";
        case ARQState::ERROR: return "ERROR";
        default: return "UNKNOWN";
    }
}

const char* arq_event_name(ARQEvent event)
{
    switch (event) {
        case ARQEvent::START_TX: return "START_TX";
        case ARQEvent::DATA_READY: return "DATA_READY";
        case ARQEvent::FRAME_SENT: return "FRAME_SENT";
        case ARQEvent::ACK_RECEIVED: return "ACK_RECEIVED";
        case ARQEvent::NAK_RECEIVED: return "NAK_RECEIVED";
        case ARQEvent::TIMEOUT: return "TIMEOUT";
        case ARQEvent::START_RX: return "START_RX";
        case ARQEvent::FRAME_RECEIVED: return "FRAME_RECEIVED";
        case ARQEvent::TRANSFER_COMPLETE: return "TRANSFER_COMPLETE";
        case ARQEvent::ERROR_EVENT: return "ERROR_EVENT";
        case ARQEvent::RESET: return "RESET";
        default: return "UNKNOWN";
    }
}

} // namespace fs1052
