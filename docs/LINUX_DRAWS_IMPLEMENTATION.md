# PC-ALE Linux + DRAWS Implementation Plan
## Phase 7: Audio I/O Integration

**Document:** Linux Implementation Roadmap  
**Author:** Alex Pennington, AAM402/KY4OLB  
**Date:** December 2024  
**Status:** Active Development

---

## Strategy

**Phase 1:** Linux + DRAWS (fast path to working system)  
**Phase 2:** Port to Circle bare metal (future, optional)

This document covers Phase 1 - getting PC-ALE running on Raspberry Pi with DRAWS hat under Linux.

---

## Hardware Configuration

### DRAWS Hat - Confirmed Specs

| Component | Specification |
|-----------|---------------|
| Audio Codec | TLV320AIC32x4 |
| Sample Rate | 48 kHz |
| ALSA Device | `udrc` or `plughw:1,0` |
| PTT Left | GPIO 12 |
| PTT Right | GPIO 23 |
| GPS | SkyTraq S1216F8-GL with PPS |

### Software Stack

```
┌─────────────────────────────────────────┐
│           PC-ALE Application            │
│  (State Machine, Protocol, LQA, ARQ)    │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│      Platform Abstraction Layer         │
│   IAudioDriver, ITimer, IPTT            │
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│         Linux Implementation            │
│  ALSA Audio, libgpiod PTT, clock_gettime│
└─────────────────┬───────────────────────┘
                  │
┌─────────────────▼───────────────────────┐
│            DRAWS Hat                    │
│    TLV320AIC32x4 + GPIO + GPS           │
└─────────────────────────────────────────┘
```

---

## Implementation Tasks

### Task 1: Platform Abstraction Layer (PAL)

**Files to create:**

```
include/platform/
├── audio_driver.h      # Audio interface
├── ptt_driver.h        # PTT interface
├── timer.h             # Timing interface
└── platform.h          # Platform detection

src/platform/
├── linux/
│   ├── alsa_audio.cpp  # ALSA implementation
│   ├── gpio_ptt.cpp    # GPIO PTT via libgpiod
│   └── linux_timer.cpp # clock_gettime wrapper
└── stub/
    └── stub_audio.cpp  # For unit testing
```

**Audio Interface:**

```cpp
// include/platform/audio_driver.h
#pragma once
#include <functional>
#include <cstdint>

class IAudioDriver {
public:
    virtual ~IAudioDriver() = default;
    
    // Configuration
    virtual bool initialize(uint32_t sample_rate, 
                           uint32_t buffer_frames) = 0;
    virtual void shutdown() = 0;
    
    // Audio streaming
    virtual bool start() = 0;
    virtual void stop() = 0;
    
    // Callback for audio processing
    // Called from audio thread with RX samples, expects TX samples back
    using AudioCallback = std::function<void(
        const float* rx_samples,    // Input from radio
        float* tx_samples,          // Output to radio
        size_t num_samples          // Number of samples
    )>;
    virtual void set_audio_callback(AudioCallback cb) = 0;
    
    // Status
    virtual bool is_running() const = 0;
    virtual uint32_t get_sample_rate() const = 0;
};
```

**PTT Interface:**

```cpp
// include/platform/ptt_driver.h
#pragma once

class IPTTDriver {
public:
    virtual ~IPTTDriver() = default;
    
    virtual bool initialize(int gpio_pin) = 0;
    virtual void shutdown() = 0;
    
    virtual void set_ptt(bool transmit) = 0;
    virtual bool get_ptt() const = 0;
};
```

---

### Task 2: ALSA Audio Implementation

**Dependencies:**
```bash
sudo apt install libasound2-dev
```

**Implementation:**

