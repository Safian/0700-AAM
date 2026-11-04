// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the "play from the start" position (JUCE free), Looper SYNC phase bug:
//   g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined PlayStartTests.cpp
// Measured on the MPC (2026-11-04): after REC, STOP, PLAY with Sync on the sequence ran ~1.3 s ahead of the loop.
// Cause: MTSequence::play() reset only playhead.bnb.bar; beat/tick stayed from where STOP happened and seek(bnb) started there.
#include "../../modules/engine/audio_engine/engine/mt_audio_bar_seek.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

using namespace mt::bar_seek;

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

struct Bnb { int bar, beat, tick; };

// 4/4, 96 ticks per beat, 120 BPM (the measured setup): position in seconds of a bar/beat/tick
static double seconds(Bnb b) { return (b.bar * 4 + b.beat + b.tick / 96.0) * 0.5; }

// The play() sequence: start from the playhead as stop() left it, with the start position applied the given way.
static Bnb playheadAfterStart(Bnb leftByStop, bool fixed, bool looping, int loopStart)
{
    Bnb h = leftByStop;
    if (fixed) { const auto s = playStart(looping, loopStart); h = { s.bar, s.beat, s.tick }; }
    else h.bar = looping && loopStart > 1 ? loopStart - 1 : 0; // old code: only the bar
    return h;
}

int main()
{
    // Playhead left by a STOP at 3.17 s of a 4 bar loop (bar 2, beat 3, tick 33; 0-based 1,2,33)
    const Bnb stopped{ 1, 2, 33 };

    const auto before = playheadAfterStart(stopped, false, false, 1);
    check(std::fabs(seconds(before) - 1.17) < 0.01, "old behaviour starts 1.17 s into the first bar (the measured -1.17 s)");

    const auto after = playheadAfterStart(stopped, true, false, 1);
    check(after.bar == 0 && after.beat == 0 && after.tick == 0, "play from start: bar 0, beat 0, tick 0");
    check(seconds(after) == 0.0, "play from start begins at 0 s");

    // Independent of whatever STOP left
    for (int bar = 0; bar < 4; ++bar) for (int beat = 0; beat < 4; ++beat) for (int tick = 0; tick < 96; tick += 13) {
        const auto h = playheadAfterStart({ bar, beat, tick }, true, false, 1);
        check(h.bar == 0 && h.beat == 0 && h.tick == 0, "start position independent of the previous playhead");
    }

    // Looping with a loop start (1-based UI value 3 -> 0-based bar 2); 0 / 1 / negative = bar 0
    check(playStart(true, 3).bar == 2 && playStart(true, 3).beat == 0 && playStart(true, 3).tick == 0, "loop start 3 -> bar 2");
    check(playStart(true, 1).bar == 0 && playStart(true, 0).bar == 0 && playStart(true, -5).bar == 0, "loop start <= 1 -> bar 0");
    check(playStart(false, 3).bar == 0, "not looping ignores the loop start");

    std::puts("PlayStartTests OK");
    return 0;
}
