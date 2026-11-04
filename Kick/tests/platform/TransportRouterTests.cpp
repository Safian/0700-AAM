// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for hardware transport routing (SA-04).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined TransportRouterTests.cpp
#include "../../apps/kick/hardware/transport_router.h"
#include <cstdio>
#include <cstdlib>

static void check(bool ok, const char* why)
{
    if (!ok) { std::fprintf(stderr, "FAIL: %s\n", why); std::exit(1); }
}

using namespace kick_hw::transport;

int main()
{
    // ---- 1. Target resolution
    // Standard screens -> Sequence
    check(resolveTarget(1001 /* HomeListView */, false, false) == Target::Sequence, "ListView -> Sequence");
    check(resolveTarget(1002 /* HomeGridView */, false, false) == Target::Sequence, "GridView -> Sequence");
    check(resolveTarget(1003 /* HomeSequencerView */, false, false) == Target::Sequence, "SequencerView -> Sequence");
    check(resolveTarget(1013 /* MenuPadMixer */, false, false) == Target::Sequence, "PadMixer -> Sequence");

    // Looper screen or looper recording -> Looper
    check(resolveTarget(kScreenMenuLooper, false, false) == Target::Looper, "MenuLooper -> Looper");
    check(resolveTarget(1001, true, false) == Target::Looper, "Looper recording on ListView -> Looper");

    // Sampler screen or sampler recording -> Sampler
    check(resolveTarget(kScreenMenuSampler, false, false) == Target::Sampler, "MenuSampler -> Sampler");
    check(resolveTarget(1001, false, true) == Target::Sampler, "Sampler recording on ListView -> Sampler");

    // Priority: Sampler active over Looper active if both somehow flag
    check(resolveTarget(kScreenMenuSampler, true, false) == Target::Sampler, "MenuSampler takes priority");

    // ---- 2. Routing: Sequence target
    check(routeButton(HW_BTN_PLAY, false, Target::Sequence) == Action::SequencePlay, "Seq Play");
    check(routeButton(HW_BTN_PLAY_START, false, Target::Sequence) == Action::SequencePlayStart, "Seq PlayStart");
    check(routeButton(HW_BTN_STOP, false, Target::Sequence) == Action::SequenceStop, "Seq Stop");
    check(routeButton(HW_BTN_REC, false, Target::Sequence) == Action::SequenceRecArm, "Seq Rec Arm");
    check(routeButton(HW_BTN_REC, true, Target::Sequence) == Action::SequenceRecall, "Seq Recall (Shift+REC)");
    check(routeButton(HW_BTN_OVERDUB, false, Target::Sequence) == Action::SequenceRecArm, "Seq Overdub");

    // ---- 3. Routing: Looper target
    check(routeButton(HW_BTN_REC, false, Target::Looper) == Action::LooperRecord, "Looper Rec");
    check(routeButton(HW_BTN_OVERDUB, false, Target::Looper) == Action::LooperOverdub, "Looper Overdub");
    check(routeButton(HW_BTN_PLAY, false, Target::Looper) == Action::LooperPlay, "Looper Play");
    check(routeButton(HW_BTN_PLAY_START, false, Target::Looper) == Action::LooperPlay, "Looper PlayStart");
    check(routeButton(HW_BTN_STOP, false, Target::Looper) == Action::LooperStop, "Looper Stop");
    check(routeButton(HW_BTN_REC, true, Target::Looper) == Action::SequenceRecall, "Shift+REC always Recall");

    // ---- 4. Routing: Sampler target (SA-04)
    check(routeButton(HW_BTN_REC, false, Target::Sampler) == Action::SamplerRecord, "Sampler Rec");
    check(routeButton(HW_BTN_PLAY, false, Target::Sampler) == Action::SamplerPlay, "Sampler Play");
    check(routeButton(HW_BTN_PLAY_START, false, Target::Sampler) == Action::SamplerPlay, "Sampler PlayStart");
    check(routeButton(HW_BTN_STOP, false, Target::Sampler) == Action::SamplerStop, "Sampler Stop");
    check(routeButton(HW_BTN_REC, true, Target::Sampler) == Action::SequenceRecall, "Shift+REC always Recall");

    // Non-transport buttons return None
    check(routeButton(HW_BTN_MAIN, false, Target::Sampler) == Action::None, "Main -> None");
    check(routeButton(HW_BTN_SHIFT, false, Target::Sequence) == Action::None, "Shift -> None");

    std::puts("TransportRouterTests: OK");
    return 0;
}
