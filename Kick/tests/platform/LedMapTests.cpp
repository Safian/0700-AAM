// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the button-LED helpers (note map, level -> Akai value, write cache, bank levels).
#include "../../apps/kick/hardware/led_map.h"
#include <cstdio>
#include <cstdlib>
#include <set>

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    using namespace kick_hw;
    { // every LED has a unique note in 0..127
        std::set<int> notes;
        for (int l = 0; l < NUM_HW_LEDS; ++l) {
            bool two = false;
            const int n = akaiLedNote((HardwareLed)l, two);
            check(n >= 0 && n <= 127, "LED has a note");
            check(notes.insert(n).second, "LED notes are unique");
        }
        bool two = false;
        check(akaiLedNote((HardwareLed)NUM_HW_LEDS, two) == -1, "out of range LED has no note");
        // Shift: left LED 50 is the main note, right LED 49 is mirrored; no other LED has a mirror note
        check(akaiLedNote(HW_LED_SHIFT, two) == 50 && !two, "SHIFT main note 50");
        check(akaiLedMirrorNote(HW_LED_SHIFT) == 49, "SHIFT mirrors to 49");
        check(akaiLedMirrorNote(HW_LED_PLAY) == -1 && akaiLedMirrorNote((HardwareLed)NUM_HW_LEDS) == -1, "only SHIFT mirrors");
        check(notes.count(49) == 0, "mirror note 49 is not the main note of any LED");
        // notes of the pre-T13 LEDs must not change
        check(akaiLedNote(HW_LED_PLAY, two) == 82 && two, "PLAY 82 two-colour");
        check(akaiLedNote(HW_LED_STOP, two) == 81 && !two, "STOP 81 single");
        check(akaiLedNote(HW_LED_REC, two) == 73 && two, "REC 73");
        check(akaiLedNote(HW_LED_OVERDUB, two) == 80 && two, "OVERDUB 80");
        check(akaiLedNote(HW_LED_OCTAVE_DOWN, two) == 7 && akaiLedNote(HW_LED_OCTAVE_UP, two) == 10, "OCT notes");
        check(akaiLedNote(HW_LED_FULL_LEVEL, two) == 39 && akaiLedNote(HW_LED_16_LEVELS, two) == 40, "level notes");
        check(akaiLedNote(HW_LED_NOTE_REPEAT, two) == 11, "note repeat 11");
        check(akaiLedNote(HW_LED_BANK_A, two) == 35 && two, "bank A");
        check(akaiLedNote(HW_LED_BANK_D, two) == 38 && two, "bank D");
        check(akaiLedNote(HW_LED_MIX, two) == 116 && two, "MIX two-colour");
        check(akaiLedNote(HW_LED_MUTE, two) == 43 && two, "MUTE two-colour");
        check(akaiLedNote(HW_LED_AUTOMATION, two) == 75 && two, "AUTOMATION two-colour");
        check(akaiLedNote(HW_LED_GRID, two) == 117 && !two, "GRID single");
        check(akaiLedNote(HW_LED_TIMING_CORRECT, two) == 12 && !two, "TC 12");
    }
    { // level -> value
        check(akaiLedValue(true, 0) == 0 && akaiLedValue(true, 1) == 1 && akaiLedValue(true, 2) == 2 &&
              akaiLedValue(true, 3) == 3 && akaiLedValue(true, 4) == 4, "two-colour passes 0..4");
        check(akaiLedValue(true, 9) == 4 && akaiLedValue(true, -1) == 0, "two-colour clamps");
        check(akaiLedValue(false, 0) == 0 && akaiLedValue(false, 1) == 1 && akaiLedValue(false, 2) == 1 &&
              akaiLedValue(false, 3) == 127 && akaiLedValue(false, 4) == 127, "single colour: off/dim/full");
    }
    { // cache: only changes are written
        LedLevelCache c;
        check(c.update(HW_LED_ARP, 0), "unknown -> first write even for off");
        check(!c.update(HW_LED_ARP, 0), "same level not written");
        check(c.update(HW_LED_ARP, 3), "change written");
        check(!c.update(HW_LED_ARP, 3), "same again not written");
        check(c.update(HW_LED_LATCH, 3), "other LED independent");
        check(c.update(HW_LED_ARP, 4), "3 -> 4 written");
        check(!c.update(HW_LED_ARP, 7), "7 clamps to 4: no change");
        check(!c.update((HardwareLed)NUM_HW_LEDS, 1), "out of range ignored");
        c.reset();
        check(c.get(HW_LED_ARP) == -1 && c.update(HW_LED_ARP, 4), "reset forgets");
    }
    { // banks: A..D full A on own LED, E..H full B on LED (bank % 4)
        for (int bank = 0; bank < 8; ++bank)
            for (int led = 0; led < 4; ++led) {
                const int want = (bank % 4 == led) ? (bank < 4 ? kLedFullA : kLedFullB) : kLedOff;
                check(bankLedLevel(bank, led) == want, "bank LED level");
            }
        check(bankLedLevel(-1, 0) == kLedFullA, "negative bank -> A");
    }
    { // wire format: Control Change 0xB0 <note> <value>, also for off
        uint8_t m[3];
        makeLedMessage(81, 3, m);
        check(m[0] == 0xB0 && m[1] == 81 && m[2] == 3, "cc");
        makeLedMessage(81, 0, m);
        check(m[0] == 0xB0 && m[1] == 81 && m[2] == 0, "cc off keeps CC");
        makeLedMessage(200, 200, m);
        check(m[0] == kAkaiLedStatus && m[1] < 128 && m[2] < 128, "data bytes masked to 7 bit");
    }
    { // REC / OVERDUB from looper state
        int r = -1, o = -1;
        recOverdubLedLevels(false, false, true, r, o);
        check(r == kLedOff && o == kLedOff, "idle: both dark");
        recOverdubLedLevels(true, false, true, r, o);
        check(r == kLedFullB && o == kLedOff, "recording, blink on: REC red");
        recOverdubLedLevels(true, false, false, r, o);
        check(r == kLedOff && o == kLedOff, "recording, blink off: dark");
        recOverdubLedLevels(true, true, false, r, o);
        check(r == kLedOff && o == kLedFullA, "overdubbing: OVERDUB steady, REC dark");
        recOverdubLedLevels(false, true, true, r, o);
        check(r == kLedOff && o == kLedOff, "overdub mode flag without recording: dark (stale flag after STOP)");
        check(blinkPhase(0) && blinkPhase(249) && !blinkPhase(250) && !blinkPhase(499) && blinkPhase(500), "blink phase");
    }
    std::puts("OK");
    return 0;
}
