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
    // The Envelopes and LFO pages write the program model, but the drum voice never reads them: DrumVoice::attachToAndReset is
    // the only place that binds its LFO / envelope atomics to the model and nothing calls it (plus the model / atomics use
    // different ids, and the pitch / filter / LFO sections are disabled by default with no switch in the UI). A page that
    // saves values into the project and changes nothing in the sound misleads, so both are hidden until the voice is bound
    // (needs an MPC listening test: see docs/plans/screens/program-edit.md PE-02). true brings the two buttons back.
    static constexpr bool kDrumEnginePagesVisible = true;

    DrumProgramEdit::DrumProgramEdit()
        : ProgEditInterface( mt::MTProgramType::Drum )
    {
        comps.reserve( DRUM_NAV_BUTTONS );
        comps.emplace_back( std::make_unique<MasterDrumProgramEdit>( pads, pDrum ) );
        comps.emplace_back( std::make_unique<DPSampleTab>( pads, pDrum ) );
        comps.emplace_back( std::make_unique<DPLayerMix>( pads, pDrum ) );
        comps.emplace_back( std::make_unique<DPEnvelopes>( pads, pDrum ) );
        comps.emplace_back( std::make_unique<DPLfoModulation>( pads, pDrum ) );

        for( auto& it : comps )
            mt::utils::addChildComponents( this, it.get() );

        mt::utils::addAndMakeVisible( this, mainButton, samplesButton, layerMixButton );
        if( kDrumEnginePagesVisible )
            mt::utils::addAndMakeVisible( this, lfoButton, envelopesButton );
        mt::utils::addListener( this, mainButton, samplesButton, lfoButton, layerMixButton, envelopesButton );
        mt::utils::setRadioGroupID( (int)RadioGroupId::DrumProgram, mainButton, samplesButton, lfoButton, layerMixButton, envelopesButton );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), mainButton, samplesButton, layerMixButton, envelopesButton, lfoButton );
        mt::utils::setValueFontHeight( 18, mainButton, samplesButton, layerMixButton, envelopesButton, lfoButton );

        // Set default.
        pads.push_back( 0 );
        mainButton.triggerClick();

        ui::themeChanged.connect( [&]() {
            mainButton.setToggleButtonText( "Main", "Main" );
            samplesButton.setToggleButtonText( "Samples", "Samples" );
            layerMixButton.setToggleButtonText( "Layer Mix", "Layer Mix" );
            envelopesButton.setToggleButtonText( "Envelopes", "Envelopes" );
            lfoButton.setToggleButtonText( "LFO", "LFO" );
        } );
    }

    void DrumProgramEdit::refresh()
    {
        if( pDrum )
            dynamic_cast<DrumInterface*>( comps[tIndex].get() )->refresh();
    }

    void DrumProgramEdit::handleCommand( CPktMgr::HWMsg_t& f_HWMsg )
    {
        if( pDrum )
            dynamic_cast<DrumInterface*>( comps[tIndex].get() )->handleCommand( f_HWMsg );
    }

    void DrumProgramEdit::resized()
    {
        auto b = getLocalBounds();

        mainButton.setBoundsRelative( .0099f, .022f, .1896f, .096f );
        samplesButton.setBoundsRelative( .2078f, .022f, .1896f, .096f );
        layerMixButton.setBoundsRelative( .4051f, .022f, .1896f, .096f );
        if( kDrumEnginePagesVisible )
        {
            envelopesButton.setBoundsRelative( .6028f, .022f, .1896f, .096f );
            lfoButton.setBoundsRelative( .8004f, .022f, .1896f, .096f );
        }

        for( auto& it : comps )
            it->setBoundsRelative( .0097f, .139f, .982f, .842f );
    }

    void DrumProgramEdit::buttonClicked( juce::Button* button )
    {
        ispadFx = false;
        if( button == &mainButton )
        {
            isDefault = false;
            tIndex    = 0;
        }
        else if( button == &samplesButton )
        {
            isDefault = false;
            tIndex    = 1;
        }
        else if( button == &layerMixButton )
        {
            tIndex    = 2;
            isDefault = true;
        }
        else if( button == &envelopesButton )
        {
            tIndex    = 3;
            isDefault = true;
        }
        else if( button == &lfoButton )
        {
            tIndex    = 4;
            isDefault = true;
        }
        for( auto& comp : comps )
            comp->setVisible( false );
        comps[tIndex]->setVisible( true );
        repaint();
    }

    void DrumProgramEdit::visibilityChanged()
    {
        auto visible = isVisible();
        if( visible )
        {
            pDrum = pIProg ? dynamic_cast<mt::MTDrumProgram*>( pIProg ) : nullptr;
            refresh();
        }
        for( size_t i = 0; i < comps.size(); ++i ) // only the chosen tab (the Main tab used to stay on top of any other tab)
            comps[i]->setVisible( visible && (int)i == tIndex );
    }

    DrumHeader::DrumHeader()
    {
        mt::utils::addAndMakeVisible( this, pad, flattenPads, convertToKG, editZone );

        pad.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
        pad.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        pad.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );

        ui::themeChanged.connect( [&]() {
            flattenPads.setImages( "PgmEdit_FP" );
            editZone.setImages( "PgmEdit_EZ" );
            convertToKG.setImages( "PgmEdit_ConvertKG" );
        } );
    }

    void DrumHeader::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 2 ) );
    }

    void DrumHeader::resized()
    {
        auto b = getLocalBounds();
        pad.setBounds( b.removeFromRight( 94 ).reduced( 10, 16 ) );
        convertToKG.setBounds( b.removeFromRight( 44 ).reduced( 0, 16 ) );
        editZone.setBounds( b.removeFromRight( 54 ).reduced( 5, 16 ) );
        flattenPads.setBounds( b.removeFromRight( 44 ).reduced( 0, 16 ) );
    }
}
