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
    struct IRowView : public ui::KickTableView
    {
        mt::MTAudioInterface&     ae{ mt::getAudioInterface() };
        const mt::MTSelected&     aeSel{ mt::getAudioInterface().getSelected() };
        ui::KickTableViewProvider provider;

        IRowView( const juce::String& ViewName, const juce::Colour& BackgroundColor = ui::Theme::backgroundColour( 5 ) );

        virtual void setupProvider() = 0;
        virtual void meterUpdate(){};

        // CM-14: Returns true if row is currently visible inside the viewport.
        bool isRowVisible( int row )
        {
            const auto va = getTableViewPort().getViewArea();
            if( va.isEmpty() )
                return true;
            return va.intersects( getRowBounds( row ) );
        }

        // Q-Link 1..4 (1-based knob codes) act on the selected row. Views override it.
        virtual void knobCommand( int /*knob*/, int /*value*/ ){};

        // The display of the row the Q-Link acts on: the selected row, else the first one. Null for an empty list.
        template<class Display>
        Display* qlinkRowDisplay()
        {
            auto& comps = getRowComponents();
            auto  row   = mt::mixlogic::qlinkRow( getSelectedRow(), comps.size() );
            return row < 0 ? nullptr : dynamic_cast<Display*>( comps[row] );
        }
    };

    // Q-Link on a switch (mute / solo): turn right = on, turn left = off. Goes through onClick (sendNotification),
    // so the engine is told - a silent setToggleState only changed the picture.
    template<class Button>
    inline void qlinkSwitch( Button& Btn, int Value )
    {
        const bool target = mt::mixlogic::toggleFromDelta( Btn.getToggleState(), Value );
        if( target != Btn.getToggleState() )
            Btn.setToggleState( target, juce::sendNotification );
    }

    // Fourth Q-Link: Mute, with Shift held Solo.
    template<class Button>
    inline void qlinkMuteOrSolo( Button& Mute, Button* Solo, int Value )
    {
        if( Solo != nullptr && contracts::DclContractsHelper::isShiftPressed() )
            qlinkSwitch( *Solo, Value );
        else
            qlinkSwitch( Mute, Value );
    }

};
