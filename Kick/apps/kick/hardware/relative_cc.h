#pragma once

// ============================================================================
// Relative (two's complement, 7 bit) controller values, e.g. the Akai MPC Key 61 Data Dial and Q-Links.
// MEASURED (`git show 35fce454:docs/hw-mailbox/TO_CLOUD.md`): Data Dial `B0 64 01` = one step clockwise, `B0 64 7F` = one step
// anticlockwise. JUCE/ALSA-free, host-testable.
//
//   0x01..0x3F -> +1..+63   (clockwise, N steps in one message when the dial is turned fast)
//   0x7F..0x41 -> -1..-63
//   0x40       -> -64       (two's complement of 7 bits)
//   0x00       ->  0        (no step: callers must not forward it as a rotation)
// ============================================================================
namespace kick_hw {
namespace relcc {

inline int decode(int cc_val)
{
    const int v = cc_val & 0x7F;
    return v < 64 ? v : v - 128;
}

} // namespace relcc
} // namespace kick_hw
