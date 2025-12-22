/**
 * \file fs1052_protocol.h
 * \brief Federal Standard 1052 Data Link Protocol
 * 
 * Implements FED-STD-1052 ARQ protocol for reliable data transfer over
 * MIL-STD-188-110A HF modem. Provides automatic repeat request, acknowledgments,
 * and adaptive rate control.
 * 
 * Key Features:
 * - 4 ARQ modes: Variable ARQ, Broadcast, Circuit, Fixed ARQ
 * - Auto rate adaptation (75-2400 bps)
 * - Selective reject and retransmission
 * - CRC-32 error detection
 * - Sequence numbering and flow control
 * 
 * Specification: Federal Standard 1052 (FED-STD-1052)
 */

#pragma once

#include <cstdint>
#include <cstring>
#include <vector>
#include <queue>

namespace fs1052 {

// ============================================================================
// Constants per FED-STD-1052
// ============================================================================

constexpr uint8_t PROTOCOL_VERSION = 0;
constexpr uint16_t MAX_DATA_BLOCK_LENGTH = 1023;
constexpr uint8_t ACK_MAP_SIZE = 32;          ///< 256 bits / 8 = 32 bytes
constexpr uint8_t MAX_SEQUENCE_NUMBER = 255;

// ============================================================================
// ARQ Modes per FED-STD-1052
// ============================================================================

/**
 * \enum ARQMode
 * Four operating modes of the data link protocol
 */
enum class ARQMode : uint8_t {
    VARIABLE_ARQ = 0,   ///< Adaptive block size, most common
    BROADCAST = 1,      ///< One-way, no ACK
    CIRCUIT = 2,        ///< Continuous bidirectional
    FIXED_ARQ = 3       ///< Fixed block size with ACK
};

/**
 * \enum LinkState
 * Link establishment states
 */
enum class LinkState : uint8_t {
    CALLING = 0,        ///< Initiating call
    CALL_ACK = 1,       ///< Call acknowledged
    LINK_UP = 2,        ///< Link established
    DROPPING = 3        ///< Terminating link
};

/**
 * \enum FrameType
 * Frame type identifiers
 */
enum class FrameType : uint8_t {
    NO_FRAME = 0,
    T1_CONTROL = 1,     ///< Type 1 control frame (short)
    T2_CONTROL = 2,     ///< Type 2 control frame (with bitmap)
    T3_CONTROL = 3,     ///< Type 3 control frame (with herald & message)
    T4_CONTROL = 4,     ///< Type 4 control frame (broadcast/fixed ARQ)
    DATA = 5            ///< Data frame
};

/**
 * \enum AddressMode
 * Address field length
 */
enum class AddressMode : uint8_t {
    SHORT_2_BYTE = 0,   ///< 2-byte address (abbreviated)
    LONG_18_BYTE = 1    ///< 18-byte address (full)
};

/**
 * \enum AckNakType
 * Acknowledgment/Negative acknowledgment types
 */
enum class AckNakType : uint8_t {
    NULL_ACK = 0,       ///< No acknowledgment field
    DATA_ACK = 1,       ///< Acknowledging data blocks
    DATA_ACK_REQ = 2,   ///< Requesting data acknowledgment
    HERALD_ACK = 3      ///< Acknowledging herald
};

/**
 * \enum DataRate
 * Absolute data rates supported by MIL-STD-188-110A modem
 */
enum class DataRate : uint8_t {
    BPS_75 = 0,
    BPS_150 = 1,
    BPS_300 = 2,
    BPS_600 = 3,
    BPS_1200 = 4,
    BPS_2400 = 5,
    BPS_4800 = 6,
    SAME = 7            ///< Keep current rate
};

/**
 * \enum DataRateFormat
 * How data rate is specified
 */
enum class DataRateFormat : uint8_t {
    ABSOLUTE = 0,       ///< Absolute value (75, 150, 300, etc.)
    RELATIVE = 1        ///< Relative change (div/2, mul/2, etc.)
};

/**
 * \enum RelativeDataRate
 * Relative data rate changes
 */
enum class RelativeDataRate : uint8_t {
    DIV_8 = 0,
    DIV_4 = 1,
    DIV_2 = 2,
    SAME = 3,
    MUL_2 = 4,
    MUL_4 = 5,
    MUL_8 = 6,
    REL_SAME = 7
};

/**
 * \enum InterleaverLength
 * Interleaver length for error correction
 */
enum class InterleaverLength : uint8_t {
    SHORT = 0,          ///< Short interleaver (faster, less robust)
    LONG = 1            ///< Long interleaver (slower, more robust)
};

/**
 * \enum NegotiationMode
 * When to negotiate modem parameters
 */
enum class NegotiationMode : uint8_t {
    CHANGES_ONLY = 0,   ///< Negotiate only when changing
    EVERY_TIME = 1      ///< Negotiate every transmission
};

// ============================================================================
// Frame Structures per FED-STD-1052
// ============================================================================

/**
 * \struct ControlFrame
 * Control frame for link management, heralds, acknowledgments
 * 
 * Format per FED-STD-1052:
 * - Header: Version, mode, addressing
 * - Addresses: Source and destination
 * - Link management: State, timeout
 * - Data transfer: ACK/NAK, bitmap
 * - Herald: Data rate, block size, series length
 * - Message: Size, ID, priority, byte positions
 * - CRC-32: Error detection
 */
struct ControlFrame {
    // Header fields
    uint8_t protocol_version;
    ARQMode arq_mode;
    NegotiationMode neg_mode;
    AddressMode address_mode;
    FrameType frame_type;
    
