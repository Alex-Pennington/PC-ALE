# PC-ALE 2.0 - System Architecture

## Overview

PC-ALE 2.0 implements a **layered architecture** following the OSI reference model, with each phase corresponding to a distinct layer. The system progresses from physical signal processing up to reliable data transfer.

---

## Layer Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│  Layer 5: APPLICATION LAYER                                      │
│  ┌────────────────────────────────────────────────────────┐     │
│  │  User Interface, Message Handling, Station Management  │     │
│  │  (Future: Phase 6 - Audio I/O, Radio Control)          │     │
│  └────────────────────────────────────────────────────────┘     │
└──────────────────────────────┬──────────────────────────────────┘
                               │ Message API
┌──────────────────────────────▼──────────────────────────────────┐
│  Layer 4: DATA LINK LAYER (Phase 5)                              │
│  ┌────────────────────────────────────────────────────────┐     │
│  │  FS-1052 ARQ Protocol                                  │     │
│  │  • Control Frames (T1-T4) & Data Frames                │     │
│  │  • Selective Repeat ARQ with 256-bit ACK bitmap        │     │
│  │  • CRC-32 frame protection                             │     │
│  │  • Automatic retransmission on timeout/NAK             │     │
│  │  • Window-based flow control                           │     │
│  │  • Multi-rate support (75-4800 bps)                    │     │
│  └────────────────────────────────────────────────────────┘     │
└──────────────────────────────┬──────────────────────────────────┘
                               │ Frame API
┌──────────────────────────────▼──────────────────────────────────┐
│  Layer 3: LINK ESTABLISHMENT LAYER (Phase 3)                     │
│  ┌────────────────────────────────────────────────────────┐     │
│  │  ALE State Machine                                     │     │
│  │  States: IDLE → SCANNING → CALLING → HANDSHAKE →      │     │
│  │          LINKED → SOUNDING                             │     │
│  │  • Channel selection and scanning                      │     │
│  │  • Link Quality Analysis (LQA)                         │     │
│  │  • Call establishment (Individual/Net/AMD)             │     │
│  │  • Timeout management                                  │     │
│  │  • Event-driven callbacks                              │     │
│  └────────────────────────────────────────────────────────┘     │
└──────────────────────────────┬──────────────────────────────────┘
                               │ Word API
┌──────────────────────────────▼──────────────────────────────────┐
│  Layer 2: PROTOCOL LAYER (Phases 2 & 4)                          │
│  ┌────────────────────────────────────────────────────────┐     │
│  │  2G ALE Word Protocol (Phase 2)                        │     │
│  │  • 24-bit word structure (3-bit preamble + 21-bit data)│     │
│  │  • Preamble types: DATA, THRU, TO, FROM, CMD, REP, etc.│     │
│  │  • ASCII-64 character encoding/decoding                │     │
│  │  • Address book & wildcard matching                    │     │
│  │  • Message assembly from word sequences                │     │
│  └────────────────────────────────────────────────────────┘     │
│  ┌────────────────────────────────────────────────────────┐     │
│  │  AQC-ALE Extensions (Phase 4)                          │     │
│  │  • Data Element (DE1-DE9) extraction                   │     │
│  │  • Traffic class indication (16 types)                 │     │
│  │  • Transaction codes (ACK/NAK/TERMINATE)               │     │
│  │  • Slotted response mechanism (8 × 200ms)              │     │
│  │  • CRC-8 & CRC-16 orderwire protection                 │     │
│  └────────────────────────────────────────────────────────┘     │
└──────────────────────────────┬──────────────────────────────────┘
                               │ Symbol API
┌──────────────────────────────▼──────────────────────────────────┐
│  Layer 1: PHYSICAL LAYER (Phase 1)                               │
│  ┌────────────────────────────────────────────────────────┐     │
│  │  8-FSK Modem                                           │     │
│  │  • FFT-based demodulator (64-point sliding FFT)        │     │
│  │  • 8 tone generator (750-1750 Hz, 125 Hz spacing)      │     │
│  │  • Symbol decoder with peak detection                  │     │
│  │  • Majority voting (3x redundancy)                     │     │
│  │  • Golay (24,12) FEC encoder/decoder                   │     │
│  │  • 3-bit error correction per codeword                 │     │
│  └────────────────────────────────────────────────────────┘     │
└──────────────────────────────┬──────────────────────────────────┘
                               │ Audio Samples
                       ┌───────▼────────┐
                       │  Audio I/O     │ (Phase 6 - Future)
                       │  Sound Card    │
                       │  Radio CAT     │
                       └────────────────┘
