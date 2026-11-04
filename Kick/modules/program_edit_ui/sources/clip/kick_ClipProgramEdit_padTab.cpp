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

    ClipPadTab::ClipPadTab( int& SelectedPad, mt::MTClipProgram* const& PClip )
        : selectedPad( SelectedPad ), pClip( PClip )
    {
        mt::utils::addAndMakeVisible( this, nameLabel, start, end, semi, fine, muteGroup, wrap, reverse, fade, wav );

        nameLabel.setPadding( 10 );
        nameLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        muteGroup.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        muteGroup.setJustificationType( juce::Justification::centred );

        start.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        end.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        semi.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        fine.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );

        wrap.setValueColour( ui::Theme::textColour( 1 ) );
        wrap.setValueFontHeight( 18 );
        reverse.setValueColour( ui::Theme::textColour( 1 ) );
        reverse.setValueFontHeight( 18 );
        fade.setValueColour( ui::Theme::textColour( 1 ) );
        fade.setValueFontHeight( 18 );

        semi.setRange( SEMI::MIN, SEMI::MAX, SEMI::STEP );
        fine.setRange( FINE::MIN, FINE::MAX, FINE::STEP );

        start.onValueChanged = [&]( juce::int64 Value ) {
            if( auto p = pClip )
            {
                if( end.getValue() >= Value )
                {
                    if( auto pSlice = p->getLayerSourceSlice( selectedPad, 0 ) )
                        pSlice->loopStart = Value;
                }
                else
                    start.setValue( end.getValue(), juce::dontSendNotification );
            }
        };

        end.onValueChanged = [&]( juce::int64 Value ) {
            if( auto p = pClip )
            {
                if( start.getValue() <= Value )
                {
                    if( auto pSlice = p->getLayerSourceSlice( selectedPad, 0 ) )
                        pSlice->loopEnd = Value;
                }
                else
                    end.setValue( start.getValue(), juce::dontSendNotification );
            }
        };

        semi.onValueChanged = [&]( juce::int64 Value ) {
            if( auto p = pClip )
                if( auto pSrc = p->getLayerSourceSlice( selectedPad, 0 ) )
                    pSrc->semi = Value;
        };

        fine.onValueChanged = [&]( juce::int64 Value ) {
            if( auto p = pClip )
                if( auto pSrc = p->getLayerSourceSlice( selectedPad, 0 ) )
                    pSrc->fine = Value;
        };

        reverse.onClick = [&]() {
            if( auto p = pClip )
                if( auto pSrc = p->getLayerSourceSlice( selectedPad, 0 ) )
                    pSrc->isReversed = reverse.getToggleState();
        };

        fade.onClick = [&]() {
            if( auto p = pClip )
            {
                if( auto pSrc = p->getLayerSourceSlice( selectedPad, 0 ) )
                    pSrc->isCrossFade = fade.getToggleState();
            }
        };

        wrap.onClick = [&]() {
            if( auto p = pClip )
                if( auto pSrc = p->getLayerSourceSlice( selectedPad, 0 ) )
                    pSrc->isWarp = wrap.getToggleState();
        };

        muteGroup.onSelectionChanged = [&]( int Value, const juce::String& ) {
            if( auto p = pClip )
                if( auto clipData = p->getClip( selectedPad ) )
                    clipData->muteGroup = Value - 1; // list: Off, 1..32 -> -1..32
        };

        ui::themeChanged.connect( [&]() {
            wrap.setToggleButtonText( "Wrap", "Wrap" );
            reverse.setToggleButtonText( "Reverse", "Reverse" );
            fade.setToggleButtonText( "Fade", "Fade" );
        } );
    }

    void ClipPadTab::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            auto padNum = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            selectedPad = padNum + mt::getAudioInterface().getBankOffset();
            refresh();
        }
    }

    void ClipPadTab::refresh()
    {
        auto p = pClip;
        if( !p )
            return;

        if( auto clipData = p->getClip( selectedPad ) )
        {
            auto pSlice = p->getLayerSourceSlice( selectedPad, 0 );

            wav.setSource( pSlice ? pSlice->getSourceBuffer() : EMPTY::SampleBuffer, pSlice ? pSlice->getSourceSampleRate() : 0 );
            {
                // Range = the whole source, value = the slice's own start / end (the range used to be the current start / end,
                // so a narrowed slice could never be widened again).
                const int len = pSlice ? (int)pSlice->getSourceLength() : 0;
                const int lo  = pSlice ? pSlice->getStart() : 0;
                const int rawEnd = pSlice ? pSlice->getEnd() : 1;
                const int hi     = rawEnd < 0 ? len : rawEnd; // -1 = the whole source
                start.setValueBounds( 0, juce::jmax( len, hi, 1 ), 0 );
                end.setValueBounds( 0, juce::jmax( len, hi, 1 ), juce::jmax( len, hi, 1 ) );
                start.setValue( lo, juce::dontSendNotification );
                end.setValue( hi, juce::dontSendNotification );
            }

            mt::utils::setEnabled( pSlice, start, end, semi, fine, muteGroup, wrap, reverse, fade );

            semi.setValue( pSlice ? pSlice->semi() : 0, juce::dontSendNotification );
            fine.setValue( pSlice ? pSlice->fine() : 0, juce::dontSendNotification );
            reverse.setToggleState( pSlice ? pSlice->isReversed() : false, juce::dontSendNotification );
            wrap.setToggleState( pSlice ? pSlice->isWarp() : false, juce::dontSendNotification );
            fade.setToggleState( pSlice ? pSlice->isCrossFade() : false, juce::dontSendNotification );

            muteGroup.setSelected( (int)( clipData ? (int)clipData->muteGroup.get() : MUTEGROUP::DEFAULT ) + 1, juce::dontSendNotification ); // Off = -1 is index 0
            nameLabel.setText( "Pad " + juce::String( selectedPad + 1 ) + ( pSlice ? "  " + juce::File( pSlice->sourceFile.get() ).getFileNameWithoutExtension() : juce::String() ), juce::dontSendNotification );
        }
    }

    void ClipPadTab::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 8, 10, 1004, 396 }, ui::Theme::backgroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, { 8, 10, 1004, 192 }, ui::Theme::backgroundColour( 11 ) );
    }

    void ClipPadTab::resized()
    {
        wav.setBoundsRelative( .0097f, .03f, .978f, .472f );

        nameLabel.setBoundsRelative( .029f, .578f, .156f, .107f );

        start.setBoundsRelative( .235f, .529f, .117f, .156f );
        end.setBoundsRelative( .433f, .529f, .117f, .156f );
        semi.setBoundsRelative( .631f, .529f, .117f, .156f );
        fine.setBoundsRelative( .828f, .529f, .117f, .156f );

        muteGroup.setBoundsRelative( .235f, .757f, .117f, .156f );

        wrap.setBoundsRelative( .433f, .806f, .117f, .1111f );
        reverse.setBoundsRelative( .631f, .806f, .117f, .1111f );
        fade.setBoundsRelative( .828f, .806f, .117f, .1111f );
    }

    void ClipPadTab::visibilityChanged()
    {
        if( isVisible() )
            refresh();
    }

}