```cpp
// src/platform/linux/alsa_audio.cpp
#include "platform/audio_driver.h"
#include <alsa/asoundlib.h>
#include <thread>
#include <atomic>
#include <vector>

class ALSAAudioDriver : public IAudioDriver {
    snd_pcm_t* capture_handle = nullptr;
    snd_pcm_t* playback_handle = nullptr;
    
    std::thread audio_thread;
    std::atomic<bool> running{false};
    
    AudioCallback callback;
    uint32_t sample_rate = 48000;
    uint32_t buffer_frames = 512;
    
    // Resampler for 48kHz <-> 8kHz conversion
    std::vector<float> resample_buffer;
    
public:
    bool initialize(uint32_t rate, uint32_t frames) override {
        sample_rate = rate;
        buffer_frames = frames;
        
        // Open capture device (RX from radio)
        int err = snd_pcm_open(&capture_handle, "plughw:udrc,0",
                               SND_PCM_STREAM_CAPTURE, 0);
        if (err < 0) {
            fprintf(stderr, "Cannot open capture: %s\n", 
                    snd_strerror(err));
            return false;
        }
        
        // Open playback device (TX to radio)
        err = snd_pcm_open(&playback_handle, "plughw:udrc,0",
                          SND_PCM_STREAM_PLAYBACK, 0);
        if (err < 0) {
            fprintf(stderr, "Cannot open playback: %s\n",
                    snd_strerror(err));
            return false;
        }
        
        // Configure both devices
        if (!configure_device(capture_handle, rate, frames) ||
            !configure_device(playback_handle, rate, frames)) {
            return false;
        }
        
        return true;
    }
    
    void shutdown() override {
        stop();
        if (capture_handle) snd_pcm_close(capture_handle);
        if (playback_handle) snd_pcm_close(playback_handle);
        capture_handle = nullptr;
        playback_handle = nullptr;
    }
    
    bool start() override {
        if (running) return true;
        running = true;
        audio_thread = std::thread(&ALSAAudioDriver::audio_loop, this);
        return true;
    }
    
    void stop() override {
        running = false;
        if (audio_thread.joinable()) {
            audio_thread.join();
        }
    }
    
    void set_audio_callback(AudioCallback cb) override {
        callback = cb;
    }
    
    bool is_running() const override { return running; }
    uint32_t get_sample_rate() const override { return sample_rate; }
    
private:
    bool configure_device(snd_pcm_t* handle, uint32_t rate, uint32_t frames) {
        snd_pcm_hw_params_t* params;
        snd_pcm_hw_params_alloca(&params);
        snd_pcm_hw_params_any(handle, params);
        
        // Interleaved stereo, 16-bit
        snd_pcm_hw_params_set_access(handle, params,
            SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(handle, params,
            SND_PCM_FORMAT_S16_LE);
        snd_pcm_hw_params_set_channels(handle, params, 2);
        
        unsigned int actual_rate = rate;
        snd_pcm_hw_params_set_rate_near(handle, params, &actual_rate, 0);
        
        snd_pcm_uframes_t actual_frames = frames;
        snd_pcm_hw_params_set_period_size_near(handle, params, 
                                                &actual_frames, 0);
        
        int err = snd_pcm_hw_params(handle, params);
        if (err < 0) {
            fprintf(stderr, "Cannot set params: %s\n", snd_strerror(err));
            return false;
        }
        
        return true;
    }
    
    void audio_loop() {
        std::vector<int16_t> capture_buf(buffer_frames * 2);  // Stereo
        std::vector<int16_t> playback_buf(buffer_frames * 2);
        std::vector<float> rx_float(buffer_frames);
        std::vector<float> tx_float(buffer_frames);
        
        while (running) {
            // Read from capture
            snd_pcm_sframes_t frames = snd_pcm_readi(
                capture_handle, capture_buf.data(), buffer_frames);
            
            if (frames < 0) {
                snd_pcm_prepare(capture_handle);
                continue;
            }
            
            // Convert stereo S16 -> mono float (use left channel)
            for (size_t i = 0; i < (size_t)frames; i++) {
                rx_float[i] = capture_buf[i * 2] / 32768.0f;
            }
            
            // Clear TX buffer
            std::fill(tx_float.begin(), tx_float.end(), 0.0f);
            
            // Call user callback
            if (callback) {
                callback(rx_float.data(), tx_float.data(), frames);
            }
            
            // Convert mono float -> stereo S16
            for (size_t i = 0; i < (size_t)frames; i++) {
                int16_t sample = (int16_t)(tx_float[i] * 32767.0f);
                playback_buf[i * 2] = sample;      // Left
                playback_buf[i * 2 + 1] = sample;  // Right
            }
            
            // Write to playback
            snd_pcm_writei(playback_handle, playback_buf.data(), frames);
        }
    }
};
```

