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
    class DPLayerMix : public ui::KickGrid
        , public DrumInterface
    {
    public:
        DPLayerMix( std::vector<int>& Pads, mt::MTDrumProgram*& PDrum );
        ~DPLayerMix() = default;

        void refresh() override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;
        void refreshPads();

    private:
        std::vector<int>&   pads;
        mt::MTDrumProgram*& pDrum;

        std::array<ui::KickRotary, 4> semi;
        std::array<ui::KickRotary, 4> fine;
        std::array<ui::KickRotary, 4> amp;
        std::array<ui::KickRotary, 4> pan;
        // Velocity range of each layer (Layer Play: Velocity). Closed range 1..127; start never above end.
        ui::KickNumberT<int> vs0{ "VEL START", 1, 127, 1 }, vs1{ "VEL START", 1, 127, 1 }, vs2{ "VEL START", 1, 127, 1 }, vs3{ "VEL START", 1, 127, 1 };
        ui::KickNumberT<int> ve0{ "VEL END", 1, 127, 127 }, ve1{ "VEL END", 1, 127, 127 }, ve2{ "VEL END", 1, 127, 127 }, ve3{ "VEL END", 1, 127, 127 };
        std::array<ui::KickNumberT<int>*, 4> velStart{ &vs0, &vs1, &vs2, &vs3 };
        std::array<ui::KickNumberT<int>*, 4> velEnd{ &ve0, &ve1, &ve2, &ve3 };

        void visibilityChanged() override;
    };
}
