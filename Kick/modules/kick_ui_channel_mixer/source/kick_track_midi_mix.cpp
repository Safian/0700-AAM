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
    TrackMidiMixer::TrackMidiMixer()
        : kick_ui::IRowView( "Program View" )
    {
        setupProvider();
        setMargin( 0, 6, 2, 0 );

        mt::signals::midiInputsChanged.connect( [&]() {
            refresh();
        } );
    }

    void TrackMidiMixer::setupProvider()
    {
        provider.onRowSelected = [&]( int Row ) {
            // A row is the n-th non-audio track, not track number n: map the row to the track index.
            const auto& midiTracks = ae.getMidiTrackIndexes( aeSel.sequence );
            const auto  track      = mt::mixlogic::rowToTrack( std::vector<int>( midiTracks.begin(), midiTracks.end() ), Row );
            if( track >= 0 )
                ae.setSelectedTrack( track );
        };

        provider.numRows = [&]() -> int {
            return ae.getNumMidiTracks( aeSel.sequence );
        };

        provider.cellComponent = [&]( int Row, Component* Existing ) -> juce::Component* {
            auto midiTracks = ae.getMidiTrackIndexes( aeSel.sequence );
            if( juce::isPositiveAndBelow( Row, midiTracks.size() ) )
            {
                auto comp = dynamic_cast<TrackMidiMixerDisplay*>( Existing );
                if( comp == nullptr )
                {
                    comp = new TrackMidiMixerDisplay();
                }
                comp->setNumber( midiTracks[Row] );
                comp->setMidiPorts( _midiInPorts );
                comp->setSendToList( ae.getValidRoutableTracks( aeSel.sequence, midiTracks[Row] ) );
                comp->setData( &soloMuteData[midiTracks[Row]] );
                comp->onSoloMuteChanged = [&]() {
                    refreshSoloMute();
                };
                comp->onRouteChanged = [&, midiTracks]() {
                    auto& rowComps = getRowComponents();
                    auto  compRow{ 0 };
                    for( auto& rowComp : rowComps )
                    {
                        auto midiComp = dynamic_cast<TrackMidiMixerDisplay*>( rowComp );
                        midiComp->setSendToList( ae.getValidRoutableTracks( aeSel.sequence, midiTracks[compRow] ) );
                        ++compRow;
                    }
                };
                return comp;
            }
            return nullptr;
        };
        _initialized = true;
    }

    void TrackMidiMixer::populate( void )
    {
        if( !_initialized.load() )
            return;

        refresh();
    }

    void TrackMidiMixer::meterUpdate()
    {
        auto& comps = getRowComponents();
        for( auto& it : comps )
        {
            if( auto r = dynamic_cast<TrackMidiMixerDisplay*>( it ) )
            {
                r->meterUpdate();
            }
        }
    }

    void TrackMidiMixer::knobCommand( int knob, int value )
    {
        // Q-Link 1..4 act on the selected row (not the first visible one); an empty list does nothing.
        if( knob < contracts::DeviceControlContracts::EN_KNOB_1 || knob > contracts::DeviceControlContracts::EN_KNOB_4 )
            return;
        if( auto* display = qlinkRowDisplay<TrackMidiMixerDisplay>() )
            display->linkRotated( knob - contracts::DeviceControlContracts::EN_KNOB_1, value );
    }

    void TrackMidiMixer::refreshSoloMute()
    {
        auto& rowComps = getRowComponents();

        bool isSoloed = std::any_of( rowComps.begin(), rowComps.end(), [&]( auto& rowComp ) {
            if( auto midiComp = dynamic_cast<TrackMidiMixerDisplay*>( rowComp ) )
            {
                return soloMuteData[midiComp->getNumber()].solo;
            }
            return false;
        } );

        for( auto& comp : rowComps )
        {
            auto pComp = dynamic_cast<TrackMidiMixerDisplay*>( comp );

            auto idx = pComp->getNumber();
            pComp->setMuteToggleButton( soloMuteData[idx].solo ? false : isSoloed ? true :
                                                                                    soloMuteData[idx].mute,
                                        soloMuteData[idx].solo ? false : isSoloed ? true :
                                                                                    false );
            pComp->setSoloToggleButton( soloMuteData[idx].solo, juce::dontSendNotification );
        }
    }

    //==============================================================================
    void TrackMidiMixer::visibilityChanged()
    {
        IRowView::visibilityChanged();
        const auto& midiTracks = ae.getMidiTrackIndexes( aeSel.sequence );
        const auto  row        = mt::mixlogic::trackToRow( std::vector<int>( midiTracks.begin(), midiTracks.end() ), ae.getSelectedTrack() );
        if( row >= 0 )
            selectRow( row );
        refreshSoloMute();
    }

    //////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////    Row content - TrackMidiMixerDisplay
    //////////////////////////////////////////////////////////////////////////////////////////
    TrackMidiMixerDisplay::TrackMidiMixerDisplay()
    {
        trackName.setEditable( false );
        trackName.setValueBackgroundColour( ui::Theme::textColour( 4 ) );
        trackName.setPadding( 10 );
        trackHeader.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        trackHeader.setValueColour( ui::Theme::textColour( 1 ) );
        trackHeader.setValueJustification( juce::Justification::topLeft );

        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 13 ), midiCh, midiInput, sendTo );
        mt::utils::setTitleBackgroundColour( ui::Theme::backgroundColour( 9 ), midiCh, midiInput, sendTo );

        midiCh.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
        midiInput.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );
        sendTo.setJustificationType( juce::Justification::centredTop, juce::Justification::centred );

        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), midiCh, midiInput, sendTo );
        mt::utils::setValueColour( ui::Theme::textColour( 2 ), midiCh, midiInput, sendTo );

        mt::utils::setTitleFontHeight( 16, midiCh, midiInput, sendTo );

        srNo.setFontProperties( 36 );
        srNo.setColours( ui::Theme::textColour( 0 ), ui::Theme::backgroundColour( 6 ), ui::Theme::backgroundColour( 6 ) );
        icon.toFront( true );

        trackName.setTitleJustification( juce::Justification::centredLeft );
        trackName.setValueJustification( juce::Justification::topLeft );
        trackName.setTitleFontHeight( 24 );
        trackName.setValueFontHeight( 18 );

        midiInput.onValueChanged = [&]( const juce::String& Port ) {
            if( pTrack )
                pTrack->midiPort = Port;
        };

        levelSlider.onValueChanged = [&]( double Value ) {
            if( pTrack )
                pTrack->level = (int)Value;
        };

        midiCh.onSelectionChanged = [&]( int Index, const juce::String& Value ) {
            if( pTrack )
                pTrack->midiChannel = Index;
        };

        muteButton.onClick = [&]() {
            if( !pTrack ) return;

            if( trackData->solo )
                muteButton.setToggleState( false, juce::dontSendNotification );
            else if( muteDueToSolo )
                muteButton.setToggleState( true, juce::dontSendNotification );
            else
            {
                // Through the engine, so the mute group is honoured (the Track Mute screen does the same).
                ae.setTrackMute( seq, pTrack->index, muteButton.getToggleState() );
                trackData->mute = pTrack->mute.get();
                if( onSoloMuteChanged )
                    onSoloMuteChanged(); // other tracks of the mute group changed too
            }
        };

        // Monitor: Off -> In -> Auto -> Off (same as the Track screen, mt_track_monitor.h); "A" on the lit button = Auto.
        monitorButton.onClick = [&]() {
            if( !pTrack ) return;
            pTrack->monitor = mt::trackmonitor::next( pTrack->monitor.get() );
            updateMonitor();
        };

        soloButton.onClick = [&]() {
            ae.setTrackSolo( seq, pTrack->index, soloButton.getToggleState() );
            trackData->solo = ae.getTrackSolo( seq, pTrack->index );
            onSoloMuteChanged();
        };

        sendTo.onSelectionChanged = [&]( int Index, const juce::String& Option ) {
            if( pTrack )
            {
                /// Try To Avoid Circular
                if( Option != trackName.getText() && Index != -1 )
                {
                    pTrack->route = Option;
                    if( onRouteChanged )
                        onRouteChanged();
                }
            }
        };

        ui::themeChanged.connect( [&]() {
            armButton.setImages( "Arm_normal" );
            monitorButton.setImages( "Monitor_normal", "Monitor_active" );
            soloButton.setImages( "Solo_normal", "Solo_active" );
            muteButton.setImages( "Mute_normal", "Mute_active" );
        } );

        mt::utils::addChildComponents( this, srNo, icon, trackName, trackHeader, levelMeter, midiInput, midiCh, sendTo, levelSlider, monitorButton, soloButton, muteButton );
        // The record arm lives on the Track screen (setTrackRecordArm); the picture-only button is not shown here.
        addChildComponent( armButton );
    };

    void TrackMidiMixerDisplay::updateMonitor()
    {
        monitorMode = pTrack ? mt::trackmonitor::clamp( pTrack->monitor.get() ) : (int)mt::trackmonitor::Default;
        monitorButton.setToggleState( monitorMode != mt::trackmonitor::Off, juce::dontSendNotification );
        repaint( monitorButton.getBounds() );
    }

    void TrackMidiMixerDisplay::paintOverChildren( juce::Graphics& g )
    {
        if( monitorMode == mt::trackmonitor::Auto )
            mt::utils::renderText( g, "A", monitorButton.getBounds(), ui::Theme::textColour( 1 ), juce::Justification::centred, 18.f );
    }

    void TrackMidiMixerDisplay::setSoloToggleButton( bool shouldBeOn, bool notification )
    {
        soloButton.setToggleState( shouldBeOn, notification );
    }

    void TrackMidiMixerDisplay::setMuteToggleButton( bool shouldBeOn, bool MuteDueToSolo )
    {
        muteDueToSolo         = MuteDueToSolo;
        pTrack->muteDueToSolo = MuteDueToSolo;

        muteButton.setToggleState( shouldBeOn, false );
    }

    void TrackMidiMixerDisplay::setData( TrackSoloMuteData* Data )
    {
        trackData = Data;
    }

    void TrackMidiMixerDisplay::setNumber( int TrackNo )
    {
        seq    = ae.getSelectedSequence();
        no     = TrackNo;
        pTrack = ae.getTrackData( seq, no );

        auto midiChannelValueHandler = [&]() { midiCh.setSelected( pTrack ? (int)pTrack->midiChannel.get() : 0, juce::dontSendNotification ); };
        auto midiPortValueHandler    = [&]() { midiInput.setSelected( pTrack ? pTrack->midiPort.get() : "", juce::dontSendNotification ); };
        auto muteValueHandler        = [&]() { trackData->mute = pTrack->mute; };
        auto soloValueHandler        = [&]() { trackData->solo = pTrack->solo; };
        auto levelValueHandler       = [&]() { levelSlider.setValue( pTrack ? (double)pTrack->level.get() : 0.0, juce::dontSendNotification ); };
        auto routeHandler            = [&]() { sendTo.setSelected( pTrack ? pTrack->route.get() : "None", juce::dontSendNotification ); };
        auto nameValueHandler        = [&]() {
            trackName.setText( pTrack ? pTrack->name.get() : "", juce::dontSendNotification );
            auto pProg = ae.getProgramDataForTrack( seq, no );
            trackName.setLine2Text( pProg ? pProg->name.get() : "" );
        };

        scope.reset();
        if( pTrack )
        {
            scope = std::make_shared<mt::Kick::ScopeTracker>();

            pTrack->name.onValueChange.connect( ( nameValueHandler ), scope );
            pTrack->midiChannel.onValueChange.connect( ( midiChannelValueHandler ), scope );
            pTrack->midiPort.onValueChange.connect( ( midiPortValueHandler ), scope );
            pTrack->mute.onValueChange.connect( ( muteValueHandler ), scope );
            pTrack->solo.onValueChange.connect( ( soloValueHandler ), scope );
            pTrack->level.onValueChange.connect( ( levelValueHandler ), scope );
            pTrack->route.onValueChange.connect( ( routeHandler ), scope );
            pTrack->monitor.onValueChange.connect( [&]() { updateMonitor(); }, scope );
        }

        updateData();
    }

    void TrackMidiMixerDisplay::updateData()
    {
        if( isVisible() )
        {
            setProgramIcon( icon, mt::MTProgramType::Unknown );

            auto srNoValueHandler = [&]() { srNo.setText( juce::String( no + 1 ) ); };
            auto iconValueHandler = [&]() {
                auto pProg = ae.getProgramDataForTrack( seq, no );
                setProgramIcon( icon, pProg ? pProg->type.get() : mt::MTProgramType::Unknown );
            };

            // Force data update.
            srNoValueHandler();
            iconValueHandler();
            if( pTrack )
            {
                pTrack->name.onValueChange();
                pTrack->midiPort.onValueChange();
                pTrack->midiChannel.onValueChange();
                pTrack->mute.onValueChange();
                pTrack->solo.onValueChange();
                pTrack->level.onValueChange();
                pTrack->route.onValueChange();
                updateMonitor();
            }
        }
    }

    void TrackMidiMixerDisplay::setSendToList( const juce::Array<juce::String>& Tracks )
    {
        if( pTrack )
        {
            auto                      selSendTo = pTrack->route.get();
            std::vector<juce::String> validTracks( Tracks.begin(), Tracks.end() );
            if( ( ( std::find( Tracks.begin(), Tracks.end(), selSendTo ) ) != Tracks.end() ) && Tracks.size() >= 1 )
            {
                auto it            = std::find( Tracks.begin(), Tracks.end(), selSendTo );
                auto selectedIndex = 0;
                if( it != Tracks.end() )
                    selectedIndex = std::distance( Tracks.begin(), it );
                sendTo.setListOptions( validTracks, -1 );
                sendTo.setSelected( selectedIndex, juce::dontSendNotification );
            }
            else if( Tracks.size() )
            {
                sendTo.setListOptions( validTracks, -1 );
                sendTo.setSelected( 0, juce::dontSendNotification );
            }
        }
    }

    void TrackMidiMixerDisplay::setMidiPorts( const std::vector<juce::String>& MidiPorts )
    {
        if( pTrack )
        {
            juce::String selMidiPort = pTrack->midiPort.get();
            auto         ports       = MidiPorts;
            if( ( ( std::find( ports.begin(), ports.end(), selMidiPort ) ) != ports.end() ) && ports.size() >= 1 )
            {
                auto it            = std::find( ports.begin(), ports.end(), selMidiPort );
                auto selectedIndex = 0;
                if( it != ports.end() )
                    selectedIndex = std::distance( ports.begin(), it );
                midiInput.setListOptions( ports, selectedIndex );
            }
            else if( selMidiPort.isEmpty() && ports.size() )
                midiInput.setListOptions( ports );
        }
    }

    void TrackMidiMixerDisplay::meterUpdate()
    {
        const auto& in = ae.getTrackMeter( seq, no );
        if( in.size() )
            levelMeter.setSignalStrength( in[0].rms, in[1].rms, in[0].peak, in[1].peak );
    }

    void TrackMidiMixerDisplay::clear()
    {
        no = -1;
        trackName.setText( "", juce::dontSendNotification );
        trackName.setLine2Text( "" );
        setProgramIcon( icon, mt::MTProgramType::Unknown );

        midiInput.setSelected( 0, juce::dontSendNotification );
        midiCh.setSelected( 0, juce::dontSendNotification );
        sendTo.setSelected( 0, juce::dontSendNotification );
    }

    void TrackMidiMixerDisplay::resized()
    {
        srNo.setBoundsRelative( .001f, .0f, .044f, .982f );
        icon.setBoundsRelative( .001f, .491f, .044f, .491f );
        trackHeader.setBoundsRelative( .065f, .0734f, .1603f, .2201f );
        trackName.setBoundsRelative( .065f, .267f, .1603f, .404f );
        levelMeter.setBoundsRelative( .065f, .68f, .1603f, .2935f );
        midiInput.setBoundsRelative( .2495f, .0892f, .1188f, .6071f );
        midiCh.setBoundsRelative( .3970f, .0892f, .1188f, .6071f );
        sendTo.setBoundsRelative( .5455f, .0892f, .1188f, .6071f );
        levelSlider.setBoundsRelative( .7089f, .0892f, .1188f, .8214f );
        soloButton.setBoundsRelative( .9108f, .0892f, .0594f, .3928f );
        monitorButton.setBoundsRelative( .8465f, .5178f, .0594f, .3928f );
        muteButton.setBoundsRelative( .9108f, .5178f, .0594f, .3928f );
    }

    void TrackMidiMixerDisplay::mouseUp( const juce::MouseEvent& e )
    {
        if( e.mouseWasClicked() )
        {
            updateData();
        }
    }

    void TrackMidiMixerDisplay::visibilityChanged()
    {
        bool visible = isVisible();

        for( auto child : getChildren() )
            child->setVisible( visible );
        armButton.setVisible( false );

        if( visible )
            updateData();
    }

    void TrackMidiMixerDisplay::linkRotated( int ID, int value )
    {
        switch( ID )
        {
            case 0: midiInput.knobRotated( value ); break;
            case 1: midiCh.knobRotated( value ); break;
            case 2: levelSlider.knobRotated( mt::mixlogic::qlinkDelta( value, levelSlider.getIncrement() ) ); break;
            case 3: qlinkMuteOrSolo( muteButton, &soloButton, value ); break;
        }
    }

    juce::Rectangle<int> TrackMidiMixerDisplay::getElementBounds( int state )
    {
        switch( state )
        {
            case 1: return midiInput.getBounds(); break;
            case 2: return midiCh.getBounds(); break;
            case 3: return levelSlider.getBounds(); break;
            case 4: return muteButton.getBounds(); break;
            default: return midiInput.getBounds(); break;
        }
    }
}
