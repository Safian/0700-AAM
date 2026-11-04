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

#include "kick_AppMainMode.h"

namespace kick_ui
{
    class AppSelectProgramMode : public ui::KickControlBase<juce::Component>
    {
    public:
        AppSelectProgramMode()
        {
            setName( "Create new program" );
            setInterceptsMouseClicks( false, true );
            buttonAdder( drum, mt::MTProgramType::Drum );
            buttonAdder( keygroup, mt::MTProgramType::Keygroup );
            buttonAdder( clip, mt::MTProgramType::Clip );
            buttonAdder( audio, mt::MTProgramType::Audio );
            buttonAdder( midi, mt::MTProgramType::Midi );
            buttonAdder( cv, mt::MTProgramType::Cv );
            buttonAdder( plugin, mt::MTProgramType::Plugin );

            mt::utils::addAndMakeVisible( this, drumLabel, keyLabel, audioLabel, clipLabel, pluginLabel, midiLabel, cvLabel, apply, cancel, createTrackToggle );
            createTrackToggle.setToggleState( false, juce::dontSendNotification );
            createTrackToggle.setValueFontHeight( 16 );
            apply.setEnabled( false );
            drumLabel.setValueColour( ui::Theme::textColour( 1 ) );
            drumLabel.setValueJustification( juce::Justification::centred );
            drumLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
            keyLabel.setValueColour( ui::Theme::textColour( 1 ) );
            keyLabel.setValueJustification( juce::Justification::centred );
            keyLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

            audioLabel.setValueColour( ui::Theme::textColour( 1 ) );
            audioLabel.setValueJustification( juce::Justification::centred );
            audioLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

            clipLabel.setValueColour( ui::Theme::textColour( 1 ) );
            clipLabel.setValueJustification( juce::Justification::centred );
            clipLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

            pluginLabel.setValueColour( ui::Theme::textColour( 1 ) );
            pluginLabel.setValueJustification( juce::Justification::centred );
            pluginLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

            midiLabel.setValueColour( ui::Theme::textColour( 1 ) );
            midiLabel.setValueJustification( juce::Justification::centred );
            midiLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

            cvLabel.setValueColour( ui::Theme::textColour( 1 ) );
            cvLabel.setValueJustification( juce::Justification::centred );
            cvLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

            apply.onClick  = [&]() {
                if( selectedProgram != mt::MTProgramType::Unknown )
                    mt::utils::exitDialog( this, UI::APPLY );
            };
            cancel.onClick = [&]() { mt::utils::exitDialog( this, UI::CANCEL ); };

            ui::themeChanged.connect( [&]() {
                drum.setImages( "Drum", "Drum_Active" );
                keygroup.setImages( "Piano", "Piano_Active" );
                audio.setImages( "Movie", "Movie_Active" );
                clip.setImages( "Guitar", "Guitar_Active" );
                plugin.setImages( "Plugin", "Plugin_Active" );
                midi.setImages( "Cord", "Cord_Active" );
                cv.setImages( "CV", "CV_Active" );
                apply.setImages( "Icon_Apply" );
                cancel.setImages( "Icon_Cancel" );
            } );
        }
        mt::MTProgramType selectedProgram{ mt::MTProgramType::Unknown };

        bool shouldCreateTrack() const noexcept { return createTrackToggle.getToggleState(); }

        void reset()
        {
            selectedProgram = mt::MTProgramType::Unknown;
            apply.setEnabled( false );
            drum.setToggleState( false, juce::dontSendNotification );
            keygroup.setToggleState( false, juce::dontSendNotification );
            clip.setToggleState( false, juce::dontSendNotification );
            audio.setToggleState( false, juce::dontSendNotification );
            midi.setToggleState( false, juce::dontSendNotification );
            cv.setToggleState( false, juce::dontSendNotification );
            plugin.setToggleState( false, juce::dontSendNotification );
            createTrackToggle.setToggleState( false, juce::dontSendNotification );
        }

