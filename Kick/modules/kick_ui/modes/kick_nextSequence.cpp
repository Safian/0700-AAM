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
    KickNextSequenceDisplay::KickNextSequenceDisplay( SequenceData data )
    {
        mt::utils::addAndMakeVisible( this, srNo, seqName, length, bpm );
        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), srNo, seqName, length, bpm );
        mt::utils::setValueJustification( juce::Justification::centred, srNo, seqName, length, bpm );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), srNo, seqName, length, bpm );
        mt::utils::setValueFontHeight( 17, srNo, seqName, length, bpm );

        srNo.setEditable( false );
        length.setEnabled( false );
        bpm.setEnabled( false );

        seqData = data;
        updateData( seqData );
    }

    void KickNextSequenceDisplay::updateData( SequenceData Data )
    {
        seqData = Data;
        srNo.setText( seqData.srNo, juce::dontSendNotification );
        seqName.setText( seqData.name, juce::dontSendNotification );
        bpm.setValue( seqData.bpm, juce::dontSendNotification );
        length.setValue( seqData.length, juce::dontSendNotification );
    }

    void KickNextSequenceDisplay::resized()
    {
        srNo.setBoundsRelative( .018f, .077f, .12f, .846f );
        seqName.setBoundsRelative( .15f, .077f, .42f, .846f );
        length.setBoundsRelative( .581f, .077f, .2f, .846f );
        bpm.setBoundsRelative( .79f, .077f, .2f, .846f );
    }

    KickNextSequenceContent::KickNextSequenceContent()
        : ae( mt::getAudioInterface() )
        , pads( std::make_unique<ui::KickPadButtonGroupComponent>( ui::KickPadButton::PadButtonType::with1Row ) )
        , playingSequence( ae.getSelectedSequence() )
    {
        ae.addListener( this );

        tableView.reset( new ui::KickTableView{ "Sequences", provider } );

        mt::utils::addChildComponents( this, tableView.get(), pads.get(), nextBar, sudden, hold, clear, toSong, srNo, sequence, length, bpm );

        tableView->setRowSpacing( 1 );
        tableView->setRowHeight( 52 );
        tableView->setMargin( 0, 0, 2, 0 );
        tableView->setRowBackgroundColour( ui::Theme::backgroundColour( 3 ) );
        tableView->setSelectedRowBackgroundColour( ui::Theme::backgroundColour( 0 ) );
        tableView->setAutoScrollToSelection( true );

        clear.setEnabled( false );
        nextBar.setEnabled( false );
        pads->setActive( -1, false );
        pads->setHighlight( -1 );
        pads->enableBank( true );
        pads->setPadButtonSpacer( 12 );
        pads->setFocusOutline( false );

        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), srNo, sequence, length, bpm );
        mt::utils::setValueJustification( juce::Justification::centred, srNo, sequence, length, bpm );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), nextBar, sudden, clear, hold, toSong );
        mt::utils::setValueFontHeight( 18, srNo, sequence, length, bpm, nextBar, sudden, clear, hold, toSong );

        provider.numRows = [&]() {
            return ae.getNumSequences();
        };

        provider.cellComponent = [&]( int Row, juce::Component* Existing ) -> juce::Component* {
            auto comp = dynamic_cast<KickNextSequenceDisplay*>( Existing );
            if( !juce::isPositiveAndBelow( Row, ae.getNumSequences() ) )
                return nullptr;
            auto data = ae.getSequenceData( Row );
            if( data == nullptr )
                return nullptr;

            seqData.srNo   = juce::String( Row + 1 );
            seqData.name   = data->name.get();
            seqData.length = data->length.get();
            seqData.bpm    = data->tempo;
            seqData.index  = Row;

            if( comp == nullptr )
                comp = new KickNextSequenceDisplay( seqData );
            comp->updateData( seqData );
            return comp;
        };

        provider.onRowSelected = [&]( int Index ) {
            selectSequence( Index );
            ae.setSelectedSequence( Index );
            auto startID = pads->getBankOffset();
            auto endID   = startID + _NUM_PADS_IN_COMPONENT;
            if( Index >= startID && Index < endID )
            {
                //current bank : No need to change
            }
            else
            {
                auto bankIndex = Index / _NUM_PADS_IN_COMPONENT;
                ae.setBank( pads->getBank( bankIndex ) );
            }
            refreshClear();
        };

        pads->OnButtonClicked = [&]( int Id ) {
            if( isPlaying )
            {
                auto startId = ae.getBankOffset();
                auto endId   = startId + 16;
                if( Id >= startId && Id < endId )
                {
                    if( hold.getToggleState() )
                        holdSelect( Id ); // Hold: the pad is only selected, nothing is queued
                    else
                    {
                        selectSequence( Id );
                        ae.setNextSequence( nextSequence );
                    }
                    refreshClear();
                }
            }
            else
            {
                tableView->selectRow( Id );
            }
        };

        hold.onClick = [&]() {
            const bool on   = hold.getToggleState();
            const auto next = sequence_logic::setHold( state(), on );
            if( on && nextSequence >= 0 && next.next < 0 )
            {
                pads->removeBreathing( nextSequence );
                setPadBlinking( nextSequence, contracts::DeviceControlContracts::StateOffBlink, contracts::DeviceControlContracts::PadColourOrange );
                nextSequence = -1;
            }
            clearHeld();
            if( on )
                ae.setNextSequence( -1 ); // the current sequence repeats until Hold is released
            refreshClear();
        };

        // Sudden: switch to the selected sequence (the held selection under Hold, else the queued one) at once.
        sudden.onClick = [&]() {
            const int target = isPlaying ? sequence_logic::switchTarget( state() ) : -1;
            if( target >= 0 )
            {
                const int old = playingSequence;
                clearHeld();
                ae.setNextSequence( -1 );
                ae.playSequence( target );
                changePlayingPad( old, target );
            }
            sudden.setToggleState( false, juce::dontSendNotification );
            refreshClear();
        };

        // Next Bar: switch to the selected sequence at the start of the next bar.
        nextBar.onClick = [&]() {
            if( isPlaying )
            {
                const int target = sequence_logic::switchTarget( state() );
                if( target >= 0 )
                {
                    ae.setNextSequence( target );
                    ae.setNextBarFlag( true );
                }
            }
            nextBar.setToggleState( false, juce::dontSendNotification );
        };

        // Clear: delete the selected sequence from the playlist (not the one that is playing); asks first, UNDO brings it back.
        clear.onClick = [&]() {
            clear.setToggleState( false, juce::dontSendNotification );
            deleteSelectedSequence();
        };

        toSong.onClick = [&]() {
            toSong.setToggleState( false, juce::dontSendNotification );
            copyToSong();
        };

        nextPlayConn = mt::signals::onNextSequencePlay.connect( [&]( int Sequence ) {
            juce::MessageManager::callAsync( [this, Sequence] {
                // The switch happened: the engine's own queue must not replay it at the end of the sequence.
                if( nextSequence == Sequence || nextSequence < 0 )
                    ae.setNextSequence( -1 );
                changePlayingPad( playingSequence, Sequence );
                refreshClear();
            } );
        } );

        // The transport state comes from the engine, not from this screen's own buttons: STOP on another
        // screen or a song / looper start is picked up here too.
        playStateConn = mt::signals::playStateChanged.connect( [this]( mt::TransportState ) {
            juce::MessageManager::callAsync( [this] { syncTransport(); } );
        } );

        nextSequence.onValueChanged = [&]( int ) { refreshClear(); };
        themeConn = ui::themeChanged.connect( [&]() {
            nextBar.setToggleButtonText( "Next Bar", "Next Bar" );
            sudden.setToggleButtonText( "Sudden", "Sudden" );
            clear.setToggleButtonText( "Clear", "Clear" );
            hold.setToggleButtonText( "Hold", "Hold" );
            toSong.setToggleButtonText( "Song", "Song" );
        } );

        bankConn = mt::signals::bankChanged.connect( [&]( int Index, juce::String Bank ) {
            if( kick_ui ::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuNextSequence )
            {
                pads->setBank( Index );
                if( pads->isBreathing( playingSequence ) ) //while not Playing
                    setPadBlinking( playingSequence, contracts::DeviceControlContracts::PadButtonState::StateOffBlink, contracts::DeviceControlContracts::PadColourTrans );
                else
                    setPadBlinking( nextSequence, contracts::DeviceControlContracts::PadButtonState::StateOffBlink, contracts::DeviceControlContracts::PadColourTrans );
                updateData();
                restoreData();
            }
        } );
    }

    KickNextSequenceContent::~KickNextSequenceContent()
    {
        nextPlayConn.disconnect();
        playStateConn.disconnect();
        themeConn.disconnect();
        bankConn.disconnect();
        ae.removeListener( this );
        tableView.reset();
        pads.reset();
    }

    void KickNextSequenceContent::selectSequence( int Index ) //engine side index - 128
    {
        if( isPlaying )
        {
            const int before = nextSequence;
            const int after  = sequence_logic::queueNext( { playingSequence, before, true }, Index ).next;

            // turn off old next seq
            if( before >= 0 && before != after )
            {
                pads->removeBreathing( before );
                pads->setBorderColor( before, ui::Theme::outlineColour( 1 ), 1, juce::dontSendNotification );
                setPadBlinking( before, contracts::DeviceControlContracts::PadButtonState::StateOffBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::PadColourOrange );
            }
            // turn on new next seq (pressing the playing sequence or the queued one again changes nothing)
            if( after >= 0 && after != before )
            {
                pads->setBreathing( after );
                setPadBlinking( after, contracts::DeviceControlContracts::PadButtonState::StateOnBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::padColourlightGreen1, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::PadColourOrange );
            }
            nextSequence = after;
        }

        else if( !isPlaying )
        {
            if( playingSequence == Index )
            {
                //Don't do anything
            }

            if( nextSequence >= 0 )
            {
                pads->setBorderColor( nextSequence, ui::Theme::outlineColour( 1 ), 1, juce::dontSendNotification ); //Orange
                pads->removeBreathing( nextSequence );
                setPadBlinking( nextSequence, contracts::DeviceControlContracts::StateOffBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::PadColourOrange );

                pads->setBreathing( Index );
                setPadBlinking( Index, contracts::DeviceControlContracts::DeviceControlContracts::StateOnBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::padColourlightGreen1, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::PadColourOrange );

                playingSequence = Index;
                nextSequence    = -1;
            }

            if( playingSequence != Index && nextSequence < 0 )
            {
                pads->setBorderColor( playingSequence, ui::Theme::outlineColour( 1 ), 1, juce::dontSendNotification ); //Orange
                pads->removeBreathing( playingSequence );
                setPadBlinking( playingSequence, contracts::DeviceControlContracts::StateOffBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::PadColourOrange );

                pads->setBreathing( Index );
                setPadBlinking( Index, contracts::DeviceControlContracts::DeviceControlContracts::StateOnBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::padColourlightGreen1, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::PadColourOrange );

                playingSequence = Index;
            }
        }
    }

    void KickNextSequenceContent::changePlayingPad( int OldPlayingPad, int NewPlayingPad )
    {
        pads->setBorderColor( OldPlayingPad, ui::Theme::outlineColour( 1 ), 1, juce::dontSendNotification ); //Orange
        setPadLED( OldPlayingPad, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::PadColourOrange );

        pads->removeBreathing( NewPlayingPad );
        pads->setBorderColor( NewPlayingPad, ui::Theme::selectionColour( 2 ), 1, juce::dontSendNotification ); //Green
        setPadBlinking( NewPlayingPad, contracts::DeviceControlContracts::StateOffBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 );

        clearHeld();
        playingSequence = NewPlayingPad; //updating the playing index
        nextSequence    = -1;

        tableView->selectRow( NewPlayingPad, juce::dontSendNotification );
        ae.setSelectedSequence( NewPlayingPad ); //update the engine sequence
    }

    void KickNextSequenceContent::clearNextSequence()
    {
        if( isPlaying )
        {
            pads->removeBreathing( nextSequence ); //orange
            setPadBlinking( nextSequence, contracts::DeviceControlContracts::StateOffBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::PadColourOrange );
            nextSequence = -1;
        }
        else
        {
            playingSequence = sequence_logic::clearNext( { playingSequence, nextSequence, false }, ae.getSelectedSequence() ).playing;

            pads->removeBreathing( nextSequence ); //orange
            setPadBlinking( nextSequence, contracts::DeviceControlContracts::StateOffBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::PadColourOrange );

            pads->setBreathing( playingSequence );
            setPadBlinking( playingSequence, contracts::DeviceControlContracts::DeviceControlContracts::StateOnBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::padColourlightGreen1, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );

            nextSequence = -1;
        }
    }

    void KickNextSequenceContent::startPlaying( bool FromBeginning, bool Engine )
    {
        pads->removeBreathing( playingSequence );
        pads->setBorderColor( playingSequence, ui::Theme::selectionColour( 2 ), 1, juce::dontSendNotification ); //Green
        setPadBlinking( playingSequence, contracts::DeviceControlContracts::StateOffBlink, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 );
        setPadLED( playingSequence, contracts::DeviceControlContracts::DeviceControlContracts::StateOn, contracts::DeviceControlContracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 );

        if( nextSequence >= 0 )
        {
            pads->setBreathing( nextSequence );
            setPadBlinking( nextSequence, contracts::DeviceControlContracts::StateOnBlink, contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
        }
        if( Engine )
            ae.setSequencePlay( playingSequence, FromBeginning, true ); // PLAY continues, PLAY START restarts: as on every other screen
    }

    void KickNextSequenceContent::stopPlaying( bool Engine )
    {
        if( nextSequence >= 0 )
        {
            pads->setBorderColor( playingSequence, ui::Theme::outlineColour( 1 ), 1, juce::dontSendNotification ); //Orange
            setPadLED( playingSequence, contracts::DeviceControlContracts::StateOn, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
        }
        else
        {
            pads->setBorderColor( playingSequence, ui::Theme::outlineColour( 1 ), 1, juce::dontSendNotification ); //Orange - for toggeling
            pads->setBreathing( playingSequence );
            setPadBlinking( playingSequence, contracts::DeviceControlContracts::StateOnBlink, contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
        }
        if( Engine )
            ae.setSequencePause();
    }

    void KickNextSequenceContent::restoreData()
    {
        const int oldNext = nextSequence;
        auto      r       = sequence_logic::restore( { playingSequence, oldNext, isPlaying }, ae.isSequencePlaying(), ae.getSelectedSequence() );
        playingSequence   = r.state.playing;
        isPlaying         = r.state.isPlaying;

        // A queue that cannot exist any more (the engine stopped while this screen was away or a bank was
        // changed after STOP) is dropped; it used to abort the application here.
        if( oldNext >= 0 && r.state.next < 0 )
        {
            pads->removeBreathing( oldNext );
            setPadBlinking( oldNext, contracts::DeviceControlContracts::StateOffBlink, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
            nextSequence = -1;
        }
        if( r.clearEngineNext )
            ae.setNextSequence( -1 );

        if( isPlaying )
        {
            pads->removeBreathing( playingSequence );
            pads->setBorderColor( playingSequence, ui::Theme::selectionColour( 2 ), 1, juce::dontSendNotification ); //Green
            setPadLED( playingSequence, contracts::DeviceControlContracts::StateOn, contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 );

            if( nextSequence >= 0 )
            {
                pads->setBreathing( nextSequence );
                setPadBlinking( nextSequence, contracts::DeviceControlContracts::StateOnBlink, contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
            }
        }
        else
        {
            pads->setBreathing( playingSequence );
            setPadBlinking( playingSequence, contracts::DeviceControlContracts::StateOnBlink, contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
        }

        nextBar.setEnabled( isPlaying );
        tableView->setInterceptsMouseClicks( !isPlaying, !isPlaying );
        tableView->selectRow( playingSequence, juce::dontSendNotification );
        ae.setSelectedSequence( playingSequence );
        refreshClear();
        if( onPlayingChanged )
            onPlayingChanged( isPlaying );
    }

    sequence_logic::NextState KickNextSequenceContent::state() const
    {
        return { playingSequence, nextSequence, isPlaying, hold.getToggleState(), heldSequence };
    }

    // Hold is on and a pad is pressed: it is selected (border), not queued.
    void KickNextSequenceContent::holdSelect( int Index )
    {
        const int before = heldSequence;
        heldSequence     = sequence_logic::pressPad( state(), Index ).held;
        if( before >= 0 && before != heldSequence )
            pads->setBorderColor( before, ui::Theme::outlineColour( 1 ), 1, juce::dontSendNotification );
        if( heldSequence >= 0 )
            pads->setBorderColor( heldSequence, ui::Theme::selectionColour( 1 ), 2, juce::dontSendNotification );
    }

    void KickNextSequenceContent::clearHeld()
    {
        if( heldSequence >= 0 )
            pads->setBorderColor( heldSequence, ui::Theme::outlineColour( 1 ), 1, juce::dontSendNotification );
        heldSequence = -1;
    }

    // Clear (delete the selected sequence) and Song (copy the playlist) are only offered when they can act.
    void KickNextSequenceContent::refreshClear()
    {
        const int num = ae.isProjectLoaded() ? ae.getNumSequences() : 0;
        clear.setEnabled( sequence_logic::clearTarget( state(), num ) >= 0 );
        toSong.setEnabled( !isPlaying && num > 0 );
    }

    void KickNextSequenceContent::deleteSelectedSequence()
    {
        const int target = sequence_logic::clearTarget( state(), ae.getNumSequences() );
        if( target < 0 )
            return;
        auto* sd = ae.getSequenceData( target );
        if( sd == nullptr )
            return;
        auto* p = new ui::FilePopup{ "Delete sequence \"" + sd->name.get() + "\" from the playlist?", false, false };
        mt::utils::showOptions(
            "WARNING!", p, [this, p, target]( int R ) {
                if( R == UI::APPLY && juce::isPositiveAndBelow( target, ae.getNumSequences() ) )
                {
                    const int  before = ae.getNumSequences();
                    const auto after  = sequence_logic::afterDelete( state(), target, before );
                    if( nextSequence == target || heldSequence == target )
                        ae.setNextSequence( -1 ); // the engine must not keep a queue pointing at a deleted sequence
                    clearHeld();
                    ae.deleteSequence( target );
                    playingSequence = after.playing;
                    nextSequence    = after.next;
                    ae.setSelectedSequence( playingSequence );
                    updateData();
                    restoreData();
                    AppStatusbar::Instance().offerUndo( "Sequence deleted", [this]() {
                        ae.performUndo();
                        updateData();
                        restoreData();
                    } );
                }
                delete p;
            },
            ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
    }

    // "Song" (stopped): append every sequence of the playlist as a step (repeat 1) to the chosen or a new song. One UNDO step.
    void KickNextSequenceContent::copyToSong()
    {
        const int num = ae.getNumSequences();
        if( isPlaying || num <= 0 )
            return;
        auto* dlg = new NextSequenceSongDialog( ae.getSongsList() );
        mt::utils::showOptions( dlg->getName(), dlg, [this, dlg, num]( int R ) {
            if( R == UI::APPLY )
            {
                juce::String name = dlg->getSong();
                bool         create = name.isEmpty();
                if( create )
                {
                    const auto existing = ae.getSongsList();
                    for( int i = 1; i < 1000 && name.isEmpty(); ++i )
                    {
                        const auto candidate = "Song " + juce::String( i );
                        bool       used      = false;
                        for( auto& e : existing )
                            used = used || e.equalsIgnoreCase( candidate );
                        if( !used )
                            name = candidate;
                    }
                }
                std::vector<mt::MTSongStepInfo> steps;
                for( int i = 0; i < sequence_logic::copyToSongStepCount( num ); ++i )
                {
                    mt::MTSongStepInfo step;
                    step.seqNo       = i;
                    step.repeatCount = 1;
                    steps.push_back( step );
                }
                const int added = ae.appendSongSteps( name, steps, create );
                mt::utils::showNotification( juce::String( added ) + " sequence(s) copied to song \"" + name + "\"" );
                AppStatusbar::Instance().offerUndo( "Copied to song " + name, []() { mt::getAudioInterface().performUndo(); } );
            }
            delete dlg;
        } );
    }

    void KickNextSequenceContent::onPlay( bool FromBeginning )
    {
        if( isPlaying )
        {
            // Already running: PLAY START restarts it, PLAY changes nothing.
            if( FromBeginning )
                ae.setSequencePlay( playingSequence, true, true );
            return;
        }
        isPlaying = true;
        startPlaying( FromBeginning, true );
        nextBar.setEnabled( true );
        tableView->setInterceptsMouseClicks( false, false );
        refreshClear();
        if( onPlayingChanged )
            onPlayingChanged( true );
    }

    void KickNextSequenceContent::onStop()
    {
        isPlaying = false;
        stopPlaying( true );
        nextBar.setEnabled( false );
        tableView->setInterceptsMouseClicks( true, true );
        clearHeld();
        refreshClear();
        if( onPlayingChanged )
            onPlayingChanged( false );
    }

    // The engine's transport changed by something other than this screen's buttons: follow it (UI only).
    void KickNextSequenceContent::syncTransport()
    {
        if( !isVisible() || !ae.isProjectLoaded() )
            return;
        const bool enginePlaying = ae.isSequencePlaying();
        if( enginePlaying == isPlaying )
            return;
        isPlaying = enginePlaying;
        if( enginePlaying )
            startPlaying( false, false );
        else
            stopPlaying( false );
        nextBar.setEnabled( isPlaying );
        tableView->setInterceptsMouseClicks( !isPlaying, !isPlaying );
        if( !isPlaying )
            clearHeld();
        refreshClear();
        if( onPlayingChanged )
            onPlayingChanged( isPlaying );
    }

    void KickNextSequenceContent::enableRowSelection( bool State )
    {
        tableView->enableRowSelection( State );
    }

    void KickNextSequenceContent::onSequenceSelectionChange( const mt::MTSelected& Selected )
    {
        if( kick_ui::KickHome::Instance().getCurrentScreen() != kick_ui::Screen::MenuNextSequence )
            juce::MessageManager::callAsync( [this, Selected] { selectSequence( Selected.sequence ); } );
    }

    void KickNextSequenceContent::updateData()
    {
        pads->clearAllPadData();
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
        tableView->refresh();

        auto startId = ae.getBankOffset();
        auto endId   = startId + 16;

        for( auto id = startId, row = 0; id < endId; ++id, ++row )
        {
            if( juce::isPositiveAndBelow( id, ae.getNumSequences() ) )
            {
                auto data = ae.getSequenceData( id );
                pads->setActive( id, true );
                pads->setPadId( id + 1, id % 16 );
                pads->setHeaderText( id, "S" + juce::String( id + 1 ).paddedLeft( '0', 2 ), juce::dontSendNotification );
                pads->setRow1Text( id, data->name.get(), juce::dontSendNotification );
                if( id != playingSequence && id != nextSequence )
                    setPadLED( id, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
            }
            else
            {
                pads->setActive( id, false );
                pads->setHeaderText( id, "S" + juce::String( id + 1 ).paddedLeft( '0', 2 ), juce::dontSendNotification );
                pads->setRow1Text( id, " ", juce::dontSendNotification );
            }
        }
    }

    void KickNextSequenceContent::setPadLED( const int Id, int State, int Color )
    {
        if( kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuNextSequence )
        {
            //Make sure we are  setting LED for the current Bank
            int idx = Id - pads->getBankOffset();
            if( juce::isPositiveAndBelow( idx, 16 ) )
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setPadDataMessage( idx, State, Color ) );
        }
    }

    void KickNextSequenceContent::setPadBlinking( const int Id, int State, int OnColorID, int OffColorID )
    {
        if( ae.isProjectLoaded() && kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuNextSequence )
        {
            auto startId = ae.getBankOffset();
            auto endId   = startId + 16;
            if( Id >= startId && Id < endId )
            {
                int idx = Id - pads->getBankOffset();
                if( juce::isPositiveAndBelow( idx, 16 ) )
                    contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setPadBlinkingMessage( idx, State, OnColorID, OffColorID ) );
            }
        }
    }

    void KickNextSequenceContent::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 10, 10, 342, 396 }, ui::Theme::backgroundColour( 3 ) );
        mt::utils::fillRoundedRect( g, { 10, 10, 342, 54 }, ui::Theme::backgroundColour( 1 ), 5.0f, true, true, false, false );
        mt::utils::renderRoundedRect( g, { 10, 10, 342, 396 }, ui::Theme::backgroundColour( 0 ), 1.f, 6.f );
    }

    void KickNextSequenceContent::resized()
    {
        srNo.setBoundsRelative( 0.0156f, 0.033f, 0.04f, 0.096f );
        sequence.setBoundsRelative( 0.059f, 0.033f, 0.137f, 0.096f );
        length.setBoundsRelative( 0.2f, 0.033f, 0.0636f, 0.096f );
        bpm.setBoundsRelative( 0.272f, 0.033f, 0.0636f, 0.096f );

        tableView->setBoundsRelative( .0097f, .1391f, .3346f, .7434f );
        pads->setBoundsRelative( .3581f, .0304f, .6301f, .8434f );

        nextBar.setBoundsRelative( .5068f, .9083f, .1174f, .0742f );
        sudden.setBoundsRelative( .6282f, .9083f, .1174f, .0742f );
        hold.setBoundsRelative( .7495f, .9083f, .1174f, .0742f );
        clear.setBoundsRelative( .8708f, .9083f, .1174f, .0742f );
        toSong.setBoundsRelative( .0097f, .9083f, .1174f, .0742f );
    }

    void KickNextSequenceContent::visibilityChanged()
    {
        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );

        if( visible )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );

            tableView->refresh();
            updateData();
            restoreData();
        }
        else
        {
            if( isPlaying )
            {
                if( nextSequence >= 0 )
                    setPadBlinking( nextSequence, contracts::DeviceControlContracts::StateOffBlink, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
            }
            else
            {
                pads->clearAllBreathing();

                if( playingSequence >= 0 )
                    setPadBlinking( playingSequence, contracts::DeviceControlContracts::StateOffBlink, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );

                if( nextSequence >= 0 )
                {
                    auto h = sequence_logic::hiddenWhileStopped( { playingSequence, nextSequence, false } );
                    ae.setSelectedSequence( h.playing );

                    playingSequence = h.playing;
                    nextSequence    = h.next;
                }
            }
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
            contracts::DclSignals::refreshPadLeds();
        }
    }

    void KickNextSequenceContent::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
    }

    //
    //
    //
    //

    KickNextSequenceMode::KickNextSequenceMode()
        : kick_ui::IScreen( kick_ui::Screen::MenuNextSequence )
    {
        mt::utils::addChildComponents( this, header, content );

        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        // Touch Play / Stop in the header (no hardware needed); its state follows the engine.
        header.showPlayButton( true );
        header.onPlayStopClick = [this]( bool Play ) {
            if( Play )
                content.onPlay( false );
            else
                content.onStop();
        };
        content.onPlayingChanged = [this]( bool Playing ) { header.setPlayState( Playing, juce::dontSendNotification ); };

        ui::themeChanged.connect( [&]() {
            header.setIcon( "Icon_NextSeq" );
        } );
    }

    void KickNextSequenceMode::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        content.setBounds( b );
    }

    void KickNextSequenceMode::visibilityChanged()
    {
        bool visible = isVisible();

        if( visible )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( mt::PadColorMode::DISABLE_PAD_COLOUR_MODE ) );
        }
        else
        {
            auto MTIProg = ae.getProgramData( ae.getSelectedProgram() );
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setPadColorModeMessage( MTIProg ? MTIProg->colourMode.get() : mt::OFF ) ); //update the mode only for current screen
        }
        for( auto it : getChildren() )
            it->setVisible( visible );
    }

    void KickNextSequenceMode::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
                knobCommand( knob, value );
        }
        else
        {
            if( ae.isProjectLoaded() && contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
            {
                auto buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
                setPlayState( buttonCode );
            }
            content.handleCommand( f_HwMsg );
        }
    }

    void KickNextSequenceMode::setPlayState( int PlayType )
    {
        switch( PlayType )
        {
            case contracts::DeviceControlContracts::EN_PLAY: content.onPlay( false ); break;
            case contracts::DeviceControlContracts::EN_PLAYSTART: content.onPlay( true ); break;
            case contracts::DeviceControlContracts::EN_STOP:
                content.onStop();
                break;
        }
    }

    void KickNextSequenceMode::knobCommand( int knob, int value )
    {
        applySequenceKnob( ae, metronome, knob, value );
    }

    // Q-Link on the selected sequence / track. Shared with the Song screen. Knobs 1-4 exist on the
    // Key 61; 5-13 are the Behringer layout. A missing sequence or track does nothing.
    void applySequenceKnob( mt::MTAudioInterface& ae, kick_ui::MetronomePopup& metronome, int knob, int value )
    {
        using K = contracts::DeviceControlContracts;
        const auto seq = ae.getSelectedSequence();

        if( knob >= K::KnobSequence::EN_KNOB_1 && knob <= K::KnobSequence::EN_KNOB_4 )
        {
            auto* seqData = ae.getSequenceData( seq );
            if( seqData == nullptr )
                return;
            sequence_logic::SeqValues v;
            v.valid     = true;
            v.bpm       = (int)seqData->tempo;
            v.length    = seqData->length.get();
            v.loopStart = seqData->loopStart.get();
            v.loopEnd   = seqData->loopEnd.get();
            const auto e = sequence_logic::sequenceQLink( knob - K::KnobSequence::EN_KNOB_1 + 1, v, value );
            switch( e.kind )
            {
                case sequence_logic::QLinkEdit::Kind::Bpm: ae.setSequenceBPM( seq, e.a ); break;
                case sequence_logic::QLinkEdit::Kind::Length: ae.setSequenceLength( seq, e.a ); break;
                case sequence_logic::QLinkEdit::Kind::LoopPoints: ae.setSequenceLoopPoints( seq, e.a, e.b ); break;
                default: break;
            }
            return;
        }

        auto trackData = ae.getTrackData( seq, ae.getSelectedTrack() );
        switch( knob )
        {
            case K::KnobSequence::EN_KNOB_5:
                ae.setSelectedSequence( seq + value );
                break;
            //track
            case K::KnobSequence::EN_KNOB_6:
                ae.setTrackMidiChannel( seq, ae.getSelectedTrack(), trackData ? trackData->midiChannel.get() + value : value );
                break;
            case K::KnobSequence::EN_KNOB_7:
                ae.setTrackLength( seq, ae.getSelectedTrack(), trackData ? trackData->length.get() + value : value );
                break;
            case K::KnobSequence::EN_KNOB_8:
                ae.setTrackLevel( seq, ae.getSelectedTrack(), trackData ? trackData->level.get() + value : value );
                break;
            case K::KnobSequence::EN_KNOB_9:
                ae.setTrackTranspose( seq, ae.getSelectedTrack(), trackData ? trackData->transpose.get() + value : value );
                break;
            case K::KnobSequence::EN_KNOB_10:
                ae.setSelectedTrack( ae.getSelectedTrack() + value );
                break;
                //metronome
            case K::KnobSequence::EN_KNOB_11:
            case K::KnobSequence::EN_KNOB_12:
            case K::KnobSequence::EN_KNOB_13:
                metronome.linkRotated( knob % 10, value );
                break;
        }
    }
}
