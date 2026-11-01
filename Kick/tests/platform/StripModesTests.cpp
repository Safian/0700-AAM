// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the touch strip mode maths.
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
    check(nextMode(kSlider) == kPitchBend, "slider -> pitch");
    check(nextMode(kPitchBend) == kModWheel, "pitch -> mod");
    check(nextMode(kModWheel) == kSlider, "mod -> slider (wraps)");
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

    check(modeLedLevel(kSlider) == 0, "slider LED off");
    check(modeLedLevel(kPitchBend) != modeLedLevel(kModWheel), "pitch / mod LED differ");
    // the two mode levels must give different velocities on the (single colour) TOUCH STRIP LED
    check(kick_hw::akaiLedValue(false, modeLedLevel(kPitchBend)) != kick_hw::akaiLedValue(false, modeLedLevel(kModWheel)),
          "pitch / mod LED velocities differ");
    check(kick_hw::akaiLedValue(false, modeLedLevel(kSlider)) == 0, "slider LED velocity 0");
    std::printf("strip modes tests passed\n");
    return 0;
}