---

### Task 3: GPIO PTT Implementation

**Using libgpiod (modern GPIO interface):**

```bash
sudo apt install libgpiod-dev
```

```cpp
// src/platform/linux/gpio_ptt.cpp
#include "platform/ptt_driver.h"
#include <gpiod.h>

class GPIOPTTDriver : public IPTTDriver {
    struct gpiod_chip* chip = nullptr;
    struct gpiod_line* line = nullptr;
    int gpio_pin = -1;
    bool ptt_state = false;
    
public:
    bool initialize(int pin) override {
        gpio_pin = pin;
        
        // Open GPIO chip
        chip = gpiod_chip_open("/dev/gpiochip0");
        if (!chip) {
            perror("Cannot open GPIO chip");
            return false;
        }
        
        // Get the line (pin)
        line = gpiod_chip_get_line(chip, gpio_pin);
        if (!line) {
            perror("Cannot get GPIO line");
            return false;
        }
        
        // Request as output, initially LOW (RX mode)
        if (gpiod_line_request_output(line, "pcale-ptt", 0) < 0) {
            perror("Cannot request GPIO as output");
            return false;
        }
        
        return true;
    }
    
    void shutdown() override {
        if (line) {
            gpiod_line_set_value(line, 0);  // Ensure PTT off
            gpiod_line_release(line);
            line = nullptr;
        }
        if (chip) {
            gpiod_chip_close(chip);
            chip = nullptr;
        }
    }
    
    void set_ptt(bool transmit) override {
        if (line) {
            gpiod_line_set_value(line, transmit ? 1 : 0);
            ptt_state = transmit;
        }
    }
    
    bool get_ptt() const override {
        return ptt_state;
    }
};
```

---

### Task 4: Sample Rate Conversion

**PC-ALE FFT expects 8 kHz, DRAWS runs at 48 kHz.**

```cpp
// include/platform/resampler.h
#pragma once
#include <vector>
#include <cmath>

class Resampler {
    static constexpr int RATIO = 6;  // 48000 / 8000
    static constexpr int FILTER_TAPS = 48;
    
    std::vector<float> coeffs;
    std::vector<float> history;
    size_t history_pos = 0;
    
public:
    Resampler() : coeffs(FILTER_TAPS), history(FILTER_TAPS, 0.0f) {
        // Design lowpass filter: Fc = 3500 Hz, Fs = 48000 Hz
        // Windowed sinc (Hamming)
        float fc = 3500.0f / 48000.0f;  // Normalized cutoff
        int M = FILTER_TAPS - 1;
        
        for (int i = 0; i < FILTER_TAPS; i++) {
            float n = i - M / 2.0f;
            if (n == 0) {
                coeffs[i] = 2.0f * fc;
            } else {
                coeffs[i] = sin(2.0f * M_PI * fc * n) / (M_PI * n);
            }
            // Hamming window
            coeffs[i] *= 0.54f - 0.46f * cos(2.0f * M_PI * i / M);
        }
        
        // Normalize
        float sum = 0;
        for (auto c : coeffs) sum += c;
        for (auto& c : coeffs) c /= sum;
    }
    
    // Decimate: 48 kHz -> 8 kHz
    size_t decimate(const float* in, size_t in_count, float* out) {
        size_t out_count = 0;
        for (size_t i = 0; i < in_count; i++) {
            // Add to history
            history[history_pos] = in[i];
            history_pos = (history_pos + 1) % FILTER_TAPS;
            
            // Output every RATIO samples
            if ((i % RATIO) == 0) {
                float sum = 0;
                for (int j = 0; j < FILTER_TAPS; j++) {
                    size_t idx = (history_pos + j) % FILTER_TAPS;
                    sum += history[idx] * coeffs[j];
                }
                out[out_count++] = sum;
            }
        }
        return out_count;
    }
    
    // Interpolate: 8 kHz -> 48 kHz
    size_t interpolate(const float* in, size_t in_count, float* out) {
        size_t out_count = 0;
        for (size_t i = 0; i < in_count; i++) {
            // Insert sample followed by RATIO-1 zeros
            for (int j = 0; j < RATIO; j++) {
                history[history_pos] = (j == 0) ? in[i] * RATIO : 0.0f;
                history_pos = (history_pos + 1) % FILTER_TAPS;
                
                float sum = 0;
                for (int k = 0; k < FILTER_TAPS; k++) {
                    size_t idx = (history_pos + k) % FILTER_TAPS;
                    sum += history[idx] * coeffs[k];
                }
                out[out_count++] = sum;
            }
        }
        return out_count;
    }
};
```

