// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Pure (JUCE / ALSA free) maths of the Akai MPC Key 61 touch strip modes (T12, Y4). Kick has only the slider
// packet (EN_SLIDER_TOUCH_INPUT 0..100), which stays the first mode (Kick-native default). After it the TOUCH STRIP
// button cycles in the MPC v3.10 manual order (p. 101), leaving out the modes Kick has no counterpart for
// (Q-Link, Touch FX, Notes, Crossfader, Note Strum; see docs/akai_mpc_hardware/KEY61_TOUCH_STRIP_MODES.md):
// Pitch Bend, Pad Level, Mod Wheel, Track Level, Sustain, Expression, Off.
namespace kick_hw {
namespace strip {

enum Mode { kSlider = 0, kPitchBend = 1, kPadLevel = 2, kModWheel = 3, kTrackLevel = 4, kSustain = 5,
            kExpression = 6, kOff = 7, kNumModes = 8 };

constexpr int kPitchCentre = 8192;  // 14-bit pitch wheel centre
constexpr int kPitchMax    = 16383;

inline int nextMode(int mode) { return (mode < 0 || mode + 1 >= kNumModes) ? kSlider : mode + 1; }

// CC33 (MSB) + CC34 (LSB), both 0..127 -> 0..16383.
inline int combine14(int msb, int lsb)
{
    if (msb < 0) msb = 0;
    if (msb > 127) msb = 127;
    if (lsb < 0) lsb = 0;
    if (lsb > 127) lsb = 127;
    return (msb << 7) | lsb;
}

// Strip position (0..16383) -> pitch wheel value (0..16383). The strip centre is the wheel centre.
inline int pitchFromPosition(int pos14)
{
    return pos14 < 0 ? 0 : (pos14 > kPitchMax ? kPitchMax : pos14);
}

// Value the wheel returns to when the finger leaves the strip.
inline int pitchSpringBackTarget() { return kPitchCentre; }

// Strip position 0..127 -> Kick slider packet 0..100 (rounded), the pre-T12 scaling.
inline int sliderPercent(int val127)
{
    if (val127 < 0) val127 = 0;
    if (val127 > 127) val127 = 127;
    return (val127 * 100 + 63) / 127;
}

// Strip position 0..127 -> CC1 value (identity, clamped). On release the value stays (DUMMY: not in the manual).
inline int modWheelFromPosition(int val127) { return val127 < 0 ? 0 : (val127 > 127 ? 127 : val127); }

// Strip position 0..127 -> CC11 value (identity, clamped). On release the value stays (DUMMY: not in the manual).
inline int expressionFromPosition(int val127) { return modWheelFromPosition(val127); }

// DUMMY (the manual gives no range / scale for Pad Level and Track Level): the strip 0..127 maps 1:1 to the
// whole 0..127 range of Kick's pad level (MIDI 0..127, gain = midiToGain) and track level (0..127). The value
// stays on release.
inline int padLevelFromPosition(int val127) { return modWheelFromPosition(val127); }
inline int trackLevelFromPosition(int val127) { return modWheelFromPosition(val127); }

// DUMMY (the manual gives no on/off threshold for Sustain): upper half of the strip (>= 64) = pedal down,
// below = pedal up; a finger release always lifts the pedal.
constexpr int kSustainThreshold = 64;
inline bool sustainDownFromPosition(int val127) { return val127 >= kSustainThreshold; }

// Mode LED level (see led_map.h). The TOUCH STRIP LED is a single colour LED, so the modes cannot be told
// apart. Simple rule: Off = LED off; any active mode = lit (dim for the Kick slider default, full for the rest).
// (Whether the device really shows a dim state is not verified.)
inline int modeLedLevel(int mode)
{
    return mode == kOff ? 0 : (mode == kSlider ? 1 : 3);
}

} // namespace strip
} // namespace kick_hw
