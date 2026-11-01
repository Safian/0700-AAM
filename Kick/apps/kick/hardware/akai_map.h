// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "hardware_adapter.h"   // enum HardwareButton only (no ALSA / JUCE)

// ============================================================================
// Akai MPC Key 61 note tables (Port 1 / "MPC Private" mode, host-testable).
// Pure functions, no ALSA / JUCE dependency.
// ============================================================================
namespace kick_hw {
namespace akai {

// Port 1, channel 1: Q-Link touch notes 84..87 (encoder 0..3) and the touch strip finger-touch note.
constexpr int kQLinkTouchFirstNote = 84;
constexpr int kQLinkTouchLastNote  = 87;
constexpr int kTouchStripTouchNote = 78;
constexpr int kNumButtons = 45;   // physical button notes
constexpr int kNumPads    = 16;

// Port 1, channel 10: the 16 pads in Akai GM layout -> pad index 0..15 (Pad 1 = A01 ...). -1 = not a pad note.
inline int get_akai_pad_index(int note)
{
    switch (note) {
        case 37: return 0;  // Pad 1  (A01)
        case 36: return 1;  // Pad 2  (A02)
        case 42: return 2;  // Pad 3  (A03)
        case 82: return 3;  // Pad 4  (A04)
        case 40: return 4;  // Pad 5  (A05)
        case 38: return 5;  // Pad 6  (A06)
        case 46: return 6;  // Pad 7  (A07)
        case 44: return 7;  // Pad 8  (A08)
        case 48: return 8;  // Pad 9  (A09)
        case 47: return 9;  // Pad 10 (A10)
        case 45: return 10; // Pad 11 (A11)
        case 43: return 11; // Pad 12 (A12)
        case 49: return 12; // Pad 13 (A13)
        case 55: return 13; // Pad 14 (A14)
        case 51: return 14; // Pad 15 (A15)
        case 53: return 15; // Pad 16 (A16)
        default: return -1;
    }
}

// Port 1, channel 1: 45 physical button notes. Notes 49 and 50 (right / left Shift) both map to HW_BTN_SHIFT,
// so the 45 notes yield 44 distinct buttons. HW_BTN_NONE = unmapped note.
inline HardwareButton get_akai_button(int note)
{
    switch (note) {
        // Bal szekcio (Tempo / Metro / Arp / Octave)
        case 53:  return HW_BTN_TAP_TEMPO;
        case 112: return HW_BTN_METRONOME;
        case 118: return HW_BTN_ARP;
        case 119: return HW_BTN_LATCH;
        case 7:   return HW_BTN_OCTAVE_DOWN;
        case 10:  return HW_BTN_OCTAVE_UP;

        // Bal-kozepso szekcio (Bank A-D + Touch Strip)
        case 35:  return HW_BTN_BANK_A;
        case 36:  return HW_BTN_BANK_B;
        case 37:  return HW_BTN_BANK_C;
        case 38:  return HW_BTN_BANK_D;
        case 4:   return HW_BTN_TOUCH_STRIP;
        case 42:  return HW_BTN_TOUCH_FX;
        case 11:  return HW_BTN_NOTE_REPEAT;

        // Kozepso 2x4 oszlop
        case 39:  return HW_BTN_FULL_LEVEL;
        case 40:  return HW_BTN_16_LEVELS;
        case 9:   return HW_BTN_ERASE;
        case 122: return HW_BTN_COPY;
        case 50:  return HW_BTN_SHIFT; // Left Shift
        case 123: return HW_BTN_MENU;
        case 121: return HW_BTN_SOUNDS;
        case 52:  return HW_BTN_MAIN;

        // Jobb felso (Q-Link gomb + Data Dial + +/-)
        case 0:   return HW_BTN_QLINK;
        case 111: return HW_BTN_ENTER; // Data Dial Push
        case 55:  return HW_BTN_MINUS;
        case 54:  return HW_BTN_PLUS;

        // 2x5 Mod matrix
        case 2:   return HW_BTN_PROG_EDIT;
        case 117: return HW_BTN_GRID;
        case 116: return HW_BTN_TRACK_MIX;
        case 43:  return HW_BTN_TRACK_MUTE;
        case 75:  return HW_BTN_AUTOMATION;
        case 120: return HW_BTN_KEYBOARD_CONTROL;
        case 6:   return HW_BTN_SAMPLE_EDIT;
        case 12:  return HW_BTN_TC;
        case 67:  return HW_BTN_UNDO;
        case 49:  return HW_BTN_SHIFT; // Right Shift

        // Transzport es Locate
        case 68:  return HW_BTN_STEP_PREV; // <
        case 69:  return HW_BTN_STEP_NEXT; // >
        case 70:  return HW_BTN_LOCATE;
        case 71:  return HW_BTN_BAR_PREV;  // <<
        case 72:  return HW_BTN_BAR_NEXT;  // >>
        case 73:  return HW_BTN_REC;
        case 80:  return HW_BTN_OVERDUB;
        case 81:  return HW_BTN_STOP;
        case 82:  return HW_BTN_PLAY;
        case 83:  return HW_BTN_PLAY_START;

        default:  return HW_BTN_NONE;
    }
}

} // namespace akai
} // namespace kick_hw