---

### Task 5: Integration with PC-ALE Core

```cpp
// src/pcale_linux.cpp - Main application

#include "ale/state_machine.h"
#include "ale/fsk_modem.h"
#include "ale/lqa_database.h"
#include "platform/linux/alsa_audio.cpp"
#include "platform/linux/gpio_ptt.cpp"
#include "platform/resampler.h"

class PCALEApplication {
    ALSAAudioDriver audio;
    GPIOPTTDriver ptt;
    Resampler rx_resampler;
    Resampler tx_resampler;
    
    FFTDemodulator demodulator;
    ToneGenerator modulator;
    ALEStateMachine state_machine;
    LQADatabase lqa;
    
    // Buffers
    std::vector<float> rx_8k;   // 8 kHz RX samples
    std::vector<float> tx_8k;   // 8 kHz TX samples
    std::vector<float> tx_48k;  // 48 kHz TX samples
    
public:
    bool initialize(const std::string& callsign, int ptt_gpio) {
        // Initialize PTT first (safety!)
        if (!ptt.initialize(ptt_gpio)) {
            fprintf(stderr, "Failed to initialize PTT GPIO %d\n", ptt_gpio);
            return false;
        }
        
        // Initialize audio
        // 48 kHz, 512 frames = ~10.7ms latency
        if (!audio.initialize(48000, 512)) {
            fprintf(stderr, "Failed to initialize audio\n");
            return false;
        }
        
        // Configure ALE
        state_machine.set_self_address(callsign);
        
        // Set audio callback
        audio.set_audio_callback([this](const float* rx, float* tx, size_t n) {
            process_audio(rx, tx, n);
        });
        
        // Allocate buffers
        rx_8k.resize(512 / 6 + 16);  // ~85 samples at 8 kHz
        tx_8k.resize(512 / 6 + 16);
        tx_48k.resize(512);
        
        return true;
    }
    
    void add_channel(uint32_t frequency_hz) {
        state_machine.add_channel(frequency_hz);
    }
    
    void start() {
        audio.start();
        state_machine.start_scanning();
        printf("PC-ALE started, scanning...\n");
    }
    
    void run() {
        while (true) {
            // Update state machine (handles timeouts, scanning, etc.)
            state_machine.update();
            
            // Check for TX requests
            if (state_machine.has_tx_pending() && !ptt.get_ptt()) {
                start_transmission();
            }
            
            // Brief sleep
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
private:
    void process_audio(const float* rx_48k, float* tx_48k_out, size_t n) {
        // Decimate RX: 48 kHz -> 8 kHz
        size_t rx_8k_count = rx_resampler.decimate(rx_48k, n, rx_8k.data());
        
        // Process through demodulator
        for (size_t i = 0; i + 64 <= rx_8k_count; i += 64) {
            auto result = demodulator.process_samples(&rx_8k[i], 64);
            if (result.symbol_valid) {
                state_machine.process_symbol(result.symbol);
            }
        }
        
        // Generate TX if transmitting
        if (ptt.get_ptt()) {
            // Generate 8 kHz samples
            size_t tx_8k_count = rx_8k_count;
            modulator.generate_samples(tx_8k.data(), tx_8k_count);
            
            // Interpolate TX: 8 kHz -> 48 kHz
            tx_resampler.interpolate(tx_8k.data(), tx_8k_count, tx_48k_out);
        } else {
            // Silence
            std::fill_n(tx_48k_out, n, 0.0f);
        }
    }
    
    void start_transmission() {
        auto word = state_machine.get_tx_word();
        
        // Key PTT
        ptt.set_ptt(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Queue word for modulation
        modulator.queue_word(word);
        
        // Wait for completion (handled in audio callback)
        while (!modulator.is_done()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        
        // Unkey PTT
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        ptt.set_ptt(false);
    }
};

int main(int argc, char* argv[]) {
    PCALEApplication app;
    
    // Initialize with callsign and PTT GPIO
    // GPIO 12 = LEFT connector, GPIO 23 = RIGHT connector
    if (!app.initialize("KY4OLB", 12)) {
        return 1;
    }
    
    // Add ALE channels
    app.add_channel(7102000);   // 40m
    app.add_channel(14109000);  // 20m
    
    // Start and run
    app.start();
    app.run();
    
    return 0;
}
```

