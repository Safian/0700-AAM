// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Pure (JUCE-free) state of the keybed transposition: octave offset (OCT- / OCT+), semitone transpose
// (Shift + OCT- / OCT+) and the "both pressed together" reset chord. The final pitch is clamped to 0..127.
// The caller stores the sounding pitch at note-on, so changing the state never leaves a note hanging.
class KeyTranspose {
public:
    static constexpr int kMaxOctave   = 3;   // existing -3..+3 octave offset
    static constexpr int kMaxSemitone = 12;  // -12..+12 semitones (assumption, not from a spec)

    enum Action { None, OctaveChanged, TransposeChanged, Reset };

    int octave() const { return m_octave; }
    int semitone() const { return m_semitone; }

    // Sounding pitch for a key, clamped to the MIDI range.
    int soundingPitch(int pitch) const
    {
        const int p = pitch + m_octave * 12 + m_semitone;
        return p < 0 ? 0 : (p > 127 ? 127 : p);
    }

    // OCT- / OCT+ pressed. With shift the semitone transpose moves, otherwise the octave offset.
    // The second of the two buttons held at once resets both to 0.
    Action press(bool up, bool shift)
    {
        (up ? m_up_held : m_down_held) = true;
        if (m_up_held && m_down_held) {
            m_octave = 0;
            m_semitone = 0;
            return Reset;
        }
        const int d = up ? 1 : -1;
        if (shift) {
            const int v = m_semitone + d;
            if (v < -kMaxSemitone || v > kMaxSemitone) return None;
            m_semitone = v;
            return TransposeChanged;
        }
        const int v = m_octave + d;
        if (v < -kMaxOctave || v > kMaxOctave) return None;
        m_octave = v;
        return OctaveChanged;
    }

    void release(bool up) { (up ? m_up_held : m_down_held) = false; }

private:
    int  m_octave = 0;
    int  m_semitone = 0;
    bool m_up_held = false;
    bool m_down_held = false;
};
