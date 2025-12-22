# PC-ALE 2.0 - API Reference

Complete API documentation for all public interfaces across all five phases.

---

## Table of Contents

- [Phase 1: 8-FSK Modem Core](#phase-1-8-fsk-modem-core)
- [Phase 2: Protocol Layer](#phase-2-protocol-layer)
- [Phase 3: Link State Machine](#phase-3-link-state-machine)
- [Phase 4: AQC-ALE Extensions](#phase-4-aqc-ale-extensions)
- [Phase 5: FS-1052 ARQ Protocol](#phase-5-fs-1052-arq-protocol)

---

## Phase 1: 8-FSK Modem Core

### FFTDemodulator

**Purpose**: Demodulates 8-FSK audio signals using FFT analysis.

#### Constructor

```cpp
FFTDemodulator(int sample_rate = 8000);
```

**Parameters:**
- `sample_rate` - Audio sample rate in Hz (default: 8000)

**Example:**
```cpp
FFTDemodulator demod(8000);
```

#### Methods

##### `demodulate()`

```cpp
bool demodulate(const int16_t* samples, int count, uint8_t& symbol);
```

Processes audio samples and extracts the next 8-FSK symbol.

**Parameters:**
- `samples` - Array of audio samples (16-bit signed)
- `count` - Number of samples
- `symbol` - Output: Decoded symbol (0-7)

**Returns:**
- `true` if a symbol was successfully decoded
- `false` if more samples are needed

**Example:**
```cpp
int16_t audio_buffer[128];
uint8_t decoded_symbol;

if (demod.demodulate(audio_buffer, 128, decoded_symbol)) {
    std::cout << "Symbol: " << (int)decoded_symbol << "\n";
}
```

##### `get_snr()`

```cpp
float get_snr() const;
```

Returns the current Signal-to-Noise Ratio estimate.

**Returns:**
- SNR in dB (typical range: 0-30 dB)

---

### ToneGenerator

**Purpose**: Generates 8-FSK audio tones for transmission.

#### Constructor

```cpp
ToneGenerator(int sample_rate = 8000);
```

#### Methods

##### `generate_tone()`

```cpp
int generate_tone(uint8_t symbol, int16_t* buffer, int max_samples);
```

Generates audio samples for a given symbol.

**Parameters:**
- `symbol` - Symbol to transmit (0-7)
- `buffer` - Output buffer for audio samples
- `max_samples` - Maximum number of samples to generate

**Returns:**
- Number of samples generated

**Example:**
```cpp
ToneGenerator gen(8000);
int16_t tx_buffer[640];  // 80ms at 8 kHz

int samples = gen.generate_tone(5, tx_buffer, 640);
// Send tx_buffer to sound card
```

##### `get_tone_frequency()`

```cpp
static int get_tone_frequency(uint8_t symbol);
```

Returns the frequency (Hz) for a given symbol.

**Parameters:**
- `symbol` - Symbol index (0-7)

**Returns:**
- Frequency in Hz (750, 875, 1000, ..., 1750)

**Example:**
```cpp
int freq = ToneGenerator::get_tone_frequency(3);  // Returns 1125 Hz
```

---

### GolayEncoder / GolayDecoder

**Purpose**: Golay (24,12) forward error correction.

#### GolayEncoder

```cpp
class GolayEncoder {
public:
    static uint32_t encode(uint16_t data_12bit);
};
```

**Example:**
```cpp
uint16_t data = 0x0ABC;  // 12-bit data
uint32_t codeword = GolayEncoder::encode(data);  // 24-bit codeword
```

#### GolayDecoder

```cpp
class GolayDecoder {
public:
    static bool decode(uint32_t received, uint16_t& data_out, int& errors_corrected);
};
```

**Parameters:**
- `received` - 24-bit received codeword
- `data_out` - Output: Corrected 12-bit data
- `errors_corrected` - Output: Number of bit errors corrected (0-3)

**Returns:**
- `true` if decoding successful (≤3 errors)
- `false` if uncorrectable (>3 errors)

**Example:**
```cpp
uint32_t received_word = 0x00ABCDEF;
uint16_t data;
int errors;

if (GolayDecoder::decode(received_word, data, errors)) {
    std::cout << "Data: " << std::hex << data 
              << " (corrected " << errors << " errors)\n";
} else {
    std::cout << "Uncorrectable errors\n";
}
```

---

## Phase 2: Protocol Layer

### ALEWord

**Purpose**: Represents a single 24-bit ALE word (3-bit preamble + 21-bit payload).

#### Structure

```cpp
struct ALEWord {
    PreambleType preamble;  // Word type
    uint32_t payload;       // 21-bit data
    
    // Methods
    std::string to_string() const;
    bool is_valid() const;
};
```

#### Enumerations

```cpp
enum class PreambleType {
    DATA  = 0,  // Data character
    THRU  = 1,  // THRU (pass through)
    TO    = 2,  // TO address
    TIS   = 3,  // TIS (This Is)
    TWS   = 4,  // TWS (This Was)
    FROM  = 5,  // FROM address
    CMD   = 6,  // Command
    REP   = 7   // Repeat
};
```

#### Functions

##### `encode_character()`

```cpp
ALEWord encode_character(char c, PreambleType preamble);
```

Encodes an ASCII character into an ALE word.

**Parameters:**
- `c` - Character to encode
- `preamble` - Word type (usually DATA)

**Returns:**
- Encoded `ALEWord`

**Example:**
```cpp
ALEWord word = encode_character('A', PreambleType::DATA);
```

##### `decode_character()`

```cpp
char decode_character(const ALEWord& word);
```

Decodes an ALE word to an ASCII character.

**Returns:**
- Decoded character, or `'\0'` if invalid

**Example:**
```cpp
char ch = decode_character(word);
if (ch != '\0') {
    std::cout << "Character: " << ch << "\n";
}
```

---

### ALEMessage

**Purpose**: Assembles and manages multi-word ALE messages.

#### Constructor

```cpp
ALEMessage();
```

#### Methods

##### `add_word()`

```cpp
bool add_word(const ALEWord& word);
```

Adds a word to the message sequence.

**Returns:**
- `true` if word was added
- `false` if message is complete or invalid

##### `is_complete()`

```cpp
bool is_complete() const;
```

Checks if the message is complete (has ending marker).

**Returns:**
- `true` if message is complete

##### `get_type()`

```cpp
MessageType get_type() const;
```

Returns the message type.

**Returns:**
- `INDIVIDUAL_CALL`, `NET_CALL`, `SOUNDING`, `AMD`, or `UNKNOWN`

##### `get_to_address()`

```cpp
std::string get_to_address() const;
```

Returns the TO address (destination).

##### `get_from_address()`

```cpp
std::string get_from_address() const;
```

Returns the FROM address (source).

##### `get_content()`

```cpp
std::string get_content() const;
```

Returns the message content (for AMD messages).

**Example:**
```cpp
ALEMessage msg;

// Add words received from demodulator
for (const auto& word : received_words) {
    msg.add_word(word);
    
    if (msg.is_complete()) {
        std::cout << "Call from: " << msg.get_from_address() << "\n";
        std::cout << "Call to: " << msg.get_to_address() << "\n";
        
        if (msg.get_type() == MessageType::AMD) {
            std::cout << "Message: " << msg.get_content() << "\n";
        }
        break;
    }
}
```

---

### AddressBook

**Purpose**: Manages station addresses, net membership, and wildcard matching.

#### Constructor

```cpp
AddressBook();
```

#### Methods

##### `set_self_address()`

```cpp
void set_self_address(const std::string& address);
```

Sets this station's address.

**Parameters:**
- `address` - Station address (up to 15 characters)

**Example:**
```cpp
AddressBook addr_book;
addr_book.set_self_address("MYSTATION");
```

##### `add_station()`

```cpp
void add_station(const std::string& address);
```

Adds a station to the address book.

##### `add_net()`

```cpp
void add_net(const std::string& net_address);
```

Adds a net to the address book.

##### `is_for_me()`

```cpp
bool is_for_me(const std::string& address) const;
```

Checks if an address matches this station or any nets.

**Returns:**
- `true` if address matches self or member net

**Example:**
```cpp
addr_book.add_net("NETONE");

if (addr_book.is_for_me("NETONE")) {
    // This call is for us (we're in NETONE)
}
```

##### `match_address()`

```cpp
bool match_address(const std::string& pattern, const std::string& address) const;
```

Matches address with wildcard support.

**Parameters:**
- `pattern` - Pattern with wildcards (`*`, `?`)
- `address` - Address to match

**Returns:**
- `true` if address matches pattern

**Example:**
```cpp
bool match = addr_book.match_address("BASE*", "BASE01");  // true
match = addr_book.match_address("B?SE01", "BASE01");      // true
```

---

## Phase 3: Link State Machine

### ALEStateMachine

**Purpose**: Core ALE state machine for link establishment and management.

#### Constructor

```cpp
ALEStateMachine();
```

#### Initialization

##### `init()`

```cpp
void init(
    std::function<void(const ALEWord&)> word_callback,
    std::function<void(ALEState, ALEState)> state_callback = nullptr,
    std::function<void(const std::string&, const std::string&)> link_callback = nullptr
);
```

Initializes the state machine with callbacks.

**Parameters:**
- `word_callback` - Called when a word should be transmitted
- `state_callback` - Called on state transitions (optional)
- `link_callback` - Called when link is established (optional)

**Example:**
```cpp
ALEStateMachine sm;

sm.init(
    // Word TX callback
    [](const ALEWord& word) {
        transmit_word(word);
    },
    
    // State change callback
    [](ALEState old_state, ALEState new_state) {
        std::cout << "State: " << state_name(old_state) 
                  << " → " << state_name(new_state) << "\n";
    },
    
    // Link established callback
    [](const std::string& from, const std::string& to) {
        std::cout << "Link established: " << from << " → " << to << "\n";
    }
);
```

#### Channel Management

##### `add_channel()`

```cpp
void add_channel(uint32_t frequency_hz);
```

Adds a channel to the scan list.

**Parameters:**
- `frequency_hz` - Frequency in Hz (e.g., 7100000 for 7.1 MHz)

**Example:**
```cpp
sm.add_channel(7100000);   // 7.1 MHz
sm.add_channel(14109000);  // 14.109 MHz
sm.add_channel(21109000);  // 21.109 MHz
```

##### `set_dwell_time()`

```cpp
void set_dwell_time(uint32_t milliseconds);
```

Sets the dwell time per channel when scanning.

**Parameters:**
- `milliseconds` - Dwell time in ms (default: 200ms)

##### `get_current_channel()`

```cpp
uint32_t get_current_channel() const;
```

Returns the currently selected frequency.

**Returns:**
- Frequency in Hz

#### State Control

##### `start_scanning()`

```cpp
void start_scanning();
```

Starts channel scanning.

**Example:**
```cpp
sm.start_scanning();
// State machine will cycle through channels
```

##### `make_call()`

```cpp
void make_call(const std::string& to_address);
```

Initiates an individual call.

**Parameters:**
- `to_address` - Destination station address

**Example:**
```cpp
sm.make_call("BASE01");
// State machine will handle call sequence
```

##### `make_net_call()`

```cpp
void make_net_call(const std::string& net_address);
```

Initiates a net call.

##### `send_amd()`

```cpp
void send_amd(const std::string& to_address, const std::string& message);
```

Sends an Automatic Message Display (AMD) message.

**Parameters:**
- `to_address` - Destination station
- `message` - Text message (up to 90 characters)

**Example:**
```cpp
sm.send_amd("BASE01", "STATUS OK");
```

##### `terminate_link()`

```cpp
void terminate_link();
```

Terminates the current link and returns to scanning.

#### Event Processing

##### `process_received_word()`

```cpp
void process_received_word(const ALEWord& word);
```

Processes a word received from the demodulator.

**Parameters:**
- `word` - Received ALE word

**Example:**
```cpp
// In receive loop
ALEWord received_word;
if (demod.get_next_word(received_word)) {
    sm.process_received_word(received_word);
}
```

##### `update()`

```cpp
void update(uint32_t current_time_ms);
```

Updates timers and checks for timeouts. Call periodically (e.g., every 100ms).

**Parameters:**
- `current_time_ms` - Current time in milliseconds

**Example:**
```cpp
// Main loop
while (running) {
    uint32_t now = get_time_ms();
    sm.update(now);
    // ... other processing
}
```

#### State Queries

##### `get_state()`

```cpp
ALEState get_state() const;
```

Returns the current state.

**Returns:**
- Current `ALEState` (IDLE, SCANNING, CALLING, HANDSHAKE, LINKED, SOUNDING)

##### `is_linked()`

```cpp
bool is_linked() const;
```

Checks if a link is currently established.

**Returns:**
- `true` if in LINKED state

#### LQA (Link Quality Analysis)

##### `get_lqa()`

```cpp
int get_lqa(uint32_t frequency_hz) const;
```

Gets the Link Quality Analysis score for a frequency.

**Parameters:**
- `frequency_hz` - Frequency to query

**Returns:**
- LQA score (0-31, higher is better)

**Example:**
```cpp
int quality = sm.get_lqa(7100000);
std::cout << "7.1 MHz LQA: " << quality << "/31\n";
```

---

## Phase 4: AQC-ALE Extensions

### AQCParser

**Purpose**: Parses AQC-ALE enhanced messages with Data Elements.

#### Data Elements Structure

```cpp
struct DataElements {
    uint8_t de1;  // Reserved
    uint8_t de2;  // Slot position (0-7)
    uint8_t de3;  // Traffic class (0-15)
    uint8_t de4;  // LQA (0-31)
    uint8_t de5;  // Command/status
    uint8_t de6;  // Extended command
    uint8_t de7;  // Extended status
    uint8_t de8;  // Orderwire command count
    uint8_t de9;  // Transaction code (0-7)
};
```

#### Methods

##### `extract_de()`

```cpp
static DataElements extract_de(uint32_t payload_21bit);
```

Extracts Data Elements from 21-bit payload.

**Parameters:**
- `payload_21bit` - 21-bit payload from ALE word

**Returns:**
- `DataElements` structure with all DE fields

**Example:**
```cpp
ALEWord word = /* received word */;
DataElements de = AQCParser::extract_de(word.payload);

std::cout << "Slot: " << (int)de.de2 << "\n";
std::cout << "Traffic: " << traffic_class_name(de.de3) << "\n";
std::cout << "LQA: " << (int)de.de4 << "\n";
```

##### `parse_call_probe()`

```cpp
bool parse_call_probe(const ALEWord* words, int count, AQCCallProbe& probe);
```

Parses an AQC call probe (TO call with DE fields).

**Parameters:**
- `words` - Array of received words
- `count` - Number of words
- `probe` - Output: Parsed call probe

**Returns:**
- `true` if successfully parsed

##### `traffic_class_name()`

```cpp
static const char* traffic_class_name(uint8_t traffic_class);
```

Converts traffic class code to human-readable string.

**Returns:**
- Traffic class name (e.g., "Clear Voice", "HF Email")

**Example:**
```cpp
const char* name = AQCParser::traffic_class_name(de.de3);
std::cout << "Traffic: " << name << "\n";
```

##### `transaction_code_name()`

```cpp
static const char* transaction_code_name(uint8_t code);
```

Converts transaction code to name.

**Returns:**
- Transaction name (e.g., "ACK", "NAK", "TERMINATE")

---

### AQCCRC

**Purpose**: CRC calculation and validation for orderwire messages.

#### Methods

##### `calculate_crc8()`

```cpp
static uint8_t calculate_crc8(const uint8_t* data, size_t length);
```

Calculates CRC-8 (polynomial 0x07).

**Parameters:**
- `data` - Data bytes
- `length` - Number of bytes

**Returns:**
- 8-bit CRC value

**Example:**
```cpp
const char* message = "HELLO";
uint8_t crc = AQCCRC::calculate_crc8((const uint8_t*)message, strlen(message));
std::cout << "CRC-8: 0x" << std::hex << (int)crc << "\n";
```

##### `calculate_crc16()`

```cpp
static uint16_t calculate_crc16(const uint8_t* data, size_t length);
```

Calculates CRC-16 CCITT (polynomial 0x1021).

**Returns:**
- 16-bit CRC value

##### `validate_crc8()` / `validate_crc16()`

```cpp
static bool validate_crc8(const uint8_t* data, size_t length);
static bool validate_crc16(const uint8_t* data, size_t length);
```

Validates CRC (assumes CRC is appended to data).

**Returns:**
- `true` if CRC is valid

**Example:**
```cpp
uint8_t rx_data[100];
int rx_len = receive_orderwire(rx_data);

if (AQCCRC::validate_crc16(rx_data, rx_len)) {
    // CRC valid, process message
} else {
    // CRC error, discard
}
```

---

### SlotManager

**Purpose**: Manages slotted response mechanism for AQC-ALE.

#### Methods

##### `assign_slot()`

```cpp
static uint8_t assign_slot(const std::string& address);
```

Assigns a slot (0-7) based on station address hash.

**Parameters:**
- `address` - Station address

**Returns:**
- Slot number (0-7)

**Example:**
```cpp
uint8_t slot = SlotManager::assign_slot("MYSTATION");
std::cout << "Assigned slot: " << (int)slot << "\n";
```

##### `calculate_slot_time()`

```cpp
static uint32_t calculate_slot_time(uint8_t slot, uint32_t base_time_ms);
```

Calculates the transmission time for a slot.

**Parameters:**
- `slot` - Slot number (0-7)
- `base_time_ms` - Base time (call end time)

**Returns:**
- Transmission time in milliseconds

**Example:**
```cpp
uint8_t my_slot = SlotManager::assign_slot("MYSTATION");
uint32_t tx_time = SlotManager::calculate_slot_time(my_slot, call_end_time);

// Wait until tx_time, then transmit response
```

---

## Phase 5: FS-1052 ARQ Protocol

### VariableARQ

**Purpose**: Implements FED-STD-1052 Variable ARQ mode for reliable data transfer.

#### Constructor

```cpp
VariableARQ();
```

#### Initialization

##### `init()`

```cpp
void init(
    std::function<void(const uint8_t*, int)> tx_callback,
    std::function<void(ARQState, ARQState)> state_callback = nullptr,
    std::function<void(const char*)> error_callback = nullptr
);
```

Initializes ARQ with callbacks.

**Parameters:**
- `tx_callback` - Called to transmit a frame
- `state_callback` - Called on state transitions (optional)
- `error_callback` - Called on errors (optional)

**Example:**
```cpp
VariableARQ arq;

arq.init(
    // TX callback
    [](const uint8_t* frame, int length) {
        modem_transmit(frame, length);
    },
    
    // State callback
    [](ARQState old_state, ARQState new_state) {
        std::cout << "ARQ: " << arq_state_name(old_state)
                  << " → " << arq_state_name(new_state) << "\n";
    },
    
    // Error callback
    [](const char* error) {
        std::cerr << "ARQ Error: " << error << "\n";
    }
);
```

#### Configuration

##### `set_data_rate()`

```cpp
void set_data_rate(DataRate rate);
```

Sets the data rate.

**Parameters:**
- `rate` - Data rate (BPS_75, BPS_150, ..., BPS_4800)

**Example:**
```cpp
arq.set_data_rate(DataRate::BPS_2400);
```

##### `set_window_size()`

```cpp
void set_window_size(uint8_t size);
```

Sets the transmission window size (number of outstanding unacknowledged blocks).

**Parameters:**
- `size` - Window size (default: 16)

##### `set_ack_timeout()`

```cpp
void set_ack_timeout(uint32_t timeout_ms);
```

Sets the ACK timeout in milliseconds.

**Parameters:**
- `timeout_ms` - Timeout in ms (default: 5000)

##### `set_max_retransmissions()`

```cpp
void set_max_retransmissions(uint8_t max);
```

Sets the maximum retransmission attempts.

**Parameters:**
- `max` - Max retries (default: 3)

#### Transmission

##### `start_transmission()`

```cpp
bool start_transmission(const uint8_t* data, uint32_t length);
```

Starts transmitting a message.

**Parameters:**
- `data` - Message data
- `length` - Message length in bytes

**Returns:**
- `true` if transmission started
- `false` if error (not IDLE, no callback, etc.)

**Example:**
```cpp
const char* message = "Important message";
if (arq.start_transmission((const uint8_t*)message, strlen(message))) {
    std::cout << "Transmission started\n";
}
```

#### Reception

##### `handle_received_frame()`

```cpp
void handle_received_frame(const uint8_t* frame, int length);
```

Processes a received frame (data frame or ACK).

**Parameters:**
- `frame` - Received frame bytes
- `length` - Frame length

**Example:**
```cpp
// In receive loop
uint8_t rx_frame[1200];
int rx_len = modem_receive(rx_frame, sizeof(rx_frame));

if (rx_len > 0) {
    arq.handle_received_frame(rx_frame, rx_len);
}
```

##### `get_received_data()`

```cpp
const std::vector<uint8_t>& get_received_data() const;
```

Returns the reassembled received message.

**Returns:**
- Reference to received data buffer

**Example:**
```cpp
if (arq.is_transfer_complete()) {
    const auto& data = arq.get_received_data();
    std::string message((char*)data.data(), data.size());
    std::cout << "Received: " << message << "\n";
}
```

#### Status

##### `get_state()`

```cpp
ARQState get_state() const;
```

Returns the current ARQ state.

**Returns:**
- Current state (IDLE, TX_DATA, WAIT_ACK, RX_DATA, SEND_ACK, RETRANSMIT, ERROR)

##### `is_transfer_complete()`

```cpp
bool is_transfer_complete() const;
```

Checks if the current transfer is complete.

**Returns:**
- `true` if transfer complete or idle

##### `get_stats()`

```cpp
const ARQStats& get_stats() const;
```

Returns statistics.

**Returns:**
- Reference to `ARQStats` structure

**Example:**
```cpp
const auto& stats = arq.get_stats();
std::cout << "Blocks sent: " << stats.blocks_sent << "\n";
std::cout << "Retransmissions: " << stats.blocks_retransmitted << "\n";
std::cout << "Timeouts: " << stats.timeouts << "\n";
std::cout << "CRC errors: " << stats.crc_errors << "\n";
```

#### Maintenance

##### `update()`

```cpp
void update(uint32_t current_time_ms);
```

Updates timers and checks for timeouts. Call periodically.

**Parameters:**
- `current_time_ms` - Current time in milliseconds

**Example:**
```cpp
// Main loop
while (!arq.is_transfer_complete()) {
    uint32_t now = get_time_ms();
    arq.update(now);
    // ... other processing
}
```

##### `reset()`

```cpp
void reset();
```

Resets the ARQ state machine to IDLE.

---

### FrameFormatter / FrameParser

**Purpose**: Formats and parses FS-1052 frames.

#### FrameFormatter Methods

##### `format_control_frame()`

```cpp
static int format_control_frame(const ControlFrame& frame, uint8_t* buffer, int max_length);
```

Formats a control frame.

**Parameters:**
- `frame` - Control frame structure
- `buffer` - Output buffer
- `max_length` - Buffer size

**Returns:**
- Number of bytes written, or -1 on error

##### `format_data_frame()`

```cpp
static int format_data_frame(const DataFrame& frame, uint8_t* buffer, int max_length);
```

Formats a data frame.

**Example:**
```cpp
DataFrame frame;
frame.sequence_number = 0;
frame.msg_byte_offset = 0;
frame.data_length = 10;
memcpy(frame.data, "TEST DATA!", 10);

uint8_t buffer[1200];
int length = FrameFormatter::format_data_frame(frame, buffer, sizeof(buffer));

// Transmit buffer over modem
modem_transmit(buffer, length);
```

#### FrameParser Methods

##### `parse_control_frame()`

```cpp
static bool parse_control_frame(const uint8_t* buffer, int length, ControlFrame& frame);
```

Parses a control frame.

**Returns:**
- `true` if successfully parsed and CRC valid

##### `parse_data_frame()`

```cpp
static bool parse_data_frame(const uint8_t* buffer, int length, DataFrame& frame);
```

Parses a data frame.

##### `detect_frame_type()`

```cpp
static FrameType detect_frame_type(const uint8_t* buffer);
```

Detects the frame type from the first byte.

**Returns:**
- `FrameType` (T1_CONTROL, T2_CONTROL, T3_CONTROL, T4_CONTROL, DATA, NO_FRAME)

**Example:**
```cpp
uint8_t rx_buffer[1200];
int rx_len = receive_frame(rx_buffer);

FrameType type = FrameParser::detect_frame_type(rx_buffer);

if (type == FrameType::DATA) {
    DataFrame df;
    if (FrameParser::parse_data_frame(rx_buffer, rx_len, df)) {
        process_data(df);
    }
} else if (type != FrameType::NO_FRAME) {
    ControlFrame cf;
    if (FrameParser::parse_control_frame(rx_buffer, rx_len, cf)) {
        process_ack(cf);
    }
}
```

---

## Complete Usage Example

Putting it all together:

```cpp
#include "fft_demodulator.h"
#include "ale_state_machine.h"
#include "fs1052_arq.h"

int main() {
    // Phase 1: Physical Layer
    FFTDemodulator demod(8000);
    ToneGenerator tones(8000);
    
    // Phase 2 & 3: Protocol and Link Layer
    ALEStateMachine ale_sm;
    
    ale_sm.init(
        [&](const ALEWord& word) {
            // Transmit word over 8-FSK modem
            for (int i = 0; i < 49; i++) {  // 49 symbols per word
                uint8_t symbol = get_symbol_from_word(word, i);
                int16_t samples[64];
                int n = tones.generate_tone(symbol, samples, 64);
                send_to_sound_card(samples, n);
            }
        }
    );
    
    // Add channels
    ale_sm.add_channel(7100000);
    ale_sm.add_channel(14109000);
    
    // Phase 5: Data Link Layer
    VariableARQ arq;
    
    arq.init(
        [&](const uint8_t* frame, int length) {
            // Send frame over established ALE link
            send_frame_over_ale(frame, length);
        }
    );
    
    arq.set_data_rate(DataRate::BPS_2400);
    arq.set_window_size(16);
    
    // Start ALE scanning
    ale_sm.start_scanning();
    
    // Main loop
    while (true) {
        // Process received audio
        int16_t audio[128];
        int n = read_from_sound_card(audio, 128);
        
        uint8_t symbol;
        if (demod.demodulate(audio, n, symbol)) {
            // Symbol decoded, build ALE word
            ALEWord word = build_word_from_symbols(symbols);
            ale_sm.process_received_word(word);
        }
        
        // Update timers
        uint32_t now = get_time_ms();
        ale_sm.update(now);
        arq.update(now);
        
        // If linked, can send data
        if (ale_sm.is_linked() && have_data_to_send) {
            const char* msg = "Hello from PC-ALE 2.0!";
            arq.start_transmission((const uint8_t*)msg, strlen(msg));
        }
    }
    
    return 0;
}
```

---

## Error Codes & Return Values

### Common Return Values

| Value | Meaning |
|-------|---------|
| `true` | Success |
| `false` | Failure / Invalid |
| `-1` | Error (in integer functions) |
| `0` | Not ready / No data |
| `> 0` | Success with count/length |

### Error Handling Pattern

```cpp
// Pattern 1: Boolean return
if (!parser.parse_frame(buffer, length, frame)) {
    std::cerr << "Parse error\n";
    return;
}

// Pattern 2: Check for validity
if (!frame.is_valid()) {
    std::cerr << "Invalid frame\n";
    return;
}

// Pattern 3: Exception safety (none thrown in core APIs)
// All APIs are noexcept or return error codes
```

---

## Threading Considerations

### Thread-Safe APIs

❌ **NOT thread-safe** (require external synchronization):
- `ALEStateMachine`
- `VariableARQ`
- `FFTDemodulator`

✅ **Thread-safe** (can call from multiple threads):
- `GolayEncoder::encode()`
- `GolayDecoder::decode()`
- `AQCCRC::calculate_crc8()`, `calculate_crc16()`
- Utility functions (`arq_state_name()`, etc.)

### Recommended Usage

```cpp
// Single-threaded (simple)
while (true) {
    process_audio();
    sm.update(time);
    arq.update(time);
}

// Multi-threaded (advanced)
// Thread 1: Audio I/O
void audio_thread() {
    while (true) {
        read_samples();
        queue.push(samples);  // Lock-free queue
    }
}

// Thread 2: DSP + Protocol
void protocol_thread() {
    while (true) {
        auto samples = queue.pop();
        process_samples(samples);
        sm.process_event();  // Protected by mutex
    }
}
```

---

*See also:*
- [Architecture](ARCHITECTURE.md) - System design
- [Integration Guide](INTEGRATION_GUIDE.md) - Integration patterns
- [Testing Guide](TESTING.md) - API testing examples
