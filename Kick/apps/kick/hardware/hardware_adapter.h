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
    HW_BTN_GRID,
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
    HW_BTN_METRONOME,
    HW_BTN_ARP,
    HW_BTN_LATCH,
    HW_BTN_TOUCH_STRIP,
    HW_BTN_TOUCH_FX,
    HW_BTN_COPY,
    HW_BTN_SOUNDS,
    HW_BTN_QLINK,
    HW_BTN_AUTOMATION,
    HW_BTN_KEYBOARD_CONTROL,
    HW_BTN_TC,
    HW_BTN_STEP_PREV,
    HW_BTN_STEP_NEXT,
    HW_BTN_LOCATE,
    HW_BTN_BAR_PREV,
    HW_BTN_BAR_NEXT,
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
    HW_LED_BANK_A,
    HW_LED_BANK_B,
    HW_LED_BANK_C,
    HW_LED_BANK_D,
    // T13 (append only): further button LEDs
    HW_LED_ARP,
    HW_LED_LATCH,
    HW_LED_SHIFT,
    HW_LED_LOCATE,
    HW_LED_TIMING_CORRECT,
    HW_LED_AUTOMATION,
    HW_LED_COPY,
    HW_LED_ERASE,
    HW_LED_EDIT,            // PROG EDIT
    HW_LED_GRID,
    HW_LED_MIX,
    HW_LED_MUTE,
    HW_LED_KEYBOARD_CONTROL,
    HW_LED_SAMPLE_EDIT,
    HW_LED_UNDO,
    HW_LED_MENU,
    HW_LED_SOUNDS,
    HW_LED_MAIN,
    HW_LED_TOUCH_FX,
    HW_LED_TOUCH_STRIP,
    HW_LED_METRONOME,
    HW_LED_TAP_TEMPO,
    HW_LED_QLINK,
    // Octave indicator row under the OCT buttons, -3 .. +3 (append only)
    HW_LED_OCTAVE_M3,
    HW_LED_OCTAVE_M2,
    HW_LED_OCTAVE_M1,
    HW_LED_OCTAVE_ZERO,
    HW_LED_OCTAVE_P1,
    HW_LED_OCTAVE_P2,
    HW_LED_OCTAVE_P3,
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
    // Touch strip finger down / up (Akai Note 78). Optional.
    virtual void on_touch_strip_touch(bool /*down*/) {}
    // Touch strip 14-bit position 0..16383 (CC33 MSB + last CC34 LSB). Optional, fires in addition to on_touch_strip.
    virtual void on_touch_strip_fine(int /*pos14*/) {}
    virtual void on_pitch_bend(int val) = 0;
    virtual void on_sustain_pedal(bool down) = 0;
    // Optional keybed / wheel / pedal inputs (0..127); adapters without them need not implement these.
    virtual void on_channel_aftertouch(int /*val*/) {}
    virtual void on_mod_wheel(int /*val*/) {}
    virtual void on_expression(int /*val*/) {}
    virtual void on_footswitch2(bool /*down*/) {}
    // Hardware Main Volume knob (Akai: incoming SysEx on Port 1, see hardware/main_volume.h), scaled to 0..127
    // (the Kick master level scale). Optional.
    virtual void on_main_volume(int /*val127*/) {}
    // Backward-compatibility aliases
    virtual void on_expression_pedal(int val) { on_expression(val); }
    virtual void on_aftertouch(int val) { on_channel_aftertouch(val); }
    // Polyphonic pad pressure after the initial hit (pad 0..15, value 0..127).
    virtual void on_pad_aftertouch(int /*pad_num*/, int /*val*/) {}
    // External DIN MIDI message (5-pin DIN In on MPC Key 61, Port 2)
    virtual void on_din_midi_message(int /*port*/, uint8_t /*status*/, uint8_t /*d0*/, uint8_t /*d1*/) {}

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
    virtual void blit(const uint32_t* logical_pixels, int w, int h, int line_stride_pixels = 0) = 0;
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
    // Level based LED: 0 = off, 1 = dim A, 2 = dim B, 3 = full A, 4 = full B (see hardware/led_map.h).
    // Default: no-op, so adapters without intensity / second colour need not implement it.
    virtual void set_led_level(HardwareLed /*led*/, int /*level*/) {}
    virtual void set_pad_rgb(int /*pad*/, uint8_t /*r*/, uint8_t /*g*/, uint8_t /*b*/) {}
    // Feeds raw MIDI bytes through the same parser + dispatch path as the hardware port 'port' (0..2), so byte level
    // tests need no hardware (kick_cmd "rawmidi"). Default: no-op.
    virtual void inject_bytes(int /*port*/, const uint8_t* /*data*/, size_t /*len*/) {}
    // Sends raw MIDI bytes to a physical hardware port (port 2 = 5-pin DIN Out on MPC Key 61).
    virtual void send_rawmidi_out(int /*port*/, const uint8_t* /*data*/, size_t /*len*/) {}
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
