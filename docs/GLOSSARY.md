# PC-ALE 2.0 - Glossary of Terms

Comprehensive terminology reference for Automatic Link Establishment and related standards.

---

## ALE Terminology

### **2G ALE** (Second Generation ALE)
MIL-STD-188-141B Appendix A protocol. Uses 8-FSK modulation at 125 baud for automatic link establishment on HF radio. Provides robust calling, scanning, and channel selection.

### **3G ALE** (Third Generation ALE)
MIL-STD-188-141C protocol. Enhanced version with higher data rates (up to 8000 bps), better LQA, and advanced features. Not yet implemented in PC-ALE 2.0.

### **ALE** (Automatic Link Establishment)
Protocol for automatically establishing HF radio links without operator intervention. Handles frequency selection, calling, handshaking, and channel quality assessment.

### **AllCall**
Broadcast call to all stations monitoring ALE channels. Uses wildcard addressing (e.g., `***`) to reach any listening station.

### **AMD** (Auto Message Display)
Short message (up to 90 characters) transmitted during ALE call. Displayed automatically on receiving station. Used for tactical messaging.

### **AQC-ALE** (Advanced Quick Call ALE)
MIL-STD-188-141B Appendix C enhancements. Adds data elements (DE fields), CRC protection, slotted responses, and transaction codes to 2G ALE.

### **ASCII-64**
64-character subset of ASCII used in ALE words. Includes A-Z, 0-9, space, and 27 special characters. Encoded in 6 bits.

---

## ARQ Terminology

### **ACK** (Acknowledgment)
Positive acknowledgment that data was received correctly. In FS-1052, sent as ACK bitmap indicating which blocks were received.

### **ARQ** (Automatic Repeat Request)
Error control method where receiver requests retransmission of corrupted data. FS-1052 uses selective repeat ARQ.

### **Broadcast ARQ**
One-way ARQ mode (FED-STD-1052). Transmitter sends data without waiting for acknowledgments. Used for point-to-multipoint.

### **Circuit ARQ**
Bidirectional ARQ mode (FED-STD-1052). Both stations can send data simultaneously. Not implemented in PC-ALE 2.0.

### **Fixed ARQ**
FED-STD-1052 mode with fixed block size (128 bytes). Simpler than Variable ARQ but less efficient. Not implemented in PC-ALE 2.0.

### **FS-1052** (FED-STD-1052)
Federal Standard 1052: HF Radio Data Link Protocol. Defines ARQ modes, frame formats, and error recovery for HF data transmission.

### **NAK** (Negative Acknowledgment)
Indicates data was received with errors. In selective repeat ARQ, NAK bitmap specifies which blocks need retransmission.

### **Selective Repeat ARQ**
ARQ mode where only corrupted blocks are retransmitted. Uses bitmap to track which blocks were received correctly. Most efficient ARQ mode.

### **Variable ARQ**
FED-STD-1052 mode with variable block size (1-1023 bytes). Adaptive to channel conditions. Fully implemented in PC-ALE 2.0 Phase 5.

---

## Modulation and Coding

### **8-FSK** (8-tone Frequency Shift Keying)
Modulation used in 2G ALE. Eight tones spaced 125 Hz apart (750-1750 Hz) encode 3 bits per symbol. Symbol rate is 125 baud.

### **BER** (Bit Error Rate)
Percentage of bits received incorrectly. Used to measure channel quality. Lower BER indicates better quality.

### **Baud**
Symbol rate (symbols per second). For 8-FSK at 125 baud, 3 bits per symbol = 375 bps.

### **CRC** (Cyclic Redundancy Check)
Error detection code appended to data. PC-ALE uses:
- CRC-8 (AQC-ALE): Polynomial 0x07
- CRC-16 (AQC-ALE): Polynomial 0x1021 (CCITT)
- CRC-32 (FS-1052): Polynomial 0x04C11DB7 (FED-STD-1003A)

### **FEC** (Forward Error Correction)
Error correction added before transmission. 2G ALE uses Golay (24,12) code with 3-bit error correction capability.

### **FFT** (Fast Fourier Transform)
Algorithm for frequency analysis. Used in PC-ALE demodulator to detect which of 8 tones is present.

### **Golay (24,12) Code**
FEC code encoding 12 data bits into 24-bit codeword. Can correct up to 3 bit errors. Used in 2G ALE for robust transmission.

### **SNR** (Signal-to-Noise Ratio)
Ratio of signal power to noise power (in dB). Higher SNR indicates better signal quality. Used in LQA calculation.

---

## Protocol Terminology

### **Channel**
Radio frequency used for communication. ALE scans multiple channels to find best one. Each channel has associated LQA.

### **Data Element (DE)**
Field in AQC-ALE word containing additional information:
- DE1: Reserved
- DE2: Slot position (0-7)
- DE3: Traffic class (0-15)
- DE4: LQA (0-31)
- DE5-DE8: Reserved
- DE9: Transaction code (0-7)

