// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Pure (JUCE / ALSA free) maths of the Akai MPC Key 61 touch strip modes (T12). Kick has only the slider
// packet (EN_SLIDER_TOUCH_INPUT 0..100), so the extra modes follow the Akai convention: the TOUCH STRIP
// button cycles Kick slider -> Pitch bend -> Mod wheel.
namespace kick_hw {
namespace strip {

enum Mode { kSlider = 0, kPitchBend = 1, kModWheel = 2, kNumModes = 3 };

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

// Strip position 0..127 -> CC1 value (identity, clamped).
inline int modWheelFromPosition(int val127) { return val127 < 0 ? 0 : (val127 > 127 ? 127 : val127); }

// Mode LED level (see led_map.h). The TOUCH STRIP LED is a single colour LED, whose velocity only takes
// off / 1 (dim) / 127 (full); levels 3 and 4 would both be 127 and not distinguishable. So: off = slider,
// dim = pitch bend, full = mod wheel (whether the device really shows a dim state is not verified).
inline int modeLedLevel(int mode)
{
    return mode == kPitchBend ? 1 : (mode == kModWheel ? 3 : 0);
}

} // namespace strip
} // namespace kick_hw
