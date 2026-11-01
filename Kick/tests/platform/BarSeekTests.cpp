// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the bar-seek arithmetic (JUCE free):
//   g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined BarSeekTests.cpp
#include "../../modules/engine/audio_engine/engine/mt_audio_bar_seek.h"
#include <climits>
#include <cstdio>
#include <cstdlib>

using namespace mt::bar_seek;

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    // clampBar
    check(clampBar(0, 4) == 0 && clampBar(3, 4) == 3, "clamp inside");
    check(clampBar(-1, 4) == 0 && clampBar(INT_MIN, 4) == 0, "clamp low");
    check(clampBar(4, 4) == 3 && clampBar(INT_MAX, 4) == 3, "clamp high");
    check(clampBar(5, 0) == 0 && clampBar(5, -3) == 0 && clampBar(-5, 0) == 0, "clamp degenerate length");
    check(clampBar(0, 1) == 0 && clampBar(1, 1) == 0, "clamp one bar");

    // relativeBar: no wrap, overflow safe
    check(relativeBar(2, -1, 8) == 1 && relativeBar(2, 1, 8) == 3, "relative +-1");
    check(relativeBar(0, -1, 8) == 0, "relative: << at bar 1 stays at bar 1");
    check(relativeBar(7, 1, 8) == 7, "relative: >> at last bar stays");
    check(relativeBar(INT_MAX, INT_MAX, 8) == 7 && relativeBar(INT_MIN, INT_MIN, 8) == 0, "relative overflow safe");
    check(relativeBar(3, 100, 0) == 0, "relative degenerate length");

    // lastBar
    check(lastBar(false, 4, 16) == 15, "last bar, not looping = length - 1");
    check(lastBar(true, 4, 16) == 3, "last bar, looping = loopEnd - 1");
    check(lastBar(true, 16, 16) == 15, "last bar, loop covers all");
    check(lastBar(true, 99, 8) == 7, "last bar, loopEnd beyond length is limited (default loopEnd is 4 for any length)");
    check(lastBar(true, 4, 2) == 1, "last bar, loopEnd 4 on a 2 bar sequence");
    check(lastBar(true, 0, 8) == 0 && lastBar(true, -1, 8) == 0, "last bar, loopEnd unset");
    check(lastBar(false, 0, 0) == 0, "last bar, degenerate length");

    // shift: the seek target is hit exactly, also across the 2^32 wrap
    const uint32_t raws[]    = { 0u, 1u, 7u, 1535u, 4999u, 0x7FFFFFFFu, 0xFFFFFFF0u, 0xFFFFFFFFu };
    const uint32_t targets[] = { 0u, 1u, 384u, 1536u, 6144u, 0x80000000u, 0xFFFFFFFFu };
    for (uint32_t raw : raws)
        for (uint32_t target : targets)
        {
            const uint32_t s = shiftFor(target, raw);
            check(shifted(raw, s) == target, "shifted(raw) == target");
            for (uint32_t d = 0; d < 5; ++d)
                check(static_cast<uint32_t>(shifted(raw + d, s) - target) == d, "shifted ticks advance 1:1");
        }

    // prevTick: first position played after the seek is exactly the target, including target 0
    check(prevTickFor(0u) == 0xFFFFFFFFu && static_cast<uint32_t>(prevTickFor(0u) + 1u) == 0u, "prevTick wrap for bar 1");
    for (uint32_t target : targets)
    {
        const uint32_t prev  = prevTickFor(target);
        const uint32_t first = static_cast<uint32_t>(prev + 1u); // fillMidiMessages: pos = PrevTick + 1
        check(first == target, "first played position == target");
        // The tick delivered to the track right after the seek is the target itself (diff 0 for the sequence,
        // diff 1 for the track): exactly one position (the downbeat) is played, none skipped, none repeated.
        check(static_cast<uint32_t>(target - prev) == 1u, "track diffTick == 1 on the seek tick");
    }

    // wrapToLength
    check(wrapToLength(1536u, 1536u) == 0u && wrapToLength(1537u, 1536u) == 1u && wrapToLength(5u, 0u) == 0u, "wrap to length");

    // Whole-pipeline model: 4/4 @ 384 ticks/bar, 8 bars, seek to every bar from every phase of a running clock.
    const uint32_t tpb = 384, bars = 8;
    for (uint32_t bar = 0; bar < bars; ++bar)
        for (uint32_t raw = 100000; raw < 100000 + tpb; raw += 17)
        {
            const uint32_t target = bar * tpb;
            const uint32_t s      = shiftFor(target, raw);
            uint32_t       prevSeq = target;                 // MTSequence::prevTicks after the seek
            uint32_t       prevTrk = prevTickFor(target);    // MTTrack::prevTick after the seek
            uint32_t       expectPos = target;
            for (uint32_t d = 0; d < 3 * tpb; ++d)           // seek tick (d == 0) and the following ticks
            {
                const uint32_t t = shifted(raw + d, s);
                check(static_cast<uint32_t>(t - prevSeq) == (d == 0 ? 0u : 1u), "sequence diff");
                prevSeq = t;
                for (uint32_t pos = prevTrk + 1u; pos != t + 1u; ++pos) // (prev, t]
                {
                    check(pos == expectPos, "contiguous track positions");
                    ++expectPos;
                }
                prevTrk = t;
            }
            check(expectPos == target + 3 * tpb, "no position lost");
        }

    std::puts("kick-bar-seek-tests: OK");
    return 0;
}
