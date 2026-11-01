// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the LOCATE + < / > event selection (JUCE free):
//   g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined EventSeekTests.cpp
#include "../../modules/engine/audio_engine/engine/mt_event_seek.h"
#include <climits>
#include <cstdio>
#include <cstdlib>

using namespace mt::event_seek;

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    const std::vector<int> none;
    check(previousEvent(none, 100) == -1 && nextEvent(none, 100) == -1, "empty list");

    // Unsorted, with duplicates (chord) and a bogus negative start.
    const std::vector<int> v = { 960, 0, 384, 384, -5, 1536 };
    check(previousEvent(v, 1000) == 960, "prev between events");
    check(previousEvent(v, 960) == 384, "prev is strictly before (standing on an event)");
    check(previousEvent(v, 385) == 384, "prev just after an event");
    check(previousEvent(v, 384) == 0, "prev skips the duplicate");
    check(previousEvent(v, 0) == -1, "prev before the first event: none");
    check(previousEvent(v, 1) == 0, "prev finds an event at tick 0");
    check(nextEvent(v, 0) == 384, "next from tick 0 is strictly after");
    check(nextEvent(v, 383) == 384 && nextEvent(v, 384) == 960, "next strictly after, duplicates collapse");
    check(nextEvent(v, 1536) == -1 && nextEvent(v, 5000) == -1, "next after the last event: none (no wrap)");
    check(nextEvent(v, -1) == 0, "next from before the start finds tick 0");
    check(nextEvent(v, INT_MAX) == -1 && previousEvent(v, INT_MIN) == -1, "extreme current");

    check(findEvent(v, 500, -1) == 384 && findEvent(v, 500, 1) == 960 && findEvent(v, 500, 0) == -1, "direction dispatch");

    // Walking forward visits every distinct start exactly once, backward the reverse.
    int visited = 0, cur = -1;
    while ((cur = nextEvent(v, cur)) >= 0) ++visited;
    check(visited == 4, "forward walk: 4 distinct events");
    visited = 0; cur = 100000;
    while ((cur = previousEvent(v, cur)) >= 0) ++visited;
    check(visited == 4, "backward walk: 4 distinct events");

    check(clampTick(-1, 6144) == 0 && clampTick(6143, 6144) == 6143 && clampTick(6144, 6144) == 6143, "clampTick");
    check(clampTick(100, 0) == 0 && clampTick(100, -4) == 0 && clampTick(LLONG_MAX, 10) == 9, "clampTick degenerate");

    // Track-aware (track of 384 ticks inside a longer sequence), starts {0, 96, 200}.
    const std::vector<int> t = { 200, 0, 96, 400 /* beyond the track: ignored */ };
    check(findEventInTrack(t, 100, 1, 384) == 200, "track: next inside the first repetition");
    check(findEventInTrack(t, 300, 1, 384) == 384, "track: next past last event = first event of next repetition");
    check(findEventInTrack(t, 300, -1, 384) == 200, "track: prev inside the first repetition");
    check(findEventInTrack(t, 0, -1, 384) == -1, "track: prev at the very start: none");
    check(findEventInTrack(t, 384, -1, 384) == 200, "track: prev at start of 2nd repetition = last event of the 1st");
    check(findEventInTrack(t, 384 + 50, 1, 384) == 384 + 96, "track: next in 2nd repetition adds base");
    check(findEventInTrack(t, 384 + 50, -1, 384) == 384, "track: prev in 2nd repetition adds base");
    check(findEventInTrack(t, 384 * 2 + 300, 1, 384) == 384 * 3, "track: next across 3rd -> 4th repetition");
    check(findEventInTrack(t, 10, 0, 384) == -1 && findEventInTrack(none, 10, 1, 384) == -1, "track: direction 0 / no events");
    check(findEventInTrack(v, 500, 1, 0) == 960 && findEventInTrack(v, 500, -1, 0) == 384, "track: len 0 falls back to findEvent");
    check(findEventInTrack(std::vector<int>{ 10 }, 10, 1, 384) == 384 + 10, "track: single event wraps to the next repetition");

    std::puts("EventSeekTests OK");
    return 0;
}
