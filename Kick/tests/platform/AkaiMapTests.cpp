// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the Akai MPC Key 61 note tables (button + pad). ALSA / JUCE free.
#include "../../apps/kick/hardware/akai_map.h"
#include <cstdio>
#include <cstdlib>
#include <set>

using namespace kick_hw::akai;

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    // Every note 0..127 that maps to a button: exactly 45 notes.
    int mapped = 0;
    std::set<int> buttons;
    std::set<int> shiftNotes;
    for (int n = 0; n < 128; ++n) {
        const HardwareButton b = get_akai_button(n);
        check(b >= HW_BTN_NONE && b < NUM_HW_BUTTONS, "button value in range");
        if (b == HW_BTN_NONE) continue;
        ++mapped;
        buttons.insert((int)b);
        if (b == HW_BTN_SHIFT) shiftNotes.insert(n);
    }
    check(mapped == kNumButtons, "45 button notes are mapped");
    // Notes 49 (right Shift) and 50 (left Shift) share HW_BTN_SHIFT: 44 distinct buttons, all others unique.
    check(shiftNotes.size() == 2 && shiftNotes.count(49) && shiftNotes.count(50), "49 and 50 are the two Shift notes");
    check((int)buttons.size() == kNumButtons - 1, "44 distinct buttons (two notes -> SHIFT)");

    // Spot checks of the hardware-verified layout.
    check(get_akai_button(82) == HW_BTN_PLAY, "82 = PLAY");
    check(get_akai_button(81) == HW_BTN_STOP, "81 = STOP");
    check(get_akai_button(73) == HW_BTN_REC, "73 = REC");
    check(get_akai_button(111) == HW_BTN_ENTER, "111 = ENTER (data dial push)");
    check(get_akai_button(53) == HW_BTN_TAP_TEMPO, "53 = TAP TEMPO (button) while pad 16 on the pad channel");

    // Unmapped notes, Q-Link touch notes and the touch strip touch note are not buttons.
    check(get_akai_button(1) == HW_BTN_NONE, "1 unmapped");
    check(get_akai_button(127) == HW_BTN_NONE, "127 unmapped");
    check(get_akai_button(-1) == HW_BTN_NONE, "-1 unmapped");
    check(get_akai_button(128) == HW_BTN_NONE, "128 unmapped");
    for (int n = kQLinkTouchFirstNote; n <= kQLinkTouchLastNote; ++n)
        check(get_akai_button(n) == HW_BTN_NONE, "Q-Link touch notes are not buttons");
    check(get_akai_button(kTouchStripTouchNote) == HW_BTN_NONE, "78 (strip touch) is not a button");

    // Pads: 16 notes -> indices 0..15, bijective.
    const int padNotes[16] = { 37, 36, 42, 82, 40, 38, 46, 44, 48, 47, 45, 43, 49, 55, 51, 53 };
    bool seen[16] = { false };
    for (int i = 0; i < 16; ++i) {
        const int idx = get_akai_pad_index(padNotes[i]);
        check(idx == i, "pad note maps to its index");
        check(!seen[idx], "pad index unique");
        seen[idx] = true;
    }
    int padCount = 0;
    for (int n = -2; n < 130; ++n) {
        const int idx = get_akai_pad_index(n);
        if (idx >= 0) { ++padCount; check(idx < kNumPads, "pad index < 16"); }
        else check(idx == -1, "unmapped pad note -> -1");
    }
    check(padCount == kNumPads, "exactly 16 pad notes");
    check(get_akai_pad_index(0) == -1 && get_akai_pad_index(60) == -1, "non-pad notes -> -1");

    std::puts("akai-map-tests OK");
    return 0;
}
