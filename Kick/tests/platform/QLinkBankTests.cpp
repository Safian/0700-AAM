// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for Akai MPC 4-encoder Q-Link bank/column switching (SS-22, LE-24, QL-07, QL-11).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined QLinkBankTests.cpp
#include "../../apps/kick/hardware/qlink_bank.h"
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    using namespace kick_hw::qlink;

    // Bank cycling: 0 -> 1 -> 2 -> 3 -> 0
    check(nextBank(0) == 1, "0 -> 1");
    check(nextBank(1) == 2, "1 -> 2");
    check(nextBank(2) == 3, "2 -> 3");
    check(nextBank(3) == 0, "3 -> 0 (wrap)");
    check(nextBank(-1) == 0, "-1 -> 0 (sanitize)");
    check(nextBank(99) == 0, "99 -> 0 (sanitize)");

    // Bank 0: encoders 0..3 map to offsets 0..3 (Knobs 1..4)
    check(knobOffset(0, 0) == 0, "bank 0 enc 0 -> offset 0");
    check(knobOffset(1, 0) == 1, "bank 0 enc 1 -> offset 1");
    check(knobOffset(2, 0) == 2, "bank 0 enc 2 -> offset 2");
    check(knobOffset(3, 0) == 3, "bank 0 enc 3 -> offset 3");

    // Bank 1: encoders 0..3 map to offsets 4..7 (Knobs 5..8)
    check(knobOffset(0, 1) == 4, "bank 1 enc 0 -> offset 4");
    check(knobOffset(1, 1) == 5, "bank 1 enc 1 -> offset 5");
    check(knobOffset(2, 1) == 6, "bank 1 enc 2 -> offset 6");
    check(knobOffset(3, 1) == 7, "bank 1 enc 3 -> offset 7");

    // Bank 2: encoders 0..3 map to offsets 8..11 (Knobs 9..12)
    check(knobOffset(0, 2) == 8, "bank 2 enc 0 -> offset 8");
    check(knobOffset(1, 2) == 9, "bank 2 enc 1 -> offset 9");
    check(knobOffset(2, 2) == 10, "bank 2 enc 2 -> offset 10");
    check(knobOffset(3, 2) == 11, "bank 2 enc 3 -> offset 11");

    // Bank 3: encoders 0..3 map to offsets 12..15 (Knobs 13..16)
    check(knobOffset(0, 3) == 12, "bank 3 enc 0 -> offset 12");
    check(knobOffset(1, 3) == 13, "bank 3 enc 1 -> offset 13");
    check(knobOffset(2, 3) == 14, "bank 3 enc 2 -> offset 14");
    check(knobOffset(3, 3) == 15, "bank 3 enc 3 -> offset 15");

    // Out-of-bounds guards
    check(knobOffset(-1, 0) == 0, "negative encoder clamped to 0");
    check(knobOffset(4, 0) == 0, "encoder >= 4 clamped to 0");
    check(knobOffset(2, -1) == 2, "negative bank defaults to 0");
    check(knobOffset(2, 4) == 2, "bank >= 4 defaults to 0");

    std::puts("QLinkBankTests: OK");
    return 0;
}