```

---

## Data Flow

### Transmit Path

```
Application Message
        │
        ▼
┌───────────────────┐
│  FS-1052 ARQ      │  Split into blocks, add sequence numbers
│  (Phase 5)        │  Format data frames with CRC-32
└────────┬──────────┘
         │ Data Frames
         ▼
┌───────────────────┐
│  ALE State        │  Manage link, determine when to transmit
│  Machine          │  Handle timing and channel selection
│  (Phase 3)        │
└────────┬──────────┘
         │ ALE Words
         ▼
┌───────────────────┐
│  Protocol Layer   │  Encode message as ALE words
│  (Phases 2 & 4)   │  Add preambles, DE fields (if AQC)
│                   │  Apply ASCII-64 encoding
└────────┬──────────┘
         │ 24-bit Words
         ▼
┌───────────────────┐
│  Golay FEC        │  Encode to 24-bit codewords
│  (Phase 1)        │  Add error correction
└────────┬──────────┘
         │ Symbols (3 bits each)
         ▼
┌───────────────────┐
│  8-FSK Modulator  │  Convert symbols to tones
│  (Phase 1)        │  Generate audio waveform
└────────┬──────────┘
         │ Audio samples
         ▼
     Sound Card → Radio
```

### Receive Path

```
     Radio → Sound Card
         │ Audio samples
         ▼
┌───────────────────┐
│  FFT Demodulator  │  64-point FFT, peak detection
│  (Phase 1)        │  Decode 8-FSK tones to symbols
└────────┬──────────┘
         │ Symbols (3 bits each)
         ▼
┌───────────────────┐
│  Golay Decoder    │  Decode 24-bit codewords
│  (Phase 1)        │  Correct up to 3-bit errors
└────────┬──────────┘
         │ 24-bit Words
         ▼
┌───────────────────┐
│  Protocol Parser  │  Extract preamble and payload
│  (Phases 2 & 4)   │  Decode ASCII-64 characters
│                   │  Parse DE fields (if AQC)
└────────┬──────────┘
         │ ALE Words
         ▼
┌───────────────────┐
│  ALE State        │  Process call sequences
│  Machine          │  Update LQA, manage state transitions
│  (Phase 3)        │  Determine message complete
└────────┬──────────┘
         │ Data Frames
         ▼
┌───────────────────┐
│  FS-1052 ARQ      │  Validate CRC-32
│  (Phase 5)        │  Track sequences, send ACKs
│                   │  Reassemble message from blocks
└────────┬──────────┘
         │
         ▼
  Complete Message → Application
```

---

## Phase Dependencies

```
Phase 1: 8-FSK Modem Core
    │
    │ Provides: Modulation/Demodulation, Golay FEC
    ▼
Phase 2: Protocol Layer
    │
    │ Provides: Word parsing, ASCII-64, Address book
    ▼
Phase 3: Link State Machine
    │
    │ Provides: Link establishment, Channel management
    ├──────────────┐
    │              │
    ▼              ▼
Phase 4:       Phase 5:
AQC-ALE        FS-1052 ARQ
Extensions     Data Link
    │              │
    └──────┬───────┘
           │
           ▼
    Phase 6: Audio I/O
    (Future Integration)
```

**Key Points:**
- Phases 1-3 form the **2G ALE core**
- Phase 4 (AQC-ALE) **extends** Phase 2 (same modem, enhanced protocol)
- Phase 5 (FS-1052) sits **above** Phase 3 (reliable data transfer)
- All phases **coexist** - can use 2G ALE alone, or add AQC/ARQ as needed

---

## Component Relationships

### Libraries

```
libale_fec.a
    └─ Golay (24,12) encoder/decoder
    └─ Used by: ale_fsk_core