### **Dwell Time**
Time spent listening on each channel during scanning. Default is 200ms. Shorter dwell = faster scanning but more missed calls.

### **Handshake**
Exchange of messages establishing a link:
1. Caller sends TO + FROM + TIS
2. Called station responds with TIS
3. Caller confirms with TWS

### **LQA** (Link Quality Analysis)
Measurement of channel quality (0-31 scale). Based on SNR and BER. Stored per-channel to guide frequency selection.

### **Net**
Group of stations sharing common address. Net calls reach all members. Example: `NET1` includes `ALPHA`, `BRAVO`, `CHARLIE`.

### **Preamble**
3-bit field in ALE word indicating word type:
- `000` = DATA
- `001` = THRU
- `010` = TO
- `011` = FROM
- `100` = TIS (This Is)
- `101` = TWS (This Was)
- `110` = CMD
- `111` = REP

### **Scanning**
Process of listening to multiple channels sequentially. ALE scanner dwells on each channel, looking for calls.

### **Sounding**
Transmission of TIS words on multiple channels to measure channel quality. Allows stations to build LQA table.

### **TIS** (This Is)
ALE word identifying a station. Used in handshakes and soundings. Contains station address.

### **TWS** (This Was)
ALE word confirming link establishment. Sent by caller after receiving TIS from called station.

### **Wildcard Address**
Address pattern using `*` (match any characters) or `?` (match single character). Example: `ALF*` matches `ALFA`, `ALFRED`, `ALFONSO`.

---

## State Machine Terminology

### **CALLING State**
State machine state where station is transmitting a call (TO + FROM + TIS sequence). Waiting for TIS response.

### **HANDSHAKE State**
State where station is responding to incoming call with TIS. Will transition to LINKED if caller sends TWS.

### **IDLE State**
Initial state. Not scanning or calling. Waiting for command.

### **LINKED State**
State where link is established. Can send AMD or transition to data mode. Link quality is known.

### **RECEIVING State**
State where station is processing incoming ALE words. Parsing word sequence to determine action.

### **SCANNING State**
State where station is scanning channels. Listening for calls on each frequency in channel list.

---

## Data Link Terminology

### **ACK Bitmap**
256-bit field (32 bytes) in FS-1052 ACK frame. Each bit indicates whether corresponding sequence number was received correctly.

### **Abbreviated Address**
2-byte short address in FS-1052 control frames. Efficient for frequently-used addresses.

### **Block**
Unit of data transmission in ARQ. In Variable ARQ, block size is configurable (1-1023 bytes).

### **Control Frame**
FS-1052 frame carrying control information (not user data). Four types: T1-T4.

### **Data Frame**
FS-1052 frame carrying user payload. Up to 1023 bytes of data plus header and CRC-32.

### **Flow Control**
Mechanism preventing buffer overflow. FS-1052 uses flow control bit in ACK bitmap to pause transmission.

### **Full Address**
18-byte long address in FS-1052 control frames. Used for extended addressing or routing.

### **Retransmission Queue**
List of blocks that failed to send or were NAKed. Blocks in queue are retransmitted.

### **Selective Reject**
ARQ mechanism where receiver NAKs specific blocks (not entire window). More efficient than go-back-N.

### **Sequence Number**
8-bit identifier for each data block (0-255). Wraps around after 255. Used to detect duplicates and order blocks.

### **Sliding Window**
Flow control mechanism. Sender can have up to window_size blocks outstanding. Default window size is 16.

### **T1 Frame**
FS-1052 control frame: Link setup request. Contains source/destination addresses and mode.

### **T2 Frame**
FS-1052 control frame: Link setup confirmation. Acknowledges T1 and confirms link.

### **T3 Frame**
FS-1052 control frame: Link termination request. Cleanly closes link.

### **T4 Frame**
FS-1052 control frame: Link status. Exchanges state information (not implemented in PC-ALE 2.0).

### **Transaction Code**
3-bit AQC-ALE field indicating next action:
- 0: BUSY
- 1: MS_141A (MIL-STD messaging)
- 2: NAK
- 3: ACK
- 4: TERMINATE
- 5: Reserved
- 6: AQC_CMD
- 7: Reserved

### **Traffic Class**
4-bit AQC-ALE field indicating data type:
- 0: Clear voice
- 1-4: Digital voice modes
- 8: ALE messaging
- 9-11: Data modes (PSK, 39-tone, HF email)
- 15: Emergency

### **Window Size**
Maximum number of unacknowledged blocks allowed in flight. Larger windows improve throughput but require more memory.

---

## Implementation Terminology

### **Callback**
Function pointer registered with PC-ALE libraries. Called when events occur (word received, link established, data received).

### **Clean-Room Implementation**
Software written from specifications without reference to existing code. Avoids copyright issues. PC-ALE 2.0 is 100% clean-room.

