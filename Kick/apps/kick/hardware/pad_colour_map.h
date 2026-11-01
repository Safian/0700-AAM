// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE/ALSA-free) mapping from the project's pad colour id + pad state to the
// 7-bit RGB range of the Akai MPC Key 61 pad SysEx. Host-testable.
#pragma once
#include <cstdint>

namespace kick_hw
{
    struct PadRgb7 { uint8_t r, g, b; };

    // Mirrors contracts::DeviceControlContracts::PadColourIds (0..17) and PadButtonState.
    // kick_hardware_bridge.h static_asserts that these numbers stay in sync.
    constexpr int kPadColourCount = 18;
    constexpr int kPadColourTrans = 15;
    constexpr int kPadStateOff = 0;
    constexpr int kPadStateOn = 1;
    constexpr int kPadStateSampleAssigned = 2;
    constexpr int kPadStateSampleNotAssigned = 3;
    constexpr int kPadStateOnBlink = 4;
    constexpr int kPadStateOffBlink = 5;

    // Kick/assets/Default/theme.xml, <PADBUTTON _0.._17> (0xAARRGGBB).
    constexpr uint32_t kPadPalette[kPadColourCount] = {
        0xff94172E, 0xff7C00F8, 0xffAB7B00, 0xff1DF800, 0xffE4F800, 0xff22AB00,
        0xff0DF0FF, 0xff3DAEFF, 0xff0D34FF, 0xffFF00E9, 0xffCC00FF, 0xffFF89FA,
        0xffFF250D, 0xffFF730D, 0xffFFB608, 0x00000000, 0xffFFFFFF, 0xffC2C2C2 };

    // Brightness in 1/1000 of the 7-bit colour. Idle pads are dim so the hit flash (127,127,127) stands out.
    constexpr int kDimAssignedPermille = 300;
    constexpr int kDimNotAssignedPermille = 80;

    inline uint8_t scaleTo7bit(uint32_t channel8, int permille)
    {
        int v = static_cast<int>((channel8 & 0xFFu) >> 1);
        v = (v * permille + 500) / 1000;
        return static_cast<uint8_t>(v < 0 ? 0 : (v > 127 ? 127 : v));
    }

    // Returns false for an unknown colour id (caller falls back to the bank colour).
    // Unknown states are treated as "assigned"; blink states are not animated.
    inline bool padColourToRgb7(int colourId, int state, PadRgb7& out)
    {
        if (colourId < 0 || colourId >= kPadColourCount) return false;
        out = { 0, 0, 0 };
        if (colourId == kPadColourTrans || state == kPadStateOff) return true;
        const int permille = (state == kPadStateSampleNotAssigned || state == kPadStateOffBlink)
                                 ? kDimNotAssignedPermille : kDimAssignedPermille;
        const uint32_t c = kPadPalette[colourId];
        out = { scaleTo7bit(c >> 16, permille), scaleTo7bit(c >> 8, permille), scaleTo7bit(c, permille) };
        return true;
    }
}
