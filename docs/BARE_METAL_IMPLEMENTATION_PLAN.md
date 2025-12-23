# PC-ALE Bare Metal Implementation Plan
## Raspberry Pi + Circle Framework + DRAWS Hat

**Document:** Implementation Roadmap  
**Author:** Alex Pennington, AAM402/KY4OLB  
**Date:** December 2024  
**Status:** Planning Phase

---

## Executive Summary

This document outlines the path from concept to working bare-metal ALE station using:
- **Hardware:** Raspberry Pi 4/5 + NW Digital Radio DRAWS Hat
- **Framework:** Circle (bare-metal C++ framework for Pi)
- **Software:** PC-ALE 2.0 protocol stack (Phases 1-6, already complete)

**Goal:** Deterministic, hard real-time ALE transceiver with 2-3 second boot time.

---

## Part 0: Why DRAWS + Bare Metal?

**Key Advantages (from OH8STN's analysis):**

1. **Single 12V Power** - No buck converters, no USB adapters. Same supply as your rig powers everything.

2. **Dual Radio Ports** - Two mini DIN-6 sockets means you can connect two rigs (or rig + spare).

3. **GPS + PPS + RTC** - Accurate timing for ALE without internet:
   - GPS provides precise time with Pulse-Per-Second sync
   - Battery-backed RTC survives power cycles
   - Critical for symbol timing in 8-FSK

4. **Standard TNC Pinout** - Works with any radio that has TNC/Data port (Icom, Yaesu, Kenwood, Elecraft).

5. **No USB Audio Latency** - I2S direct to codec, deterministic timing.

6. **Field Portable** - 12V operation means battery/solar compatible out of the box.

**Why Bare Metal on top of DRAWS?**

| Aspect | Linux + DRAWS | Circle + DRAWS |
|--------|---------------|----------------|
| Boot time | 20-30 seconds | 2-3 seconds |
| Audio latency | 50-500 µs | 5-10 µs |
| Jitter | ±100 µs | ±2 µs |
| Memory | ~100 MB OS | ~4 MB total |
| Determinism | Soft real-time | Hard real-time |
| Power draw | Higher (OS overhead) | Lower (no background tasks) |

For ALE, symbol timing is 8ms with <1ms jitter tolerance. Linux *can* do it, but bare metal *guarantees* it.

---

## Part 1: Knowledge Gap Analysis

### ✅ KNOWN (We Have This)

| Component | Status | Location |
|-----------|--------|----------|
| 8-FSK Modem | Complete | `src/fsk/` |
| Golay FEC | Complete | `src/fec/` |
| Protocol Parser | Complete | `src/protocol/` |
| State Machine | Complete | `src/link/` |
| AQC-ALE | Complete | `src/protocol/` |
| FS-1052 ARQ | Complete | `src/fs1052/` |
| LQA System | Complete | `src/lqa_*.cpp` |

**Key Insight:** The entire protocol stack is platform-agnostic C++17. No OS dependencies. Ready for bare-metal port.

---

### ❓ GAPS - Need Research/Verification

#### Gap 1: Circle Framework Specifics
| Question | Why It Matters | Action |
|----------|----------------|--------|
| Circle I2S driver API | Need exact callback signatures | Review Circle `lib/sound/` source |
| Circle memory model | Heap vs static allocation patterns | Review Circle examples |
| Circle C++17 support | Our code requires C++17 features | Verify toolchain compatibility |
| Circle timer resolution | Need sub-ms timing for symbol sync | Review `CTimer` documentation |

**Research Required:**
- [ ] Clone Circle repo, review I2S sound driver implementation
- [ ] Build Circle sample apps, verify toolchain
- [ ] Test C++17 features (std::optional, structured bindings) compile

#### Gap 2: DRAWS Hat Hardware - ✅ FULLY CONFIRMED

**Hardware Specifications (from schematic + N7NIX repo):**

| Component | Specification | Notes |
|-----------|---------------|-------|
| Audio Codec | TLV320AIC32x4 (AIC3204 compatible) | I2S interface |
| Audio Ports | 2× Mini DIN-6 (TNC standard) | LEFT and RIGHT connectors |
| GPS | SkyTraq S1216F8-GL | With PPS (Pulse Per Second) |
| RTC | Battery-backed | Maintains time without GPS |
| Power Input | 12VDC | Powers DRAWS + Pi via buck regulator |
| GPS Antenna | SMA connector | For external LNA antenna |
| Dual UART | 16IS752 | Serial connections |
| A/D Converter | TLA2024 | 4-channel, auxiliary connector |
| EEPROM | 24C32 | Board identification |

**GPIO Pinout (CONFIRMED):**

| GPIO | Function | Connector |
|------|----------|----------|
| **GPIO 12** | PTT | LEFT mini-DIN6 |
| **GPIO 23** | PTT | RIGHT mini-DIN6 |

⚠️ **CRITICAL for Bare Metal:**
> "GPIO 12 & 23 are used to drive PTT and the pull-up is sufficient to turn 
> the transistor on and key the radio, even though the red PTT LED is not lit.
> This only happens during boot-up or shutdown."
>
> **Solution:** Configure GPIO 12/23 as outputs with LOW state IMMEDIATELY on boot!
> Or add 2.2k pull-down resistor on Pi header.

**Audio Configuration (CONFIRMED):**

| Parameter | Value |
|-----------|-------|
| ALSA Device Name | `udrc` |
| Sample Rate | **48000 Hz** |
| I2S Device String | `bcm2835-i2s-tlv320aic32x4-hifi` |
| Bits | 16-bit |

**Remaining Research:**
- [ ] Codec I2C address (likely 0x18, verify in driver source)
- [ ] Full codec init sequence from Linux driver
- [ ] I2C bus number on Pi 4 vs Pi 5

**Resources:**
- Pre-built image: http://images.nwdigitalradio.com/downloads (current_beta.zip)
- Getting Started: https://nw-digital-radio.groups.io/g/udrc/wiki/DRAWS%3A-Image-getting-started
- Linux driver/scripts: https://github.com/nwdigitalradio/n7nix
- Schematic: draws3 (dated 9/17/18)

#### Gap 3: Sample Rate Conversion
| Question | Why It Matters | Action |
|----------|----------------|--------|
| DRAWS native rate | TLV320 supports 8-192 kHz | Determine optimal rate |
| Resample 48→8 kHz | Our FFT expects 8 kHz | Need efficient decimator |
| Resample 8→48 kHz | TX audio generation | Need efficient interpolator |

**Analysis:**
```
DRAWS I2S: 48 kHz (native)
PC-ALE FFT: 8 kHz (designed for)

Options:
A) Resample in software (6:1 decimation/interpolation)
B) Reconfigure codec to 8 kHz (if supported)
C) Redesign FFT for 48 kHz (384-point FFT instead of 64)

Recommendation: Option A (software resample)
- Most flexible
- Keeps PC-ALE core unchanged
- Well-understood algorithms (polyphase FIR)
```

#### Gap 4: Real-Time Constraints
| Question | Why It Matters | Action |
|----------|----------------|--------|
| I2S DMA buffer size | Latency vs jitter tradeoff | Tune experimentally |
| Symbol boundary alignment | 8ms symbol = 384 samples @ 48kHz | Verify math |
| IRQ processing budget | Must complete before next IRQ | Profile on hardware |

**Timing Analysis:**
```
At 48 kHz, 16-bit stereo:
- 1 sample = 20.83 µs
- 512 samples = 10.67 ms (reasonable buffer)
- Symbol period = 8 ms = 384 samples

IRQ must process 512 samples in <10 ms
- ARM Cortex-A72 @ 1.5 GHz = 15M cycles available
- FFT (64-point): ~5000 cycles
- Decimation (6:1): ~2000 cycles per sample = 1M cycles
- Plenty of headroom ✓
```

#### Gap 5: Platform Abstraction
| Question | Why It Matters | Action |
|----------|----------------|--------|
| Audio interface abstraction | Same code on Linux/Circle | ✅ SOLVED |
| Timer abstraction | Timeouts, symbol timing | ✅ SOLVED |
| GPIO abstraction | PTT control | ✅ SOLVED |

**✅ SOLVED:** Platform Abstraction Layer interfaces are defined in the separate [PC-ALE-PAL](https://github.com/Alex-Pennington/PC-ALE-PAL) repository.

**Available Interfaces:**
- ✅ `IAudioDriver` interface (defined in PC-ALE-PAL)
- ✅ `ITimer` interface (defined in PC-ALE-PAL)
- ✅ `IRadio` interface (defined in PC-ALE-PAL)
- ✅ `ILogger` interface (defined in PC-ALE-PAL)
- ✅ `IEventHandler` interface (defined in PC-ALE-PAL)
- ✅ `ISIS` interface (defined in PC-ALE-PAL)

**What's Needed:**
- [ ] Implement `IAudioDriver` for Circle (I2S + TLV320 codec)
- [ ] Implement `IRadio` for Hamlib (Linux) or direct GPIO (bare metal)
- [ ] Implement `ITimer` for Circle's CTimer
- [ ] Wire PAL implementations into PC-ALE core

---

## Part 2: Implementation Phases

### Phase 7A: Implement Platform Drivers for Circle
**Duration:** 1-2 weeks  
**Dependencies:** PC-ALE-PAL interfaces (already defined)

#### Using PC-ALE-PAL

```bash
# Add PAL as git submodule
git submodule add https://github.com/Alex-Pennington/PC-ALE-PAL.git extern/PAL
git submodule update --init --recursive

# Your Circle implementation includes PAL headers
#include "extern/PAL/include/pal/IAudioDriver.h"
```

#### Deliverables:
```cpp
// src/platform/circle/circle_audio_driver.cpp
#include <pal/IAudioDriver.h>
#include <circle/sound/i2ssoundbasedevice.h>

class CircleAudioDriver : public IAudioDriver {
    CI2SSoundBaseDevice* sound_device_;
    
public:
    bool initialize(uint32_t sample_rate, uint32_t buffer_size) override {
        // Initialize Circle I2S + TLV320 codec
        sound_device_ = new CI2SSoundBaseDevice(...);
        // Configure for DRAWS hat
    }
    
    bool read(float* samples, size_t count) override {
        // Read from Circle DMA ring buffer
    }
    
    bool write(const float* samples, size_t count) override {
        // Write to Circle DMA ring buffer
    }
};
    
    // Initialize audio subsystem
    virtual bool initialize(uint32_t sample_rate, 
                           uint32_t buffer_size) = 0;
    
    // Start audio streaming
    virtual bool start() = 0;
    virtual void stop() = 0;
    
    // Callback registration
    using AudioCallback = std::function<void(
        const float* rx_samples,
        float* tx_samples,
        size_t num_samples
    )>;
    virtual void set_callback(AudioCallback cb) = 0;
    
    // TX/RX control
    virtual void set_ptt(bool transmit) = 0;
    virtual bool get_ptt() const = 0;
};

// include/platform/timer.h
class ITimer {
public:
    virtual ~ITimer() = default;
    virtual uint64_t get_time_ms() const = 0;
    virtual uint64_t get_time_us() const = 0;
    virtual void delay_ms(uint32_t ms) = 0;
    virtual void delay_us(uint32_t us) = 0;
};
```

#### Implementation Files:
```
src/platform/
├── audio_driver.h          # Interface definition
├── timer.h                 # Interface definition
├── linux/
│   ├── alsa_driver.cpp     # Linux ALSA implementation
│   └── linux_timer.cpp
├── circle/
│   ├── circle_driver.cpp   # Circle I2S implementation
│   └── circle_timer.cpp
└── windows/
    ├── wasapi_driver.cpp   # Windows WASAPI implementation
    └── windows_timer.cpp
```

#### Tasks:
- [ ] Design interfaces (1 day)
- [ ] Implement Linux/ALSA driver (3 days) - for testing
- [ ] Write unit tests with loopback (2 days)
- [ ] Verify PC-ALE core works with abstraction (2 days)

---

### Phase 7B: Circle Framework Setup
**Duration:** 1 week  
**Dependencies:** None (parallel with 7A)

#### Tasks:
- [ ] Clone Circle framework
- [ ] Set up ARM cross-compiler toolchain
- [ ] Build Circle sample apps (01-sp-helloworld, 30-sp-sound)
- [ ] Verify boot on Raspberry Pi 4
- [ ] Document build process

#### Environment Setup:
```bash
# Ubuntu/Debian development machine
sudo apt install gcc-aarch64-linux-gnu g++-aarch64-linux-gnu

# Clone Circle
git clone https://github.com/rsta2/circle.git
cd circle

# Configure for Pi 4
./configure -r 4 -p aarch64-linux-gnu-

# Build Circle library
./makeall clean
./makeall

# Build sound sample
cd sample/30-sp-sound
make
```

#### Validation Criteria:
- [ ] `kernel8-rpi4.img` boots on Pi 4
- [ ] Sound sample produces audio output
- [ ] I2S timing measured with oscilloscope

---

### Phase 7C: DRAWS Hat Bringup
**Duration:** 1-2 weeks  
**Dependencies:** 7B complete

#### Tasks:
- [ ] Obtain DRAWS hat and schematic
- [ ] Identify I2C bus and codec address
- [ ] Write TLV320AIC3204 initialization code
- [ ] Verify audio loopback (mic → speaker)
- [ ] Verify PTT GPIO control
- [ ] Measure audio quality (THD, SNR)

#### Codec Initialization (Based on Linux Driver):
```cpp
class TLV320AIC3204 {
    CI2CMaster& i2c;
    static constexpr uint8_t CODEC_ADDR = 0x18;
    
public:
    bool initialize() {
        // Page 0 - Main registers
        select_page(0);
        
        // Software reset
        write_reg(0x01, 0x01);
        delay_ms(10);
        
        // Clock configuration
        // Assume I2S BCLK/WCLK from Pi, 48kHz
        write_reg(0x04, 0x00);  // PLL disabled, CODEC_CLKIN = MCLK
        write_reg(0x05, 0x91);  // PLL P=1, R=1
        write_reg(0x06, 0x04);  // PLL J=4
        write_reg(0x07, 0x00);  // PLL D MSB
        write_reg(0x08, 0x00);  // PLL D LSB
        
        // DAC setup
        write_reg(0x0B, 0x82);  // NDAC = 2, powered
        write_reg(0x0C, 0x88);  // MDAC = 8, powered
        write_reg(0x0D, 0x00);  // DOSR MSB
        write_reg(0x0E, 0x80);  // DOSR = 128
        
        // ADC setup
        write_reg(0x12, 0x82);  // NADC = 2, powered
        write_reg(0x13, 0x88);  // MADC = 8, powered
        write_reg(0x14, 0x80);  // AOSR = 128
        
        // Audio interface
        write_reg(0x1B, 0x00);  // I2S, 16-bit, BCLK/WCLK inputs
        
        // Page 1 - Analog routing
        select_page(1);
        
        // Power up analog blocks
        write_reg(0x01, 0x08);  // Disable weak AVDD
        write_reg(0x02, 0x00);  // Enable AVDD LDO
        write_reg(0x7B, 0x01);  // REF power up (40ms)
        delay_ms(50);
        
        // Route IN2L/R to ADC (DRAWS mic input)
        write_reg(0x34, 0x30);  // IN2L to LADC+
        write_reg(0x37, 0x30);  // IN2R to RADC+
        
        // Route DAC to headphone/line out
        write_reg(0x0C, 0x08);  // LDAC to HPL
        write_reg(0x0D, 0x08);  // RDAC to HPR
        
        // Power up DAC
        select_page(0);
        write_reg(0x3F, 0xD4);  // DAC powered, left+right
        
        // Power up ADC
        write_reg(0x51, 0xC0);  // ADC powered, left+right
        
        // Unmute
        write_reg(0x40, 0x00);  // DAC unmute
        
        return true;
    }
    
private:
    void select_page(uint8_t page) {
        write_reg(0x00, page);
    }
    
    void write_reg(uint8_t reg, uint8_t value) {
        uint8_t data[2] = {reg, value};
        i2c.Write(CODEC_ADDR, data, 2);
    }
};
```

#### Validation Criteria:
- [ ] Codec initializes without I2C errors
- [ ] Audio input captures microphone
- [ ] Audio output produces clean tones
- [ ] PTT GPIO toggles TX LED

---

### Phase 7D: Sample Rate Conversion
**Duration:** 1 week  
**Dependencies:** 7A complete

#### Design:
```cpp
// Polyphase FIR resampler
// 48 kHz → 8 kHz (decimate by 6)
// 8 kHz → 48 kHz (interpolate by 6)

class Resampler {
    // Anti-aliasing filter coefficients
    // Designed for 4 kHz cutoff (Nyquist for 8 kHz)
    static constexpr int TAPS = 48;  // 8 taps per phase
    static constexpr int PHASES = 6;
    float coeffs[TAPS];
    float history[TAPS];
    
public:
    Resampler() {
        // Design lowpass filter: Fc = 4kHz, Fs = 48kHz
        // Using windowed sinc (Hamming window)
        design_filter(4000.0f, 48000.0f, coeffs, TAPS);
    }
    
    // Decimate: 48kHz → 8kHz
    size_t decimate(const float* input, size_t in_samples,
                    float* output) {
        size_t out_samples = 0;
        for (size_t i = 0; i < in_samples; i++) {
            // Shift history
            shift_history(input[i]);
            
            // Output every 6th sample
            if ((i % PHASES) == 0) {
                output[out_samples++] = apply_filter();
            }
        }
        return out_samples;
    }
    
    // Interpolate: 8kHz → 48kHz
    size_t interpolate(const float* input, size_t in_samples,
                       float* output) {
        size_t out_samples = 0;
        for (size_t i = 0; i < in_samples; i++) {
            // Insert sample and 5 zeros
            for (int p = 0; p < PHASES; p++) {
                if (p == 0) {
                    shift_history(input[i] * PHASES);
                } else {
                    shift_history(0.0f);
                }
                output[out_samples++] = apply_filter();
            }
        }
        return out_samples;
    }
};
```

#### Optimization (NEON SIMD):
```cpp
// ARM NEON optimized FIR filter
#include <arm_neon.h>

float apply_filter_neon(const float* history, 
                        const float* coeffs, 
                        int taps) {
    float32x4_t sum = vdupq_n_f32(0.0f);
    
    for (int i = 0; i < taps; i += 4) {
        float32x4_t h = vld1q_f32(&history[i]);
        float32x4_t c = vld1q_f32(&coeffs[i]);
        sum = vmlaq_f32(sum, h, c);  // sum += h * c
    }
    
    // Horizontal sum
    float32x2_t sum2 = vadd_f32(vget_low_f32(sum), vget_high_f32(sum));
    return vget_lane_f32(vpadd_f32(sum2, sum2), 0);
}
```

#### Validation Criteria:
- [ ] 1 kHz tone decimated correctly (frequency preserved)
- [ ] THD < 0.1% through resample chain
- [ ] Latency < 2 ms added

---

### Phase 7E: Circle Audio Driver
**Duration:** 2 weeks  
**Dependencies:** 7A, 7B, 7C, 7D complete

#### Implementation:
```cpp
// src/platform/circle/circle_audio_driver.cpp

#include <circle/i2ssoundbasedevice.h>
#include <circle/gpiopin.h>
#include "platform/audio_driver.h"
#include "resampler.h"

class CircleAudioDriver : public IAudioDriver, 
                          public CI2SSoundCallback {
    CI2SSoundBaseDevice i2s;
    CGPIOPin ptt_gpio;
    TLV320AIC3204 codec;
    Resampler resampler;
    
    AudioCallback user_callback;
    
    // Buffers (48 kHz, stereo)
    static constexpr size_t I2S_BUFFER = 512;
    int16_t i2s_rx[I2S_BUFFER * 2];  // Stereo
    int16_t i2s_tx[I2S_BUFFER * 2];
    
    // Buffers (8 kHz, mono)
    static constexpr size_t ALE_BUFFER = I2S_BUFFER / 6;
    float ale_rx[ALE_BUFFER];
    float ale_tx[ALE_BUFFER];
    
    bool is_transmitting = false;
    
public:
    bool initialize(uint32_t sample_rate, 
                   uint32_t buffer_size) override {
        // Initialize codec
        if (!codec.initialize()) {
            return false;
        }
        
        // Initialize I2S
        if (!i2s.Initialize(48000, I2S_BUFFER, this)) {
            return false;
        }
        
        // Initialize PTT GPIO
        ptt_gpio.AssignPin(23);  // DRAWS PTT
        ptt_gpio.SetMode(GPIOModeOutput);
        ptt_gpio.Write(LOW);
        
        return true;
    }
    
    bool start() override {
        return i2s.Start();
    }
    
    void stop() override {
        i2s.Cancel();
    }
    
    void set_callback(AudioCallback cb) override {
        user_callback = cb;
    }
    
    void set_ptt(bool transmit) override {
        is_transmitting = transmit;
        ptt_gpio.Write(transmit ? HIGH : LOW);
    }
    
    bool get_ptt() const override {
        return is_transmitting;
    }
    
    // I2S callback (called from DMA interrupt)
    unsigned GetChunk(s16* buffer, unsigned chunk_size) override {
        // Convert stereo s16 → mono float
        for (unsigned i = 0; i < chunk_size; i++) {
            float left = buffer[i * 2] / 32768.0f;
            float right = buffer[i * 2 + 1] / 32768.0f;
            // Mono mix for RX
            float mono = (left + right) * 0.5f;
            i2s_rx[i] = mono;
        }
        
        // Decimate 48kHz → 8kHz
        size_t ale_samples = resampler.decimate(
            i2s_rx, chunk_size, ale_rx);
        
        // Call user callback (PC-ALE processing)
        if (user_callback) {
            user_callback(ale_rx, ale_tx, ale_samples);
        }
        
        // If transmitting, interpolate TX audio
        if (is_transmitting) {
            // Interpolate 8kHz → 48kHz
            resampler.interpolate(ale_tx, ale_samples, i2s_tx);
            
            // Convert mono float → stereo s16
            for (unsigned i = 0; i < chunk_size; i++) {
                s16 sample = (s16)(i2s_tx[i] * 32767.0f);
                buffer[i * 2] = sample;      // Left
                buffer[i * 2 + 1] = sample;  // Right
            }
        } else {
            // Silence TX
            memset(buffer, 0, chunk_size * 4);
        }
        
        return chunk_size;
    }
};
```

#### Validation Criteria:
- [ ] Audio callback fires at correct rate
- [ ] Symbols decoded from known test signal
- [ ] TX audio measures correct frequencies
- [ ] PTT timing verified with oscilloscope

---

### Phase 7F: Integration & Testing
**Duration:** 2 weeks  
**Dependencies:** All above complete

#### Tasks:
- [ ] Integrate CircleAudioDriver with PC-ALE state machine
- [ ] Implement main application loop
- [ ] Test scanning on 40m ALE frequency
- [ ] Test call initiation (individual call)
- [ ] Test link establishment (full handshake)
- [ ] Verify LQA updates
- [ ] OTA test with another ALE station

#### Main Application:
```cpp
// main.cpp - PC-ALE Bare Metal Application

#include <circle/startup.h>
#include <circle/logger.h>
#include "platform/circle/circle_audio_driver.h"
#include "ale/state_machine.h"
#include "ale/fsk_modem.h"
#include "ale/lqa_database.h"

class PCaleApp {
    CircleAudioDriver audio;
    FFTDemodulator demodulator;
    ToneGenerator modulator;
    ALEStateMachine state_machine;
    LQADatabase lqa;
    
    // Lock-free queues for RT ↔ non-RT communication
    CircularBuffer<uint8_t, 1024> rx_symbols;
    CircularBuffer<uint8_t, 1024> tx_symbols;
    
public:
    bool Initialize() {
        // Initialize audio
        if (!audio.initialize(8000, 512)) {
            CLogger::Get()->Write("PCALE", LogError, 
                                  "Audio init failed");
            return false;
        }
        
        // Set up audio callback
        audio.set_callback([this](const float* rx, float* tx, 
                                   size_t samples) {
            ProcessAudio(rx, tx, samples);
        });
        
        // Configure ALE
        state_machine.set_self_address("KY4OLB");
        state_machine.add_channel(7102000);   // 40m
        state_machine.add_channel(14109000);  // 20m
        
        // Start audio
        audio.start();
        
        // Start scanning
        state_machine.start_scanning();
        
        CLogger::Get()->Write("PCALE", LogNotice, 
                              "PC-ALE initialized, scanning...");
        return true;
    }
    
    void Run() {
        while (true) {
            // Process received symbols
            uint8_t symbol;
            while (rx_symbols.pop(symbol)) {
                state_machine.process_symbol(symbol);
            }
            
            // Update state machine
            state_machine.update();
            
            // Handle TX requests
            if (state_machine.has_tx_pending()) {
                HandleTransmission();
            }
            
            // Brief sleep to avoid busy-wait
            CTimer::Get()->MsDelay(10);
        }
    }
    
private:
    // Called from I2S interrupt context (MUST BE FAST)
    void ProcessAudio(const float* rx, float* tx, size_t samples) {
        // RX: Demodulate symbols
        for (size_t i = 0; i + 64 <= samples; i += 64) {
            auto result = demodulator.process_samples(&rx[i], 64);
            if (result.symbol_valid) {
                rx_symbols.push(result.symbol);
            }
        }
        
        // TX: Generate modulated audio
        if (audio.get_ptt()) {
            uint8_t sym;
            size_t generated = 0;
            while (generated < samples && tx_symbols.pop(sym)) {
                generated += modulator.generate_symbol(
                    sym, &tx[generated], 64);
            }
            // Pad with silence if no more symbols
            while (generated < samples) {
                tx[generated++] = 0.0f;
            }
        } else {
            memset(tx, 0, samples * sizeof(float));
        }
    }
    
    void HandleTransmission() {
        auto word = state_machine.get_tx_word();
        
        // Key PTT
        audio.set_ptt(true);
        CTimer::Get()->MsDelay(50);  // PTT delay
        
        // Queue symbols for modulation
        auto symbols = word.to_symbols();
        for (uint8_t sym : symbols) {
            tx_symbols.push(sym);
        }
        
        // Wait for TX complete
        while (!tx_symbols.empty()) {
            CTimer::Get()->MsDelay(10);
        }
        
        // Unkey PTT
        CTimer::Get()->MsDelay(50);  // Tail delay
        audio.set_ptt(false);
    }
};

// Circle kernel entry point
int main() {
    PCaleApp app;
    
    if (!app.Initialize()) {
        return 1;
    }
    
    app.Run();  // Never returns
    
    return 0;
}
```

---

## Part 3: Timeline Summary

| Phase | Description | Duration | Dependencies |
|-------|-------------|----------|--------------|
| 7A | Platform Abstraction Layer | 1-2 weeks | None |
| 7B | Circle Framework Setup | 1 week | None |
| 7C | DRAWS Hat Bringup | 1-2 weeks | 7B |
| 7D | Sample Rate Conversion | 1 week | 7A |
| 7E | Circle Audio Driver | 2 weeks | 7A, 7B, 7C, 7D |
| 7F | Integration & Testing | 2 weeks | All above |
| **Total** | **Bare Metal ALE Station** | **8-11 weeks** | |

---

## Part 4: Hardware Bill of Materials

| Item | Source | Price | Notes |
|------|--------|-------|-------|
| Raspberry Pi 4 (4GB) | Amazon/Adafruit | ~$55 | 8GB also works |
| Raspberry Pi 5 (8GB) | Amazon/Adafruit | ~$80 | Alternative |
| NW Digital DRAWS Hat | nwdigitalradio.com | ~$200 | Audio + GPS + PTT |
| microSD Card (32GB) | Amazon | ~$10 | For Circle kernel |
| Heatsink + Fan | Amazon | ~$15 | Recommended for Pi 4/5 |
| 12V Power Supply | Amazon | ~$20 | For DRAWS (powers Pi) |
| **Total** | | **~$300-380** | |

---

## Part 5: Risk Assessment

| Risk | Impact | Probability | Mitigation |
|------|--------|-------------|------------|
| Circle I2S driver issues | High | Medium | Fall back to Linux ALSA initially |
| TLV320 codec quirks | Medium | Medium | Study Linux driver carefully |
| Real-time deadline misses | High | Low | Profile early, optimize if needed |
| C++17 toolchain issues | Medium | Low | Verify before starting |
| DRAWS hat availability | Medium | Medium | Order early, have backup plan |

---

## Part 6: Success Criteria

### Minimum Viable Product (MVP)
- [ ] Boot to scanning in <5 seconds
- [ ] Decode ALE calls from recorded audio
- [ ] Initiate individual call (TX verified on spectrum analyzer)
- [ ] Complete link establishment with another station

### Full Feature Set
- [ ] All MVP features
- [ ] LQA tracking across channels
- [ ] AMD message display
- [ ] Net call participation
- [ ] FS-1052 ARQ data transfer
- [ ] GPS time sync (DRAWS GPS)
- [ ] Web-based status display (ethernet)

---

## Part 7: Research Tasks (Immediate)

### Week 1 Actions:
1. **Order Hardware**
   - [ ] Raspberry Pi 4 or 5
   - [ ] DRAWS hat from NW Digital Radio
   - [ ] Quality SD card

2. **Circle Investigation**
   - [ ] Clone and build Circle
   - [ ] Study `lib/sound/i2ssoundbasedevice.cpp`
   - [ ] Study `sample/30-sp-sound/`
   - [ ] Verify C++17 support

3. **DRAWS Investigation**
   - [ ] Request schematic from NW Digital Radio
   - [ ] Review Linux `udrc` driver source
   - [ ] Document GPIO pinout

4. **Toolchain Setup**
   - [ ] Install ARM cross-compiler
   - [ ] Build Circle samples
   - [ ] Verify boot on Pi hardware

---

## Appendix A: DRAWS GPIO Quick Reference

```cpp
// DRAWS Hat GPIO Definitions for Circle

// PTT Control - ACTIVE HIGH
#define GPIO_PTT_LEFT   12  // Left mini-DIN6 connector
#define GPIO_PTT_RIGHT  23  // Right mini-DIN6 connector

// CRITICAL: Initialize GPIO LOW immediately on boot!
// Pi firmware enables pull-ups which can key transmitter during boot.
// Either:
//   1. Set GPIO as output LOW in first lines of kernel init, OR
//   2. Add 2.2k pull-down resistor (Pin 32 to Pin 30 for GPIO 12)

class DRAWSHardware {
    CGPIOPin ptt_left;
    CGPIOPin ptt_right;
    
public:
    void init() {
        // IMMEDIATELY set PTT pins LOW to prevent spurious TX
        ptt_left.AssignPin(GPIO_PTT_LEFT);
        ptt_left.SetMode(GPIOModeOutput);
        ptt_left.Write(LOW);  // RX mode
        
        ptt_right.AssignPin(GPIO_PTT_RIGHT);
        ptt_right.SetMode(GPIOModeOutput);
        ptt_right.Write(LOW);  // RX mode
    }
    
    void set_ptt(bool transmit, bool use_left = true) {
        CGPIOPin& ptt = use_left ? ptt_left : ptt_right;
        ptt.Write(transmit ? HIGH : LOW);
    }
};
```

**Audio Configuration:**
```cpp
// DRAWS Audio - TLV320AIC32x4 codec
#define DRAWS_SAMPLE_RATE   48000   // Native sample rate
#define DRAWS_BITS          16      // Bits per sample
#define DRAWS_CHANNELS      2       // Stereo (L/R connectors)

// PC-ALE expects 8kHz, so we need 6:1 resampling
#define ALE_SAMPLE_RATE     8000
#define RESAMPLE_RATIO      6       // 48000 / 8000
```

---

## Appendix B: Circle Resources

**Official Repository:** https://github.com/rsta2/circle

**Key Documentation:**
- `doc/` - Reference documentation
- `include/circle/` - API headers
- `sample/` - Example applications

**Relevant Samples:**
- `01-sp-helloworld` - Basic kernel
- `30-sp-sound` - I2S audio output
- `31-sp-midi` - MIDI I/O (USB reference)
- `42-sp-net` - Ethernet networking

**Community:**
- Circle GitHub Issues
- Raspberry Pi forums

---

## Appendix C: DRAWS Resources

**Manufacturer:** NW Digital Radio (nwdigitalradio.com)

**Key Documents:**
- Schematic: `draws3` dated 9/17/18 (available from groups.io wiki)
- Linux driver/scripts: https://github.com/nwdigitalradio/n7nix
- Groups.io wiki: https://nw-digital-radio.groups.io/g/udrc/wiki

**Chips on DRAWS Hat:**

| Chip | Function | Datasheet |
|------|----------|----------|
| TLV320AIC32x4 | Audio codec | TI SLAU578 |
| SkyTraq S1216F8-GL | GPS with PPS | SkyTraq datasheet |
| 16IS752 | Dual UART | NXP SC16IS752 |
| TLA2024 | 4-ch ADC | TI SBAS673 |
| 24C32 | EEPROM | Microchip AT24C32 |

**TLV320AIC32x4 Key Specs:**
- Sample rates: 8 kHz to 192 kHz
- ADC/DAC: 24-bit
- I2C address: 0x18 (default)
- I2S interface to Pi
- Supports multiple clock sources

**Connector Pinout (Mini DIN-6 TNC Standard):**
```
    1  2
   3    4
    5  6

Pin 1: Data Out (to radio MIC)
Pin 2: Ground
Pin 3: PTT
Pin 4: Data In (from radio AFOUT/DISCOUT)
Pin 5: Ground  
Pin 6: SQL (not used by DRAWS)
```

---

*Document Version: 1.0*  
*Last Updated: December 2024*