libale_fsk_core.a
    └─ FFT demodulator, tone generator, symbol decoder
    └─ Depends on: ale_fec
    └─ Used by: ale_protocol

libale_protocol.a
    └─ ALEWord parser, ALEMessage assembly, Address book
    └─ Depends on: ale_fsk_core, ale_fec
    └─ Used by: ale_link, ale_aqc

libale_link.a
    └─ ALEStateMachine (6 states, event-driven)
    └─ Depends on: ale_protocol, ale_fsk_core, ale_fec
    └─ Used by: Applications

libale_aqc.a
    └─ AQCParser, AQCCRC, SlotManager
    └─ Depends on: ale_protocol, ale_fsk_core, ale_fec
    └─ Used by: Applications (optional)

libale_fs1052.a
    └─ FrameFormatter, FrameParser, VariableARQ
    └─ Depends on: ale_protocol, ale_fsk_core, ale_fec
    └─ Used by: Applications (optional)
```

### Class Hierarchy

```
Phase 1 (8-FSK Modem):
    FFTDemodulator
    ToneGenerator
    SymbolDecoder
    GolayEncoder
    GolayDecoder

Phase 2 (Protocol):
    ALEWord
        ├─ PreambleType (enum)
        └─ Methods: encode(), decode(), to_string()
    ALEMessage
        ├─ MessageType (enum)
        └─ Methods: add_word(), is_complete(), get_content()
    AddressBook
        └─ Methods: add_station(), match_address()

Phase 3 (Link State Machine):
    ALEStateMachine
        ├─ ALEState (enum): IDLE, SCANNING, CALLING, etc.
        ├─ ALEEvent (enum): EV_TIMEOUT, EV_WORD_RECEIVED, etc.
        └─ Methods: process_event(), handle_*_state()
    ChannelInfo
    LQATracker

Phase 4 (AQC-ALE):
    DataElements
        └─ Fields: de1-de9
    AQCParser
        └─ Methods: extract_de(), parse_call_probe(), etc.
    AQCCRC
        └─ Methods: calculate_crc8(), calculate_crc16()
    SlotManager
        └─ Methods: assign_slot(), calculate_slot_time()

Phase 5 (FS-1052 ARQ):
    ControlFrame
    DataFrame
    FrameFormatter
        └─ Methods: format_control_frame(), format_data_frame()
    FrameParser
        └─ Methods: parse_control_frame(), parse_data_frame()
    VariableARQ
        ├─ ARQState (enum): IDLE, TX_DATA, WAIT_ACK, etc.
        └─ Methods: start_transmission(), handle_received_frame()
```

---

## Threading Model

### Current Implementation: **Single-Threaded**

All processing happens in the **main application thread**:

```
Main Loop:
    1. Read audio samples from buffer
    2. Feed to FFT demodulator
    3. Decode symbols → words
    4. Process through state machine
    5. Handle events, update state
    6. Generate response (if needed)
    7. Modulate and queue audio for TX
    8. Repeat
```

### Future Multi-Threaded Model (Phase 6)

```
┌─────────────────┐
│  Audio Thread   │  Real-time priority
│  (Capture/Play) │  100-200 sample blocks
└────────┬────────┘
         │ Lock-free queue
         ▼
┌─────────────────┐
│  DSP Thread     │  Normal priority
│  (FFT/Decode)   │  Process samples → symbols
└────────┬────────┘
         │ Event queue
         ▼
┌─────────────────┐
│  Protocol Thread│  Normal priority
│  (State Machine)│  Handle ALE logic
└────────┬────────┘
         │ Callback
         ▼
┌─────────────────┐
│  UI Thread      │  Low priority
│  (Display/User) │  Update display, handle input
└─────────────────┘
```

**Threading Considerations:**
- Audio I/O requires **real-time** thread (low latency)
- DSP processing can be **normal** priority
- State machine is event-driven, **not time-critical**
- Lock-free queues recommended for audio↔DSP communication

---

## Memory Architecture

### Stack vs Heap Usage

**Design Principle: Minimize dynamic allocation for real-time performance**

```
Stack (Fixed, Fast):
    ✓ Audio sample buffers (8000 samples × 4 bytes = 32 KB)
    ✓ FFT working buffers (64 × 8 bytes = 512 bytes)
    ✓ Symbol decode buffer (49 symbols × 1 byte = 49 bytes)
    ✓ ALE word temporary storage (24 bits)
    ✓ FS-1052 frame buffers (1200 bytes max)