    // Addresses
    uint8_t src_address_length;
    uint8_t src_address[18];
    uint8_t des_address_length;
    uint8_t des_address[18];
    
    // Link management
    LinkState link_state;
    uint16_t link_timeout;
    
    // Data transfer fields
    AckNakType ack_nak_type;
    uint8_t bit_map[ACK_MAP_SIZE];  ///< 256-bit acknowledgment bitmap
    bool flow_control;
    
    // Herald fields (next data series parameters)
    bool herald_present;
    DataRateFormat data_rate_format;
    uint8_t data_rate;              ///< DataRate or RelativeDataRate
    InterleaverLength interleaver_length;
    uint16_t bytes_in_data_frames;  ///< Bytes per block
    uint8_t frames_in_next_series;  ///< Number of blocks in series
    
    // Message fields (application layer)
    bool message_present;
    uint32_t tx_msg_size;           ///< Total message size in bytes
    uint16_t tx_msg_id;             ///< Message identifier
    uint16_t tx_con_id;             ///< Connection identifier
    uint8_t tx_msg_priority;        ///< Priority (0-15)
    uint32_t tx_msg_next_byte_pos;  ///< Next byte position to send
    uint32_t rx_msg_next_byte_pos;  ///< Next byte position expected
    
    // Extension functions (future use)
    bool extension_function_present;
    uint32_t function_bits[2];
    
    // CRC (calculated on transmission)
    uint32_t crc32;
    
    ControlFrame() : protocol_version(PROTOCOL_VERSION),
                     arq_mode(ARQMode::VARIABLE_ARQ),
                     neg_mode(NegotiationMode::CHANGES_ONLY),
                     address_mode(AddressMode::SHORT_2_BYTE),
                     frame_type(FrameType::NO_FRAME),
                     src_address_length(0), des_address_length(0),
                     link_state(LinkState::CALLING), link_timeout(0),
                     ack_nak_type(AckNakType::NULL_ACK), flow_control(false),
                     herald_present(false), data_rate_format(DataRateFormat::ABSOLUTE),
                     data_rate(static_cast<uint8_t>(DataRate::BPS_2400)),
                     interleaver_length(InterleaverLength::LONG),
                     bytes_in_data_frames(0), frames_in_next_series(0),
                     message_present(false), tx_msg_size(0), tx_msg_id(0),
                     tx_con_id(0), tx_msg_priority(0), tx_msg_next_byte_pos(0),
                     rx_msg_next_byte_pos(0),
                     extension_function_present(false), crc32(0) {
        memset(src_address, 0, sizeof(src_address));
        memset(des_address, 0, sizeof(des_address));
        memset(bit_map, 0, sizeof(bit_map));
        function_bits[0] = function_bits[1] = 0;
    }
};

/**
 * \struct DataFrame
 * Data frame for payload transmission
 * 
 * Format per FED-STD-1052:
 * - Header: Data rate, interleaver
 * - Sequence: Block number (0-255)
 * - Offset: Byte position in message
 * - Payload: Up to 1023 bytes
 * - CRC-32: Error detection
 */
struct DataFrame {
    // Header
    DataRateFormat data_rate_format;
    uint8_t data_rate;
    InterleaverLength interleaver_length;
    
