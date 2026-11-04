// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE/ALSA-free) mapping from the project's pad colour id + pad state to the
// 7-bit RGB range of the Akai MPC Key 61 pad SysEx, plus packet parsing for DCL pad LED commands
// (CE-02, CE-08, SS-07, NS-04, SG-10). Host-testable.
#pragma once
#include <cstddef>
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

    // Mirrors contracts::DeviceControlContracts::PadCmd.
    constexpr int kCmdPadSetSinglePad       = 1;
    constexpr int kCmdPadSetAllSingleColour = 2;
    constexpr int kCmdPadSetAllMultiColour  = 3;
    constexpr int kCmdPadColorMode          = 5;
    constexpr int kCmdPadSetColourState     = 6;
    constexpr int kCmdPadSetBlinking        = 8;
    constexpr int kCmdPadSetStepSinglePad   = 9;
    constexpr int kCmdPadSetStepAllPad      = 10;

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

    // Pad bank state representation for DCL command handling (CE-02, CE-08, SS-07, NS-04, SG-10).
    struct PadStateArray {
        uint8_t colours[16]{};
        uint8_t states[16]{};
        int     colorMode{ -1 }; // -1 = unchanged
    };

    // Applies a DCL pad command packet to the 16-pad state array.
    // Returns a 16-bit bitmask of changed pads (bit 0 = pad 0, bit 15 = pad 15; 0xFFFF if all changed).
    inline uint16_t applyPadPacket(int cmd, const int* data, size_t size, PadStateArray& target)
    {
        uint16_t changed = 0;
        if (!data || size == 0) return 0;

        switch (cmd) {
            case kCmdPadColorMode: {
                target.colorMode = data[0];
                changed = 0xFFFF; // mode change invalidates all pad appearances
                break;
            }
            case kCmdPadSetColourState: {
                // [16 x colour id][16 x state]
                if (size >= 32) {
                    for (int i = 0; i < 16; ++i) {
                        const uint8_t cid = static_cast<uint8_t>(data[i]);
                        const uint8_t st  = static_cast<uint8_t>(data[16 + i]);
                        if (target.colours[i] != cid || target.states[i] != st) {
                            target.colours[i] = cid;
                            target.states[i]  = st;
                            changed |= static_cast<uint16_t>(1u << i);
                        }
                    }
                }
                break;
            }
            case kCmdPadSetSinglePad: {
                // [padId, state, colourId]
                if (size >= 3) {
                    const int pad = data[0];
                    if (pad >= 0 && pad < 16) {
                        const uint8_t st  = static_cast<uint8_t>(data[1]);
                        const uint8_t cid = static_cast<uint8_t>(data[2]);
                        if (target.colours[pad] != cid || target.states[pad] != st) {
                            target.colours[pad] = cid;
                            target.states[pad]  = st;
                            changed |= static_cast<uint16_t>(1u << pad);
                        }
                    }
                }
                break;
            }
            case kCmdPadSetAllSingleColour: {
                // [state, colourId]
                if (size >= 2) {
                    const uint8_t st  = static_cast<uint8_t>(data[0]);
                    const uint8_t cid = static_cast<uint8_t>(data[1]);
                    for (int i = 0; i < 16; ++i) {
                        if (target.colours[i] != cid || target.states[i] != st) {
                            target.colours[i] = cid;
                            target.states[i]  = st;
                            changed |= static_cast<uint16_t>(1u << i);
                        }
                    }
                }
                break;
            }
            case kCmdPadSetAllMultiColour: {
                // [state, colour0, ..., colour15]
                if (size >= 17) {
                    const uint8_t st = static_cast<uint8_t>(data[0]);
                    for (int i = 0; i < 16; ++i) {
                        const uint8_t cid = static_cast<uint8_t>(data[1 + i]);
                        if (target.colours[i] != cid || target.states[i] != st) {
                            target.colours[i] = cid;
                            target.states[i]  = st;
                            changed |= static_cast<uint16_t>(1u << i);
                        }
                    }
                }
                break;
            }
            case kCmdPadSetStepSinglePad: {
                // [padId, state, colourId, velocity]
                if (size >= 4) {
                    const int pad = data[0];
                    if (pad >= 0 && pad < 16) {
                        const uint8_t st  = static_cast<uint8_t>(data[1]);
                        const uint8_t cid = static_cast<uint8_t>(data[2]);
                        if (target.colours[pad] != cid || target.states[pad] != st) {
                            target.colours[pad] = cid;
                            target.states[pad]  = st;
                            changed |= static_cast<uint16_t>(1u << pad);
                        }
                    }
                }
                break;
            }
            case kCmdPadSetStepAllPad: {
                // [state, c0..c15, v0..v15] or [state, c0..c15]
                if (size >= 17) {
                    const uint8_t st = static_cast<uint8_t>(data[0]);
                    for (int i = 0; i < 16; ++i) {
                        const uint8_t cid = static_cast<uint8_t>(data[1 + i]);
                        if (target.colours[i] != cid || target.states[i] != st) {
                            target.colours[i] = cid;
                            target.states[i]  = st;
                            changed |= static_cast<uint16_t>(1u << i);
                        }
                    }
                }
                break;
            }
            case kCmdPadSetBlinking: {
                // [padId, state, onColor, offColor]
                if (size >= 4) {
                    const int pad = data[0];
                    if (pad >= 0 && pad < 16) {
                        const uint8_t st  = static_cast<uint8_t>(data[1]);
                        const uint8_t cid = static_cast<uint8_t>(data[2]); // active on-colour
                        if (target.colours[pad] != cid || target.states[pad] != st) {
                            target.colours[pad] = cid;
                            target.states[pad]  = st;
                            changed |= static_cast<uint16_t>(1u << pad);
                        }
                    }
                }
                break;
            }
            default:
                break;
        }
        return changed;
    }
}
