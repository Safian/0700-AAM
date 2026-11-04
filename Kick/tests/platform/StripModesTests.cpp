// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the touch strip mode maths (XY-11, PP-24).
#include "../../apps/kick/hardware/strip_modes.h"
#include "../../apps/kick/hardware/led_map.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    using namespace kick_hw::strip;
    // cycle: Kick slider first, then the manual's order (p. 101)
    const int expected[] = { kSlider, kPitchBend, kPadLevel, kModWheel, kTrackLevel, kSustain, kTouchFx, kExpression, kNotes, kOff };
    check(kNumModes == 10 && (int)(sizeof(expected) / sizeof(expected[0])) == kNumModes, "10 modes");
    int m = kSlider;
    for (int i = 0; i < kNumModes; ++i) { check(m == expected[i], "cycle order"); m = nextMode(m); }
    check(m == kSlider, "off -> slider (wraps)");
    check(nextMode(99) == kSlider && nextMode(-1) == kSlider, "invalid mode -> slider");

    check(combine14(0, 0) == 0, "combine min");
    check(combine14(127, 127) == 16383, "combine max");
    check(combine14(64, 0) == kPitchCentre, "MSB 64 LSB 0 is centre");
    check(combine14(1, 5) == 133, "combine msb/lsb");
    check(combine14(200, -5) == (127 << 7), "combine clamps");

    check(pitchFromPosition(-10) == 0 && pitchFromPosition(20000) == 16383, "pitch clamps");
    check(pitchFromPosition(8192) == 8192, "pitch identity");
    check(pitchSpringBackTarget() == 8192, "spring back to centre");

    check(sliderPercent(0) == 0 && sliderPercent(127) == 100, "slider ends");
    check(sliderPercent(64) == 50, "slider middle");
    check(sliderPercent(500) == 100 && sliderPercent(-3) == 0, "slider clamps");
    for (int v = 0; v <= 127; ++v) // same as the bridge's original float formula
        check(sliderPercent(v) == (int)std::round(v * 100.0f / 127.0f), "slider scaling equals the original formula");
    check(modWheelFromPosition(200) == 127 && modWheelFromPosition(-1) == 0 && modWheelFromPosition(42) == 42, "mod");

    check(expressionFromPosition(200) == 127 && expressionFromPosition(-1) == 0 && expressionFromPosition(11) == 11, "expression");
    check(padLevelFromPosition(0) == 0 && padLevelFromPosition(127) == 127 && padLevelFromPosition(64) == 64, "pad level");
    check(padLevelFromPosition(500) == 127 && padLevelFromPosition(-9) == 0, "pad level clamps");
    check(trackLevelFromPosition(0) == 0 && trackLevelFromPosition(127) == 127 && trackLevelFromPosition(1) == 1, "track level");
    check(trackLevelFromPosition(500) == 127 && trackLevelFromPosition(-9) == 0, "track level clamps");
    check(!sustainDownFromPosition(0) && !sustainDownFromPosition(63), "sustain up below threshold");
    check(sustainDownFromPosition(64) && sustainDownFromPosition(127), "sustain down from 64");

    // XY-11: Touch FX Y coordinate
    check(std::fabs(touchFxFromPosition(0) - 0.0f) < 1e-5, "touch fx bottom 0");
    check(std::fabs(touchFxFromPosition(127) - 1.0f) < 1e-5, "touch fx top 1");
    check(std::fabs(touchFxFromPosition(-10) - 0.0f) < 1e-5, "touch fx clamp low");
    check(std::fabs(touchFxFromPosition(200) - 1.0f) < 1e-5, "touch fx clamp high");

    // PP-24: Notes mode position mapping (16 notes)
    check(notesFromPosition(0, 16) == 0, "notes pos 0 -> note 0");
    check(notesFromPosition(127, 16) == 15, "notes pos 127 -> note 15");
    check(notesFromPosition(64, 16) == 8, "notes pos 64 -> note 8");
    check(notesFromPosition(-5, 16) == 0, "notes pos clamp low");
    check(notesFromPosition(200, 16) == 15, "notes pos clamp high");

    // AF1a: Pad Level note = engine bank offset + pad (not a bridge-side bank)
    check(padNote(0, 0) == 0 && padNote(16, 0) == 16 && padNote(16, 15) == 31 && padNote(48, 3) == 51, "pad note = bank offset + pad");
    check(padNote(16, -1) == -1 && padNote(16, 16) == -1, "bad pad");

    // LED: Off = dark, every other mode lit (slider dim, others full)
    check(modeLedLevel(kOff) == 0, "off LED dark");
    for (int i = 0; i < kNumModes; ++i)
        check((modeLedLevel(i) == 0) == (i == kOff), "only Off is dark");
    check(kick_hw::akaiLedValue(false, modeLedLevel(kOff)) == 0, "off LED velocity 0");
    check(kick_hw::akaiLedValue(false, modeLedLevel(kSlider)) == 1, "slider LED dim");
    check(kick_hw::akaiLedValue(false, modeLedLevel(kPadLevel)) == 127 &&
          kick_hw::akaiLedValue(false, modeLedLevel(kTouchFx)) == 127 &&
          kick_hw::akaiLedValue(false, modeLedLevel(kNotes)) == 127 &&
          kick_hw::akaiLedValue(false, modeLedLevel(kExpression)) == 127, "other modes full");
    std::printf("strip modes tests passed\n");
    return 0;
}
