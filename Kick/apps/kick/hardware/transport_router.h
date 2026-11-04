// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "hardware_adapter.h"

// Pure (JUCE-free, header only) decision table for hardware REC / STOP / PLAY button routing (SA-04).
// Routes hardware transport events to Sequence, Looper, or Sampler depending on current screen and recorder state.
namespace kick_hw::transport {

enum class Target {
    Sequence,
    Looper,
    Sampler
};

enum class Action {
    None,
    // Sequence
    SequencePlay,
    SequencePlayStart,
    SequenceStop,
    SequenceRecArm,
    SequenceRecall,
    // Looper (handled via DCL messages / Looper screen)
    LooperRecord,
    LooperOverdub,
    LooperPlay,
    LooperStop,
    // Sampler (handled via Sampler recorder / preview)
    SamplerRecord,
    SamplerPlay,
    SamplerStop
};

// Mirrors kick_ui::Screen enum numbers:
// MenuSampler = 1009, MenuLooper = 1010
constexpr int kScreenMenuSampler = 1009;
constexpr int kScreenMenuLooper  = 1010;

// Resolve which subsystem owns the transport buttons.
inline Target resolveTarget(int screenId, bool isLooperRecording, bool isSamplerRecording) noexcept
{
    if (screenId == kScreenMenuSampler || isSamplerRecording) {
        return Target::Sampler;
    }
    if (screenId == kScreenMenuLooper || isLooperRecording) {
        return Target::Looper;
    }
    return Target::Sequence;
}

// Routes a hardware transport button press based on shift state and resolved target.
inline Action routeButton(HardwareButton btn, bool isShiftDown, Target target) noexcept
{
    switch (btn) {
        case HW_BTN_REC:
            if (isShiftDown) return Action::SequenceRecall;
            if (target == Target::Sampler) return Action::SamplerRecord;
            if (target == Target::Looper)  return Action::LooperRecord;
            return Action::SequenceRecArm;

        case HW_BTN_OVERDUB:
            if (target == Target::Looper)  return Action::LooperOverdub;
            return Action::SequenceRecArm;

        case HW_BTN_PLAY:
            if (target == Target::Sampler) return Action::SamplerPlay;
            if (target == Target::Looper)  return Action::LooperPlay;
            return Action::SequencePlay;

        case HW_BTN_PLAY_START:
            if (target == Target::Sampler) return Action::SamplerPlay;
            if (target == Target::Looper)  return Action::LooperPlay;
            return Action::SequencePlayStart;

        case HW_BTN_STOP:
            if (target == Target::Sampler) return Action::SamplerStop;
            if (target == Target::Looper)  return Action::LooperStop;
            return Action::SequenceStop;

        default:
            return Action::None;
    }
}

} // namespace kick_hw::transport
