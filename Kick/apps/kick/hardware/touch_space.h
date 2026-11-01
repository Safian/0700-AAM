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

#include "device_probe.h"

namespace kick_hw {
namespace probe {

// Touch coordinate spaces on the framebuffer hardware (no JUCE / ALSA, host-tested):
//   raw ILI2117 (0..range)  --mapTouch-->  framebuffer 1280x800 (what the adapter reports to the listener)
//   framebuffer 1280x800    --fbToUi---->  UI component space (1024x600; the bridge paints the UI scaled to the
//                                           framebuffer with scale(1280/uiW, 800/uiH), so this is its inverse).
// Hit-testing the component tree with framebuffer coordinates (the old behaviour) lands 25% (x) / 33% (y) off.
inline void fbToUi(int fbX, int fbY, int fbW, int fbH, int uiW, int uiH, int& uiX, int& uiY)
{
    if (fbW <= 0 || fbH <= 0 || uiW <= 0 || uiH <= 0) { uiX = fbX; uiY = fbY; return; }
    if (fbX < 0) fbX = 0;
    if (fbX > fbW - 1) fbX = fbW - 1;
    if (fbY < 0) fbY = 0;
    if (fbY > fbH - 1) fbY = fbH - 1;
    uiX = (int)(((long long)fbX * uiW) / fbW);
    uiY = (int)(((long long)fbY * uiH) / fbH);
    if (uiX > uiW - 1) uiX = uiW - 1;
    if (uiY > uiH - 1) uiY = uiH - 1;
}

// Whole chain, for diagnostics and tests.
inline void rawToUi(int rawX, int rawY, int rangeX, int rangeY, int fbW, int fbH, int uiW, int uiH, int& uiX, int& uiY)
{
    int fx = 0, fy = 0;
    mapTouch(rawX, rawY, rangeX, rangeY, fbW, fbH, fx, fy);
    fbToUi(fx, fy, fbW, fbH, uiW, uiH, uiX, uiY);
}

} // namespace probe
} // namespace kick_hw
