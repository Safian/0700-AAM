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

#include "kick_menu_logic.h"

namespace ui
{
    ///
    ///

    struct MenuItem
    {
        juce::String Name;
        juce::String Icon;
        bool         enabled = true;
        //String Help;
        MenuItem() {}
        MenuItem( const juce::String& N, const juce::String& I )
            : Name( N ), Icon( I ){};
    };

    class KickMenuComponent : public KickControlBase<juce::Component>
    {
    public:
        std::function<void( const juce::String& Option, int index, const juce::String& Group )> onMenuOptionClicked;

        explicit KickMenuComponent( const juce::String& Name, const int NumColumns = 4,
                                    const juce::Rectangle<int>& Hdr = ui::layout::title );
        ~KickMenuComponent();

        void setColumns( int NumColumns );

        void addOption( const MenuItem& Item, const juce::String& Group );
        void addOptions( const std::vector<MenuItem>& Items, const juce::String& Group = "" );

        void hide();
        void show( juce::Rectangle<int> bounds );

        void visibilityChanged();

        // Keyboard / dial highlight of one tile, and graying out a tile that cannot be used now.
        void setHighlight( int Index );
        int  getHighlight() const { return navSelect; }
        void setItemEnabled( int Index, bool Enabled );
        // Presses a tile as a touch would; false when the tile does not exist or is disabled.
        bool clickItem( int Index );

        // Removes every tile (the Menu rebuilds its page).
        void clearOptions();
        // Layout: at least this many rows (so a short last page keeps the tile size) and a strip kept free at the bottom.
        void setMinRows( int Rows );
        void setBottomReserved( int Pixels );

        // Hooks of a screen that wants more than one page of tiles. Return true when the event was handled.
        std::function<bool( int delta, bool shift )> onDialStep;
        std::function<bool( bool shift )>            onDialClick;
        // The highlighted tile changed (dial or setHighlight); index within the tiles.
        std::function<void( int index )> onHighlightChanged;

    protected:
        void resized() override;

    private:
        struct MenuGroup
        {
            KickLabel                         name;
            juce::OwnedArray<KickImageButton> items;
        };

        int                         noOfColumns{ 4 }, navSelect{ 0 }, navGroup{ 0 }, minRows{ 0 }, bottomReserved{ 0 };
        juce::OwnedArray<MenuGroup> model;
        juce::Rectangle<int>        headerSize;

        void refreshHighlight();
        void AddOptionInternal( const std::vector<MenuItem>& Items, const juce::String& Group = "" );

        void knobRotated( double ) override;
        void knobClicked() override;
        void padPressed( int PadNo, int State, int Velocity, int Aftertouch ) override;
        void sliderMoved( float ) override;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickMenuComponent )
    };

}