---

## Build System Updates

### CMakeLists.txt additions:

```cmake
# Platform-specific sources
if(UNIX AND NOT APPLE)
    # Linux
    find_package(ALSA REQUIRED)
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GPIOD REQUIRED libgpiod)
    
    set(PLATFORM_SOURCES
        src/platform/linux/alsa_audio.cpp
        src/platform/linux/gpio_ptt.cpp
        src/platform/linux/linux_timer.cpp
    )
    
    set(PLATFORM_LIBS
        ${ALSA_LIBRARIES}
        ${GPIOD_LIBRARIES}
        pthread
    )
    
    include_directories(${ALSA_INCLUDE_DIRS} ${GPIOD_INCLUDE_DIRS})
endif()

# Main executable
add_executable(pcale_linux
    src/pcale_linux.cpp
    ${PLATFORM_SOURCES}
)

target_link_libraries(pcale_linux
    ale_fsk_core
    ale_fec
    ale_protocol
    ale_link
    ale_aqc
    ale_fs1052
    ale_lqa
    ${PLATFORM_LIBS}
)
```

---

## Development Setup on Raspberry Pi

```bash
# Install DRAWS image
# Download from http://images.nwdigitalradio.com/downloads/current_beta.zip

# Or add to existing Raspbian:
sudo apt update
sudo apt install -y git cmake build-essential
sudo apt install -y libasound2-dev libgpiod-dev

# Clone n7nix for DRAWS setup
git clone https://github.com/nwdigitalradio/n7nix
cd n7nix/config
sudo ./app_config.sh core

# Verify DRAWS is working
aplay -l  # Should show 'udrc' device
cat /proc/asound/cards

# Clone PC-ALE
git clone https://github.com/Alex-Pennington/PC-ALE
cd PC-ALE
mkdir build && cd build
cmake ..
make
```

---

## Testing Plan

### Phase 1: Audio Loopback
```bash
# Generate test tone, verify through DRAWS
./test_audio_loopback
```

### Phase 2: Decode Recorded ALE
```bash
# Play recorded ALE audio, verify decode
./pcale_linux --input recorded_ale.wav
```

### Phase 3: TX Test (into dummy load)
```bash
# Generate ALE sounding, verify on spectrum analyzer
./pcale_linux --test-tx --channel 7102000
```

### Phase 4: OTA Test
```bash
# Full duplex with another ALE station
./pcale_linux --callsign KY4OLB --channels 7102000,14109000
```

---

## Timeline

| Week | Task | Deliverable |
|------|------|-------------|
| 1 | PAL interfaces + ALSA driver | Audio loopback working |
| 2 | GPIO PTT + resampler | PTT control verified |
| 3 | Integration with PC-ALE core | Decoding recorded ALE |
| 4 | Hardware testing | OTA validation |

---

## Future: Port to Circle (Bare Metal)

Once Linux version is validated:

1. Create `src/platform/circle/` implementations
2. Same interfaces, different backends
3. Recompile PC-ALE core with Circle toolchain
4. Test on bare metal

The Platform Abstraction Layer means **zero changes** to protocol code.

---

*Document Version: 1.0*  
*Last Updated: December 2024*
