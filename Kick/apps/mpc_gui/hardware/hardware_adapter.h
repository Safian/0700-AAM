#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

// ============================================================================
// Kick DAW - Hardware Abstraction Layer (HAL)
//
// Separates the core Kick DAW logic & GUI from physical hardware quirks:
// - Screen orientation & framebuffer blitters (e.g. 90-deg CW panel rotation)
// - Touch digitizer coordinate transformation matrices
// - Physical button & encoder mapping (RawMIDI, USB HID, OSC)
// - Low-latency audio backend drivers (ACVM, ALSA, ASIO, CoreAudio)
// ============================================================================

enum HardwareButton {
    HW_BTN_NONE = 0,
    HW_BTN_PLAY,
    HW_BTN_PLAY_START,
    HW_BTN_STOP,
    HW_BTN_REC,
    HW_BTN_OVERDUB,
    HW_BTN_TAP_TEMPO,
    HW_BTN_PLUS,
    HW_BTN_MINUS,
    HW_BTN_ENTER,
    HW_BTN_CURSOR_UP,
    HW_BTN_CURSOR_DOWN,
    HW_BTN_CURSOR_LEFT,
    HW_BTN_CURSOR_RIGHT,
    HW_BTN_OCTAVE_DOWN,
    HW_BTN_OCTAVE_UP,
    HW_BTN_UNDO,
    HW_BTN_ERASE,
    HW_BTN_FULL_LEVEL,
    HW_BTN_16_LEVELS,
    HW_BTN_NOTE_REPEAT,
    HW_BTN_SHIFT,
    HW_BTN_MAIN,
    HW_BTN_BROWSE,
    HW_BTN_TRACK_MIX,
    HW_BTN_PAD_MIX,
    HW_BTN_TRACK_MUTE,
    HW_BTN_PAD_MUTE,
    HW_BTN_NEXT_SEQ,
    HW_BTN_STEP_SEQ,
    HW_BTN_SAMPLER,
    HW_BTN_SAMPLE_EDIT,
    HW_BTN_PROG_EDIT,
    HW_BTN_MENU,
    HW_BTN_BANK_A,
    HW_BTN_BANK_B,
    HW_BTN_BANK_C,
    HW_BTN_BANK_D,
    NUM_HW_BUTTONS
};

enum HardwareLed {
    HW_LED_PLAY = 0,
    HW_LED_STOP,
    HW_LED_REC,
    HW_LED_OVERDUB,
    HW_LED_OCTAVE_DOWN,
    HW_LED_OCTAVE_UP,
    HW_LED_FULL_LEVEL,
    HW_LED_16_LEVELS,
    HW_LED_NOTE_REPEAT,
    NUM_HW_LEDS
};

// Event callback interface implemented by the DAW Core
class IHardwareEventListener {
public:
    virtual ~IHardwareEventListener() = default;

    // Touch events in logical UI coordinate space (e.g. 0..1279, 0..799)
    virtual void on_touch_down(int slot, int x, int y) = 0;
    virtual void on_touch_move(int slot, int x, int y) = 0;
    virtual void on_touch_up(int slot) = 0;

    // Control surface hardware interactions
    virtual void on_button_press(HardwareButton btn) = 0;
    virtual void on_button_release(HardwareButton btn) = 0;
    virtual void on_jog_dial(int delta) = 0;
    virtual void on_encoder_turn(int encoder_idx, int delta) = 0;
    virtual void on_encoder_touch(int encoder_idx, bool touched) = 0;
    virtual void on_touch_strip(int val) = 0;
    virtual void on_pitch_bend(int val) = 0;
    virtual void on_sustain_pedal(bool down) = 0;

    // Musical performance inputs
    virtual void on_keybed_note_on(int pitch, int vel) = 0;
    virtual void on_keybed_note_off(int pitch) = 0;
    virtual void on_pad_note_on(int pad_num, int vel) = 0;
    virtual void on_pad_note_off(int pad_num) = 0;
};

// 1. Display Adapter: Abstracts physical screen resolution and rotation
class IDisplayAdapter {
public:
    virtual ~IDisplayAdapter() = default;
    virtual bool init(int logical_w, int logical_h) = 0;
    virtual void blit(const uint32_t* logical_pixels, int w, int h) = 0;
    virtual void shutdown() = 0;
    virtual int get_physical_width() const = 0;
    virtual int get_physical_height() const = 0;
};

// 2. Touch Adapter: Abstracts digitizers, multi-touch slots, coordinate conversion
class ITouchAdapter {
public:
    virtual ~ITouchAdapter() = default;
    virtual bool start(IHardwareEventListener* listener, int logical_w, int logical_h) = 0;
    virtual void stop() = 0;
};

// 3. Control Surface Adapter: Abstracts buttons, encoders, wheels, LEDs
class IControlSurfaceAdapter {
public:
    virtual ~IControlSurfaceAdapter() = default;
    virtual bool start(IHardwareEventListener* listener) = 0;
    virtual void set_led(HardwareLed led, bool on, uint32_t rgb = 0) = 0;
    virtual void stop() = 0;
};

// 4. Audio Adapter: Low-latency audio stream
class IAudioAdapter {
public:
    typedef void (*AudioCallback)(float* out_l, float* out_r, int num_frames, void* user_data);
    virtual ~IAudioAdapter() = default;
    virtual bool start(AudioCallback cb, void* user_data, int sample_rate, int buffer_size) = 0;
    virtual void stop() = 0;
};

// Unified Hardware Device Adapter Interface
class IHardwareAdapter {
public:
    virtual ~IHardwareAdapter() = default;
    virtual const char* get_device_name() const = 0;
    virtual const char* get_device_badge() const = 0;
    virtual IDisplayAdapter* display() = 0;
    virtual ITouchAdapter* touch() = 0;
    virtual IControlSurfaceAdapter* control_surface() = 0;
    virtual IAudioAdapter* audio() = 0;
    virtual bool init() = 0;
    virtual void shutdown() = 0;
};
