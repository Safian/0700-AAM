// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

// Pure (JUCE- and ALSA-free) logic for Akai MPC 4-encoder Q-Link bank/column switching (SS-22, LE-24, QL-07, QL-11).
// The MPC Key 61 has 4 physical Q-Link knobs. The Q-LINK button cycles through 4 columns/banks (0..3).
// Bank 0 = Knobs 1..4 (DCL EN_KNOB_1..4)
// Bank 1 = Knobs 5..8 (DCL EN_KNOB_5..8)
// Bank 2 = Knobs 9..12 (DCL EN_KNOB_9..12)
// Bank 3 = Knobs 13..16 (DCL EN_KNOB_13..16)
namespace kick_hw::qlink {

constexpr int kEncodersPerBank = 4;
constexpr int kNumBanks        = 4;
constexpr int kTotalKnobs      = 16;

inline int nextBank(int currentBank) noexcept
{
    return (currentBank < 0 || currentBank >= kNumBanks - 1) ? 0 : currentBank + 1;
}

// Maps physical encoder index 0..3 and bank 0..3 to DCL knob sequence offset 0..15.
// Adding contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1 yields EN_KNOB_1..EN_KNOB_16.
inline int knobOffset(int encoderIdx, int bank) noexcept
{
    if (encoderIdx < 0 || encoderIdx >= kEncodersPerBank) return 0;
    if (bank < 0 || bank >= kNumBanks) bank = 0;
    return encoderIdx + (bank * kEncodersPerBank);
}

} // namespace kick_hw::qlink
