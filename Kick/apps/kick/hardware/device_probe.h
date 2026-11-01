// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <vector>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// ============================================================================
// Device discovery helpers: pure text parsing, no ALSA / JUCE / syscalls (host-testable).
// The adapter reads /proc and /sys and feeds the text in here. Every function has a SAFE fallback:
// when nothing is found the caller's hard-coded value is returned, so the known hardware behaves as before.
// ============================================================================
namespace kick_hw {
namespace probe {

constexpr int kDefaultTouchRange = 2048;   // ILI2117 raw ABS range used so far

inline std::string lower(const std::string& s)
{
    std::string r(s);
    for (auto& c : r) if (c >= 'A' && c <= 'Z') c = (char)(c - 'A' + 'a');
    return r;
}

// Case-insensitive substring test.
inline bool containsNoCase(const std::string& hay, const std::string& needle)
{
    if (needle.empty()) return false;
    return lower(hay).find(lower(needle)) != std::string::npos;
}

// /proc/asound/cards text -> index of the first card whose block (header line + continuation lines) contains
// any of the needles (case-insensitive). -1 = none. Format:
//    0 [M61            ]: USB-Audio - MPC Key 61
//                         Akai Professional MPC Key 61 at usb-...
inline int findCardIndex(const std::string& cardsText, const std::vector<std::string>& needles)
{
    int cur = -1;
    std::string block;
    auto matches = [&]() {
        if (cur < 0) return false;
        for (const auto& n : needles) if (containsNoCase(block, n)) return true;
        return false;
    };
    size_t pos = 0;
    while (pos <= cardsText.size()) {
        const size_t nl = cardsText.find('\n', pos);
        const std::string line = cardsText.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? cardsText.size() + 1 : nl + 1;

        // header line: optional spaces, digits, optional spaces, '['
        size_t i = 0;
        while (i < line.size() && line[i] == ' ') ++i;
        const size_t d0 = i;
        while (i < line.size() && line[i] >= '0' && line[i] <= '9') ++i;
        size_t j = i;
        while (j < line.size() && line[j] == ' ') ++j;
        const bool header = (i > d0) && j < line.size() && line[j] == '[';
        if (header) {
            if (matches()) return cur;
            cur = std::atoi(line.substr(d0, i - d0).c_str());
            block = line;
        } else {
            block += "\n" + line;
        }
    }
    return matches() ? cur : -1;
}

// 'found' if >= 0, otherwise the hard-coded 'fallback'.
inline int chooseOrFallback(int found, int fallback) { return found >= 0 ? found : fallback; }

inline std::string rawmidiPortName(int card, int sub) {
    char b[48]; std::snprintf(b, sizeof(b), "hw:%d,0,%d", card, sub); return b;
}
inline std::string plughwName(int card, int dev = 0) {
    char b[48]; std::snprintf(b, sizeof(b), "plughw:%d,%d", card, dev); return b;
}

// Touch controller name as the kernel reports it (/sys/class/input/eventN/device/name).
inline bool isTouchDeviceName(const std::string& name) { return containsNoCase(name, "ILI2117"); }

// /proc/bus/input/devices text -> N of "eventN" for the first device whose "N: Name=" matches
// isTouchDeviceName(). -1 = none.
inline int findTouchEventInProcDevices(const std::string& text)
{
    bool match = false;
    size_t pos = 0;
    while (pos < text.size()) {
        const size_t nl = text.find('\n', pos);
        const std::string line = text.substr(pos, nl == std::string::npos ? std::string::npos : nl - pos);
        pos = (nl == std::string::npos) ? text.size() : nl + 1;
        if (line.compare(0, 2, "I:") == 0) match = false;
        else if (line.compare(0, 8, "N: Name=") == 0) match = isTouchDeviceName(line);
        else if (match && line.compare(0, 12, "H: Handlers=") == 0) {
            const size_t e = line.find("event");
            if (e != std::string::npos) return std::atoi(line.c_str() + e + 5);
        }
    }
    return -1;
}

// Real ABS_MT_POSITION_* maximum (EVIOCGABS) -> range used for scaling; non-positive (ioctl failed) -> 2048.
inline int touchRangeOrDefault(int absMaximum) { return absMaximum > 0 ? absMaximum : kDefaultTouchRange; }

// Raw ILI2117 coordinates -> UI coordinates. The panel is mounted rotated: UI x follows the inverted raw Y,
// UI y follows raw X. With rangeX = rangeY = 2048 this is the original formula.
inline void mapTouch(int rawX, int rawY, int rangeX, int rangeY, int logicalW, int logicalH, int& uiX, int& uiY)
{
    if (rawX < 0) rawX = 0;
    if (rawX > rangeX) rawX = rangeX;
    if (rawY < 0) rawY = 0;
    if (rawY > rangeY) rawY = rangeY;
    uiX = ((rangeY - rawY) * (logicalW - 1)) / rangeY;
    uiY = (rawX * (logicalH - 1)) / rangeX;
}

} // namespace probe
} // namespace kick_hw
