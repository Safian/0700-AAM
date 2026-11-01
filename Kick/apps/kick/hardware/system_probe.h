// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "device_probe.h"
#include <algorithm>
#include <string>
#include <vector>
#include <cstdio>
#include <dirent.h>

// ============================================================================
// Read-only system facts for the "probe" command of /data/kick_cmd (T32 / T19). JUCE / ALSA free.
// Rules: only fopen(...,"r") on /proc, /sys and /etc text files and opendir/readdir on directories.
// It NEVER opens a device node (no /dev/i2c-*, no /dev/snd/*), never calls ioctl and never writes to a device:
// /dev/i2c-* and /dev/snd entries are only listed by name.
// ============================================================================
namespace kick_hw {
namespace sysprobe {

// Whole (small) text file, empty on failure. Sets 'ok' to false when it could not be opened.
inline std::string readTextFile(const std::string& path, bool* ok = nullptr)
{
    std::string out;
    FILE* f = std::fopen(path.c_str(), "r");
    if (ok) *ok = (f != nullptr);
    if (!f) return out;
    char buf[512];
    size_t n;
    while ((n = std::fread(buf, 1, sizeof(buf), f)) > 0) out.append(buf, n);
    std::fclose(f);
    return out;
}

// Sorted entry names of a directory (without "." and ".."), optionally only those starting with 'prefix'.
// Returns false when the directory cannot be opened.
inline bool listDir(const std::string& path, std::vector<std::string>& names, const std::string& prefix = "")
{
    names.clear();
    DIR* d = opendir(path.c_str());
    if (!d) return false;
    while (struct dirent* e = readdir(d)) {
        const std::string n = e->d_name;
        if (n == "." || n == "..") continue;
        if (!prefix.empty() && n.compare(0, prefix.size(), prefix) != 0) continue;
        names.push_back(n);
    }
    closedir(d);
    std::sort(names.begin(), names.end());
    return true;
}

// "=== title ===\nbody\n" (a missing trailing newline is added; empty body -> "(empty)").
inline std::string formatSection(const std::string& title, const std::string& body)
{
    std::string s = "=== " + title + " ===\n";
    if (body.empty()) s += "(empty)\n";
    else {
        s += body;
        if (body.back() != '\n') s += "\n";
    }
    return s;
}

inline std::string formatList(const std::string& title, bool opened, const std::vector<std::string>& names)
{
    if (!opened) return formatSection(title, "(cannot open directory)");
    std::string body;
    for (const auto& n : names) body += n + "\n";
    return formatSection(title, body);
}

// Builds the whole report. 'root' is "" on the device; tests can pass a fake root directory.
inline std::string buildReport(const std::string& root = "")
{
    std::string r;
    auto file = [&](const char* path) {
        bool ok = false;
        const std::string t = readTextFile(root + path, &ok);
        r += formatSection(path, ok ? t : std::string("(cannot read)"));
    };
    auto dir = [&](const char* path, const char* prefix = "") {
        std::vector<std::string> names;
        const bool ok = listDir(root + path, names, prefix);
        std::string title = path;
        if (*prefix) title += std::string("/") + prefix + "*";
        r += formatList(title, ok, names);
    };

    file("/proc/asound/cards");
    file("/proc/asound/pcm");
    file("/proc/bus/input/devices");
    file("/etc/asound.conf");
    dir("/dev", "i2c-");     // names only, never opened
    dir("/dev/snd");         // names only, never opened
    dir("/sys/bus/i2c/devices");
    {
        // kernel name of every i2c device (sysfs attribute, read-only)
        std::vector<std::string> names;
        std::string body;
        if (listDir(root + "/sys/bus/i2c/devices", names)) {
            for (const auto& n : names) {
                bool ok = false;
                std::string t = readTextFile(root + "/sys/bus/i2c/devices/" + n + "/name", &ok);
                while (!t.empty() && (t.back() == '\n' || t.back() == '\r')) t.pop_back();
                body += n + ": " + (ok ? t : std::string("(no name)")) + "\n";
            }
        }
        r += formatSection("/sys/bus/i2c/devices/*/name", body);
    }

    // What the Akai adapter picks: the same discovery functions on the same text (device_probe.h).
    {
        const std::string cards = readTextFile(root + "/proc/asound/cards");
        const std::string inputs = readTextFile(root + "/proc/bus/input/devices");
        const int midiFound = probe::findCardIndex(cards, { "M61", "MPC", "Key 61" });
        const int audioFound = probe::findCardIndex(cards, { "ACVM" });
        const int touchFound = probe::findTouchEventInProcDevices(inputs);
        char b[512];
        std::snprintf(b, sizeof(b),
            "rawmidi card: found=%d -> used %d (fallback 0), ports %s %s %s\n"
            "audio card (ACVM): found=%d -> used %d (fallback 1), pcm %s\n"
            "touch device (ILI2117) in /proc/bus/input/devices: event%d -> used %s (fallback /dev/input/event0 or /sys scan)\n",
            midiFound, probe::chooseOrFallback(midiFound, 0),
            probe::rawmidiPortName(probe::chooseOrFallback(midiFound, 0), 0).c_str(),
            probe::rawmidiPortName(probe::chooseOrFallback(midiFound, 0), 1).c_str(),
            probe::rawmidiPortName(probe::chooseOrFallback(midiFound, 0), 2).c_str(),
            audioFound, probe::chooseOrFallback(audioFound, 1),
            probe::plughwName(probe::chooseOrFallback(audioFound, 1)).c_str(),
            touchFound, touchFound >= 0 ? ("/dev/input/event" + std::to_string(touchFound)).c_str() : "(none)");
        r += formatSection("adapter discovery (recomputed, same functions as the adapter)", b);
    }
    return r;
}

} // namespace sysprobe
} // namespace kick_hw
