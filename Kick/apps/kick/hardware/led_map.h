#pragma once

#include "hardware_adapter.h"
#include <atomic>
#include <cstdint>

// JUCE- and ALSA-free helpers for the button LEDs of the control surface (T13).
//
// HAL LED levels (IControlSurfaceAdapter::set_led_level), same numbering as the two-colour LEDs of the
// Akai MPC Key 61 (docs/AKAI_MPC_KEY61_HARDWARE_REVIEW.md 3.5):
//   0 = off, 1 = dim A, 2 = dim B, 3 = full A, 4 = full B.
// A single-colour LED shows 'B' as 'A' (1,2 -> dim; 3,4 -> full).
namespace kick_hw {

constexpr int kLedOff   = 0;
constexpr int kLedDimA  = 1;
constexpr int kLedDimB  = 2;
constexpr int kLedFullA = 3;
constexpr int kLedFullB = 4;

inline int clampLedLevel(int level) { return level < 0 ? 0 : (level > kLedFullB ? kLedFullB : level); }

// Akai MPC Key 61: LED note == note of the button (Port 1, channel 0). Returns -1 for an LED without a mapping.
// twoColour = the LED takes the 0..4 levels (REC, OVERDUB, PLAY, MUTE, MIX, PAD BANK A..D, AUTOMATION).
// Source of the two-colour list: review 3.5 (not verified on the device).
inline int akaiLedNote(HardwareLed led, bool& twoColour)
{
    twoColour = false;
    switch (led) {
        case HW_LED_PLAY:             twoColour = true; return 82;
        case HW_LED_STOP:             return 81;
        case HW_LED_REC:              twoColour = true; return 73;
        case HW_LED_OVERDUB:          twoColour = true; return 80;
        case HW_LED_OCTAVE_DOWN:      return 7;
        case HW_LED_OCTAVE_UP:        return 10;
        case HW_LED_FULL_LEVEL:       return 39;
        case HW_LED_16_LEVELS:        return 40;
        case HW_LED_NOTE_REPEAT:      return 11;
        case HW_LED_BANK_A:           twoColour = true; return 35;
        case HW_LED_BANK_B:           twoColour = true; return 36;
        case HW_LED_BANK_C:           twoColour = true; return 37;
        case HW_LED_BANK_D:           twoColour = true; return 38;
        case HW_LED_ARP:              return 118;
        case HW_LED_LATCH:            return 119;
        case HW_LED_SHIFT:            return 50; // left Shift; the right Shift (49) is mirrored, see akaiLedMirrorNote()
        case HW_LED_LOCATE:           return 70;
        case HW_LED_TIMING_CORRECT:   return 12;
        case HW_LED_AUTOMATION:       twoColour = true; return 75;
        case HW_LED_COPY:             return 122;
        case HW_LED_ERASE:            return 9;
        case HW_LED_EDIT:             return 2;
        case HW_LED_GRID:             return 117;
        case HW_LED_MIX:              twoColour = true; return 116;
        case HW_LED_MUTE:             twoColour = true; return 43;
        case HW_LED_KEYBOARD_CONTROL: return 120;
        case HW_LED_SAMPLE_EDIT:      return 6;
        case HW_LED_UNDO:             return 67;
        case HW_LED_MENU:             return 123;
        case HW_LED_SOUNDS:           return 121;
        case HW_LED_MAIN:             return 52;
        case HW_LED_TOUCH_FX:         return 42;
        case HW_LED_TOUCH_STRIP:      return 4;
        case HW_LED_METRONOME:        return 112;
        case HW_LED_TAP_TEMPO:        return 53;
        case HW_LED_QLINK:            return 0;
        case NUM_HW_LEDS:
        default:                      return -1;
    }
}

// Second LED note that has to show the same level as the LED's main note (-1 = none). Shift has one button per
// side (akai_map.h: notes 49 right / 50 left, both HW_BTN_SHIFT), so both LEDs follow the single Shift state.
inline int akaiLedMirrorNote(HardwareLed led)
{
    return led == HW_LED_SHIFT ? 49 : -1;
}

// HAL level -> value of the LED Control Change data byte.
inline uint8_t akaiLedValue(bool twoColour, int level)
{
    level = clampLedLevel(level);
    if (twoColour) return (uint8_t)level;                                    // 0..4 as is
    return (uint8_t)(level == kLedOff ? 0 : (level <= kLedDimB ? 1 : 127));  // off / dim / full
}

// Status byte of a button LED write: Control Change 0xB0 <note> <byte> on channel 0 (Port 1). Verified on the
// MPC Key 61 with a camera (docs/akai_mpc_hardware/KEY61_LED_COLOURS.md); Note On/Off does not switch the LEDs.
constexpr uint8_t kAkaiLedStatus = 0xB0;

// The 3 bytes of one LED write (channel 0). 'byte' comes from akaiLedValue().
inline void makeLedMessage(int note, uint8_t byte, uint8_t out[3])
{
    out[0] = kAkaiLedStatus;
    out[1] = (uint8_t)(note & 0x7F);
    out[2] = (uint8_t)(byte & 0x7F);
}

// Last level per LED, to skip writes that would not change anything. Lock-free: LEDs are set from the
// message thread (screen signal, timer) and from the MIDI reader thread.
class LedLevelCache
{
public:
    LedLevelCache() { reset(); }
    // Forget everything: the next update() of each LED reports a change.
    void reset() { for (auto& v : m_level) v.store(-1); }
    // Stores 'level'; true when the LED has to be written (differs from the last level or is unknown).
    bool update(HardwareLed led, int level)
    {
        if ((int)led < 0 || (int)led >= NUM_HW_LEDS) return false;
        level = clampLedLevel(level);
        return m_level[led].exchange(level) != level;
    }
    int get(HardwareLed led) const { return ((int)led < 0 || (int)led >= NUM_HW_LEDS) ? -1 : m_level[led].load(); }
private:
    std::atomic<int> m_level[NUM_HW_LEDS];
};

// Pad bank (0..7 = A..H), LED index 0..3 (A..D): banks A..D light their LED in colour A (full A),
// banks E..H light the LED of (bank % 4) in colour B (full B).
inline int bankLedLevel(int bank, int ledIndex)
{
    if (bank < 0) bank = 0;
    bank %= 8;
    return (bank % 4 == ledIndex) ? (bank < 4 ? kLedFullA : kLedFullB) : kLedOff;
}

// REC / OVERDUB button LEDs from the looper state (the only consumer of EN_RECORD / EN_OVERDUB is the looper).
// Colour A (3) = green, colour B (4) = red is proven for AUTOMATION (Read green, Write red, KEY61_LED_COLOURS.md);
// that REC / OVERDUB use the same A/B colours is NOT verified (the camera could not tell, section 2). Dummy choice:
//   recording (not overdubbing): REC blinks red (colour B), OVERDUB dark;
//   recording and overdubbing: OVERDUB steady colour A, REC dark;
//   idle: both dark. The overdubbing input is a MODE flag of the recorder that nothing clears after STOP, so it only
//   counts while the looper is recording. The engine has no "armed" state (REC starts the looper at once), so nothing is lit steady.
inline void recOverdubLedLevels(bool recording, bool overdubbing, bool blinkOn, int& recLevel, int& overdubLevel)
{
    recLevel     = kLedOff;
    overdubLevel = kLedOff;
    if (recording && overdubbing)
        overdubLevel = kLedFullA;
    else if (recording)
        recLevel = blinkOn ? kLedFullB : kLedOff;
}

// Blink clock: 2 Hz (250 ms on / 250 ms off) from a millisecond counter.
inline bool blinkPhase(long long ms) { return ((ms / 250) & 1) == 0; }

} // namespace kick_hw
