// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Pure (JUCE / ALSA free) maths of the Akai MPC Key 61 touch strip modes (T12, Y4, XY-11, PP-24). Kick has only the slider
// packet (EN_SLIDER_TOUCH_INPUT 0..100), which stays the first mode (Kick-native default). After it the TOUCH STRIP
// button cycles in the MPC v3.10 manual order (p. 101):
// Pitch Bend, Pad Level, Mod Wheel, Track Level, Sustain, Touch FX (XY-11), Expression, Notes (PP-24), Off.
// (Modes without engine counterparts in Kick: Q-Link, Crossfader, Note Strum; see docs/akai_mpc_hardware/KEY61_TOUCH_STRIP_MODES.md)
namespace kick_hw {
namespace strip {

enum Mode {
    kSlider      = 0,
    kPitchBend   = 1,
    kPadLevel    = 2,
    kModWheel    = 3,
    kTrackLevel  = 4,
    kSustain     = 5,
    kTouchFx     = 6,
    kExpression  = 7,
    kNotes       = 8,
    kOff         = 9,
    kNumModes    = 10,
    // QL-12: modes of the MPC that the bridge does not drive yet (the maths below is ready and tested). They are NOT in the
    // TOUCH STRIP button cycle: nextMode() stops at kNumModes, so the strip never lands on a mode that does nothing. When the
    // bridge handles them (see REPORT.md, QL-12) they move in front of kOff and kNumModes grows.
    kQLink       = 10,
    kCrossfader  = 11
};

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

// Strip position 0..127 -> Touch FX Y-axis position (0.0f..1.0f) (XY-11).
inline float touchFxFromPosition(int val127)
{
    if (val127 < 0) val127 = 0;
    if (val127 > 127) val127 = 127;
    return val127 / 127.0f;
}

// Strip position 0..127 -> note slot index 0..(numNotes - 1) on the current bank/scale (PP-24).
inline int notesFromPosition(int val127, int numNotes = 16)
{
    if (numNotes <= 0) return 0;
    if (val127 < 0) val127 = 0;
    if (val127 > 127) val127 = 127;
    int idx = (val127 * numNotes) / 128;
    return idx >= numNotes ? numNotes - 1 : idx;
}

// Strip position 0..127 -> Q-Link normalized parameter value (0.0f..1.0f) (QL-12)
inline float qlinkFromPosition(int val127)
{
    if (val127 < 0) val127 = 0;
    if (val127 > 127) val127 = 127;
    return val127 / 127.0f;
}

// Strip position 0..127 -> Crossfader value (-1.0f..+1.0f): center at 64 is 0.0f, left/A side < 0, right/B side > 0 (QL-12)
inline float crossfaderFromPosition(int val127)
{
    if (val127 < 0) val127 = 0;
    if (val127 > 127) val127 = 127;
    return (val127 - 64) / 64.0f;
}

// Pad index 0..15 selection for Touch Strip Q-Link target
inline int qlinkIndexFromPad(int pad)
{
    return (pad < 0 || pad > 15) ? 0 : pad;
}

// Pad Level target: the note the Grid plays for `pad` (0..15) is the engine bank offset + pad. -1 for a bad pad.
inline int padNote(int bankOffset, int pad) { return (pad < 0 || pad > 15) ? -1 : bankOffset + pad; }

// Mode LED level (see led_map.h). The TOUCH STRIP LED is a single colour LED, so the modes cannot be told
// apart. Simple rule: Off = LED off; any active mode = lit (dim for the Kick slider default, full for the rest).
// (Whether the device really shows a dim state is not verified.)
inline int modeLedLevel(int mode)
{
    return mode == kOff ? 0 : (mode == kSlider ? 1 : 3);
}

} // namespace strip
} // namespace kick_hw
