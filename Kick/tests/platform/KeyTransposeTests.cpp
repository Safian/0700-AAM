// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the keybed octave / semitone transpose state.
#include "../../apps/kick/hardware/key_transpose.h"
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    { // octave steps and clamp at +-3
        KeyTranspose k;
        for (int i = 0; i < 5; ++i) { k.press(true, false); k.release(true); }
        check(k.octave() == 3 && k.semitone() == 0, "octave clamps at +3");
        check(k.press(true, false) == KeyTranspose::None, "no change at limit");
        k.release(true);
        for (int i = 0; i < 8; ++i) { k.press(false, false); k.release(false); }
        check(k.octave() == -3, "octave clamps at -3");
    }
    { // shift = semitone, separate from octave
        KeyTranspose k;
        check(k.press(true, true) == KeyTranspose::TransposeChanged, "shift+OCT+ is transpose");
        k.release(true);
        k.press(false, true); k.release(false);
        k.press(false, true); k.release(false);
        check(k.semitone() == -1 && k.octave() == 0, "semitone -1, octave untouched");
        k.press(true, false); k.release(true);
        check(k.octave() == 1 && k.semitone() == -1, "octave independent of semitone");
        check(k.soundingPitch(60) == 60 + 12 - 1, "pitch = key + octave*12 + semitone");
    }
    { // semitone limit
        KeyTranspose k;
        for (int i = 0; i < 20; ++i) { k.press(true, true); k.release(true); }
        check(k.semitone() == KeyTranspose::kMaxSemitone, "semitone clamps");
    }
    { // chord resets both, with or without shift
        KeyTranspose k;
        k.press(true, false); k.release(true);
        k.press(true, true); k.release(true);
        check(k.octave() == 1 && k.semitone() == 1, "setup");
        k.press(false, false);
        check(k.press(true, false) == KeyTranspose::Reset, "chord resets");
        check(k.octave() == 0 && k.semitone() == 0, "both zero");
        k.release(false); k.release(true);
        check(k.press(true, false) == KeyTranspose::OctaveChanged, "released buttons step again");
        k.release(true);
        k.press(true, true); k.release(true);
        k.press(true, false); k.press(false, true);
        check(k.octave() == 0 && k.semitone() == 0, "chord with shift resets too");
    }
    { // final pitch clamped 0..127
        KeyTranspose k;
        for (int i = 0; i < 3; ++i) { k.press(true, false); k.release(true); }
        for (int i = 0; i < 12; ++i) { k.press(true, true); k.release(true); }
        check(k.soundingPitch(96) == 127, "clamp high");
        check(k.soundingPitch(0) == 48, "low key");
        KeyTranspose d;
        for (int i = 0; i < 3; ++i) { d.press(false, false); d.release(false); }
        check(d.soundingPitch(10) == 0, "clamp low");
    }
    { // chord keypress: Middle C (60) sets transposition to 0
        KeyTranspose k;
        k.press(false, false);
        check(!k.isChordActive(), "chord not active with 1 button");
        check(k.press(true, false) == KeyTranspose::Reset, "second button resets and activates chord");
        check(k.isChordActive(), "chord active with both buttons");
        check(k.pressKey(60) == KeyTranspose::Reset, "Middle C sets transpose to 0");
        check(k.octave() == 0 && k.semitone() == 0, "0 octave and 0 semitone");
        check(k.totalTranspose() == 0, "total transpose is 0");
        check(k.soundingPitch(60) == 60, "sounding pitch unchanged");
        k.release(false);
        check(!k.isChordActive(), "chord inactive after releasing one button");
        k.release(true);
        check(!k.isChordKeyPressed(), "flag cleared after both released");
        check(k.soundingPitch(60) == 60, "still 60 after full release");
    }
    { // chord keypress: semitone transpose (D4 = 62 -> +2 semitones, B3 = 59 -> -1 semitone)
        KeyTranspose k;
        k.press(true, false);
        k.press(false, false); // chord active
        check(k.pressKey(62) == KeyTranspose::TransposeChanged, "D4 sets transpose +2");
        check(k.octave() == 0 && k.semitone() == 2, "+2 semitones");
        check(k.totalTranspose() == 2, "total transpose +2");
        check(k.soundingPitch(60) == 62, "Middle C sounds as D4 (62)");
        k.release(true);
        k.release(false);
        check(k.octave() == 0 && k.semitone() == 2, "preserved after release");
        check(k.soundingPitch(60) == 62, "normal play sounds at 62");

        // Re-enter chord and set to B3 (59 -> -1 semitone)
        k.press(false, false);
        k.press(true, false);
        check(k.pressKey(59) == KeyTranspose::TransposeChanged, "B3 sets transpose -1");
        check(k.octave() == 0 && k.semitone() == -1, "-1 semitone");
        check(k.soundingPitch(60) == 59, "Middle C sounds as B3 (59)");
        k.release(false);
        k.release(true);
        check(k.octave() == 0 && k.semitone() == -1, "preserved after release");
        check(k.soundingPitch(60) == 59, "normal play sounds at 59");
    }
    { // chord keypress: octave shifts via keys (C5 = 72 -> +1 oct, C3 = 48 -> -1 oct)
        KeyTranspose k;
        k.press(false, false);
        k.press(true, false);
        check(k.pressKey(72) == KeyTranspose::TransposeChanged, "C5 sets transpose +12");
        check(k.octave() == 1 && k.semitone() == 0, "+1 octave");
        check(k.soundingPitch(60) == 72, "Middle C sounds as C5 (72)");
        k.release(false);
        k.release(true);
        check(k.octave() == 1 && k.semitone() == 0, "preserved after release");

        k.press(false, false);
        k.press(true, false);
        check(k.pressKey(48) == KeyTranspose::TransposeChanged, "C3 sets transpose -12");
        check(k.octave() == -1 && k.semitone() == 0, "-1 octave");
        check(k.soundingPitch(60) == 48, "Middle C sounds as C3 (48)");
        k.release(false);
        k.release(true);
        check(k.octave() == -1 && k.semitone() == 0, "preserved after release");
    }
    { // 61-key hardware bounds: lowest key C2 (36) and highest key C7 (96)
        KeyTranspose k;
        k.press(false, false);
        k.press(true, false);
        check(k.pressKey(36) == KeyTranspose::TransposeChanged, "C2 sets transpose -24");
        check(k.octave() == -2 && k.semitone() == 0, "-2 octaves");
        check(k.totalTranspose() == -24, "total transpose -24");
        check(k.soundingPitch(60) == 36, "Middle C sounds as C2 (36)");
        k.release(false);
        k.release(true);

        k.press(false, false);
        k.press(true, false);
        check(k.pressKey(96) == KeyTranspose::TransposeChanged, "C7 sets transpose +36");
        check(k.octave() == 3 && k.semitone() == 0, "+3 octaves");
        check(k.totalTranspose() == 36, "total transpose +36");
        check(k.soundingPitch(60) == 96, "Middle C sounds as C7 (96)");
        k.release(false);
        k.release(true);
    }
    { // extreme clamping and invalid pitch rejection
        KeyTranspose k;
        k.press(false, false);
        k.press(true, false);
        check(k.pressKey(-1) == KeyTranspose::None, "negative pitch rejected");
        check(k.pressKey(128) == KeyTranspose::None, "pitch > 127 rejected");
        check(k.pressKey(125) == KeyTranspose::TransposeChanged, "high pitch clamped to max transpose");
        check(k.totalTranspose() == KeyTranspose::kMaxTransposeSemitones, "clamped at +36");
        check(k.pressKey(5) == KeyTranspose::TransposeChanged, "low pitch clamped to min transpose");
        check(k.totalTranspose() == -KeyTranspose::kMaxTransposeSemitones, "clamped at -36");
        k.release(false);
        k.release(true);
    }
    { // key press ignored when chord is inactive
        KeyTranspose k;
        check(k.pressKey(62) == KeyTranspose::None, "ignored when no buttons held");
        check(k.octave() == 0 && k.semitone() == 0, "unchanged");

        k.press(true, false); // only OCT+ held
        check(!k.isChordActive(), "chord not active with only 1 button");
        check(k.pressKey(62) == KeyTranspose::None, "ignored when only 1 button held");
        check(k.octave() == 1 && k.semitone() == 0, "octave unchanged by pressKey");
        k.release(true);
    }
    { // multiple key presses during one chord session
        KeyTranspose k;
        k.press(false, false);
        k.press(true, false);
        check(k.pressKey(62) == KeyTranspose::TransposeChanged, "first key D4");
        check(k.totalTranspose() == 2, "+2");
        check(k.pressKey(65) == KeyTranspose::TransposeChanged, "second key F4");
        check(k.totalTranspose() == 5, "+5");
        k.release(false);
        k.release(true);
        check(k.totalTranspose() == 5, "+5 preserved after release");
    }
    { // simulated bridge behavior: chord key does not sound; normal play sounds
        KeyTranspose k;
        int key_pitch_map[128];
        for (int i = 0; i < 128; ++i) key_pitch_map[i] = -1;
        int live_notes_on = 0;
        int live_notes_off = 0;

        auto bridge_note_on = [&](int pitch, int vel) {
            (void)vel;
            if (k.isChordActive()) {
                k.pressKey(pitch);
                return; // Suppressed under chord!
            }
            int sp = k.soundingPitch(pitch);
            key_pitch_map[pitch] = sp;
            ++live_notes_on;
        };

        auto bridge_note_off = [&](int pitch) {
            if (pitch >= 0 && pitch < 128 && key_pitch_map[pitch] < 0) {
                return; // Was not sounding (chord key or stray release)
            }
            key_pitch_map[pitch] = -1;
            ++live_notes_off;
        };

        // 1. Enter chord: hold both OCT buttons
        k.press(false, false);
        k.press(true, false);
        check(k.isChordActive(), "chord active");

        // 2. Press a key on keybed during chord: e.g. D4 (62)
        bridge_note_on(62, 100);
        check(live_notes_on == 0, "chord key did NOT trigger note-on sound!");
        check(key_pitch_map[62] == -1, "key_pitch_map remained empty");
        check(k.totalTranspose() == 2, "transposition set to +2");

        // 3. Release the chord key while buttons still held
        bridge_note_off(62);
        check(live_notes_off == 0, "chord key release did NOT trigger note-off!");

        // 4. Release both OCT buttons
        k.release(false);
        k.release(true);
        check(!k.isChordActive(), "chord inactive");
        check(k.totalTranspose() == 2, "transposition still +2");

        // 5. Normal playing now: play Middle C (60)
        bridge_note_on(60, 100);
        check(live_notes_on == 1, "normal note-on triggered sound");
        check(key_pitch_map[60] == 62, "sounding pitch is transposed to 62 (D4)");

        // 6. Release Middle C (60)
        bridge_note_off(60);
        check(live_notes_off == 1, "normal note-off triggered");
        check(key_pitch_map[60] == -1, "pitch map cleared");
    }
    { // W3-A: on-screen steps share the physical transposition (no held-button state)
        KeyTranspose k;
        check(k.step(true, false) == KeyTranspose::OctaveChanged && k.octave() == 1, "screen octave up");
        check(k.step(true, true) == KeyTranspose::TransposeChanged && k.semitone() == 1, "screen semitone up");
        check(k.soundingPitch(60) == 60 + 12 + 1, "sounding pitch follows the screen step");
        for (int i = 0; i < 10; ++i) k.step(true, false);
        check(k.octave() == KeyTranspose::kMaxOctave, "octave clamps at +3");
        check(k.step(true, false) == KeyTranspose::None, "no step beyond the clamp");
        check(!k.isChordActive(), "screen steps never enter chord mode");
        check(k.resetTranspose() == KeyTranspose::Reset && k.octave() == 0 && k.semitone() == 0, "screen reset");
        check(k.resetTranspose() == KeyTranspose::None, "reset at 0 is no change");
        // physical OCT button held while a screen step happens: the held state is untouched
        k.press(true, false);
        k.step(false, false);
        check(k.isChordActive() == false, "one physical button held, still no chord");
        k.release(true);
    }
    std::puts("ok");
    return 0;
}