Heap (Dynamic, Flexible):
    • Address book (std::vector of stations)
    • Channel list (std::vector of frequencies)
    • Message history (std::deque<ALEMessage>)
    • TX/RX block queues (std::vector<DataBlock>)
    • FS-1052 reassembly buffer (std::vector<uint8_t>)
```

### Typical Memory Footprint

| Component | Memory | Notes |
|-----------|--------|-------|
| FFT buffers | ~32 KB | Double-buffered audio |
| State machine | ~4 KB | State variables, timers |
| Address book | ~10 KB | 100 stations × ~100 bytes |
| Channel list | ~1 KB | 10 channels × ~100 bytes |
| FS-1052 TX window | ~16 KB | 16 blocks × 1 KB |
| FS-1052 RX buffer | Variable | Grows with message size |
| **Total baseline** | ~65 KB | Excluding messages |

---

## Configuration & Initialization

### Initialization Sequence

```cpp
// 1. Create Physical Layer
FFTDemodulator demod(8000);  // 8 kHz sample rate
ToneGenerator tones;
SymbolDecoder decoder;

// 2. Create Protocol Layer
AddressBook address_book;
address_book.set_self_address("MYSTATION");
address_book.add_station("BASE01");
address_book.add_net("NETONE");

// 3. Create Link Layer
ALEStateMachine state_machine;
state_machine.init(
    // Callbacks
    on_word_received,
    on_state_changed,
    on_link_established
);

// Add channels
state_machine.add_channel(7100000);  // 7.1 MHz
state_machine.add_channel(14109000); // 14.109 MHz

// 4. (Optional) Create AQC Extensions
AQCParser aqc_parser;

// 5. (Optional) Create FS-1052 ARQ
VariableARQ arq;
arq.init(tx_callback, state_callback);
arq.set_data_rate(DataRate::BPS_2400);
arq.set_window_size(16);

// 6. Start Operations
state_machine.start_scanning();
```

### Configuration Files

**Planned structure (Phase 6):**

```
config/
├── stations.ini      # Address book entries
├── channels.ini      # Frequency list with priorities
├── settings.ini      # ALE parameters (timeouts, LQA thresholds)
└── audio.ini         # Sound device configuration
```

---

## Error Handling Strategy

### Layered Error Recovery

```
Layer 5 (Application):
    └─ Retry message transmission
    └─ Alert user to link failure

Layer 4 (FS-1052 ARQ):
    └─ Retransmit on timeout (up to 3 attempts)
    └─ Drop to lower data rate on repeated errors
    └─ Report transfer failure if max retries exceeded

Layer 3 (Link State Machine):
    └─ Retry call if no handshake (up to 3 attempts)
    └─ Switch channel on repeated failures
    └─ Return to scanning on link timeout

Layer 2 (Protocol):
    └─ Validate preamble and word structure
    └─ Discard malformed words
    └─ Continue processing valid words

Layer 1 (Physical):
    └─ Golay FEC corrects up to 3-bit errors
    └─ Majority voting (3×) for symbol reliability
    └─ SNR-based quality estimation
