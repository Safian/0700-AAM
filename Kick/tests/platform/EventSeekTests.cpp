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

    std::puts("EventSeekTests OK");
    return 0;
}
