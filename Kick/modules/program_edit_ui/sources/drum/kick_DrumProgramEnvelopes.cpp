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

namespace kick_ui
{

    DPEnvelopes::DPEnvelopes( std::vector<int>& Pads, mt::MTDrumProgram*& PDrum )
        : pads( Pads ), pDrum( PDrum ), KickGrid( 3, 7 )
    {
        setRowTitles( { "AMPLITUDE", "FILTER", "PITCH" } );

        // Controls shown for an envelope type. DEPTH of the pitch / filter envelope shows for every type: the voice applies the
        // depth whatever the type is (an AD pitch fall - the 808 kick - had no way to set its depth).
        showFor = [&]( int row, int type ) {
            h[row].setVisible( type >= 2 && type <= 4 );
            s[row].setVisible( type == 1 || type == 3 || type == 4 );
            dep[row].setVisible( row != (int)mt::AMPLITUDE || type == 1 || type == 4 );
        };

        for( auto i = 0; i < 3; ++i )
        {
            //env[i].showTitle( true );
            env[i].setTitle( "TYPE" );
            env[i].setListOptions( { "AD", "ADSR", "AHD", "AHDS", "AHDSR" } );
            env[i].setJustificationType( juce::Justification::centred );

            a[i].setTitle( "ATTACK" );
            h[i].setTitle( "HOLD" );
            d[i].setTitle( "DECAY" );
            s[i].setTitle( "SUSTAIN" );
            dep[i].setTitle( "DEPTH" );

            a[i].showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );
            h[i].showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );
            d[i].showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );
            s[i].showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );
            dep[i].showTitleAndValue( true, juce::Slider::TextBoxAbove, juce::Justification::centred );

            a[i].setRange( 0.0, 127.0, 1.0 );
            h[i].setRange( 0.0, 127.0, 1.0 );
            d[i].setRange( 0.0, 127.0, 1.0 );
            s[i].setRange( 0.0, 127.0, 1.0 );
            dep[i].setRange( 0.0, 127.0, 0.01 );

            a[i].setDoubleClickReturnValue( true, 0, 0 );
            h[i].setDoubleClickReturnValue( true, 0, 0 );
            d[i].setDoubleClickReturnValue( true, 0, 0 );
            s[i].setDoubleClickReturnValue( true, 127, 0 );
            dep[i].setDoubleClickReturnValue( true, 0, 0 );

            addComponent( env[i], i, 1 );
            addComponent( a[i], i, 2 );
            addComponent( h[i], i, 3 );
            addComponent( d[i], i, 4 );
            addComponent( s[i], i, 5 );
            addComponent( dep[i], i, 6 );

            showFor( i, 0 );
            env[i].onSelectionChanged = [&, i]( int Index, const juce::String& Option ) {
                showFor( i, Index );
                if( pDrum )
                    pDrum->setEnvType( pads[0], toEnum<mt::ADSRType>( Index ), toEnum<mt::EnvelopeType>( i ) );
            };

            a[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pDrum )
                    pDrum->setEnvelopeAttack( pads[0], toEnum<mt::EnvelopeType>( i ), Value );
            };

            h[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pDrum )
                    pDrum->setEnvelopeHold( pads[0], toEnum<mt::EnvelopeType>( i ), Value );
            };

            d[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pDrum )
                    pDrum->setEnvelopeDecay( pads[0], toEnum<mt::EnvelopeType>( i ), Value );
            };

            s[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pDrum )
                    pDrum->setEnvelopeSustain( pads[0], toEnum<mt::EnvelopeType>( i ), Value );
            };

            dep[i].onValueChanged = [&, i]( juce::int64 Value ) {
                if( pDrum )
                    pDrum->setEnvelopeDepth( pads[0], toEnum<mt::EnvelopeType>( i ), Value );
            };
        }
    }

    void DPEnvelopes::refresh()
    {
        if( pDrum && pads.size() )
            refreshPads();
    }

    void DPEnvelopes::refreshPads()
    {
        if( pDrum && pads.size() )
        {
            for( auto index = 0; index < 3; ++index )
            {
                // No notification: showing the page must not write the model (modified flag, undo step) on every pad change.
                env[index].setSelected( pDrum->getEnvType( pads[0], toEnum<mt::EnvelopeType>( index ) ), juce::dontSendNotification );
                showFor( index, env[index].getSelectedIndex() );
                a[index].setValue( pDrum->getEnvelopeAttack( pads[0], toEnum<mt::EnvelopeType>( index ) ), juce::dontSendNotification );
                h[index].setValue( pDrum->getEnvelopeHold( pads[0], toEnum<mt::EnvelopeType>( index ) ), juce::dontSendNotification );
                d[index].setValue( pDrum->getEnvelopeDecay( pads[0], toEnum<mt::EnvelopeType>( index ) ), juce::dontSendNotification );
                s[index].setValue( pDrum->getEnvelopeSustain( pads[0], toEnum<mt::EnvelopeType>( index ) ), juce::dontSendNotification );
                dep[index].setValue( pDrum->getEnvelopeDepth( pads[0], toEnum<mt::EnvelopeType>( index ) ), juce::dontSendNotification );
            }
        }
    }

    void DPEnvelopes::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            pads[0]     = padNum + mt::getAudioInterface().getBankOffset();
            refresh();
        }
    }

    void DPEnvelopes::visibilityChanged()
    {
        auto visible = isVisible();
        if( visible )
            refresh();
    }
}
