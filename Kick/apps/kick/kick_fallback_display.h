/*
 * Copyright (c) 2022 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace kick_app
{
    // Headless hardware mode (no X server): juce::Desktop::getDisplays() is empty, and JUCE 7.0.12 dereferences
    // getDisplayForRect() unchecked in Component::getParentMonitorArea() (used by every parent-less
    // setBoundsRelative / getParentWidth / getParentHeight call, e.g. FileLeftEntry::resized()). Register one
    // fallback display the size of the UI root (1024x600, ui::layout::screen) so those calls resolve. The bridge
    // scales that root to the 1280x800 framebuffer itself, so the logical area is the UI size, scale 1.
    // No-op when JUCE found a real display, so normal (X / macOS / iOS) behaviour is unchanged.
    // Displays::displays is a public member; getDisplays() only hands out a const reference, and refresh()
    // is not reached without a native display system, so adding the entry in place is safe. Message thread only.
    inline bool ensureFallbackDisplay (int width = 1024, int height = 600)
    {
       #if JUCE_LINUX || JUCE_BSD || defined (__linux__)
        auto& displays = const_cast<juce::Displays&> (juce::Desktop::getInstance().getDisplays());
        if (! displays.displays.isEmpty())
            return false;

        juce::Displays::Display d {};
        d.isMain            = true;
        d.totalArea         = { 0, 0, width, height };
        d.userArea          = d.totalArea;
        d.topLeftPhysical   = { 0, 0 };
        d.scale             = 1.0;
        d.dpi               = 96.0;
        displays.displays.add (d);
        return true;
       #else
        juce::ignoreUnused (width, height);
        return false;
       #endif
    }
}
