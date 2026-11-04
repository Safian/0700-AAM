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

namespace kick_ui
{
    DPFilter::DPFilter( std::vector<int>& Pads, mt::MTDrumProgram*& PDrum )
        : pads( Pads ), pDrum( PDrum ), KickGrid( 1, 3 )
    {
        setRowTitles( { "FILTER" } );

        addComponent( filterType, 0, 0 );
        addComponent( cutoff, 0, 1 );
        addComponent( resonance, 0, 2 );

        filterType.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        filterType.setJustificationType( juce::Justification::centred );

        cutoff.showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );
        resonance.showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );
        cutoff.setRange( 20.0, 20000.0, 1.0 );
        cutoff.setSkewFactorFromMidPoint( 1000.0 ); // the voice maps the cutoff through the same kind of curve (Filter::freqRange)
        resonance.setRange( 0.0, 127.0, 1.0 );
        cutoff.setDoubleClickReturnValue( true, 20000, 0 );
        resonance.setDoubleClickReturnValue( true, 10, 0 );

        filterType.onSelectionChanged = [&]( int Index, const juce::String& ) {
            if( pDrum && pads.size() )
                pDrum->setFilterType( pads[0], Index );
        };

        cutoff.onValueChanged = [&]( double Value ) {
            if( pDrum && pads.size() )
                pDrum->setFilterCutoff( pads[0], (int)Value );
        };

        resonance.onValueChanged = [&]( double Value ) {
            if( pDrum && pads.size() )
                pDrum->setFilterResonance( pads[0], (int)Value );
        };
    }

    void DPFilter::refresh()
    {
        if( pDrum && pads.size() )
        {
            // No notification: showing the page must not write the model.
            filterType.setSelected( pDrum->getFilterType( pads[0] ), juce::dontSendNotification );
            cutoff.setValue( pDrum->getFilterCutoff( pads[0] ), juce::dontSendNotification );
            resonance.setValue( pDrum->getFilterResonance( pads[0] ), juce::dontSendNotification );
        }
    }

    void DPFilter::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            pads[0]     = padNum + mt::getAudioInterface().getBankOffset();
            refresh();
        }
    }

    void DPFilter::visibilityChanged()
    {
        if( isVisible() )
            refresh();
    }
}