```

### Exception Safety

**Policy: No exceptions in real-time code paths**

- Audio callbacks: **noexcept** functions
- DSP processing: Return error codes, not exceptions
- State machine: Event handlers catch and log exceptions
- Validation: Check inputs, return false on invalid data

---

## Performance Characteristics

### Timing Requirements

| Operation | Typical | Maximum | Notes |
|-----------|---------|---------|-------|
| Audio callback | <5 ms | 10 ms | Real-time constraint |
| FFT (64-point) | <0.5 ms | 1 ms | Per audio block |
| Symbol decode | <0.1 ms | 0.5 ms | Per symbol |
| Golay decode | <0.05 ms | 0.1 ms | Per word |
| State machine event | <1 ms | 5 ms | Non-real-time |
| FS-1052 frame format | <2 ms | 10 ms | Per frame |

### Throughput

| Mode | Effective Data Rate | Overhead |
|------|---------------------|----------|
| **2G ALE calling** | ~40 bps | High (for link setup) |
| **AQC-ALE calling** | ~50 bps | Medium (enhanced) |
| **FS-1052 @ 2400 bps** | ~2100 bps | 12% (headers + CRC) |
| **FS-1052 @ 75 bps** | ~65 bps | 13% |

---

## Scalability

### Horizontal Scaling

**Multiple Simultaneous Links:**

```cpp
// Create multiple state machines for parallel channels
std::vector<ALEStateMachine> state_machines(4);

// Each can operate independently
state_machines[0].add_channel(7100000);  // HF band 1
state_machines[1].add_channel(14109000); // HF band 2
state_machines[2].add_channel(21109000); // HF band 3

// Distribute processing across threads
for (auto& sm : state_machines) {
    thread_pool.submit([&sm]() {
        sm.process_events();
    });
}
```

### Vertical Scaling

**Enhanced Features:**

- **3G ALE**: Add 3rd generation ALE (MIL-STD-188-141C)
- **Wideband modem**: Support PSK modes (188-110C)
- **Adaptive rate**: Implement automatic rate selection
- **Priority queuing**: Multiple message priorities

---

## Extension Points

### Adding Custom Features

#### 1. Custom Preamble Types

```cpp
// Extend PreambleType enum
enum class PreambleType {
    // ... existing types
    CUSTOM_1 = 0x08,  // User-defined
    CUSTOM_2 = 0x09
};

// Register handler
protocol_layer.register_preamble_handler(
    PreambleType::CUSTOM_1,
    custom_preamble_handler
);
```

#### 2. Custom ARQ Modes

```cpp
// Extend ARQMode enum
enum class ARQMode {
    VARIABLE_ARQ,
    BROADCAST,
    CIRCUIT,
    FIXED_ARQ,
    CUSTOM_ARQ  // New mode
};

// Implement custom state machine
class CustomARQ : public ARQBase {
    void handle_tx_data() override { /* custom logic */ }
    // ...
};
```

#### 3. Plugin Architecture (Future)

```cpp
class ALEPlugin {
public:
    virtual void on_word_received(const ALEWord& word) = 0;
    virtual void on_state_changed(ALEState state) = 0;
};

// Register plugin
state_machine.add_plugin(std::make_unique<MyPlugin>());
```

---

## Debugging & Instrumentation

### Logging Levels

```
TRACE:   Every sample, symbol, FFT bin (verbose)
DEBUG:   Word received, state transitions, events
INFO:    Call established, link up/down, messages
WARNING: Timeouts, retransmissions, errors corrected
ERROR:   Link failures, CRC errors, invalid data
```

### Performance Profiling

```cpp
// Built-in timing instrumentation
struct PerformanceCounters {
    uint64_t fft_calls;
    uint64_t fft_time_us;
    uint64_t decode_calls;
    uint64_t decode_time_us;
    uint64_t state_machine_calls;
    uint64_t state_machine_time_us;
};

// Access via
auto counters = demod.get_performance_counters();
std::cout << "FFT avg: " 
          << (counters.fft_time_us / counters.fft_calls) 
          << " µs\n";
```

---

## Summary

PC-ALE 2.0 architecture follows **strict layering** principles:

✅ **Separation of concerns** - Each phase has clear responsibility  
✅ **Loose coupling** - Layers communicate via defined APIs  
✅ **High cohesion** - Related functionality grouped together  
✅ **Extensibility** - Easy to add features without breaking existing code  
✅ **Testability** - Each layer independently testable  

The design supports both **simple use cases** (2G ALE only) and **complex scenarios** (AQC-ALE + FS-1052 ARQ + future enhancements) through modular composition.

---

*See also:*
- [API Reference](API_REFERENCE.md) - Detailed class documentation
- [Integration Guide](INTEGRATION_GUIDE.md) - How to use the architecture
- [Testing Guide](TESTING.md) - Testing each layer
