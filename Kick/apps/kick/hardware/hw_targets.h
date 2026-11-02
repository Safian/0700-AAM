#pragma once
// JUCE-free helpers of the hardware bridge (host-tested in tests/platform/HwTargetsTests.cpp).
//  - ShiftCounter: the Akai MPC Key 61 has two Shift buttons (left / right); the Kick contract has one Shift state.
//  - NoteTargets / HeldTarget: remember which track received a note-on / bend / pedal press so the matching
//    note-off / spring-back / release goes to the SAME track even if the track selection changed meanwhile.
// None of these classes is thread-safe; the bridge guards them with its own mutex.
#include <array>
#include <cstddef>
#include <cstdlib>
#include <string>

namespace kick_hw {

class ShiftCounter
{
public:
    // True only on the 0 -> 1 transition (send EN_SHIFT PRESSED).
    bool press() { return ++m_count == 1; }
    // True only on the 1 -> 0 transition (send EN_SHIFT RELEASED). A release without a press is ignored.
    bool release()
    {
        if (m_count <= 0) return false;
        return --m_count == 0;
    }
    bool down() const { return m_count > 0; }
    void reset() { m_count = 0; }
private:
    int m_count = 0;
};

// Per-key target (index = incoming key 0..127).
template <typename Target>
class NoteTargets
{
public:
    void set(int key, const Target& t) { if (inRange(key)) { m_t[(std::size_t)key] = t; m_has[(std::size_t)key] = true; } }
    // Target stored at note-on (and forgotten), else fallback.
    Target take(int key, const Target& fallback)
    {
        if (!inRange(key) || !m_has[(std::size_t)key]) return fallback;
        m_has[(std::size_t)key] = false;
        return m_t[(std::size_t)key];
    }
    void clear() { m_has.fill(false); }
private:
    static bool inRange(int k) { return k >= 0 && k < 128; }
    std::array<Target, 128> m_t {};
    std::array<bool, 128> m_has {};
};

// One remembered target for a controller that has an active/inactive state (sustain pedal down, pitch bend off-centre).
template <typename Target>
class HeldTarget
{
public:
    // Target for a message while the controller is active: the stored one when already active, else `current` (stored now).
    Target begin(const Target& current)
    {
        if (!m_active) { m_t = current; m_active = true; }
        return m_t;
    }
    // Target of the message that ends the activity (and forget it); `current` when nothing was active.
    Target end(const Target& current)
    {
        if (!m_active) return current;
        m_active = false;
        return m_t;
    }
    // Stored target if active, else `current`; does not change the state.
    Target peek(const Target& current) const { return m_active ? m_t : current; }
    bool active() const { return m_active; }
private:
    Target m_t {};
    bool   m_active = false;
};

// Where a CC64 really takes effect. The CC64 lane of the engine is (sequence, track), but the pedal STATE lives in the
// program the track uses (sample program: MTIProgram::sustain; MIDI program: the external port + channel). Two
// different tracks can share one program (explode / copyTrack), two MIDI programs can share port + channel, so the
// release decision compares this key too. An empty key (nothing resolved) never matches anything.
struct SustainKey
{
    std::string program; // sample program id; empty for a MIDI program
    std::string port;    // MIDI program: output port; empty for a sample program
    int         channel = -1; // MIDI program: output channel
    bool empty() const { return program.empty() && port.empty() && channel < 0; }
    bool operator==(const SustainKey& o) const { return program == o.program && port == o.port && channel == o.channel; }
};
inline bool sameSustainKey(const SustainKey& a, const SustainKey& b) { return !a.empty() && a == b; }

// Sustain (CC64) with two sources, the physical pedal and the touch strip in Sustain mode. Each source has its OWN
// HeldTarget (the release goes to the track that got that source's press) and the SustainKey resolved at its press.
// A release only sends CC64=0 when the other source does not still hold the pedal at the same effect place (same
// sequence/track lane OR same SustainKey), else the other source's pedal would be lifted early.
template <typename Target>
class SustainSources
{
public:
    enum Source { kPedal = 0, kStrip = 1 };
    // Target for the CC64=127 of `s`; `key` = the effect place of that target (kept until the release of `s`).
    Target press(Source s, const Target& current, const SustainKey& key = SustainKey())
    {
        if (!m_held[s].active()) m_key[s] = key;
        return m_held[s].begin(current);
    }
    // Ends `s`; `out` = the track that got its press. True = send CC64=0 there, false = the other source still holds
    // the pedal at the same place (a different place gets its own CC64=0).
    template <typename Eq>
    bool release(Source s, const Target& current, Eq sameTarget, Target& out)
    {
        const bool wasActive = m_held[s].active();
        out = m_held[s].end(current);
        const HeldTarget<Target>& other = m_held[1 - s];
        if (!other.active()) return true;
        if (sameTarget(other.peek(current), out)) return false;
        return !(wasActive && sameSustainKey(m_key[1 - s], m_key[s]));
    }
    bool held(Source s) const { return m_held[s].active(); }
private:
    HeldTarget<Target> m_held[2];
    SustainKey         m_key[2];
};

// Lane identity of the CC64 output: MTAudioInterface::addLiveMidiMsg -> manager->getTrack(Selection) picks the track by
// (sequence, track) only and ignores `program`, so two targets that differ only in the program share one CC64 lane.
template <typename Target>
bool sameSequenceTrack(const Target& a, const Target& b)
{
    return a.sequence == b.sequence && a.track == b.track;
}

// Sustain state machine of the bridge: both sources plus the "strip holds the pedal" flag. NOT thread-safe by itself:
// the flag change and the press/release decision must happen in ONE critical section of the bridge's m_target_mutex
// (otherwise a mode change on the message thread can interleave with the reader's press and leave CC64=127 stuck).
// The bridge also sends the CC64 message inside that same critical section so two threads cannot reorder the output.
// `resolveKey(const Target&) -> SustainKey` is called on a PRESS only (under the caller's lock), never on a release.
template <typename Target>
class SustainState
{
public:
    using Source = typename SustainSources<Target>::Source;
    // Physical pedal edge. True = send CC64=(down?127:0) to `out`.
    template <typename KeyFn>
    bool pedal(bool down, const Target& current, KeyFn resolveKey, Target& out)
    {
        return edge(SustainSources<Target>::kPedal, down, current, resolveKey, out);
    }
    // Strip edge; a repeated value (no edge) sends nothing.
    template <typename KeyFn>
    bool strip(bool down, const Target& current, KeyFn resolveKey, Target& out)
    {
        if (m_stripDown == down) return false;
        m_stripDown = down;
        return edge(SustainSources<Target>::kStrip, down, current, resolveKey, out);
    }
    bool stripDown() const { return m_stripDown; }
    bool held(Source s) const { return m_src.held(s); }
private:
    template <typename KeyFn>
    bool edge(Source s, bool down, const Target& current, KeyFn resolveKey, Target& out)
    {
        if (down) { out = m_src.press(s, current, resolveKey(current)); return true; }
        return m_src.release(s, current, sameSequenceTrack<Target>, out);
    }
    SustainSources<Target> m_src;
    bool m_stripDown = false;
};

// G16: a pitch wheel rarely rests on exactly 8192; values within +-2 of the centre count as "centred" (ends the hold).
constexpr int kBendCentre = 8192;
constexpr int kBendCentreTolerance = 2;
inline bool isBendCentre(int val) { return std::abs(val - kBendCentre) <= kBendCentreTolerance; }

// Route a 14-bit pitch value through its HeldTarget: off-centre starts/continues on the first track, centre ends there.
template <typename Target>
Target routeBend(HeldTarget<Target>& h, int val, const Target& current)
{
    return isBendCentre(val) ? h.end(current) : h.begin(current);
}

// Mod wheel CC1 / expression CC11 are continuous, spring-less controllers: no sticky target. Every value goes to the
// CURRENTLY selected track (plain MIDI behaviour); the previous track keeps its last value, no CC=0 is sent (would mute
// on expression). Only pitch bend (springs back) and sustain use a HeldTarget.
template <typename Target>
Target routeCc(int /*val*/, const Target& current)
{
    return current;
}

} // namespace kick_hw
