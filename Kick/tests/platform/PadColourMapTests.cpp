// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the Akai pad colour mapping. Optional argv[1]: path to assets/Default/theme.xml,
// the palette table is then compared against the PADBUTTON attributes.
#include "../../apps/kick/hardware/pad_colour_map.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <sstream>
#include <string>

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
    std::puts("OK");
    return 0;
}
