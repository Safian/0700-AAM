/*
 * Copyright (c) 2021 MUSIC Tribe
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
    class AppMenuOptions : public ui::KickMenuComponent
        , public IScreen
    {
    public:
        AppMenuOptions();
        ~AppMenuOptions();

        bool requiresControlBar() override { return true; }
        bool requiresStatusBar() override { return true; }
        bool requiresHandleCommand() override { return true; }

        void handleCommand( CPktMgr::HWMsg_t& ) override;

        // Presses the tile of a menu item (its page is shown first), as a touch would. False when the item is grayed out
        // (needs a project) or unknown. Used by the smoke test.
        bool activateItem( int item );

    private:
        using Layout = ui::menu_logic::MenuLayout;

        void visibilityChanged() override;
        void resized() override;

        // One page of tiles from the registry in the stored order; the highlight stays on a slot of that page.
        void rebuildPage( int slot = 0 );
        void rebuildPageAsync( int slot );
        void refreshStates();
        void showPage( int page, int slot );
        void activate( int slot );
        void pickOrDrop( int position );
        void announce( int slot );
        bool stepDial( int delta, bool shift );
        void setMoving( bool on );
        void resetLayout();
        void loadLayout();
        void saveLayout();
        void runItem( int item );

        static juce::File layoutFile();

        Layout              layout_;
        int                 page_{ 0 };
        bool                moving_{ false };
        int                 carry_{ -1 }; // position of the tile picked up in move mode
        ui::KickImageButton prevPage_{ "<", "" }, nextPage_{ ">", "" }, moveButton_{ "Move tiles", "" }, resetButton_{ "Reset menu", "" };
        ui::KickLabel       pageLabel_{ "" };
    };

}
