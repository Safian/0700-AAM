// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the Akai pad colour mapping and DCL packet parsing (CE-02, CE-08, SS-07, NS-04, SG-10).
// Optional argv[1]: path to assets/Default/theme.xml, the palette table is then compared against PADBUTTON attributes.
#include "../../apps/kick/hardware/pad_colour_map.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

using namespace kick_hw;

int main(int argc, char** argv)
{
    PadRgb7 c{};
    // id 12 = 0xFF250D, assigned: (0x25>>1=18, 0x0D>>1... 0xFF>>1=127) -> r=127*0.3=38, g=18*0.3=5, b=6*0.3=2
    check(padColourToRgb7(12, kPadStateSampleAssigned, c) && c.r == 38 && c.g == 5 && c.b == 2, "red assigned");
    // white: 127 * 0.3 = 38.1 -> 38
    check(padColourToRgb7(16, kPadStateSampleAssigned, c) && c.r == 38 && c.g == 38 && c.b == 38, "white assigned");
    check(padColourToRgb7(16, kPadStateOn, c) && c.r == 38, "On = assigned level");
    check(padColourToRgb7(16, kPadStateSampleNotAssigned, c) && c.r == 10 && c.g == 10 && c.b == 10, "not assigned very dim");
    check(padColourToRgb7(16, kPadStateOff, c) && c.r == 0 && c.g == 0 && c.b == 0, "off");
    check(padColourToRgb7(kPadColourTrans, kPadStateSampleAssigned, c) && c.r == 0 && c.g == 0 && c.b == 0, "Trans is black");
    check(!padColourToRgb7(-1, 2, c) && !padColourToRgb7(18, 2, c) && !padColourToRgb7(255, 2, c), "unknown id");
    check(padColourToRgb7(5, 99, c) && c.g > 0, "unknown state treated as assigned");

    for (int id = 0; id < kPadColourCount; ++id)
        for (int st = 0; st <= 6; ++st) {
            check(padColourToRgb7(id, st, c), "all ids valid");
            check(c.r <= 127 && c.g <= 127 && c.b <= 127, "7-bit range");
        }
    check(scaleTo7bit(0xFFFFFFFFu, 1000) == 127, "channel masked, full scale");
    check(scaleTo7bit(0xFF, 5000) == 127, "clamp high");
    check(scaleTo7bit(0xFF, -100) == 0, "clamp low");

    // ---- CE-02, CE-08, SS-07, NS-04, SG-10: DCL pad packet parsing tests
    PadStateArray arr;
    // 1. Color mode packet (CE-02, CE-08)
    const int modeData[] = { 3 }; // mode 3
    uint16_t mask = applyPadPacket(kCmdPadColorMode, modeData, 1, arr);
    check(mask == 0xFFFF, "ColorMode returns all-pad mask");
    check(arr.colorMode == 3, "ColorMode updated");

    // 2. Set single pad (NS-04, SG-10, Step Seq)
    const int singleData[] = { 4 /*pad*/, kPadStateOn /*state*/, 12 /*colour red*/ };
    mask = applyPadPacket(kCmdPadSetSinglePad, singleData, 3, arr);
    check(mask == (1u << 4), "SinglePad updates pad 4");
    check(arr.colours[4] == 12 && arr.states[4] == kPadStateOn, "Pad 4 colour and state");
    // Repeating same data returns 0 (no redundant dirty flags)
    check(applyPadPacket(kCmdPadSetSinglePad, singleData, 3, arr) == 0, "SinglePad duplicate -> 0");

    // 3. Clear all pads / All single colour (clearAllPads in NS-04, SG-10)
    const int clearData[] = { kPadStateOff, kPadColourTrans };
    mask = applyPadPacket(kCmdPadSetAllSingleColour, clearData, 2, arr);
    check(mask != 0 && arr.colours[0] == kPadColourTrans && arr.states[0] == kPadStateOff, "Clear all pads");

    // 4. All multi colour (pushToHW from PadPerform / PadColor)
    int multiData[17];
    multiData[0] = kPadStateOn;
    for (int i = 0; i < 16; ++i) multiData[1 + i] = i % kPadColourCount;
    mask = applyPadPacket(kCmdPadSetAllMultiColour, multiData, 17, arr);
    check(mask == 0xFFFF, "AllMultiColour dirty mask 0xFFFF");
    for (int i = 0; i < 16; ++i) {
        check(arr.colours[i] == (i % kPadColourCount), "Multi colour pad matches");
        check(arr.states[i] == kPadStateOn, "Multi colour state on");
    }

    // 5. Step Sequencer single pad (SS-07)
    const int stepSingleData[] = { 7 /*pad*/, kPadStateOn, 5 /*colour*/, 110 /*velocity*/ };
    mask = applyPadPacket(kCmdPadSetStepSinglePad, stepSingleData, 4, arr);
    check(mask == (1u << 7), "StepSinglePad updates pad 7");
    check(arr.colours[7] == 5 && arr.states[7] == kPadStateOn, "Pad 7 step colour and state");

    // 6. Step Sequencer all pads (SS-07)
    int stepAllData[33];
    stepAllData[0] = kPadStateOn;
    for (int i = 0; i < 16; ++i) stepAllData[1 + i] = 3;  // colour 3
    for (int i = 0; i < 16; ++i) stepAllData[17 + i] = 100; // velocity 100
    mask = applyPadPacket(kCmdPadSetStepAllPad, stepAllData, 33, arr);
    check(mask != 0 && arr.colours[0] == 3 && arr.states[0] == kPadStateOn, "StepAllPad updates all");

    // 7. Blinking pad (NS-04)
    const int blinkData[] = { 2 /*pad*/, kPadStateOnBlink, 9 /*onColor*/, 0 /*offColor*/ };
    mask = applyPadPacket(kCmdPadSetBlinking, blinkData, 4, arr);
    check(mask == (1u << 2), "Blinking updates pad 2");
    check(arr.colours[2] == 9 && arr.states[2] == kPadStateOnBlink, "Pad 2 blinking state");

    // 8. Full colour state packet
    int fullState[32];
    for (int i = 0; i < 16; ++i) fullState[i] = 1;     // colour 1
    for (int i = 0; i < 16; ++i) fullState[16 + i] = 2; // state 2
    mask = applyPadPacket(kCmdPadSetColourState, fullState, 32, arr);
    check(mask == 0xFFFF, "SetColourState dirty 0xFFFF");
    for (int i = 0; i < 16; ++i) {
        check(arr.colours[i] == 1 && arr.states[i] == 2, "SetColourState pad matches");
    }

    if (argc > 1) {
        std::ifstream f(argv[1]);
        std::stringstream ss; ss << f.rdbuf();
        const std::string xml = ss.str();
        check(!xml.empty(), "theme.xml readable");
        const auto tag = xml.find("<PADBUTTON");
        check(tag != std::string::npos, "PADBUTTON tag");
        for (int id = 0; id < kPadColourCount; ++id) {
            const std::string key = " _" + std::to_string(id) + "=\"";
            const auto p = xml.find(key, tag);
            check(p != std::string::npos, "palette attribute present");
            const auto v = std::strtoull(xml.c_str() + p + key.size(), nullptr, 16);
            check(static_cast<uint32_t>(v) == kPadPalette[id], "palette matches theme.xml");
        }
    }
    std::puts("PadColourMapTests: OK");
    return 0;
}
