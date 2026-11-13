// SPDX-License-Identifier: AGPL-3.0-or-later
// KICK - pure (JUCE-, ALSA- and CLAP-free) logic for driving a plug-in's clap.remote-controls pages from the Q-Link encoders.
//
// The MPC Key 61 has 4 physical Q-Link encoders; the Q-LINK button cycles 4 banks (hardware/qlink_bank.h), so the DCL knob codes are
// EN_KNOB_1 .. EN_KNOB_16 (offset 0..15). A remote-control page has 8 knobs. Knobs 1..8 (offsets 0..7) drive the FOCUSED page and
// knobs 9..16 (offsets 8..15) drive the page after it, so the two banks of one page are bank 0 and bank 1.
// The focused page follows what is on screen: the page whose parameters overlap most with the parameters of the visible panel.
#pragma once
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

namespace kick::clap::remote {

constexpr int knobsPerPage = 8;
constexpr int knobsTotal = 16;
constexpr uint32_t invalidId = 0xFFFFFFFFu; // CLAP_INVALID_ID

using PageParams = std::array<uint32_t, knobsPerPage>;

// Which page and which of its 8 slots a knob offset (0..15) drives when `focus` is the focused page index. page == -1: no such page.
struct Target {
    int page = -1;
    int slot = -1;
    bool valid() const noexcept { return page >= 0 && slot >= 0; }
};
inline Target resolveKnob(int focus, int knobOffset, int pageCount) noexcept {
    Target target;
    if (focus < 0 || pageCount <= 0 || knobOffset < 0 || knobOffset >= knobsTotal) return target;
    const int page = focus + knobOffset / knobsPerPage;
    if (page >= pageCount) return target;
    target.page = page;
    target.slot = knobOffset % knobsPerPage;
    return target;
}

// The parameter a knob drives, invalidId when the slot of the page is unused or the knob has no page.
inline uint32_t parameterFor(const std::vector<PageParams>& pages, int focus, int knobOffset) noexcept {
    const auto target = resolveKnob(focus, knobOffset, static_cast<int>(pages.size()));
    return target.valid() ? pages[static_cast<size_t>(target.page)][static_cast<size_t>(target.slot)] : invalidId;
}

// Previous / next page with wrap-around; -1 when there are no pages. `delta` may be any integer.
inline int stepPage(int page, int delta, int pageCount) noexcept {
    if (pageCount <= 0) return -1;
    const int count = pageCount;
    int next = (std::max(0, std::min(page, count - 1)) + delta) % count;
    if (next < 0) next += count;
    return next;
}

// The page that has the most parameters in common with `ids` (the parameters of the visible panel). Ties go to the earlier page, which is
// the main page of an area in SynthyBoy's table. -1 when no page shares a parameter.
inline int bestPage(const std::vector<uint32_t>& ids, const std::vector<PageParams>& pages) noexcept {
    int best = -1, bestCount = 0;
    for (size_t p = 0; p < pages.size(); ++p) {
        int count = 0;
        for (const auto id : pages[p])
            if (id != invalidId && std::find(ids.begin(), ids.end(), id) != ids.end()) ++count;
        if (count > bestCount) { best = static_cast<int>(p); bestCount = count; }
    }
    return best;
}

// A relative encoder turn applied to a parameter. `delta` is the turn as a fraction of the encoder range (0..1, signed). Stepped
// parameters move by whole ticks (at least one), continuous ones by delta of their range. The result stays inside [minimum, maximum].
inline double adjusted(double current, double minimum, double maximum, bool stepped, double delta) noexcept {
    if (!std::isfinite(delta) || delta == 0 || !std::isfinite(current) || maximum < minimum) return current;
    const double amount = stepped ? std::copysign(std::max(1., std::round(std::abs(delta) * 127.)), delta) : delta * (maximum - minimum);
    return std::max(minimum, std::min(maximum, current + amount));
}

} // namespace kick::clap::remote
