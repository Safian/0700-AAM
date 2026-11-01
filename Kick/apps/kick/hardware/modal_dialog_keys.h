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

} // namespace kick_hw
