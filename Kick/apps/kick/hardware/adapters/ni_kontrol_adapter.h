#pragma once

#include "../hardware_adapter.h"
#include <stdio.h>
#include <string.h>

// ============================================================================
// Native Instruments Kontrol (S-Series MK3 / Maschine MK3) Hardware Adapter
//
// Demonstrates the modularity of the Hardware Abstraction Layer (HAL):
// 1. Display: 0-degree standard landscape (Twin 480x272 displays or single display)
// 2. Touch: Standard capacitive touch / trackpad mapping
// 3. Control Surface: NI USB HID / MIDI protocol:
//    - 8 Endless Rotary Knobs
//    - 4D Push Encoder / Jog Wheel
//    - Light Guide Keybed Per-Key RGB LEDs
// 4. Audio: Standard USB Audio Class 2.0 (ASIO / ALSA / CoreAudio)
// ============================================================================

class NiKontrolDisplayAdapter : public IDisplayAdapter {
private:
    int m_w, m_h;

public:
    NiKontrolDisplayAdapter() : m_w(1280), m_h(800) {}
    virtual ~NiKontrolDisplayAdapter() {}

    bool init(int logical_w, int logical_h) override {
        m_w = logical_w;
        m_h = logical_h;
        printf("[NI-DISPLAY] NI Kontrol Dual/Color Display Target (%dx%d, 0-deg rotation) -> READY\n", m_w, m_h);
        return true;
    }

    void blit(const uint32_t* logical_pixels, int w, int h, int line_stride_pixels = 0) override {
        // Standard unrotated blit to USB bulk endpoint buffer or display
        (void)logical_pixels; (void)w; (void)h;
    }

    void shutdown() override {}
    int get_physical_width() const override { return m_w; }
    int get_physical_height() const override { return m_h; }
};

class NiKontrolTouchAdapter : public ITouchAdapter {
private:
    IHardwareEventListener* m_listener;

public:
    NiKontrolTouchAdapter() : m_listener(nullptr) {}
    virtual ~NiKontrolTouchAdapter() {}

    bool start(IHardwareEventListener* listener, int logical_w, int logical_h) override {
        m_listener = listener;
        (void)logical_w; (void)logical_h;
        printf("[NI-TOUCH] NI Capacitive Touch & Strip System -> READY\n");
        return true;
    }

    void stop() override {}
};

class NiKontrolControlSurfaceAdapter : public IControlSurfaceAdapter {
private:
    IHardwareEventListener* m_listener;

public:
    NiKontrolControlSurfaceAdapter() : m_listener(nullptr) {}
    virtual ~NiKontrolControlSurfaceAdapter() {}

    bool start(IHardwareEventListener* listener) override {
        m_listener = listener;
        printf("[NI-CONTROLLER] NI USB HID 4D-Encoder, 8 Encoders & Light Guide -> READY\n");
        return true;
    }

    void set_led(HardwareLed led, bool on, uint32_t rgb = 0) override {
        // NI Kontrol Light Guide supports full 24-bit RGB per-key LEDs
        (void)led; (void)on; (void)rgb;
    }

    void stop() override {}
};

class NiKontrolAudioAdapter : public IAudioAdapter {
public:
    virtual ~NiKontrolAudioAdapter() {}

    bool start(AudioCallback cb, void* user_data, int sample_rate, int buffer_size) override {
        (void)cb; (void)user_data; (void)sample_rate; (void)buffer_size;
        printf("[NI-AUDIO] Standard USB Audio Class 2.0 -> READY\n");
        return true;
    }

    void stop() override {}
};

class NiKontrolAdapter : public IHardwareAdapter {
private:
    NiKontrolDisplayAdapter m_display;
    NiKontrolTouchAdapter m_touch;
    NiKontrolControlSurfaceAdapter m_controls;
    NiKontrolAudioAdapter m_audio;

public:
    NiKontrolAdapter() {}
    virtual ~NiKontrolAdapter() { shutdown(); }

    const char* get_device_name() const override { return "NI KONTROL S-SERIES"; }
    const char* get_device_badge() const override { return "NI KONTROL"; }

    IDisplayAdapter* display() override { return &m_display; }
    ITouchAdapter* touch() override { return &m_touch; }
    IControlSurfaceAdapter* control_surface() override { return &m_controls; }
    IAudioAdapter* audio() override { return &m_audio; }

    bool init() override { return m_display.init(1280, 800); }
    void shutdown() override {}
};
