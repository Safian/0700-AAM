// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for Akai MPC Screen Dimmer (screen-dim-plan.md).
// Build: clang++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined ScreenDimmerTests.cpp -o /tmp/t && /tmp/t
#include "../../apps/kick/hardware/screen_dimmer.h"
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    using namespace kick_hw;

    int64_t simulatedTimeMs = 1000000;
    int lastBrightnessApplied = -1;

    ScreenDimmer dimmer(
        [&simulatedTimeMs]() { return simulatedTimeMs; },
        [&lastBrightnessApplied](int pct) { lastBrightnessApplied = pct; }
    );

    dimmer.setNormalBrightnessPercent(80);
    dimmer.setDimLevelPercent(20);
    dimmer.setDimAfterMinutes(5); // 5 minutes = 300,000 ms

    check(dimmer.getState() == ScreenDimmer::State::Awake, "initial state Awake");
    check(!dimmer.isDimmed(), "not dimmed initially");

    // Advance 4 minutes (240,000 ms) -> still Awake
    simulatedTimeMs += 240000;
    dimmer.checkIdle(false, false, false);
    check(dimmer.getState() == ScreenDimmer::State::Awake, "still awake after 4 min");

    // Advance 2 more minutes (total 6 min without activity) -> should be Dimmed
    simulatedTimeMs += 120000;
    dimmer.checkIdle(false, false, false);
    check(dimmer.getState() == ScreenDimmer::State::Dimmed, "dimmed after 6 min idle");
    check(dimmer.isDimmed(), "isDimmed is true");
    // 80% * 20% = 16% brightness applied
    check(lastBrightnessApplied == 16, "brightness dimmed to 16%");

    // Touch down while dimmed should wake and swallow touch
    check(dimmer.onTouchDown(0) == true, "waking touch down swallowed");
    check(dimmer.getState() == ScreenDimmer::State::Awake, "woke back to Awake");
    check(lastBrightnessApplied == 80, "normal brightness restored to 80%");

    // Move and up for slot 0 should be swallowed
    check(dimmer.shouldSwallowTouch(0, false) == true, "move swallowed");
    check(dimmer.shouldSwallowTouch(0, true) == true, "up swallowed");
    // After up, slot 0 is no longer swallowed
    check(dimmer.shouldSwallowTouch(0, false) == false, "subsequent move not swallowed");

    // Subsequent normal touch down is NOT swallowed
    check(dimmer.onTouchDown(0) == false, "normal touch down not swallowed");

    // Inhibit test: transport playing prevents dimming
    simulatedTimeMs += 400000; // > 5 minutes
    dimmer.checkIdle(true, false, false); // playing
    check(dimmer.getState() == ScreenDimmer::State::Awake, "playing inhibits dimming");

    // Inhibit test: modal open prevents dimming
    simulatedTimeMs += 400000;
    dimmer.checkIdle(false, false, true); // modal open
    check(dimmer.getState() == ScreenDimmer::State::Awake, "modal open inhibits dimming");

    // Inhibit test: recording prevents dimming
    simulatedTimeMs += 400000;
    dimmer.checkIdle(false, true, false); // recording
    check(dimmer.getState() == ScreenDimmer::State::Awake, "recording inhibits dimming");

    // Now idle without inhibits -> dims
    simulatedTimeMs += 350000;
    dimmer.checkIdle(false, false, false);
    check(dimmer.getState() == ScreenDimmer::State::Dimmed, "dimmed again");

    // Hardware button / pad activity wakes without swallowing
    dimmer.noteActivity();
    check(dimmer.getState() == ScreenDimmer::State::Awake, "noteActivity woke display");
    check(lastBrightnessApplied == 80, "brightness restored on noteActivity");
    check(dimmer.shouldSwallowTouch(0, false) == false, "touch not swallowed on noteActivity");

    // Disable dimmer (0 min)
    dimmer.setDimAfterMinutes(0);
    simulatedTimeMs += 1000000;
    dimmer.checkIdle(false, false, false);
    check(dimmer.getState() == ScreenDimmer::State::Awake, "disabled dimmer never dims");

    std::puts("ScreenDimmerTests: OK");
    return 0;
}
