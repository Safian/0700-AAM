// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for Akai MPC 4-encoder Q-Link bank/column switching (SS-22, LE-24, QL-07, QL-11).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined QLinkBankTests.cpp
#include "../../apps/kick/hardware/qlink_bank.h"
#include "../../clap_host/include/kick_clap/RemotePages.h"
#include <cstdio>
#include <cstdlib>
#include <cmath>
#include <vector>

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

    // ---- clap.remote-controls pages (kick_clap/RemotePages.h): the bank decides the knob, the focused page decides the parameter ----
    namespace rp = kick::clap::remote;
    const uint32_t none = rp::invalidId;
    std::vector<rp::PageParams> pages;
    for (uint32_t p = 0; p < 4; ++p) {
        rp::PageParams page; page.fill(none);
        for (uint32_t k = 0; k < (p == 3 ? 3u : 8u); ++k) page[k] = 1000 + p * 10 + k;   // the last page has only 3 knobs
        pages.push_back(page);
    }
    const int count = static_cast<int>(pages.size());
    // banks 0 and 1 are the two halves of the focused page, banks 2 and 3 the page after it
    check(rp::parameterFor(pages, 1, knobOffset(0, 0)) == 1010, "bank 0 encoder 0 -> focused page, knob 1");
    check(rp::parameterFor(pages, 1, knobOffset(3, 0)) == 1013, "bank 0 encoder 3 -> knob 4");
    check(rp::parameterFor(pages, 1, knobOffset(0, 1)) == 1014, "bank 1 encoder 0 -> knob 5");
    check(rp::parameterFor(pages, 1, knobOffset(3, 1)) == 1017, "bank 1 encoder 3 -> knob 8");
    check(rp::parameterFor(pages, 1, knobOffset(0, 2)) == 1020, "bank 2 encoder 0 -> NEXT page, knob 1");
    check(rp::parameterFor(pages, 1, knobOffset(3, 3)) == 1027, "bank 3 encoder 3 -> next page, knob 8");
    check(rp::parameterFor(pages, 2, knobOffset(2, 3)) == none, "next page (3) has no knob 7: unused slot");
    check(rp::parameterFor(pages, 3, knobOffset(0, 2)) == none, "no page after the last one");
    check(rp::parameterFor(pages, 0, knobOffset(1, 2)) == 1011, "bank 2 follows the page after the focused one");
    check(rp::parameterFor(pages, -1, 0) == none && rp::parameterFor(pages, count, 0) == none && rp::parameterFor({}, 0, 0) == none, "no focus / no pages -> nothing");
    check(rp::parameterFor(pages, 0, 16) == none && rp::parameterFor(pages, 0, -1) == none, "knob offset out of range -> nothing");
    check(!rp::resolveKnob(3, 8, count).valid() && rp::resolveKnob(2, 8, count).page == 3 && rp::resolveKnob(2, 8, count).slot == 0, "resolveKnob edge");
    // page stepping wraps
    check(rp::stepPage(0, -1, count) == 3 && rp::stepPage(3, 1, count) == 0 && rp::stepPage(1, 1, count) == 2, "page stepping wraps");
    check(rp::stepPage(1, 9, count) == 2 && rp::stepPage(1, -9, count) == 0 && rp::stepPage(0, 1, 0) == -1, "page stepping with large deltas / no pages");
    check(rp::stepPage(-1, 1, count) == 1 && rp::stepPage(99, 1, count) == 0, "an invalid focus is clamped first");
    // the page of the visible panel: most parameters in common, ties to the earlier page
    check(rp::bestPage({1021, 1022, 1023}, pages) == 2, "panel with three parameters of page 2");
    check(rp::bestPage({1001, 1021, 1022}, pages) == 2, "majority wins");
    check(rp::bestPage({1001, 1011}, pages) == 0, "tie -> earlier page");
    check(rp::bestPage({7, 8, 9}, pages) == -1 && rp::bestPage({}, pages) == -1, "no common parameter -> no page");
    check(rp::bestPage({none}, pages) == -1, "an unused slot never matches");
    // a relative turn
    check(rp::adjusted(50, 0, 100, false, 0.1) == 60, "continuous: delta is a fraction of the range");
    check(rp::adjusted(95, 0, 100, false, 0.1) == 100 && rp::adjusted(5, 0, 100, false, -0.1) == 0, "continuous: clamped");
    check(rp::adjusted(3, 0, 10, true, 1.0 / 127) == 4 && rp::adjusted(3, 0, 10, true, -1.0 / 127) == 2, "stepped: one tick, at least one step");
    check(rp::adjusted(3, 0, 10, true, 3.0 / 127) == 6, "stepped: whole ticks");
    check(rp::adjusted(10, 0, 10, true, 1.0 / 127) == 10 && rp::adjusted(0, 0, 10, true, -1.0 / 127) == 0, "stepped: clamped");
    check(rp::adjusted(5, 0, 10, false, 0.0) == 5 && rp::adjusted(5, 0, 10, false, NAN) == 5 && rp::adjusted(5, 10, 0, false, 0.5) == 5, "no change for zero / NaN / inverted range");

    std::puts("QLinkBankTests: OK");
    return 0;
}
