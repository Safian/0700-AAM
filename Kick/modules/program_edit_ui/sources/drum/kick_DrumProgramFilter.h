/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

namespace kick_ui
{
    // Drum pad filter (PE-21): type, cutoff, resonance. The voice always runs the filter; the Filter row of the Envelopes page sweeps
    // the cutoff upwards from the value set here (when its envelope is on).
    class DPFilter : public ui::KickGrid
        , public DrumInterface
    {
    public:
        DPFilter( std::vector<int>& Pads, mt::MTDrumProgram*& PDrum );
        ~DPFilter() = default;

        void refresh() override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;

    private:
        std::vector<int>&   pads;
        mt::MTDrumProgram*& pDrum;

        ui::KickListBox filterType{ "TYPE", { "LOW PASS", "HIGH PASS", "BAND PASS" }, 0, 1 };
        ui::KickRotary  cutoff{ "CUTOFF", juce::Slider::TextBoxAbove, true };
        ui::KickRotary  resonance{ "RESONANCE", juce::Slider::TextBoxAbove, true };

        void visibilityChanged() override;
    };
}
