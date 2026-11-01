#pragma once
// JUCE-free helpers of the hardware bridge (host-tested in tests/platform/HwTargetsTests.cpp).
//  - ShiftCounter: the Akai MPC Key 61 has two Shift buttons (left / right); the Kick contract has one Shift state.
//  - NoteTargets / HeldTarget: remember which track received a note-on / bend / pedal press so the matching
//    note-off / spring-back / release goes to the SAME track even if the track selection changed meanwhile.
// None of these classes is thread-safe; the bridge guards them with its own mutex.
#include <array>
#include <cstddef>
#include <cstdlib>

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
