#pragma once

#include <stdint.h>
#include <stddef.h>

// ============================================================================
// Akai MPC Key 61 "Main Volume" knob: incoming SysEx on Port 1. JUCE/ALSA-free, host-testable.
//
// MEASURED (raw MIDI capture, `git show 35fce454:docs/hw-mailbox/TO_CLOUD.md`, `amidi -p hw:0,0,1 -d -t 100`, identified by
// elimination): F0 47 7F 4B 5B 00 01 <v> F7, v = 0x00..0x75, one message per knob step.
//
// Field meaning (only partly known):
//   47        Akai manufacturer id (same as the outgoing pad RGB SysEx, akai_mpc_key61_adapter.h)
//   7F        device id (measured; "all call")
//   4B        product byte (same as in the outgoing SysEx)
//   5B        command byte. DUMMY: the meaning is NOT documented or known. It is used only to recognise this
//             message; an Akai SysEx with any other command / length is reported as UnknownAkai (logged once
//             by the adapter), never dropped silently.
//   00 01     payload length 0x0001 (same 2-byte length layout as the outgoing "65 00 04" pad RGB message)
//   <v>       0x00..0x75 (MEASURED range). 0x76..0x7F were never seen: clamped to 0x75.
// ============================================================================
namespace kick_hw {
namespace mainvol {

constexpr uint8_t kCommand = 0x5B;   // DUMMY: meaning unknown, verification only
constexpr int kMax = 0x75;           // measured maximum of <v>
constexpr size_t kMessageLen = 9;    // F0 47 7F 4B 5B 00 01 <v> F7

enum class Kind { NotAkai, UnknownAkai, MainVolume };

struct Result {
    Kind kind = Kind::NotAkai;
    int value = 0;          // MainVolume only: 0..kMax (clamped)
    bool clamped = false;   // MainVolume only: <v> was above kMax
};

// msg = one complete SysEx, F0 .. F7 inclusive (as reported by MidiParser::feed with the SysEx callback).
inline Result decode(const uint8_t* msg, size_t len)
{
    Result r;
    if (!msg || len < 3 || msg[0] != 0xF0 || msg[len - 1] != 0xF7) return r;   // not a framed message
    if (msg[1] != 0x47) return r;                                              // other manufacturer: ignored
    r.kind = Kind::UnknownAkai;
    if (len == kMessageLen && msg[2] == 0x7F && msg[3] == 0x4B && msg[4] == kCommand && msg[5] == 0x00 && msg[6] == 0x01) {
        int v = msg[7] & 0x7F;
        r.kind = Kind::MainVolume;
        r.clamped = v > kMax;
        r.value = r.clamped ? kMax : v;
    }
    return r;
}

// 0..kMax -> 0..127 (the Kick master level slider scale: CommonUtils::midiToGain takes 0..127). Rounded, clamped.
inline int toMidi127(int v)
{
    if (v < 0) v = 0;
    if (v > kMax) v = kMax;
    return (v * 127 + kMax / 2) / kMax;
}

} // namespace mainvol
} // namespace kick_hw