### **Cross-Platform**
Code that runs on multiple operating systems without modification. PC-ALE 2.0 supports Windows, Linux, macOS.

### **Demodulator**
Component extracting digital data from analog signal. PC-ALE uses FFT demodulator for 8-FSK.

### **Majority Voting**
Error correction technique. Golay decoder uses triple redundancy: transmit 3 copies, decode using majority rule.

### **Modulator**
Component converting digital data to analog signal. PC-ALE uses tone generator for 8-FSK.

### **Phase**
Development stage in PC-ALE project:
- Phase 1: FSK modem (FFT, Golay, tones)
- Phase 2: Protocol layer (words, messages, addressing)
- Phase 3: Link state machine (scanning, calling, LQA)
- Phase 4: AQC-ALE extensions (DE, CRC, slots)
- Phase 5: FS-1052 ARQ (selective repeat, CRC-32)
- Phase 6: Audio I/O integration (planned)

### **Symbol**
Unit of modulation. In 8-FSK, one symbol encodes 3 bits. Symbol duration is 1/125 second = 8ms.

### **Unit Test**
Test of single component in isolation. PC-ALE has 54 unit tests covering all phases.

---

## Standards and Specifications

### **FED-STD-1003A**
Federal Standard 1003A: CRC-32 polynomial and implementation. Used in FS-1052 for data integrity.

### **FED-STD-1052** (FS-1052)
See FS-1052 above.

### **MIL-STD-188-110A**
Military Standard 188-110A: HF modem specification. Defines PSK and QAM modes. Used with ALE for data transmission.

### **MIL-STD-188-141B**
Military Standard 188-141B: HF ALE specification. Defines 2G ALE (Appendix A) and AQC-ALE (Appendix C).

### **MIL-STD-188-141C**
Military Standard 188-141C: Enhanced ALE specification. Defines 3G ALE with higher data rates. Not implemented in PC-ALE 2.0.

---

## Radio and Audio Terminology

### **CAT** (Computer Aided Transceiver)
Serial or USB interface for controlling radio (frequency, mode, power, etc.). Hamlib library provides CAT support.

### **DirectSound**
Windows API for low-latency audio I/O. Used in PC-ALE for real-time audio processing.

### **Hamlib**
Open-source library for radio CAT control. Supports 200+ radio models. Planned for PC-ALE Phase 6.

### **HF** (High Frequency)
Radio frequency range 3-30 MHz. Long-distance propagation via ionosphere. ALE is designed for HF operation.

### **Transceiver**
Radio capable of both transmitting and receiving. ALE requires PTT control to switch between modes.

### **PTT** (Push-To-Talk)
Signal controlling transmitter. ALE must activate PTT before transmitting, deactivate after.

### **WASAPI** (Windows Audio Session API)
Modern Windows audio API. Provides lower latency than DirectSound. Supported in PC-ALE Phase 6.

---

## Acronyms Quick Reference

| Acronym | Full Name | Description |
|---------|-----------|-------------|
| ACK | Acknowledgment | Positive response to data |
| ALE | Automatic Link Establishment | Automated radio linking protocol |
| AMD | Auto Message Display | Short message in ALE call |
| AQC | Advanced Quick Call | Enhanced ALE with DE fields |
| ARQ | Automatic Repeat Request | Error recovery through retransmission |
| BER | Bit Error Rate | Percentage of bit errors |
| CAT | Computer Aided Transceiver | Radio control interface |
| CRC | Cyclic Redundancy Check | Error detection code |
| DE | Data Element | AQC-ALE extension field |
| FEC | Forward Error Correction | Error correction before transmission |
| FFT | Fast Fourier Transform | Frequency analysis algorithm |
| FS | Federal Standard | U.S. federal technical standard |
| FSK | Frequency Shift Keying | Frequency modulation scheme |
| HF | High Frequency | 3-30 MHz radio band |
| LQA | Link Quality Analysis | Channel quality measurement |
| MIL-STD | Military Standard | U.S. military technical standard |
| NAK | Negative Acknowledgment | Indicates error in reception |
| PTT | Push-To-Talk | Transmitter control signal |
| QAM | Quadrature Amplitude Modulation | Advanced modulation (188-110A) |
| SNR | Signal-to-Noise Ratio | Signal quality metric (dB) |
| TIS | This Is | ALE identification word |
| TWS | This Was | ALE confirmation word |

---

## Related Documents

- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture and design
- [API_REFERENCE.md](API_REFERENCE.md) - Complete API documentation
- [MIL_STD_COMPLIANCE.md](MIL_STD_COMPLIANCE.md) - Standards compliance matrix
- [INTEGRATION_GUIDE.md](INTEGRATION_GUIDE.md) - Integration with audio/radio
- [TESTING.md](TESTING.md) - Testing guide and coverage

---

*Last Updated: December 2024*  
*Version: PC-ALE 2.0 Phase 5*
