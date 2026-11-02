#pragma once

// ============================================================================
// Akai MPC Key 61 "Full Level / Half Level" button. JUCE/ALSA-free, host-testable.
//
// MPC Standalone OS User Guide v3.10, p. 343 (item 13) and the MPC Key 61 chapter:
//   - Press: activate / deactivate Full Level. While active the pads always trigger at the maximum velocity (127).
//   - Shift + press: activate / deactivate Half Level (called "User Level velocity" in newer chapters). While active the
//     pads always trigger at half velocity (64) - by default; a custom user level velocity can replace it.
//   - Press and hold: set the custom user level velocity. The gesture itself is NOT described in the manual, so it is not
//     implemented (the user level stays at its default 64).
//
// DUMMY (the manual does not say): Full Level and Half / User Level are mutually exclusive here (activating one switches
// the other off), because the button has a single LED and a pad hit has one velocity.
// ============================================================================
namespace kick_hw {
namespace padlevel {

constexpr int kFullVelocity        = 127;
constexpr int kDefaultUserVelocity = 64;   // "By default, this will be set to Half Level (64)"

struct State {
    bool full      = false;
    bool user      = false;
    int  userLevel = kDefaultUserVelocity;
};

inline int clampVel(int v) { return v < 1 ? 1 : (v > 127 ? 127 : v); }

// Plain press of the button.
inline State toggleFull(State s)
{
    s.full = !s.full;
    if (s.full) s.user = false;
    return s;
}

// Shift + press of the button.
inline State toggleUser(State s)
{
    s.user = !s.user;
    if (s.user) s.full = false;
    return s;
}

// Velocity of a pad hit (raw 1..127 from the pad). A raw velocity of 0 is a note off and is never changed.
inline int padVelocity(const State& s, int raw)
{
    if (raw <= 0) return raw;
    if (s.full) return kFullVelocity;
    if (s.user) return clampVel(s.userLevel);
    return raw;
}

// LED of the button: lit while either level feature is active.
inline bool ledOn(const State& s) { return s.full || s.user; }

} // namespace padlevel
} // namespace kick_hw
