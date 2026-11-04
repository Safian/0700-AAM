#pragma once
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "hardware_adapter.h"

// JUCE- and ALSA-free: which hardware button closes the open Timing Correct dialog, and how.
// The dialog is modal and has only touch buttons (Apply / Cancel); with no touch screen (Akai MPC Key 61 without
// display) the app would freeze. Akai MPC convention: ENTER / STOP accept, Shift+STOP and MAIN leave without change.
namespace kick_hw {

enum class ModalDialogAction { None, Apply, Cancel };

inline ModalDialogAction timingCorrectDialogAction(HardwareButton btn, bool shiftDown)
{
    switch (btn) {
        case HW_BTN_ENTER: return ModalDialogAction::Apply;
        case HW_BTN_STOP:  return shiftDown ? ModalDialogAction::Cancel : ModalDialogAction::Apply;
        case HW_BTN_MAIN:  return ModalDialogAction::Cancel;
        default:           return ModalDialogAction::None; // everything else (incl. SHIFT itself) is handled as usual
    }
}

// Any open dialog (numpad, tempo, metronome, STP, BBT, Humanize, confirmations ...): ENTER and STOP accept, Shift+STOP and MAIN
// leave without change. A dialog without Apply / Cancel buttons (information, resources) only closes, whichever key.
inline ModalDialogAction modalDialogAction(HardwareButton btn, bool shiftDown, bool hasApply)
{
    switch (btn) {
        case HW_BTN_ENTER: return hasApply ? ModalDialogAction::Apply : ModalDialogAction::Cancel;
        case HW_BTN_STOP:  return (shiftDown || !hasApply) ? ModalDialogAction::Cancel : ModalDialogAction::Apply;
        case HW_BTN_MAIN:  return ModalDialogAction::Cancel;
        default:           return ModalDialogAction::None;
    }
}

// While a dialog is open only these buttons still work (transport, the data dial and its keys, Shift, Tap Tempo, Octave): the
// ones that switch screens or edit the project behind the dialog (MENU, GRID, UNDO, ERASE, SAVE ...) are swallowed.
inline bool modalAllowsButton(HardwareButton btn)
{
    switch (btn) {
        case HW_BTN_PLAY: case HW_BTN_PLAY_START: case HW_BTN_STOP: case HW_BTN_REC: case HW_BTN_OVERDUB:
        case HW_BTN_TAP_TEMPO:
        case HW_BTN_PLUS: case HW_BTN_MINUS: case HW_BTN_ENTER:
        case HW_BTN_CURSOR_UP: case HW_BTN_CURSOR_DOWN: case HW_BTN_CURSOR_LEFT: case HW_BTN_CURSOR_RIGHT:
        case HW_BTN_OCTAVE_DOWN: case HW_BTN_OCTAVE_UP:
        case HW_BTN_SHIFT: case HW_BTN_TOUCH_STRIP:
            return true;
        default:
            return false; // incl. HW_BTN_NONE
    }
}

} // namespace kick_hw