    private:
        ui::KickImageButtonWithAttachedValue drum{ "Drum", "Drum", "Drum_Active" };
        ui::KickImageButton                  keygroup{ "Key Group", "Piano", "Piano_Active" };
        ui::KickImageButton                  audio{ "Audio", "Movie", "Movie_Active" };
        ui::KickImageButton                  clip{ "Clip", "Guitar", "Guitar_Active" };
        ui::KickImageButton                  plugin{ "Plugin", "Plugin", "Plugin_Active" };
        ui::KickImageButton                  midi{ "Midi", "Cord", "Cord_Active" };
        ui::KickImageButton                  cv{ "CV", "CV", "CV_Active" };
        ui::KickLabel                        drumLabel{ "DRUM" }, keyLabel{ "KEY" }, audioLabel{ "AUDIO" }, clipLabel{ "CLIP" }, pluginLabel{ "PLUGIN" }, midiLabel{ "MIDI" }, cvLabel{ "CV" };
        ui::KickToggleButton                 createTrackToggle{ "Create Track", "Create Track", "Create Track" };
        ui::KickImageButton                  apply{ "Apply", "Icon_Apply" }, cancel{ "Cancel", "Icon_Cancel" };

        void buttonAdder( ui::KickImageButton& B, mt::MTProgramType T )
        {
            addAndMakeVisible( B );
            B.setRadioGroupId( (int)kick_ui::RadioGroupId::AppSelectProgram, juce::NotificationType::dontSendNotification );
            B.setPlacementFlag( juce::Justification::centred );
            B.onClick = [&, T]() {
                selectedProgram = T;
                apply.setEnabled( true );
            };
        }

        void resized() override
        {
            drum.setBoundsRelative( .1461f, .2825f, .1538f, .1621f );
            keygroup.setBoundsRelative( .3307f, .2825f, .1538f, .1621f );
            clip.setBoundsRelative( .5153f, .2825f, .1538f, .1621f );
            audio.setBoundsRelative( .7f, .2825f, .1538f, .1621f );
            plugin.setBoundsRelative( .1461f, .5773f, .1538f, .1621f );
            midi.setBoundsRelative( .3307f, .5773f, .1538f, .1621f );
            cv.setBoundsRelative( .5153f, .5773f, .1538f, .1621f );
            drumLabel.setBoundsRelative( .1461f, .4692f, .1538f, .0343f );
            keyLabel.setBoundsRelative( .3307f, .4692f, .1538f, .0343f );
            clipLabel.setBoundsRelative( .5153f, .4692f, .1538f, .0343f );
            audioLabel.setBoundsRelative( .7f, .4692f, .1538f, .0343f );
            pluginLabel.setBoundsRelative( .1461f, .7641f, .1538f, .0343f );
            midiLabel.setBoundsRelative( .3307f, .7641f, .1538f, .0343f );
            cvLabel.setBoundsRelative( .5153f, .7641f, .1538f, .0343f );
            createTrackToggle.setBoundsRelative( .3307f, .83f, .3386f, .09f );
        };

        void knobClicked() override
        {
            apply.triggerClick();
        }

        void knobRotated( double ) override {}
        void sliderMoved( float ) override {}
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( AppSelectProgramMode )
    };

    static std::unique_ptr<AppSelectProgramMode> addProgramDialog;

