// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the read-only system probe report (fake root directory, no device access).
#include "../../apps/kick/hardware/system_probe.h"
#include <cstdio>
#include <cstdlib>
#include <string>
#include <sys/stat.h>
#include <unistd.h>

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

static void put(const std::string& path, const std::string& text)
{
    FILE* f = std::fopen(path.c_str(), "w");
    check(f != nullptr, "fixture file");
    std::fputs(text.c_str(), f);
    std::fclose(f);
}

int main()
{
    using namespace kick_hw::sysprobe;
    check(formatSection("t", "a") == "=== t ===\na\n", "section adds newline");
    check(formatSection("t", "a\n") == "=== t ===\na\n", "section keeps newline");
    check(formatSection("t", "") == "=== t ===\n(empty)\n", "empty section");
    check(formatList("d", false, {}).find("cannot open") != std::string::npos, "unopened dir");
    check(formatList("d", true, { "a", "b" }) == "=== d ===\na\nb\n", "list");

    char tmpl[] = "/tmp/kick_probe_testXXXXXX";
    const char* root = mkdtemp(tmpl);
    check(root != nullptr, "mkdtemp");
    const std::string r = root;
    mkdir((r + "/proc").c_str(), 0755);
    mkdir((r + "/proc/asound").c_str(), 0755);
    mkdir((r + "/proc/bus").c_str(), 0755);
    mkdir((r + "/proc/bus/input").c_str(), 0755);
    mkdir((r + "/dev").c_str(), 0755);
    put(r + "/dev/i2c-3", "");
    put(r + "/dev/i2c-0", "");
    put(r + "/dev/ttyS0", "");
    put(r + "/proc/asound/cards", " 0 [M61            ]: USB-Audio - MPC Key 61\n                      Akai at usb\n 1 [ACVM           ]: ACVM\n");
    put(r + "/proc/bus/input/devices", "I: Bus=0018\nN: Name=\"ILI2117 Touch\"\nH: Handlers=mouse1 event3\n");

    std::vector<std::string> n;
    check(listDir(r + "/dev", n, "i2c-") && n.size() == 2 && n[0] == "i2c-0" && n[1] == "i2c-3", "sorted prefix listing");
    check(!listDir(r + "/nope", n), "missing dir");

    const std::string rep = buildReport(r);
    check(rep.find("=== /proc/asound/cards ===") != std::string::npos, "cards section");
    check(rep.find("i2c-0\ni2c-3") != std::string::npos, "i2c names listed");
    check(rep.find("ttyS0") == std::string::npos, "prefix filter");
    check(rep.find("/proc/asound/pcm ===\n(cannot read)") != std::string::npos, "missing file noted");
    check(rep.find("rawmidi card: found=0 -> used 0") != std::string::npos, "midi card discovery");
    check(rep.find("audio card (ACVM): found=1 -> used 1") != std::string::npos, "audio card discovery");
    check(rep.find("event3") != std::string::npos, "touch discovery");
    std::printf("system probe tests passed\n");
    return 0;
}
