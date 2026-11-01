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
    std::puts("ok");
    return 0;
}
