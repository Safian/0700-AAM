// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the relative (two's complement 7 bit) controller decoding of the Data Dial / Q-Links.
#include "../../apps/kick/hardware/relative_cc.h"
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    using kick_hw::relcc::decode;
    check(decode(0x01) == 1, "0x01 (measured, right) = +1");
    check(decode(0x7F) == -1, "0x7F (measured, left) = -1");
    check(decode(0x00) == 0, "0x00 = no step");
    check(decode(0x03) == 3 && decode(0x7D) == -3, "fast turn: several steps in one message");
    check(decode(0x3F) == 63 && decode(0x41) == -63, "largest magnitudes");
    check(decode(0x40) == -64, "0x40 = -64 (two's complement)");
    // sign symmetry and exact range for every 7 bit value
    for (int v = 0; v < 128; ++v) {
        const int d = decode(v);
        check(d >= -64 && d <= 63, "range");
        check(((d + 128) & 0x7F) == v, "round trip through two's complement");
    }
    // bits above 7 are not part of a MIDI data byte
    check(decode(0x81) == 1 && decode(0xFF) == -1, "masked to 7 bits");
    std::puts("relative cc tests passed");
    return 0;
}
