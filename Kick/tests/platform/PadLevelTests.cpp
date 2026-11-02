// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the Full Level / Half Level pad velocity rules (MPC v3.10 user guide p. 343).
#include "../../apps/kick/hardware/pad_level.h"
#include <cstdio>
#include <cstdlib>

using namespace kick_hw::padlevel;

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    { // default: the pad velocity is untouched
        State s;
        for (int v = 1; v <= 127; ++v) check(padVelocity(s, v) == v, "off: velocity unchanged");
        check(!ledOn(s), "off: LED off");
    }
    { // Full Level: always 127
        State s = toggleFull(State{});
        check(s.full && !s.user && ledOn(s), "Full Level on");
        for (int v = 1; v <= 127; ++v) check(padVelocity(s, v) == 127, "Full Level: 127");
        s = toggleFull(s);
        check(!s.full && !ledOn(s), "Full Level off again");
        check(padVelocity(s, 33) == 33, "after off: velocity unchanged");
    }
    { // Half Level (user level, default 64)
        State s = toggleUser(State{});
        check(s.user && !s.full && ledOn(s), "Half Level on");
        for (int v = 1; v <= 127; ++v) check(padVelocity(s, v) == 64, "Half Level: 64");
        s = toggleUser(s);
        check(!s.user && !ledOn(s), "Half Level off again");
    }
    { // a custom user level replaces Half Level
        State s = toggleUser(State{});
        s.userLevel = 90;
        check(padVelocity(s, 10) == 90 && padVelocity(s, 127) == 90, "custom user level");
        s.userLevel = 0;   check(padVelocity(s, 50) == 1, "user level clamped to >= 1");
        s.userLevel = 500; check(padVelocity(s, 50) == 127, "user level clamped to <= 127");
    }
    { // exclusive (DUMMY, documented in the header)
        State s = toggleFull(State{});
        s = toggleUser(s);
        check(s.user && !s.full, "activating Half Level switches Full Level off");
        s = toggleFull(s);
        check(s.full && !s.user, "activating Full Level switches Half Level off");
    }
    { // note off (velocity 0) is never changed
        State s = toggleFull(State{});
        check(padVelocity(s, 0) == 0, "velocity 0 stays 0 under Full Level");
        s = toggleUser(State{});
        check(padVelocity(s, 0) == 0, "velocity 0 stays 0 under Half Level");
    }
    std::puts("PadLevelTests: all passed");
    return 0;
}
