# PC-ALE 2.0 - Integration Guide

Complete guide for integrating PC-ALE 2.0 with audio I/O, radio control, and external systems.

---

## Table of Contents

1. [Overview](#overview)
2. [Audio I/O Integration](#audio-io-integration)
3. [Radio CAT Control](#radio-cat-control)
4. [MIL-STD-188-110A Modem Integration](#mil-std-188-110a-modem-integration)
5. [Threading and Real-Time Considerations](#threading-and-real-time-considerations)
6. [Complete Integration Example](#complete-integration-example)
7. [Platform-Specific Notes](#platform-specific-notes)
8. [Troubleshooting](#troubleshooting)

---

## Overview

PC-ALE 2.0 is designed as a modular library that integrates with:

1. **Audio subsystem** - Real-time audio I/O for FSK modulation/demodulation
2. **Radio control** - CAT interface for frequency changes, PTT, power control
3. **Data modem** - MIL-STD-188-110A or similar for higher-speed data after link establishment

### Integration Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    Application Layer                         │
│  (User interface, message handling, logging, configuration)  │
└────────────┬────────────────────────────────┬────────────────┘
             │                                │
             ├────────────────┐               │
             │                │               │
┌────────────▼─────┐  ┌───────▼──────┐  ┌────▼───────────┐
│   PC-ALE 2.0     │  │  Radio CAT   │  │  Data Modem    │
│   Libraries      │  │  Control     │  │  (188-110A)    │
│                  │  │  (Hamlib)    │  │                │
│ ┌──────────────┐ │  │              │  │                │
│ │ Phase 5: ARQ │ │  │ - Frequency  │  │ - PSK/QAM     │
│ │ Phase 4: AQC │ │  │ - PTT        │  │ - FEC         │
│ │ Phase 3: SM  │ │  │ - Power      │  │ - Interleave  │
│ │ Phase 2: Prot│ │  │ - Mode       │  │                │
│ │ Phase 1: FSK │ │  │              │  │                │
│ └──────────────┘ │  └───────┬──────┘  └────┬───────────┘
└─────┬────────────┘          │              │
      │                       │              │
┌─────▼────────────┐  ┌───────▼──────┐  ┌────▼───────────┐
│  Audio I/O       │  │ Serial/USB   │  │  Audio I/O     │
│  (DirectSound,   │  │ (COM port)   │  │  (for modem)   │
│   WASAPI, ALSA,  │  │              │  │                │
│   CoreAudio)     │  │              │  │                │
└──────────────────┘  └──────────────┘  └────────────────┘
```

### Callback-Driven Design

PC-ALE uses callbacks for asynchronous events:

```cpp
// Application registers callbacks
ale_state_machine.init(
    on_word_transmit,      // Called when ALE word needs transmission
    on_link_established,   // Called when link is ready
    on_amd_received        // Called when message arrives
);

arq_engine.init(
    on_data_received,      // Called when data block arrives
    on_block_acknowledged, // Called when block is ACKed
    on_transfer_complete   // Called when file transfer finishes
);
```

---

## Audio I/O Integration

### Requirements

- **Sample rate:** 8000 Hz minimum (48000 Hz recommended for quality)
- **Sample format:** 16-bit signed integer (int16_t) or float32
- **Channels:** Mono (single channel)
- **Latency:** < 50ms for real-time operation
- **Buffer size:** 512-2048 samples (64-256ms at 8000 Hz)

### Audio Callback Pattern

PC-ALE uses a push/pull model for audio:

```cpp
// Transmit: Application pulls audio from PC-ALE
void audio_tx_callback(float* buffer, size_t num_samples) {
    // Get audio samples from tone generator
    for (size_t i = 0; i < num_samples; i++) {
        buffer[i] = tone_generator.get_next_sample();
    }
}

// Receive: Application pushes audio to PC-ALE
void audio_rx_callback(const float* buffer, size_t num_samples) {
    // Feed samples to demodulator
    for (size_t i = 0; i < num_samples; i++) {
        demod_buffer.push_back(buffer[i]);
    }
    
    // Process when enough samples accumulated
    if (demod_buffer.size() >= SYMBOL_SAMPLES) {
        uint8_t symbol = fft_demodulator.demodulate(
            demod_buffer.data(), 
            SYMBOL_SAMPLES
        );
        process_demodulated_symbol(symbol);
        demod_buffer.erase(demod_buffer.begin(), 
                          demod_buffer.begin() + SYMBOL_SAMPLES);
    }
}
```

### Windows Integration (DirectSound)

```cpp
#include <dsound.h>

class AudioEngine {
public:
    bool init(int sample_rate, int buffer_size) {
        // Create DirectSound object
        DirectSoundCreate8(NULL, &ds_, NULL);
        ds_->SetCooperativeLevel(hwnd_, DSSCL_PRIORITY);
        
        // Setup buffer format
        WAVEFORMATEX wfx = {0};
        wfx.wFormatTag = WAVE_FORMAT_PCM;
        wfx.nChannels = 1;  // Mono
        wfx.nSamplesPerSec = sample_rate;
        wfx.wBitsPerSample = 16;
        wfx.nBlockAlign = wfx.nChannels * wfx.wBitsPerSample / 8;
        wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
        
        // Create capture buffer (RX)
        DSCBUFFERDESC dscbd = {0};
        dscbd.dwSize = sizeof(DSCBUFFERDESC);
        dscbd.dwBufferBytes = buffer_size * sizeof(int16_t);
        dscbd.lpwfxFormat = &wfx;
        dsCapture_->CreateCaptureBuffer(&dscbd, &capture_buffer_, NULL);
        
        // Create playback buffer (TX)
        DSBUFFERDESC dsbd = {0};
        dsbd.dwSize = sizeof(DSBUFFERDESC);
        dsbd.dwBufferBytes = buffer_size * sizeof(int16_t);
        dsbd.lpwfxFormat = &wfx;
        ds_->CreateSoundBuffer(&dsbd, &playback_buffer_, NULL);
        
        return true;
    }
    
    void process_audio() {
        // Read from capture buffer
        DWORD read_pos, capture_pos;
        capture_buffer_->GetCurrentPosition(&capture_pos, &read_pos);
        
        if (capture_pos != last_capture_pos_) {
            // Calculate samples available
            DWORD available = (capture_pos - last_capture_pos_) 
                            % buffer_size_;
            
            // Lock buffer
            void* ptr1;
            DWORD bytes1;
            capture_buffer_->Lock(last_capture_pos_, available,
                                 &ptr1, &bytes1, NULL, NULL, 0);
            
            // Process samples
            int16_t* samples = static_cast<int16_t*>(ptr1);
            size_t num_samples = bytes1 / sizeof(int16_t);
            
            for (size_t i = 0; i < num_samples; i++) {
                // Convert to float and process
                float sample = samples[i] / 32768.0f;
                process_rx_sample(sample);
            }
            
            capture_buffer_->Unlock(ptr1, bytes1, NULL, 0);
            last_capture_pos_ = capture_pos;
        }
        
        // Write to playback buffer
        DWORD play_pos, write_pos;
        playback_buffer_->GetCurrentPosition(&play_pos, &write_pos);
        
        // Similar locking/writing for TX
    }

private:
    IDirectSound8* ds_;
    IDirectSoundCapture8* dsCapture_;
    IDirectSoundBuffer* playback_buffer_;
    IDirectSoundCaptureBuffer* capture_buffer_;
    DWORD last_capture_pos_ = 0;
    size_t buffer_size_;
};
```

### Windows Integration (WASAPI) - Recommended

```cpp
#include <Audioclient.h>
#include <mmdeviceapi.h>

class WASAPIEngine {
public:
    bool init(int sample_rate) {
        CoInitialize(NULL);
        
        // Get default audio endpoint
        IMMDeviceEnumerator* enumerator;
        CoCreateInstance(__uuidof(MMDeviceEnumerator), NULL,
                        CLSCTX_ALL, __uuidof(IMMDeviceEnumerator),
                        (void**)&enumerator);
        
        IMMDevice* device;
        enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device);
        
        // Activate audio client
        device->Activate(__uuidof(IAudioClient), CLSCTX_ALL,
                        NULL, (void**)&audio_client_);
        
        // Set format
        WAVEFORMATEX* format;
        audio_client_->GetMixFormat(&format);
        format->nChannels = 1;  // Force mono
        format->nSamplesPerSec = sample_rate;
        
        // Initialize in shared mode
        audio_client_->Initialize(
            AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            10000000,  // 1 second buffer
            0, format, NULL
        );
        
        // Get render client
        audio_client_->GetService(__uuidof(IAudioRenderClient),
                                 (void**)&render_client_);
        
        return true;
    }
    
    void process_audio() {
        UINT32 padding;
        audio_client_->GetCurrentPadding(&padding);
        
        UINT32 available = buffer_size_ - padding;
        
        if (available > 0) {
            BYTE* data;
            render_client_->GetBuffer(available, &data);
            
            // Fill buffer with PC-ALE audio
            float* buffer = reinterpret_cast<float*>(data);
            for (UINT32 i = 0; i < available; i++) {
                buffer[i] = tone_generator_.get_next_sample();
            }
            
            render_client_->ReleaseBuffer(available, 0);
        }
    }

private:
    IAudioClient* audio_client_;
    IAudioRenderClient* render_client_;
    UINT32 buffer_size_;
    ToneGenerator tone_generator_;
};
```

### Linux Integration (ALSA)

```cpp
#include <alsa/asoundlib.h>

class ALSAEngine {
public:
    bool init(int sample_rate, int buffer_size) {
        // Open PCM device for playback
        snd_pcm_open(&playback_handle_, "default",
                     SND_PCM_STREAM_PLAYBACK, 0);
        
        // Set parameters
        snd_pcm_hw_params_t* params;
        snd_pcm_hw_params_alloca(&params);
        snd_pcm_hw_params_any(playback_handle_, params);
        
        snd_pcm_hw_params_set_access(playback_handle_, params,
                                     SND_PCM_ACCESS_RW_INTERLEAVED);
        snd_pcm_hw_params_set_format(playback_handle_, params,
                                     SND_PCM_FORMAT_S16_LE);
        snd_pcm_hw_params_set_channels(playback_handle_, params, 1);
        snd_pcm_hw_params_set_rate_near(playback_handle_, params,
                                        &sample_rate, 0);
        snd_pcm_hw_params_set_period_size_near(playback_handle_, params,
                                               &buffer_size, 0);
        
        snd_pcm_hw_params(playback_handle_, params);
        
        // Similarly for capture
        snd_pcm_open(&capture_handle_, "default",
                     SND_PCM_STREAM_CAPTURE, 0);
        // ... set capture params ...
        
        return true;
    }
    
    void process_audio() {
        // Read from capture
        int16_t capture_buffer[512];
        int frames = snd_pcm_readi(capture_handle_, capture_buffer, 512);
        
        if (frames > 0) {
            for (int i = 0; i < frames; i++) {
                float sample = capture_buffer[i] / 32768.0f;
                process_rx_sample(sample);
            }
        }
        
        // Write to playback
        int16_t playback_buffer[512];
        for (int i = 0; i < 512; i++) {
            float sample = tone_generator_.get_next_sample();
            playback_buffer[i] = static_cast<int16_t>(sample * 32767.0f);
        }
        
        snd_pcm_writei(playback_handle_, playback_buffer, 512);
    }

private:
    snd_pcm_t* playback_handle_;
    snd_pcm_t* capture_handle_;
};
```

### macOS Integration (CoreAudio)

```cpp
#include <AudioToolbox/AudioToolbox.h>

class CoreAudioEngine {
public:
    bool init(int sample_rate) {
        // Setup audio format
        AudioStreamBasicDescription format = {0};
        format.mSampleRate = sample_rate;
        format.mFormatID = kAudioFormatLinearPCM;
        format.mFormatFlags = kAudioFormatFlagIsFloat | 
                             kAudioFormatFlagIsPacked;
        format.mBitsPerChannel = 32;
        format.mChannelsPerFrame = 1;  // Mono
        format.mBytesPerFrame = format.mChannelsPerFrame * 
                               sizeof(float);
        format.mFramesPerPacket = 1;
        format.mBytesPerPacket = format.mBytesPerFrame;
        
        // Create audio queue
        AudioQueueNewOutput(&format, audio_callback, this,
                           NULL, NULL, 0, &audio_queue_);
        
        // Allocate buffers
        for (int i = 0; i < 3; i++) {
            AudioQueueAllocateBuffer(audio_queue_, 2048 * sizeof(float),
                                    &buffers_[i]);
            audio_callback(this, audio_queue_, buffers_[i]);
        }
        
        AudioQueueStart(audio_queue_, NULL);
        return true;
    }
    
    static void audio_callback(void* user_data,
                              AudioQueueRef queue,
                              AudioQueueBufferRef buffer) {
        CoreAudioEngine* engine = static_cast<CoreAudioEngine*>(user_data);
        
        float* data = static_cast<float*>(buffer->mAudioData);
        size_t num_samples = buffer->mAudioDataByteSize / sizeof(float);
        
        // Fill buffer with PC-ALE audio
        for (size_t i = 0; i < num_samples; i++) {
            data[i] = engine->tone_generator_.get_next_sample();
        }
        
        AudioQueueEnqueueBuffer(queue, buffer, 0, NULL);
    }

private:
    AudioQueueRef audio_queue_;
    AudioQueueBufferRef buffers_[3];
    ToneGenerator tone_generator_;
};
```

---

## Radio CAT Control

### Hamlib Integration

Hamlib provides unified radio control across 200+ radio models.

**Installation:**

```bash
# Linux
sudo apt-get install libhamlib-dev

# macOS
brew install hamlib

# Windows: Download from https://github.com/Hamlib/Hamlib/releases
```

**Basic Integration:**

```cpp
#include <hamlib/rig.h>

class RadioController {
public:
    bool init(const std::string& model, const std::string& port) {
        // Initialize Hamlib
        rig_set_debug(RIG_DEBUG_NONE);
        
        // Create rig
        rig_ = rig_init(std::stoi(model));  // e.g., 229 for IC-7300
        if (!rig_) return false;
        
        // Set port
        strncpy(rig_->state.rigport.pathname, port.c_str(), FILPATHLEN);
        rig_->state.rigport.parm.serial.rate = 19200;  // Baud rate
        
        // Open connection
        if (rig_open(rig_) != RIG_OK) {
            return false;
        }
        
        return true;
    }
    
    bool set_frequency(uint64_t freq_hz) {
        return rig_set_freq(rig_, RIG_VFO_CURR, freq_hz) == RIG_OK;
    }
    
    bool set_mode(const std::string& mode) {
        rmode_t rmode = RIG_MODE_USB;  // Default to USB for ALE
        if (mode == "LSB") rmode = RIG_MODE_LSB;
        
        return rig_set_mode(rig_, RIG_VFO_CURR, rmode, 
                           RIG_PASSBAND_NORMAL) == RIG_OK;
    }
    
    bool set_ptt(bool tx) {
        ptt_t ptt_mode = tx ? RIG_PTT_ON : RIG_PTT_OFF;
        return rig_set_ptt(rig_, RIG_VFO_CURR, ptt_mode) == RIG_OK;
    }
    
    bool set_power(int watts) {
        // Power level as percentage (0.0 - 1.0)
        float power_pct = watts / 100.0f;  // Assuming 100W max
        return rig_set_level(rig_, RIG_VFO_CURR, RIG_LEVEL_RFPOWER,
                            value_t{.f = power_pct}) == RIG_OK;
    }
    
    ~RadioController() {
        if (rig_) {
            rig_close(rig_);
            rig_cleanup(rig_);
        }
    }

private:
    RIG* rig_ = nullptr;
};
```

### Integration with ALE State Machine

```cpp
class ALERadioController {
public:
    void init(ALEStateMachine* sm, RadioController* radio) {
        sm_ = sm;
        radio_ = radio;
        
        // Register callbacks
        sm_->init(
            [this](const ALEWord& word) { on_word_transmit(word); },
            [this](const std::string& addr) { on_link_established(addr); },
            [this](const std::string& msg) { on_amd_received(msg); }
        );
    }
    
    void on_word_transmit(const ALEWord& word) {
        // Activate PTT before transmitting
        radio_->set_ptt(true);
        
        // Wait for PTT delay (radio-specific, e.g., 50ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Transmit word (audio will be generated by tone generator)
        transmit_word(word);
        
        // Wait for word duration (49 symbols × 8ms = 392ms)
        std::this_thread::sleep_for(std::chrono::milliseconds(392));
        
        // Deactivate PTT
        radio_->set_ptt(false);
    }
    
    void on_link_established(const std::string& address) {
        std::cout << "Link established with " << address << std::endl;
        
        // Could switch to data mode here
        // radio_->set_mode("DATA");
    }
    
    void start_scanning(const std::vector<uint64_t>& channels) {
        for (uint64_t freq : channels) {
            sm_->add_channel(freq);
        }
        
        sm_->start_scanning();
        
        // Update radio frequency as channels change
        current_channel_ = 0;
        radio_->set_frequency(channels[current_channel_]);
    }
    
    void update() {
        // Update state machine
        sm_->update();
        
        // Check if channel changed
        if (sm_->get_current_channel() != current_channel_) {
            current_channel_ = sm_->get_current_channel();
            uint64_t freq = sm_->get_channel_frequency(current_channel_);
            radio_->set_frequency(freq);
        }
    }

private:
    ALEStateMachine* sm_;
    RadioController* radio_;
    size_t current_channel_ = 0;
};
```

### PTT Timing Considerations

```cpp
// PTT delay: Time between activating PTT and audio output
const int PTT_DELAY_MS = 50;  // Radio-specific (check manual)

// PTT tail: Time to keep PTT active after audio ends
const int PTT_TAIL_MS = 20;   // Prevents truncation

// Complete TX sequence:
void transmit_with_ptt(const std::vector<ALEWord>& words) {
    radio_->set_ptt(true);
    std::this_thread::sleep_for(std::chrono::milliseconds(PTT_DELAY_MS));
    
    for (const ALEWord& word : words) {
        transmit_word(word);  // 392ms per word
    }
    
    std::this_thread::sleep_for(std::chrono::milliseconds(PTT_TAIL_MS));
    radio_->set_ptt(false);
}
```

---

## MIL-STD-188-110A Modem Integration

After ALE establishes link, switch to high-speed modem for data.

### Transition Sequence

```
1. ALE establishes link (2G ALE handshake)
2. Stations agree on data mode (via AMD or AQC transaction code)
3. Switch to MIL-STD-188-110A modem
4. Transfer data using FS-1052 ARQ
5. Return to ALE scanning when done
```

### Example Integration

```cpp
class DataLinkController {
public:
    void init(ALEStateMachine* ale, VariableARQ* arq,
             MIL188110AModem* modem, RadioController* radio) {
        ale_ = ale;
        arq_ = arq;
        modem_ = modem;
        radio_ = radio;
        
        // ALE callback: link established
        ale_->init(
            nullptr,
            [this](const std::string& addr) { 
                on_link_established(addr); 
            },
            nullptr
        );
        
        // ARQ callbacks
        arq_->init(
            [this](const std::vector<uint8_t>& data) {
                on_data_received(data);
            },
            nullptr,
            [this](const std::vector<uint8_t>& frame) {
                on_frame_transmit(frame);
            }
        );
    }
    
    void on_link_established(const std::string& address) {
        std::cout << "ALE link ready, switching to data mode" << std::endl;
        
        // Switch radio to data mode (USB with 3 kHz bandwidth)
        radio_->set_mode("DATA");
        
        // Configure modem (e.g., PSK-8, 2400 bps)
        modem_->set_mode(MIL188110A::PSK_8);
        modem_->set_interleaver(MIL188110A::SHORT);
        
        // Ready to send data
        data_mode_active_ = true;
    }
    
    void send_file(const std::vector<uint8_t>& file_data) {
        if (!data_mode_active_) {
            std::cerr << "No active link!" << std::endl;
            return;
        }
        
        // Start ARQ transmission
        arq_->start_transmission(file_data);
    }
    
    void on_frame_transmit(const std::vector<uint8_t>& frame) {
        // Modulate FS-1052 frame using 188-110A modem
        auto modulated_audio = modem_->modulate(frame);
        
        // Transmit audio
        radio_->set_ptt(true);
        transmit_audio(modulated_audio);
        radio_->set_ptt(false);
    }
    
    void on_data_received(const std::vector<uint8_t>& data) {
        std::cout << "Received " << data.size() << " bytes" << std::endl;
        
        // Process received file
        save_file("received.dat", data);
        
        // Return to ALE mode
        data_mode_active_ = false;
        radio_->set_mode("USB");  // Back to ALE
        ale_->start_scanning();
    }

private:
    ALEStateMachine* ale_;
    VariableARQ* arq_;
    MIL188110AModem* modem_;
    RadioController* radio_;
    bool data_mode_active_ = false;
};
```

---

## Threading and Real-Time Considerations

### Threading Model

```
┌──────────────────────────────────────────────────────────┐
│                    Main Thread                           │
│  - UI updates                                            │
│  - User input                                            │
│  - Configuration                                         │
└────────┬──────────────────────────────────┬──────────────┘
         │                                  │
         │ Commands                         │ Status
         │                                  │
┌────────▼──────────────┐        ┌──────────▼──────────────┐
│  ALE Worker Thread    │        │  Audio Thread           │
│  - State machine      │◄──────►│  - Real-time I/O        │
│  - Protocol           │ Sync   │  - Sample processing    │
│  - Channel scanning   │        │  - Buffering            │
└───────────────────────┘        └─────────────────────────┘
         │                                  │
         │ Radio commands                   │ Audio data
         │                                  │
┌────────▼──────────────┐        ┌──────────▼──────────────┐
│  Radio Thread         │        │  DSP Thread             │
│  - CAT control        │        │  - FFT demodulation     │
│  - PTT management     │        │  - Tone generation      │
│  - Frequency changes  │        │  - Golay FEC            │
└───────────────────────┘        └─────────────────────────┘
```

### Thread Synchronization Example

```cpp
class ThreadSafeALEEngine {
public:
    void init() {
        // Start worker threads
        running_ = true;
        
        ale_thread_ = std::thread([this]() { ale_worker(); });
        audio_thread_ = std::thread([this]() { audio_worker(); });
        radio_thread_ = std::thread([this]() { radio_worker(); });
    }
    
    void make_call(const std::string& address) {
        std::lock_guard<std::mutex> lock(command_mutex_);
        pending_commands_.push({CommandType::MAKE_CALL, address});
        command_cv_.notify_one();
    }
    
    ~ThreadSafeALEEngine() {
        running_ = false;
        command_cv_.notify_all();
        
        if (ale_thread_.joinable()) ale_thread_.join();
        if (audio_thread_.joinable()) audio_thread_.join();
        if (radio_thread_.joinable()) radio_thread_.join();
    }

private:
    void ale_worker() {
        while (running_) {
            // Wait for command
            std::unique_lock<std::mutex> lock(command_mutex_);
            command_cv_.wait(lock, [this]() {
                return !pending_commands_.empty() || !running_;
            });
            
            if (!running_) break;
            
            // Process command
            Command cmd = pending_commands_.front();
            pending_commands_.pop();
            lock.unlock();
            
            if (cmd.type == CommandType::MAKE_CALL) {
                ale_sm_.make_call(cmd.data);
            }
            
            // Update state machine
            ale_sm_.update();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }
    
    void audio_worker() {
        // Real-time audio processing
        while (running_) {
            process_audio_buffers();
            
            // High-priority thread, minimal sleep
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
    }
    
    void radio_worker() {
        while (running_) {
            // Check for frequency changes
            if (ale_sm_.get_current_channel() != last_channel_) {
                last_channel_ = ale_sm_.get_current_channel();
                radio_.set_frequency(channel_frequencies_[last_channel_]);
            }
            
            std::this_thread::sleep_for(std::chrono::milliseconds(50));
        }
    }
    
    std::atomic<bool> running_;
    std::thread ale_thread_;
    std::thread audio_thread_;
    std::thread radio_thread_;
    
    std::mutex command_mutex_;
    std::condition_variable command_cv_;
    std::queue<Command> pending_commands_;
    
    ALEStateMachine ale_sm_;
    RadioController radio_;
    size_t last_channel_ = 0;
};
```

### Real-Time Priority

```cpp
// Set thread priority for audio (Linux)
#include <pthread.h>
#include <sched.h>

void set_realtime_priority(std::thread& thread) {
    sched_param sch_params;
    sch_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
    
    pthread_setschedparam(thread.native_handle(), SCHED_FIFO, &sch_params);
}

// Windows equivalent
#include <windows.h>

void set_realtime_priority_windows(std::thread& thread) {
    SetThreadPriority(thread.native_handle(), 
                     THREAD_PRIORITY_TIME_CRITICAL);
}
```

---

## Complete Integration Example

### Full Application

```cpp
#include "ale/state_machine.h"
#include "ale/variable_arq.h"
#include <hamlib/rig.h>

class PC_ALE_Application {
public:
    bool init(const std::string& radio_model, 
             const std::string& serial_port) {
        // Initialize radio
        if (!radio_.init(radio_model, serial_port)) {
            return false;
        }
        
        // Initialize audio
        if (!audio_.init(8000, 512)) {
            return false;
        }
        
        // Initialize ALE state machine
        ale_sm_.init(
            [this](const ALEWord& w) { on_ale_word_tx(w); },
            [this](const std::string& a) { on_link_established(a); },
            [this](const std::string& m) { on_amd_received(m); }
        );
        
        // Initialize ARQ
        arq_.init(
            [this](const std::vector<uint8_t>& d) { on_data_rx(d); },
            nullptr,
            [this](const std::vector<uint8_t>& f) { on_frame_tx(f); }
        );
        
        // Setup channels
        add_channel(7073000);   // 7.073 MHz
        add_channel(10142000);  // 10.142 MHz
        add_channel(14107000);  // 14.107 MHz
        
        // Set self address
        ale_sm_.set_self_address("PCALE");
        
        return true;
    }
    
    void add_channel(uint64_t freq_hz) {
        ale_sm_.add_channel(freq_hz);
        channels_.push_back(freq_hz);
    }
    
    void start_scanning() {
        ale_sm_.start_scanning();
        scanning_ = true;
    }
    
    void make_call(const std::string& address) {
        ale_sm_.make_call(address);
    }
    
    void send_file(const std::vector<uint8_t>& file_data) {
        if (!ale_sm_.is_linked()) {
            std::cerr << "No active link!" << std::endl;
            return;
        }
        
        arq_.start_transmission(file_data);
    }
    
    void run() {
        while (running_) {
            // Update state machines
            ale_sm_.update();
            arq_.update();
            
            // Process audio
            audio_.process_audio();
            
            // Update radio frequency if channel changed
            update_radio_frequency();
            
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
    }

private:
    void on_ale_word_tx(const ALEWord& word) {
        // Activate PTT
        radio_.set_ptt(true);
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
        
        // Generate tones for word
        for (uint8_t symbol : word.get_symbols()) {
            auto samples = tone_gen_.generate_tone(symbol, 49);
            audio_.transmit_samples(samples);
        }
        
        // Deactivate PTT
        std::this_thread::sleep_for(std::chrono::milliseconds(20));
        radio_.set_ptt(false);
    }
    
    void on_link_established(const std::string& address) {
        std::cout << "Link established with " << address << std::endl;
        linked_address_ = address;
    }
    
    void on_amd_received(const std::string& message) {
        std::cout << "Message from " << linked_address_ 
                 << ": " << message << std::endl;
    }
    
    void on_frame_tx(const std::vector<uint8_t>& frame) {
        // Transmit ARQ frame (would use 188-110A modem here)
        radio_.set_ptt(true);
        // ... modulate and transmit ...
        radio_.set_ptt(false);
    }
    
    void on_data_rx(const std::vector<uint8_t>& data) {
        std::cout << "Received " << data.size() << " bytes" << std::endl;
        // Save or process received data
    }
    
    void update_radio_frequency() {
        size_t current = ale_sm_.get_current_channel();
        if (current != last_channel_) {
            radio_.set_frequency(channels_[current]);
            last_channel_ = current;
        }
    }
    
    // Components
    RadioController radio_;
    AudioEngine audio_;
    ALEStateMachine ale_sm_;
    VariableARQ arq_;
    ToneGenerator tone_gen_;
    
    // State
    std::vector<uint64_t> channels_;
    std::string linked_address_;
    bool scanning_ = false;
    bool running_ = true;
    size_t last_channel_ = 0;
};

int main(int argc, char** argv) {
    PC_ALE_Application app;
    
    // Initialize with IC-7300 on COM3
    if (!app.init("229", "COM3")) {
        std::cerr << "Failed to initialize" << std::endl;
        return 1;
    }
    
    // Start scanning
    app.start_scanning();
    
    // Make a call after 10 seconds (example)
    std::this_thread::sleep_for(std::chrono::seconds(10));
    app.make_call("REMOTE");
    
    // Run main loop
    app.run();
    
    return 0;
}
```

---

## Platform-Specific Notes

### Windows
- Use WASAPI for lowest latency (< 10ms)
- DirectSound fallback for compatibility
- COM ports: `COM1`, `COM2`, etc.
- Visual Studio 2019+ required for C++17

### Linux
- ALSA recommended, PulseAudio also supported
- Serial ports: `/dev/ttyUSB0`, `/dev/ttyS0`
- May need real-time kernel for < 5ms latency
- Install: `libasound2-dev`, `libhamlib-dev`

### macOS
- CoreAudio provides excellent latency
- Serial ports: `/dev/cu.usbserial-*`
- May need to approve app in Security & Privacy
- Install Hamlib via Homebrew

---

## Troubleshooting

### Audio Issues

**Problem: High latency or dropouts**
- Reduce buffer size (try 256 samples)
- Increase thread priority
- Check for other apps using audio

**Problem: No audio output**
- Verify sample rate matches (8000 Hz)
- Check audio device selection
- Test with simple sine wave

### Radio Control Issues

**Problem: Hamlib can't open radio**
- Check COM port permissions (Linux: add user to `dialout` group)
- Verify baud rate matches radio settings
- Test with `rigctl` command-line tool

**Problem: PTT not working**
- Check CAT cable wiring (may need custom cable)
- Verify PTT control method (CAT vs VOX vs hardware)
- Test PTT manually with `rigctl` tool

### Integration Issues

**Problem: State machine not updating**
- Call `update()` in main loop (10-50ms intervals)
- Check callbacks are registered before starting
- Verify threading/synchronization

---

## Related Documents

- [API_REFERENCE.md](API_REFERENCE.md) - Detailed API documentation
- [ARCHITECTURE.md](ARCHITECTURE.md) - System architecture
- [TESTING.md](TESTING.md) - Testing and validation
- [MIL_STD_COMPLIANCE.md](MIL_STD_COMPLIANCE.md) - Standards compliance

---

*Last Updated: December 2024*  
*Version: PC-ALE 2.0 Phase 5*
