// SPDX-License-Identifier: AGPL-3.0-or-later
// The "Power" window (Shut Down / Restart / Cancel) and the shutdown sequence behind it, like the factory MPC app: the power
// button (short press, read by the hardware bridge) and the Menu "Power" tile open it. The decisions and their order are in the
// JUCE-free kick_power_logic.h (host-tested); this file only wires them to the dialogs and the engine. Message thread only.
#pragma once

#include "kick_power_logic.h"

namespace ui::power
{
    // Opens the Power window. Ignored (logged) while a shutdown / restart is already running or the window is already open.
    // Also called from a power button press that arrives while a project loads: it waits for the load.
    // `source` (a string literal) is only for the log: who asked ("power key", "Menu tile", ...).
    void showPowerWindow( const char* source = "code" );

    // Runs the sequence directly (the Power window buttons, the smoke test). `finished` gets the result.
    void request( kick_power::Action action, std::function<void( kick_power::Result )> finished = {} );

    bool isBusy();        // a request is past its point of no return or waiting for an answer
    bool isWindowOpen();  // the Power window is on screen
}
