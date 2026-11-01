// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the device discovery text parsing (cards, touch name, ranges, fallbacks). ALSA / JUCE free.
#include "../../apps/kick/hardware/device_probe.h"
#include <cstdio>
#include <cstdlib>

using namespace kick_hw::probe;

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

int main()
{
    const std::string cards =
        " 0 [M61            ]: USB-Audio - MPC Key 61\n"
        "                      Akai Professional MPC Key 61 at usb-fd000000.usb-1, high speed\n"
        " 1 [ACVM           ]: rockchip_acvm - Rockchip ACVM\n"
        "                      Rockchip ACVM 4ch\n";
    const std::vector<std::string> midiN = { "M61", "MPC", "Key 61" };
    const std::vector<std::string> audioN = { "ACVM" };

    check(findCardIndex(cards, midiN) == 0, "rawmidi card 0");
    check(findCardIndex(cards, audioN) == 1, "audio card 1");

    // Reordered cards: indices follow the file, not the hard-coded values.
    const std::string swapped =
        " 0 [Other          ]: USB-Audio - Some Headset\n"
        "                      Some Headset at usb\n"
        " 2 [ACVM           ]: rockchip_acvm - Rockchip ACVM\n"
        " 3 [M61            ]: USB-Audio - MPC Key 61\n"
        "                      Akai Professional MPC Key 61\n";
    check(findCardIndex(swapped, midiN) == 3, "rawmidi card 3");
    check(findCardIndex(swapped, audioN) == 2, "audio card 2");

    // Match only on the continuation line, case-insensitive; last card without trailing newline.
    check(findCardIndex(" 4 [Foo            ]: USB-Audio - Foo\n                      akai mpc key 61 thing", midiN) == 4,
          "continuation line / case-insensitive / no trailing newline");

    // Nothing found / empty text.
    check(findCardIndex("--- no soundcards ---\n", midiN) == -1, "no soundcards");
    check(findCardIndex("", midiN) == -1, "empty text");
    check(findCardIndex(cards, {}) == -1, "no needles");
    check(findCardIndex(cards, { "" }) == -1, "empty needle never matches");

    // Fallback choice: known hardware values are kept when discovery finds nothing.
    check(chooseOrFallback(findCardIndex("", midiN), 0) == 0, "rawmidi fallback card 0");
    check(chooseOrFallback(findCardIndex("", audioN), 1) == 1, "audio fallback card 1");
    check(chooseOrFallback(3, 0) == 3, "found value wins");
    check(chooseOrFallback(0, 7) == 0, "card 0 is a valid find");

    check(rawmidiPortName(0, 1) == "hw:0,0,1", "rawmidi name");
    check(rawmidiPortName(3, 2) == "hw:3,0,2", "rawmidi name card 3");
    check(plughwName(1) == "plughw:1,0", "plughw name");

    // Touch names.
    check(isTouchDeviceName("ILI2117 Touch"), "ILI2117 name");
    check(isTouchDeviceName("ilitek ili2117"), "case-insensitive");
    check(!isTouchDeviceName("gpio-keys"), "other device");
    check(!isTouchDeviceName(""), "empty name");

    const std::string devs =
        "I: Bus=0019 Vendor=0001 Product=0001 Version=0100\n"
        "N: Name=\"gpio-keys\"\n"
        "H: Handlers=kbd event0\n"
        "\n"
        "I: Bus=0003 Vendor=222a Product=0001 Version=0110\n"
        "N: Name=\"ILI2117 Touch\"\n"
        "P: Phys=usb-1/input0\n"
        "H: Handlers=mouse1 event3\n";
    check(findTouchEventInProcDevices(devs) == 3, "touch event3 from /proc/bus/input/devices");
    check(findTouchEventInProcDevices("I: Bus=0019\nN: Name=\"gpio-keys\"\nH: Handlers=kbd event0\n") == -1, "no touch device");
    check(findTouchEventInProcDevices("") == -1, "empty devices");

    // Touch range + mapping.
    check(touchRangeOrDefault(0) == 2048 && touchRangeOrDefault(-1) == 2048, "range fallback 2048");
    check(touchRangeOrDefault(4095) == 4095, "real range");
    int ux = 0, uy = 0;
    mapTouch(0, 0, 2048, 2048, 1280, 800, ux, uy);
    check(ux == 1279 && uy == 0, "origin (inverted raw Y)");
    mapTouch(2048, 2048, 2048, 2048, 1280, 800, ux, uy);
    check(ux == 0 && uy == 799, "far corner");
    mapTouch(1024, 1024, 2048, 2048, 1280, 800, ux, uy);
    check(ux == ((1024 * 1279) / 2048) && uy == (1024 * 799) / 2048, "centre = original formula");
    mapTouch(5000, -5, 2048, 2048, 1280, 800, ux, uy);
    check(ux == 1279 && uy == 799, "clamped");
    mapTouch(2047, 2047, 4096, 4096, 1024, 600, ux, uy);
    check(ux == ((4096 - 2047) * 1023) / 4096 && uy == (2047 * 599) / 4096, "wider range");

    std::puts("device-probe-tests OK");
    return 0;
}
