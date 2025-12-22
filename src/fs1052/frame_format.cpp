/**
 * \file frame_format.cpp
 * \brief FS-1052 frame formatting and parsing implementation
 */

#include "fs1052_protocol.h"
#include <cstring>

namespace fs1052 {

// ============================================================================
// CRC-32 Implementation (FED-STD-1003A)
// ============================================================================

// CRC-32 polynomial: 0x04C11DB7 (standard Ethernet polynomial)
static constexpr uint32_t CRC32_POLYNOMIAL = 0x04C11DB7;

static uint32_t crc32_byte(uint8_t data, uint32_t crc) {
    crc ^= (static_cast<uint32_t>(data) << 24);
    
    for (int i = 0; i < 8; i++) {
        if (crc & 0x80000000) {
            crc = (crc << 1) ^ CRC32_POLYNOMIAL;
        } else {
            crc = (crc << 1);
        }
    }
    
    return crc;
}

uint32_t FrameFormatter::calculate_crc32(const uint8_t* data, size_t length) {
    uint32_t crc = 0xFFFFFFFF;  // Initial value
    
    for (size_t i = 0; i < length; i++) {
        crc = crc32_byte(data[i], crc);
    }
    
    return ~crc;  // Final XOR
}

size_t FrameFormatter::append_crc32(uint8_t* buffer, size_t length) {
    uint32_t crc = calculate_crc32(buffer, length);
    
    // Append CRC in big-endian format
    buffer[length + 0] = (crc >> 24) & 0xFF;
    buffer[length + 1] = (crc >> 16) & 0xFF;
    buffer[length + 2] = (crc >> 8) & 0xFF;
    buffer[length + 3] = crc & 0xFF;
    
    return length + 4;
}

// ============================================================================
// Control Frame Formatting
// ============================================================================

int FrameFormatter::format_control_frame(const ControlFrame& frame, uint8_t* buffer, size_t buffer_size) {
    if (buffer_size < 256) {
        return -1;  // Insufficient buffer
    }
    
    size_t index = 0;
    
    // Byte 0: Header byte
    // Bit 0: Sync mismatch (always 1 per spec)
    // Bit 1: Control frame indicator (1 for control)
    // Bits 2-3: Protocol version
    // Bits 4-5: ARQ mode
    // Bit 6: Negotiation mode
    // Bit 7: Address mode
    buffer[index] = 0x01;  // Sync mismatch bit
    buffer[index] |= 0x02;  // Control frame bit
    buffer[index] |= (frame.protocol_version & 0x03) << 2;
    buffer[index] |= (static_cast<uint8_t>(frame.arq_mode) & 0x03) << 4;
    buffer[index] |= (static_cast<uint8_t>(frame.neg_mode) & 0x01) << 6;
    buffer[index] |= (static_cast<uint8_t>(frame.address_mode) & 0x01) << 7;
    index++;
    
    // Addresses
    if (frame.address_mode == AddressMode::SHORT_2_BYTE) {
        // 2-byte abbreviated addresses (last 2 chars of each)
        if (frame.src_address_length >= 1) {
            buffer[index++] = frame.src_address[frame.src_address_length - 1];
        } else {
            buffer[index++] = 0;
        }
        if (frame.src_address_length >= 2) {
            buffer[index++] = frame.src_address[frame.src_address_length - 2];
        } else {
            buffer[index++] = 0;
        }
        
        if (frame.des_address_length >= 1) {
            buffer[index++] = frame.des_address[frame.des_address_length - 1];
        } else {
            buffer[index++] = 0;
        }
        if (frame.des_address_length >= 2) {
            buffer[index++] = frame.des_address[frame.des_address_length - 2];
        } else {
            buffer[index++] = 0;
        }
    } else {
        // 18-byte full addresses
        memcpy(&buffer[index], frame.src_address, 18);
        index += 18;
        memcpy(&buffer[index], frame.des_address, 18);
        index += 18;
    }
    
    // Link management fields
    buffer[index++] = static_cast<uint8_t>(frame.link_state);
    buffer[index++] = (frame.link_timeout >> 8) & 0xFF;
    buffer[index++] = frame.link_timeout & 0xFF;
    
    // Data transfer fields
    buffer[index] = static_cast<uint8_t>(frame.ack_nak_type) & 0x03;
    index++;
    
    // ACK bitmap (for T2/T3 frames with DATA_ACK)
    if ((frame.frame_type == FrameType::T2_CONTROL || 
         frame.frame_type == FrameType::T3_CONTROL ||
         frame.frame_type == FrameType::T4_CONTROL) &&
        (frame.ack_nak_type == AckNakType::DATA_ACK)) {
        
        if (frame.address_mode == AddressMode::SHORT_2_BYTE) {
            // Copy bitmap
            memcpy(&buffer[index], frame.bit_map, ACK_MAP_SIZE);
            
            // Set flow control bit in last byte if needed
            if (frame.flow_control) {
                buffer[index + ACK_MAP_SIZE - 1] |= 0x80;
            }
            
            index += ACK_MAP_SIZE;
        }
    }
    
    // Herald fields (if present)
    if (frame.herald_present) {
        buffer[index++] = (static_cast<uint8_t>(frame.data_rate_format) << 7) |
                          (frame.data_rate & 0x07);
        buffer[index++] = static_cast<uint8_t>(frame.interleaver_length);
        buffer[index++] = (frame.bytes_in_data_frames >> 8) & 0xFF;
        buffer[index++] = frame.bytes_in_data_frames & 0xFF;
        buffer[index++] = frame.frames_in_next_series;
    }
    
    // Message fields (if present)
    if (frame.message_present) {
        buffer[index++] = (frame.tx_msg_size >> 24) & 0xFF;
        buffer[index++] = (frame.tx_msg_size >> 16) & 0xFF;
        buffer[index++] = (frame.tx_msg_size >> 8) & 0xFF;
        buffer[index++] = frame.tx_msg_size & 0xFF;
        
        buffer[index++] = (frame.tx_msg_id >> 8) & 0xFF;
        buffer[index++] = frame.tx_msg_id & 0xFF;
        
        buffer[index++] = (frame.tx_con_id >> 8) & 0xFF;
        buffer[index++] = frame.tx_con_id & 0xFF;
        
        buffer[index++] = frame.tx_msg_priority;
        
        buffer[index++] = (frame.tx_msg_next_byte_pos >> 24) & 0xFF;
        buffer[index++] = (frame.tx_msg_next_byte_pos >> 16) & 0xFF;
        buffer[index++] = (frame.tx_msg_next_byte_pos >> 8) & 0xFF;
        buffer[index++] = frame.tx_msg_next_byte_pos & 0xFF;
        
        buffer[index++] = (frame.rx_msg_next_byte_pos >> 24) & 0xFF;
        buffer[index++] = (frame.rx_msg_next_byte_pos >> 16) & 0xFF;
        buffer[index++] = (frame.rx_msg_next_byte_pos >> 8) & 0xFF;
        buffer[index++] = frame.rx_msg_next_byte_pos & 0xFF;
    }
    
    // Extension function fields (if present)
    if (frame.extension_function_present) {
        buffer[index++] = (frame.function_bits[0] >> 24) & 0xFF;
        buffer[index++] = (frame.function_bits[0] >> 16) & 0xFF;
        buffer[index++] = (frame.function_bits[0] >> 8) & 0xFF;
        buffer[index++] = frame.function_bits[0] & 0xFF;
        
        buffer[index++] = (frame.function_bits[1] >> 24) & 0xFF;
        buffer[index++] = (frame.function_bits[1] >> 16) & 0xFF;
        buffer[index++] = (frame.function_bits[1] >> 8) & 0xFF;
        buffer[index++] = frame.function_bits[1] & 0xFF;
    }
    
    // Append CRC-32
    index = append_crc32(buffer, index);
    
    return static_cast<int>(index);
}

// ============================================================================
// Data Frame Formatting
// ============================================================================

int FrameFormatter::format_data_frame(const DataFrame& frame, uint8_t* buffer, size_t buffer_size) {
    if (buffer_size < frame.data_length + 20) {
        return -1;  // Insufficient buffer
    }
    
    size_t index = 0;
    
    // Byte 0: Header byte
    // Bit 0: Sync mismatch (always 1)
    // Bit 1: Data frame indicator (0 for data)
    // Bits 2-3: Reserved
    // Bits 4-6: Data rate (format depends on bit 7)
    // Bit 7: Data rate format
    buffer[index] = 0x01;  // Sync mismatch bit
    buffer[index] |= (static_cast<uint8_t>(frame.data_rate_format) << 7);
    buffer[index] |= (frame.data_rate & 0x07) << 4;
    index++;
    
    // Interleaver length
    buffer[index++] = static_cast<uint8_t>(frame.interleaver_length);
    
    // Sequence number
    buffer[index++] = frame.sequence_number;
    
    // Message byte offset (4 bytes)
    buffer[index++] = (frame.msg_byte_offset >> 24) & 0xFF;
    buffer[index++] = (frame.msg_byte_offset >> 16) & 0xFF;
    buffer[index++] = (frame.msg_byte_offset >> 8) & 0xFF;
    buffer[index++] = frame.msg_byte_offset & 0xFF;
    
    // Data length (2 bytes)
    buffer[index++] = (frame.data_length >> 8) & 0xFF;
    buffer[index++] = frame.data_length & 0xFF;
    
    // Data payload
    memcpy(&buffer[index], frame.data, frame.data_length);
    index += frame.data_length;
    
    // Append CRC-32
    index = append_crc32(buffer, index);
    
    return static_cast<int>(index);
}

// ============================================================================
// Frame Parsing
// ============================================================================

FrameType FrameParser::detect_frame_type(const uint8_t* buffer) {
    if (buffer[0] & 0x02) {
        // Control frame - need more bytes to determine type
        // For now, return generic control
        return FrameType::T1_CONTROL;
    } else {
        return FrameType::DATA;
    }
}

bool FrameParser::validate_crc32(const uint8_t* buffer, size_t length) {
    if (length < 4) {
        return false;
    }
    
    // Calculate CRC on all bytes except last 4
    uint32_t calculated_crc = FrameFormatter::calculate_crc32(buffer, length - 4);
    
    // Extract CRC from last 4 bytes
    uint32_t received_crc = (static_cast<uint32_t>(buffer[length - 4]) << 24) |
                            (static_cast<uint32_t>(buffer[length - 3]) << 16) |
                            (static_cast<uint32_t>(buffer[length - 2]) << 8) |
                            static_cast<uint32_t>(buffer[length - 1]);
    
    return (calculated_crc == received_crc);
}

bool FrameParser::parse_control_frame(const uint8_t* buffer, size_t length, ControlFrame& frame) {
    if (length < 10) {
        return false;  // Too short
    }
    
    // Validate CRC first
    if (!validate_crc32(buffer, length)) {
        return false;
    }
    
    size_t index = 0;
    
    // Parse header byte
    frame.protocol_version = (buffer[index] >> 2) & 0x03;
    frame.arq_mode = static_cast<ARQMode>((buffer[index] >> 4) & 0x03);
    frame.neg_mode = static_cast<NegotiationMode>((buffer[index] >> 6) & 0x01);
    frame.address_mode = static_cast<AddressMode>((buffer[index] >> 7) & 0x01);
    index++;
    
    // Parse addresses
    if (frame.address_mode == AddressMode::SHORT_2_BYTE) {
        frame.src_address_length = 2;
        frame.src_address[1] = buffer[index++];
        frame.src_address[0] = buffer[index++];
        
        frame.des_address_length = 2;
        frame.des_address[1] = buffer[index++];
        frame.des_address[0] = buffer[index++];
    } else {
        frame.src_address_length = 18;
        memcpy(frame.src_address, &buffer[index], 18);
        index += 18;
        
        frame.des_address_length = 18;
        memcpy(frame.des_address, &buffer[index], 18);
        index += 18;
    }
    
    // Parse link management
    if (index + 3 > length - 4) return false;
    frame.link_state = static_cast<LinkState>(buffer[index++]);
    frame.link_timeout = (static_cast<uint16_t>(buffer[index]) << 8) | buffer[index + 1];
    index += 2;
    
    // Parse data transfer fields
    if (index >= length - 4) return false;
    frame.ack_nak_type = static_cast<AckNakType>(buffer[index++] & 0x03);
    
    // Parse bitmap if present (simplified - check remaining length)
    if (index + ACK_MAP_SIZE <= length - 4) {
        if (frame.address_mode == AddressMode::SHORT_2_BYTE) {
            memcpy(frame.bit_map, &buffer[index], ACK_MAP_SIZE);
            frame.flow_control = (buffer[index + ACK_MAP_SIZE - 1] & 0x80) != 0;
            index += ACK_MAP_SIZE;
        }
    }
    
    // Herald and message fields parsing would continue here
    // Simplified for now - mark as not present
    frame.herald_present = false;
    frame.message_present = false;
    frame.extension_function_present = false;
    
    return true;
}

bool FrameParser::parse_data_frame(const uint8_t* buffer, size_t length, DataFrame& frame) {
    if (length < 13) {  // Minimum: header + seq + offset + length + CRC
        return false;
    }
    
    // Validate CRC
    if (!validate_crc32(buffer, length)) {
        return false;
    }
    
    size_t index = 0;
    
    // Parse header
    frame.data_rate_format = static_cast<DataRateFormat>((buffer[index] >> 7) & 0x01);
    frame.data_rate = (buffer[index] >> 4) & 0x07;
    index++;
    
    // Interleaver length
    frame.interleaver_length = static_cast<InterleaverLength>(buffer[index++]);
    
    // Sequence number
    frame.sequence_number = buffer[index++];
    
    // Message byte offset
    frame.msg_byte_offset = (static_cast<uint32_t>(buffer[index]) << 24) |
                            (static_cast<uint32_t>(buffer[index + 1]) << 16) |
                            (static_cast<uint32_t>(buffer[index + 2]) << 8) |
                            static_cast<uint32_t>(buffer[index + 3]);
    index += 4;
    
    // Data length
    frame.data_length = (static_cast<uint16_t>(buffer[index]) << 8) | buffer[index + 1];
    index += 2;
    
    // Validate data length
    if (frame.data_length > MAX_DATA_BLOCK_LENGTH || 
        index + frame.data_length + 4 != length) {
        return false;
    }
    
    // Copy data
    memcpy(frame.data, &buffer[index], frame.data_length);
    
    return true;
}

// ============================================================================
// Utility Functions
// ============================================================================

const char* arq_mode_name(ARQMode mode) {
    switch (mode) {
        case ARQMode::VARIABLE_ARQ: return "Variable ARQ";
        case ARQMode::BROADCAST: return "Broadcast";
        case ARQMode::CIRCUIT: return "Circuit";
        case ARQMode::FIXED_ARQ: return "Fixed ARQ";
        default: return "Unknown";
    }
}

const char* data_rate_name(DataRate rate) {
    switch (rate) {
        case DataRate::BPS_75: return "75 bps";
        case DataRate::BPS_150: return "150 bps";
        case DataRate::BPS_300: return "300 bps";
        case DataRate::BPS_600: return "600 bps";
        case DataRate::BPS_1200: return "1200 bps";
        case DataRate::BPS_2400: return "2400 bps";
        case DataRate::BPS_4800: return "4800 bps";
        case DataRate::SAME: return "Same";
        default: return "Unknown";
    }
}

uint16_t data_rate_to_bps(DataRate rate) {
    switch (rate) {
        case DataRate::BPS_75: return 75;
        case DataRate::BPS_150: return 150;
        case DataRate::BPS_300: return 300;
        case DataRate::BPS_600: return 600;
        case DataRate::BPS_1200: return 1200;
        case DataRate::BPS_2400: return 2400;
        case DataRate::BPS_4800: return 4800;
        default: return 0;
    }
}

DataRate bps_to_data_rate(uint16_t bps) {
    if (bps <= 75) return DataRate::BPS_75;
    if (bps <= 150) return DataRate::BPS_150;
    if (bps <= 300) return DataRate::BPS_300;
    if (bps <= 600) return DataRate::BPS_600;
    if (bps <= 1200) return DataRate::BPS_1200;
    if (bps <= 2400) return DataRate::BPS_2400;
    return DataRate::BPS_4800;
}

} // namespace fs1052
