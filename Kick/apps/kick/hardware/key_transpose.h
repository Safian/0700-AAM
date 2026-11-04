// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Pure (JUCE-free) state of the keybed transposition: octave offset (OCT- / OCT+), semitone transpose
// (Shift + OCT- / OCT+), and the chord features:
// - Both OCT buttons held + keybed key: sets transposition to that key value (relative to Middle C / MIDI 60).
// - Both OCT buttons held and released without a key: resets transposition and octave to 0.
// Clamped to the MIDI range 0..127.
// The caller stores the sounding pitch at note-on, so changing the state never leaves a note hanging.
class KeyTranspose {
public:
    static constexpr int kMaxOctave             = 3;   // existing -3..+3 octave offset
    static constexpr int kMaxSemitone           = 12;  // -12..+12 semitones
    static constexpr int kMiddleC               = 60;  // Center / Middle C (C4)
    static constexpr int kMaxTransposeSemitones = 36;  // MPC OS spec: up to 36 semitones up or down

    enum Action { None, OctaveChanged, TransposeChanged, Reset };

    int octave() const { return m_octave; }
    int semitone() const { return m_semitone; }
    int totalTranspose() const { return m_octave * 12 + m_semitone; }
    bool isChordActive() const { return m_up_held && m_down_held; }
    bool isChordKeyPressed() const { return m_chord_key_pressed; }

    // Sounding pitch for a key, clamped to the MIDI range.
    int soundingPitch(int pitch) const
    {
        const int p = pitch + m_octave * 12 + m_semitone;
        return p < 0 ? 0 : (p > 127 ? 127 : p);
    }

    // OCT- / OCT+ pressed. With shift the semitone transpose moves, otherwise the octave offset.
    // The second of the two buttons held at once resets both to 0 and enters chord mode.
    Action press(bool up, bool shift)
    {
        (up ? m_up_held : m_down_held) = true;
        if (m_up_held && m_down_held) {
            m_chord_key_pressed = false;
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

    // While both OCT buttons are held, pressing a key on the keybed sets the transposition to that key value
    // relative to Middle C (MIDI 60 = 0 transposition). Returns TransposeChanged, Reset (if key is Middle C),
    // or None if chord is not active or pitch is invalid.
    Action pressKey(int pitch)
    {
        if (!isChordActive()) return None;
        if (pitch < 0 || pitch > 127) return None;

        const int diff = pitch - kMiddleC;
        int clampedDiff = diff;
        if (clampedDiff < -kMaxTransposeSemitones) clampedDiff = -kMaxTransposeSemitones;
        else if (clampedDiff > kMaxTransposeSemitones) clampedDiff = kMaxTransposeSemitones;

        m_octave = clampedDiff / 12;
        m_semitone = clampedDiff % 12;
        m_chord_key_pressed = true;

        if (m_octave == 0 && m_semitone == 0) {
            return Reset;
        }
        return TransposeChanged;
    }

    void release(bool up)
    {
        (up ? m_up_held : m_down_held) = false;
        if (!m_up_held && !m_down_held) {
            m_chord_key_pressed = false;
        }
    }

private:
    int  m_octave = 0;
    int  m_semitone = 0;
    bool m_up_held = false;
    bool m_down_held = false;
    bool m_chord_key_pressed = false;
};
