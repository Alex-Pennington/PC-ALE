# AQC-ALE Research Summary

## Executive Summary

After analyzing the MARS-ALE reference implementation and MIL-STD-188-141B, **AQC-ALE (Advanced Quick Call ALE) is NOT a different modulation scheme**. It uses the **same 8-FSK modem** as standard 2G ALE.

**Key Finding**: AQC-ALE is a **protocol layer enhancement**, not a modem change.

## What AQC-ALE Actually Is

### NOT A Different Modem
- ❌ **NOT** AFSK (Audio Frequency Shift Keying)
- ❌ **NOT** a different modulation scheme
- ❌ **NOT** a separate physical layer

### Protocol Layer Enhancements
- ✅ Enhanced Data Elements (DE) in word structure
- ✅ CRC error detection for orderwire
- ✅ Slotted response mechanism for reduced collisions
- ✅ AMD (Automatic Message Display) support
- ✅ Transaction codes for link management
- ✅ Traffic class identification

## AQC-ALE vs Standard 2G ALE

### Same as Standard 2G ALE:
1. **8-FSK Modulation**: 8 tones, 750-1750 Hz, 125 Hz spacing
2. **Symbol Rate**: 125 baud
3. **Word Structure**: 24-bit words (3-bit preamble + 21-bit payload)
4. **Sample Rate**: 8000 Hz
5. **FFT-based Detection**: 64-point FFT
6. **Golay (24,12) FEC**: Same forward error correction

### Enhanced in AQC:
1. **Data Elements (DE)**: Additional information fields in words
   - DE1: Reserved
   - DE2: Slot position (0-7)
   - DE3: Traffic class (voice type, data mode, etc.)
   - DE4: LQA/signal quality
   - DE5-DE6: Link quality metrics
   - DE7: Reserved for future
   - DE8: Number of orderwire commands
   - DE9: Transaction code

2. **CRC Protection**: Orderwire messages include CRC
   - `AQC_CRC_OK` / `AQC_CRC_ERROR` status

3. **Slotted Response**: Reduced collision probability
   - Stations assigned time slots (0-7)
   - Coordinated response timing

4. **Transaction Codes** (DE9):
   - `AQC_DE9_MS_141A` - MIL-STD-188-141A messaging
   - `AQC_DE9_ACK_LAST` - Acknowledge last message
   - `AQC_DE9_NAK_LAST` - Negative acknowledge
   - `AQC_DE9_TERMINATE` - Terminate link
   - `AQC_DE9_OP_ACKNAK` - Operator acknowledge/negative acknowledge
   - `AQC_DE9_AQC_CMD` - AQC command follows

5. **Traffic Class** (DE3):
   - `AQC_DE3_CLEAR_VOICE` - Clear analog voice
   - `AQC_DE3_DIGITAL_VOICE` - Digital voice
   - `AQC_DE3_HFD_VOICE` - HF digital voice
   - `AQC_DE3_SECURE_DIGITAL_VOICE` - Encrypted digital voice
   - `AQC_DE3_ALE_MSG` - ALE messaging mode
   - `AQC_DE3_PSK_MSG` - PSK data mode
   - `AQC_DE3_39TONE_MSG` - 39-tone data mode
   - `AQC_DE3_HF_EMAIL` - HF email mode
   - `AQC_DE3_KY100_ACTIVE` - KY-100 encryption active

## Implementation Strategy

Since we've already built:
- ✅ Phase 1: 8-FSK modem core
- ✅ Phase 2: Word structure & protocol
- ✅ Phase 3: Link state machine

**AQC-ALE can be implemented as Phase 4: Protocol Extensions**

### Phase 4 Components:

1. **Enhanced Word Parser**
   - Extract Data Elements (DE1-DE9) from 21-bit payload
   - CRC calculation and validation
   - Support for extended address formats

2. **AQC Message Structures**
   ```cpp
   struct AQCCallProbe {
       char to_address[16];
       char term_address[16];
       int de1, de3, de4;  // Data elements
   };
   
   struct AQCInlink {
       char to_address[16];
       char term_address[16];
       AQCCrcStatus crc_status;
       bool ack_this_flag;
       bool net_address_flag;
       int slot_position;
       int transaction_code;
   };
   ```

3. **Slot Management**
   - Assign response slots (0-7)
   - Calculate slot timing
   - Coordinate multi-station responses

4. **Orderwire Handler**
   - AMD message parsing
   - Command/response sequencing
   - CRC validation

5. **Enhanced State Machine Events**
   - `EV_AQC_CALL_PROBE`
   - `EV_AQC_CALL_HANDSHAKE`
   - `EV_AQC_INLINK`
   - `EV_AQC_NOT_FOR_ME`
   - `EV_AQC_FRAME_BEGIN`
   - `EV_AQC_FRAME_END`

## Reference Implementation Analysis

From MARS-ALE codebase:

### qparse.cpp - AQC Parser
- Parses AQC-enhanced word structures
- Extracts data elements
- Validates CRC
- Assembles multi-word messages

### qca.h - AQC Constants
- Defines all DE values
- Transaction codes
- Command identifiers

### qfmt.cpp - AQC Format
- Builds AQC-enhanced transmission frames
- Adds CRC
- Formats orderwire

### link_sm2.cpp - AQC Link State Machine
- Handles AQC-specific events
- Manages slotted responses
- Processes transaction codes

## Compatibility

**AQC-ALE is BACKWARD COMPATIBLE with standard 2G ALE**:
- Uses same physical layer
- Same word timing
- Standard 2G stations can participate in basic linking
- AQC features are extensions, not replacements

## Correct Implementation Path

**Recommendation**: Build AQC-ALE as **Phase 4: Protocol Extensions** on top of the existing clean-room 2G ALE implementation.

**NOT**: Build a separate "AFSK modem" (which doesn't exist for AQC)

### Benefits:
1. ✅ Reuses existing modem core (Phase 1)
2. ✅ Extends existing protocol layer (Phase 2)
3. ✅ Integrates with existing state machine (Phase 3)
4. ✅ Maintains backward compatibility
5. ✅ Follows MIL-STD-188-141B architecture

## Next Steps

If the goal is to implement AQC-ALE:

1. **Define AQC data structures** (word format with DE fields)
2. **Implement DE extraction** from 21-bit payload
3. **Add CRC calculation** for orderwire
4. **Extend word parser** to recognize AQC formats
5. **Add AQC events** to state machine
6. **Implement slot management** for responses
7. **Build orderwire handler** for AMD/commands

## Conclusion

**AQC-ALE does NOT have a different modem**. It's the same 8-FSK as standard 2G ALE, with enhanced protocol features. The implementation should focus on protocol extensions, not a new physical layer.

---

*PC-ALE 2.0 - Clean-Room Implementation*  
*MIL-STD-188-141B Research*  
*December 22, 2024*
