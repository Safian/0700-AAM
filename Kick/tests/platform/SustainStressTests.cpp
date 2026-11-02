// SPDX-License-Identifier: AGPL-3.0-or-later
// AG1b: two-thread stress test of kick_hw::SustainState (the bridge's strip + pedal Sustain state) used the way
// kick_hardware_bridge.h uses it: flag change + press/release decision + the (simulated) CC64 send in ONE critical
// section. Thread R = strip reader (press/release, mode re-checked under the lock), thread M = mode change (stores the
// mode, then forced release), thread P = physical pedal. Build:
//   g++ -std=c++17 -Wall -Wextra -fsanitize=thread (and address,undefined) SustainStressTests.cpp -pthread
#include "../../apps/kick/hardware/hw_targets.h"
#include <atomic>
#include <cstdio>
#include <mutex>
#include <thread>
#include <vector>

static std::atomic<int> g_fail { 0 };
#define CHECK(c) do { if (!(c)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); ++g_fail; } } while (0)

struct Sel { int sequence = 0; int track = 0; int program = 0; };

namespace {
std::mutex g_mutex;
kick_hw::SustainState<Sel> g_state;
std::atomic<bool> g_sustainMode { true };
std::vector<int> g_out;      // the CC64 values in send order (all on ONE lane: same sequence/track)
bool g_laneOn = false;       // what the receiving track believes
std::atomic<int> g_program { 0 };

// Called with g_mutex held: the simulated addLiveMidiMsg + the invariant "lane on <=> some source holds".
void send(int value)
{
    g_out.push_back(value);
    CHECK(value == 127 || value == 0);
    g_laneOn = value == 127;
}
void checkInvariant()
{
    const bool held = g_state.held(kick_hw::SustainSources<Sel>::kPedal) || g_state.held(kick_hw::SustainSources<Sel>::kStrip);
    CHECK(g_laneOn == held);
}

auto noKey = [](const Sel&) { return kick_hw::SustainKey(); };

void stripSet(bool down, bool onlyInSustainMode) // = bridge stripSustainSet
{
    std::lock_guard<std::mutex> lk(g_mutex);
    if (onlyInSustainMode && !g_sustainMode.load()) down = false;
    Sel t;
    if (g_state.strip(down, Sel{ 0, 0, g_program.load() }, noKey, t)) send(down ? 127 : 0);
    checkInvariant();
}
void pedalSet(bool down) // = bridge on_sustain_pedal
{
    std::lock_guard<std::mutex> lk(g_mutex);
    Sel t;
    if (g_state.pedal(down, Sel{ 0, 0, g_program.load() }, noKey, t)) send(down ? 127 : 0);
    checkInvariant();
}
}

int main()
{
    constexpr int N = 100000;
    std::thread reader([] {
        for (int i = 0; i < N; ++i)
        {
            if (!g_sustainMode.load()) { std::this_thread::yield(); continue; } // on_touch_strip: mode load
            stripSet((i & 1) == 0, true);                                      // press / release, mode re-checked in the lock
        }
    });
    std::thread modeChanger([] {
        for (int i = 0; i < N; ++i)
        {
            g_sustainMode.store(false); // cycleStripMode: store the new mode, then the forced release
            stripSet(false, false);
            g_sustainMode.store(true);
            g_program.store(i & 3);
        }
    });
    std::thread pedal([] {
        for (int i = 0; i < N; ++i) pedalSet((i & 1) == 0);
    });
    reader.join(); modeChanger.join(); pedal.join();

    // End of the run: mode left Sustain (forced release), pedal released.
    g_sustainMode.store(false);
    stripSet(false, false);
    pedalSet(false);

    CHECK(!g_state.stripDown());
    CHECK(!g_state.held(kick_hw::SustainSources<Sel>::kStrip));
    CHECK(!g_state.held(kick_hw::SustainSources<Sel>::kPedal));
    CHECK(!g_out.empty() && g_out.back() == 0);
    CHECK(!g_laneOn); // balanced: the lane ends released (a pedal press on a held lane may repeat CC64=127, harmless)
    { // deterministic: a press after the mode change sees the new mode and is turned into a release (nothing stuck)
        g_sustainMode.store(true);
        stripSet(true, true);
        CHECK(g_state.held(kick_hw::SustainSources<Sel>::kStrip) && g_laneOn);
        g_sustainMode.store(false);
        stripSet(true, true); // would be a press, but the mode is gone -> release
        CHECK(!g_state.held(kick_hw::SustainSources<Sel>::kStrip) && g_out.back() == 0 && !g_laneOn);
    }
    if (g_fail == 0) std::printf("sustain_stress: all passed (%zu messages)\n", g_out.size());
    return g_fail ? 1 : 0;
}