    // Sequence and position
    uint8_t sequence_number;        ///< Block sequence (0-255, wraps)
    uint32_t msg_byte_offset;       ///< Position in message
    
    // Payload
    uint16_t data_length;           ///< Actual bytes in this block
    uint8_t data[MAX_DATA_BLOCK_LENGTH];
    
    // CRC
    uint32_t crc32;
    
    DataFrame() : data_rate_format(DataRateFormat::ABSOLUTE),
                  data_rate(static_cast<uint8_t>(DataRate::BPS_2400)),
                  interleaver_length(InterleaverLength::LONG),
                  sequence_number(0), msg_byte_offset(0),
                  data_length(0), crc32(0) {
        memset(data, 0, sizeof(data));
    }
};

// ============================================================================
// Frame Formatter
// ============================================================================

/**
 * \class FrameFormatter
 * Formats FS-1052 frames for transmission
 */
class FrameFormatter {
public:
    /**
     * Format control frame to byte buffer
     * \param frame Control frame structure
     * \param buffer [out] Output buffer (min 256 bytes recommended)
     * \param buffer_size Size of output buffer
     * \return Number of bytes written, or -1 on error
     */
    static int format_control_frame(const ControlFrame& frame, uint8_t* buffer, size_t buffer_size);
    
    /**
     * Format data frame to byte buffer
     * \param frame Data frame structure
     * \param buffer [out] Output buffer (min 1100 bytes recommended)
     * \param buffer_size Size of output buffer
     * \return Number of bytes written, or -1 on error
     */
    static int format_data_frame(const DataFrame& frame, uint8_t* buffer, size_t buffer_size);
    
    /**
     * Calculate CRC-32 per FED-STD-1003A
     * \param data Input data
     * \param length Data length in bytes
     * \return 32-bit CRC value
     */
    static uint32_t calculate_crc32(const uint8_t* data, size_t length);
    
    /**
     * Append CRC-32 to frame
     * \param buffer Frame buffer
     * \param length Current frame length (before CRC)
     * \return New length (with CRC appended)
     */
    static size_t append_crc32(uint8_t* buffer, size_t length);
};

// ============================================================================
// Frame Parser
// ============================================================================

/**
 * \class FrameParser
 * Parses received FS-1052 frames
 */
class FrameParser {
public:
    /**
     * Parse control frame from buffer
     * \param buffer Input buffer
     * \param length Buffer length
     * \param frame [out] Parsed control frame
     * \return true if parsing successful and CRC valid
     */
    static bool parse_control_frame(const uint8_t* buffer, size_t length, ControlFrame& frame);
    
    /**
     * Parse data frame from buffer
     * \param buffer Input buffer
     * \param length Buffer length
     * \param frame [out] Parsed data frame
     * \return true if parsing successful and CRC valid
     */
    static bool parse_data_frame(const uint8_t* buffer, size_t length, DataFrame& frame);
    
    /**
     * Validate CRC-32 in frame
     * \param buffer Frame buffer (including CRC at end)
     * \param length Total length (data + CRC)
     * \return true if CRC valid
     */
    static bool validate_crc32(const uint8_t* buffer, size_t length);
    
    /**
     * Determine frame type from buffer
     * \param buffer Input buffer (at least 1 byte)
     * \return Frame type
     */
    static FrameType detect_frame_type(const uint8_t* buffer);
};

// ============================================================================
// Utility Functions
// ============================================================================

/**
 * Get human-readable name for ARQ mode
 */
const char* arq_mode_name(ARQMode mode);

/**
 * Get human-readable name for data rate
 */
const char* data_rate_name(DataRate rate);

/**
 * Convert data rate enum to bps value
 */
uint16_t data_rate_to_bps(DataRate rate);

/**
 * Convert bps value to data rate enum
 */
DataRate bps_to_data_rate(uint16_t bps);

} // namespace fs1052