    AppMainmode::AppMainmode()
        : IScreen( kick_ui::Screen::HomeListView ), pProg( std::make_unique<AppProgram>() ), pTrack( std::make_unique<AppTrack>() ), pSeq( std::make_unique<AppSequence>() )
    {
        mt::utils::addAndMakeVisible( this, tBar );
        mt::utils::addChildComponents( this, pSeq.get(), pTrack.get(), pProg.get(), seqoptions, trackoptions, prgOptions );
        undoConn = UndoWatcher::undoRedoDone.connect( [this]() {
            if( !isVisible() )
                return;
            refreshActiveTab( tabButtons[0].getToggleState() ? TabType::Sequence : tabButtons[2].getToggleState() ? TabType::Program : TabType::Track );
        } );

        mt::utils::setValueColour( ui::Theme::textColour( 1 ), tabButtons[0], tabButtons[1], tabButtons[2] );
        mt::utils::setValueFontHeight( 18, tabButtons[0], tabButtons[1], tabButtons[2] );

        tabButtons[0].setName( TRANS( " Sequence " ) );
        tabButtons[0].setToggleButtonText( "Sequence", "Sequence" );
        tabButtons[1].setName( TRANS( "Track" ) );
        tabButtons[1].setToggleButtonText( "Track", "Track" );
        tabButtons[2].setName( TRANS( "Program" ) );
        tabButtons[2].setToggleButtonText( "Program", "Program" );

        int idx = 0;
        for( auto& it : tabButtons )
        {
            addAndMakeVisible( it );
            it.setRadioGroupId( (int)kick_ui::RadioGroupId::AppMainMode, juce::dontSendNotification );
            it.onClick = [&, idx]() {
                if( it.getToggleState() )
                {
                    tabSelected = idx;
                    refreshTab( tabSelected );
                    contracts::DclSignals::onMainModeTabSelected( tabSelected );
                }
            };
            idx++;
        }

        signals::sigMainModeSelectTab.connect( [this]( int Index ) { selectTab( Index ); } );

        tBar.add.onClick = [&]() {
            if( tabButtons[1].getToggleState() )
            {
                if( !ae.getNumSequences() )
                    ae.createSequence();

                ae.createTrack( ae.getSelectedSequence() );
                mt::utils::makeHidden( trackoptions );
            }
            else if( tabButtons[2].getToggleState() )
            {
                if( !addProgramDialog.get() )
                    addProgramDialog = std::make_unique<AppSelectProgramMode>();

                addProgramDialog->reset();

                mt::utils::showOptions(
                    "CREATE PROGRAM", addProgramDialog.get(), [this]( int R ) {
                        if( R == UI::APPLY && addProgramDialog->selectedProgram != mt::MTProgramType::Unknown )
                        {
                            int progIdx = ae.createProgram( addProgramDialog->selectedProgram );
                            if( progIdx >= 0 )
                            {
                                if( addProgramDialog->shouldCreateTrack() )
                                {
                                    if( !ae.getNumSequences() )
                                        ae.createSequence();
                                    int seqIdx = ae.getSelectedSequence();
                                    int trackIdx = ae.createTrack( seqIdx );
                                    if( trackIdx >= 0 )
                                    {
                                        ae.setProgramToTrack( progIdx, seqIdx, trackIdx );
                                        ae.setSelectedTrack( trackIdx );
                                    }
                                }
                                ae.setSelectedProgram( progIdx );
                            }
                            mt::utils::makeHidden( prgOptions );
                        }
                        refreshDeleteButton();
                    },
                    ui::layout::dialog, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
            }
            else if( tabButtons[0].getToggleState() )
            {
                ae.createSequence();
                mt::utils::makeHidden( seqoptions );
            }
            refreshDeleteButton();
        };

        // Delete asks first (Yes / No dialog, as Clear Sequence does); undo still brings the row back.
        tBar.del.onClick = [&]() {
            tBar.del.setToggleState( false, juce::dontSendNotification );

            juce::String what;
            std::function<void()> doDelete;
            if( tabButtons[0].getToggleState() )
            {
                const int sel = ae.getSelectedSequence();
                auto      sd  = ae.getSequenceData( sel );
                if( !sd ) return;
                what     = "sequence \"" + sd->name.get() + "\"";
                doDelete = [this, sel]() {
                    ae.deleteSequence( sel );
                    mt::utils::makeHidden( seqoptions );
                };
            }
            else if( tabButtons[1].getToggleState() )
            {
                const int seqNo = ae.getSelectedSequence(), trkNo = ae.getSelectedTrack();
                auto      td    = ae.getTrackData( seqNo, trkNo );
                if( !td ) return;
                what     = "track \"" + td->name.get() + "\" with its events";
                doDelete = [this, seqNo, trkNo]() {
                    ae.deleteTrack( seqNo, trkNo );
                    mt::utils::makeHidden( trackoptions );
                };
            }
            else if( tabButtons[2].getToggleState() )
            {
                const int sel = ae.getSelectedProgram();
                auto      pd  = ae.getProgramData( sel );
                if( !pd ) return;
                what     = "program \"" + pd->name.get() + "\"";
                doDelete = [this, sel]() {
                    ae.deleteProgram( sel );
                    mt::utils::makeHidden( prgOptions );
                };
            }
            else
                return;

            auto p = new ui::FilePopup{ "Delete " + what + " ?", false, false };
            mt::utils::showOptions(
                "WARNING!", p, [this, p, doDelete]( int R ) {
                    if( R == UI::APPLY )
                        doDelete();
                    if( p ) delete p;
                    refreshDeleteButton();
                },
                ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };

        tBar.undo.onClick = [&]() {
            if( tabButtons[0].getToggleState() )
            {
                handleUndoRedo( TabType::Sequence, UndoRedo::Undo );
            }
            else if( tabButtons[1].getToggleState() )
            {
                handleUndoRedo( TabType::Track, UndoRedo::Undo );
            }
            else if( tabButtons[2].getToggleState() )
            {
                handleUndoRedo( TabType::Program, UndoRedo::Undo );
            }
            tBar.undo.setToggleState( false, juce::dontSendNotification );
        };

        tBar.history.onClick = [&]() {
            UndoHistory::show();
            tBar.history.setToggleState( false, juce::dontSendNotification );
        };

        tBar.redo.onClick = [&]() {
            if( tabButtons[0].getToggleState() )
            {
                handleUndoRedo( TabType::Sequence, UndoRedo::Redo );
            }
            else if( tabButtons[1].getToggleState() )
            {
                handleUndoRedo( TabType::Track, UndoRedo::Redo );
            }
            else if( tabButtons[2].getToggleState() )
            {
                handleUndoRedo( TabType::Program, UndoRedo::Redo );
            }
            tBar.redo.setToggleState( false, juce::dontSendNotification );
        };

        ui::themeChanged.connect( [&]() {
            tabButtons[0].setToggleButtonText( "Sequence", "Sequence" );
            tabButtons[1].setToggleButtonText( "Track", "Track" );
            tabButtons[2].setToggleButtonText( "Program", "Program" );
            tBar.add.setImages( "Add_normal", "Add_normal", "Add_active" );
        } );
        pTrack->onTrackEdit = [&]() {
            mt::utils::makeVisible( trackoptions );
            mt::utils::makeHidden( seqoptions, prgOptions );
        };
        pSeq->onSeqEdit = [&]() {
            mt::utils::makeVisible( seqoptions );
            mt::utils::makeHidden( trackoptions, prgOptions );
        };
        pProg->onPrgEdit = [&]() {
            mt::utils::makeVisible( prgOptions );
            mt::utils::makeHidden( trackoptions, seqoptions );
        };

        auto close = [&]() {
            mt::utils::makeHidden( trackoptions, prgOptions, seqoptions );
        };
        seqoptions.onClose   = close;
        trackoptions.onClose = close;
        prgOptions.onClose   = close;

        seqoptions.refreshSequence = [&]() { pSeq->refresh(); };
    }

    AppMainmode::~AppMainmode()
    {
        pProg.reset();
        pTrack.reset();
        pSeq.reset();
    }

    void AppMainmode::selectTab( int Index )
    {
        if( Index < 0 || Index >= (int)tabButtons.size() )
            return;

        tabSelected = Index;
        tabButtons[Index].setToggleState( true, juce::dontSendNotification );
        refreshTab( tabSelected );
        contracts::DclSignals::onMainModeTabSelected( tabSelected );
    }

    void AppMainmode::refreshTab( int Index )
    {
        mt::utils::makeHidden( seqoptions, trackoptions, prgOptions );
        pSeq->setVisible( ( 0 == Index ) );
        pTrack->setVisible( ( 1 == Index ) );
        pProg->setVisible( ( 2 == Index ) );
        refreshDeleteButton();
    }

    void AppMainmode::refreshDeleteButton()
    {
        if( tabButtons[0].getToggleState() )
        {
            tBar.del.setEnabled( ae.getNumSequences() > 0 );
        }
        else if( tabButtons[1].getToggleState() )
        {
            tBar.del.setEnabled( ae.getNumTracks( ae.getSelectedSequence() ) > 0 );
        }
        else if( tabButtons[2].getToggleState() )
        {
            tBar.del.setEnabled( ae.getNumPrograms() > 0 );
        }
    }

    void AppMainmode::handleUndoRedo( TabType tabType, UndoRedo undoRedoOption )
    {
        // Same entry as the hardware UNDO button (the engine's undo manager); the "Undid: ..." notification and the refresh of
        // the tab come from the UndoWatcher for both. Not while a take is being recorded: the take is not part of the history yet.
        if( ae.isSequenceRecording() )
        {
            mt::utils::showNotification( "Undo / Redo is off while recording" );
            return;
        }
        if( undoRedoOption == UndoRedo::Undo )
            ae.performUndo();
        else
            ae.performRedo();
        refreshActiveTab( tabType );
    }

    void AppMainmode::refreshActiveTab( TabType tabType )
    {
        switch( tabType )
        {
            case TabType::Sequence:
                if( pSeq ) pSeq->refresh();
                break;
            case TabType::Track:
                if( pTrack ) pTrack->refresh();
                break;
            case TabType::Program:
                if( pProg ) pProg->refresh();
                break;
        }
        tBar.repaint(); // the Undo / Redo buttons are enabled from the history
    }

    void AppMainmode::resized()
    {
        auto b  = getLocalBounds();
        auto tb = b.removeFromTop( 60 ).reduced( 10, 6 );
        tb.removeFromTop( 4 );
        tabButtons[0].setBounds( tb.removeFromLeft( 133 ) );
        tb.removeFromLeft( 4 );
        tabButtons[1].setBounds( tb.removeFromLeft( 133 ) );
        tb.removeFromLeft( 4 );
        tabButtons[2].setBounds( tb.removeFromLeft( 133 ) );
        tb.removeFromLeft( 10 );
        tBar.setBounds( tb );

        b.removeFromLeft( 10 );
        pSeq->setBounds( b );
        pTrack->setBounds( b );
        pProg->setBounds( b );
        trackoptions.setBounds( b );
        seqoptions.setBounds( b );
        prgOptions.setBounds( b );
    }

    AppMainmode::toolbar::toolbar()
    {
        mt::utils::addAndMakeVisible( this, add, del, undo, redo, history );
        del.setPlacementFlag( juce::Justification::centred );
    }

    void AppMainmode::toolbar::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 4 ) );
        g.fillRoundedRectangle( getLocalBounds().toFloat(), 4.f );
        mt::utils::setEnabled( um.canRedo(), redo );
        mt::utils::setEnabled( um.canUndo(), undo );
    }

    void AppMainmode::toolbar::resized()
    {
        auto b = getLocalBounds();
        add.setBounds( b.removeFromRight( 40 ).reduced( 0, 2 ) );
        del.setBounds( b.removeFromRight( 50 ).reduced( 5, 2 ) );
        redo.setBounds( b.removeFromRight( 60 ).reduced( 5, 2 ) );
        undo.setBounds( b.removeFromRight( 70 ).reduced( 5, 2 ) );
        history.setBounds( b.removeFromRight( 80 ).reduced( 5, 2 ) );
    }

    void AppMainmode::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            int              bankOffset = ae.getBankOffset();
            auto             padNo      = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            auto             padState   = contracts::DclContractsHelper::getPadState( f_HwMsg );
            auto             velocity   = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );
            std::vector<int> pads;

            padNo += bankOffset;
            pads.push_back( padNo );

            for( auto& it : pads )
            {
                juce::MidiMessage Msg;

                if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
                    Msg = juce::MidiMessage::noteOn( 1, it, (juce::uint8)velocity );
                else if( padState == contracts::DeviceControlContracts::BTN_RELEASED )
                    Msg = juce::MidiMessage::noteOff( 1, it );

                ae.addLiveMidiMsg( ae.getSelected(), Msg );
            }
            pads.clear();
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY:
                    if( !ae.isSequencePlaying() ) ae.setSequencePlay( aeSel.sequence, false, true );
                    break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
            }
        }
        else
        {
            // Forward to...
            if( tabButtons[0].getToggleState() )
            { // Sequence
                pSeq->handleCommand( f_HwMsg );
            }
            else if( tabButtons[1].getToggleState() )
            { // Track
                pTrack->handleCommand( f_HwMsg );
            }
            else if( tabButtons[2].getToggleState() )
            { // Program
                pProg->handleCommand( f_HwMsg );
            }
        }
    }

    void AppMainmode::visibilityChanged()
    {
        if( isVisible() )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::enableAfterTouchMessage( true ) );
            tabButtons[tabSelected].setToggleState( true, juce::dontSendNotification );
            refreshTab( tabSelected );
        }
        else
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::enableAfterTouchMessage( false ) );
            refreshTab( -1 );
        }
    };

    void AppMainmode::handleAsyncUpdate()
    {
        //refreshLEDs();
    }

}
