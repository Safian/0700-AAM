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

#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/dcl/dcl_signals.h"

namespace kick_ui
{
    namespace
    {
        // The BPM field and the Seq/Song switch only changed the numbers shown in the table, never the
        // engine, and a song has no tempo of its own. Hidden until the engine has a song tempo.
        constexpr bool kSongTempoOverrideAvailable = false;
        // Export opened a confirmation dialog whose Apply did nothing. Hidden until it is specified
        // (what to export: MIDI file, audio mixdown, sequence chain).
        constexpr bool kSongExportAvailable = false;
    }

    struct CreateNewSong : public juce::Component
    {
        ui::KickLabel Name{ "Enter Name" };

        CreateNewSong()
        {
            Name.setFontHeight( 32 );
            Name.setPadding( 20 );
            addAndMakeVisible( Name );
            Name.setEditable( true, true );
        }

        void resized() override
        {
            Name.setBounds( getLocalBounds().reduced( 20 ) );
        }
    };

    SaveFile::SaveFile( const juce::String FileName )
    {
        addAndMakeVisible( fileName );
        fileName.setValueColour( ui::Theme::textColour( 1 ) );
        fileName.setValueJustification( juce::Justification::centred );
        fileName.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        fileName.setValueFontHeight( 24 );
        fileName.setEditable( true );
        fileName.setText( "Do You Want To Export " + FileName + " Song", juce::dontSendNotification );
    }

    void SaveFile::resized()
    {
        fileName.setBoundsRelative( .15f, .4f, .7f, .2f );
    }

    ///////////////////   Song Content   ////////////////////

    KickSongContent::KickSongContent()
        : ae( mt::getAudioInterface() )
        , pads( std::make_unique<ui::KickPadButtonGroupComponent>( ui::KickPadButton::PadButtonType::with3Rows ) )

    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );

        tableView.setRowHeight( 52 );
        tableView.setRowSpacing( 1 );
        tableView.setMargin( 0, 0, 1, 0 );
        tableView.setSelectedRowBackgroundColour( ui::Theme::backgroundColour( 0 ) );
        tableView.setRowBackgroundColour( ui::Theme::backgroundColour( 3 ) );
        tableView.setHeaderHeight( 40 );
        tableView.showHeader( true );
        tableView.setTableEndInfo( "END OF SONG", ui::Theme::textColour( 1 ), 18, true );

        pads->setActive( -1, false );
        pads->setHighlight( -1 );
        pads->setPadButtonSpacer( 4 );

        mt::utils::setValueFontHeight( 18, insert, clr, del );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), insert, clr, del );
        mt::utils::addChildComponents( this, tableView, pads.get(), insert, clr, del );

        mt::signals::bankChanged.connect( [&]( int index, juce::String bankName ) {
            if( kick_ui::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuSong )
            {
                pads->setBank( bankName );
                pads->refreshAllPads();
            }
        } );

        provider.numRows      = [&]() -> int { return (int)info.steps.size(); };
        provider.hdrComponent = [&]( juce::Component* Existing ) -> juce::Component* { return dynamic_cast<KickSongContent::Header*>( Existing ) ? Existing : new KickSongContent::Header(); };

        provider.cellComponent = [&]( int Row, juce::Component* Existing ) -> juce::Component* {
            if( !juce::isPositiveAndBelow( Row, (int)info.steps.size() ) )
                return nullptr;
            // A step whose sequence was deleted has seqNo -1: it is shown as missing, with no data.
            auto* seqData = song_logic::isMissingStep( info.steps[(size_t)Row].seqNo ) ? nullptr : ae.getSequenceData( info.steps[(size_t)Row].seqNo );
            if( seqData != nullptr )
            {
                info.steps[Row].bars    = seqData->length.get();
                info.steps[Row].bpm     = isSeqSong ? bpmValue : seqData->tempo;
                info.steps[Row].loopEnd = seqData->loopEnd.get();
            }
            else
            {
                info.steps[Row].bars = 0;
                info.steps[Row].bpm  = 0;
            }
            auto comp               = dynamic_cast<KickSongTableDisplay*>( Existing );
            if( comp == nullptr )
                comp = new KickSongTableDisplay();

            comp->refreshStep( info.steps[Row] );
            comp->onRepeatChanged = [&, Row]( int Value ) {
                info.steps[Row].repeatCount = Value;
                ae.editSongStep( songName, info.steps[Row], Row );
            };

            return comp;
        };

        pads->OnButtonClicked = [this]( int Index ) {
            if( !isPlaying )
            {
                if( selectedPadSeq != Index )
                {
                    setPadLED( selectedPadSeq, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
                    setPadLED( Index, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 );
                }
                else //if user press the same selected pad again
                    setPadLED( Index, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 );
                pads->clearAllBreathing();
                pads->setBreathing( Index );
                pads->refreshAllPads();
                selectedPadSeq = Index;
            }
        };

        clr.onClick = [&]() {
            if( info.steps.size() )
            {
                if( isPlaying )
                    stopOrPause();
                ae.deleteAllSongSteps( songName );
                updateSong( songName );
            }
        };

        insert.onClick = [&]() {
            auto idx = pads->getSelected();
            if( idx >= 0 && idx < (int)seqList.size() )
            {
                mt::MTSongStepInfo _step;
                _step.seqNo = idx;

                const int numSteps = (int)info.steps.size();
                const int sel      = tableView.getSelectedRowOrNone();
                const int selSeq   = ( sel >= 0 && sel < numSteps ) ? info.steps[(size_t)sel].seqNo : -1;
                const auto plan    = song_logic::planInsert( numSteps, sel, idx, selSeq );

                if( plan.kind == song_logic::InsertPlan::Kind::RepeatPlus1 )
                { // The selected step already plays this sequence: one more repeat.
                    _step.repeatCount = info.steps[(size_t)sel].repeatCount + 1;
                    ae.editSongStep( songName, _step, plan.index );
                }
                else // After the selected step; with no selection at the END of the song.
                    ae.addSongStep( songName, _step, plan.index );

                updateSong( songName );
                if( !info.steps.empty() )
                    tableView.selectRow( juce::jlimit( 0, (int)info.steps.size() - 1, plan.selectAfter ) );
            }
        };

        del.onClick = [&]() {
            const int numSteps = (int)info.steps.size();
            const int sel      = tableView.getSelectedRowOrNone();
            if( !song_logic::canDelete( numSteps, sel ) ) // nothing selected: nothing is deleted
                return;
            ae.deleteSongStep( songName, sel );
            updateSong( songName );
            const int next = song_logic::selectionAfterDelete( numSteps, sel );
            if( next >= 0 )
                tableView.selectRow( next );
        };

        mt::signals::onSongSequencePlay.connect( [this]( int CurrentSeq, int NextSeq, int CurrentSeqIndex ) {
            juce::MessageManager::callAsync( [this, CurrentSeq, NextSeq, CurrentSeqIndex] {
                setSequenceToPlay( CurrentSeq, NextSeq );
                if( CurrentSeqIndex != -1 )
                    tableView.selectRow( CurrentSeqIndex );
            } );
        } );

        mt::signals::onNextSeqAboutToPlayInSong.connect( [this]( int NextSeq ) {
            juce::MessageManager::callAsync( [this, NextSeq] {
                setSequenceToPlay( selectedId, NextSeq );
            } );
        } );

        pads->setSelected( 0, juce::dontSendNotification );
        pads->setBreathing( 0 );
    }

    void KickSongContent::updateSong( const juce::String& Song )
    {
        songName = Song;
        info     = ae.getSongInfo( songName );
        displayState( songName );

        if( isSeqSong )
            for( auto& step : info.steps )
                step.bpm = bpmValue;

        if( tableView.getNumRows() == 1 )
            tableView.selectRow( 0 );
        refreshTable();
    }

    // Kept for the interface: steps of deleted sequences stay in the song, marked "! missing sequence".
    void KickSongContent::displayState( juce::String& Song )
    {
        info = ae.getSongInfo( songName );
    }

    // A different song was chosen: nothing is selected in it yet.
    void KickSongContent::selectSong( const juce::String& Song )
    {
        tableView.clearSelection();
        updateSong( Song );
    }

    bool KickSongContent::hasMissingStep() const
    {
        for( auto& step : info.steps )
            if( song_logic::isMissingStep( step.seqNo ) )
                return true;
        return false;
    }

    // Returns true when the song is playing afterwards.
    bool KickSongContent::playOrResume( bool FromStart )
    {
        if( info.steps.empty() )
            return false;
        if( hasMissingStep() )
        {
            mt::utils::showNotification( "A step uses a deleted sequence: delete the marked step to play the song" );
            return false;
        }
        if( isPlaying && !FromStart )
            return true; // PLAY while playing leaves it running (it used to restart from the selected step)

        if( FromStart )
            tableView.selectRow( 0 );
        isPlaying = true;
        tableView.setInterceptsMouseClicks( false, false );
        pads->clearAllBreathing();
        ae.playSong( songName, juce::jmax( 0, tableView.getSelectedRowOrNone() ) );
        del.setEnabled( false );
        return true;
    }

    void KickSongContent::stopOrPause()
    {
        isPlaying = false;
        del.setEnabled( !isPlaying );
        tableView.setInterceptsMouseClicks( true, true );
        ae.stopSong();
        pads->resetBorderColor( selectedId );
        pads->clearAllBreathing();
        pads->setSelected( 0, juce::sendNotification );
        setPadLED( selectedId, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourIds::PadColourOrange ); // setPadLED subtracts the bank offset itself

        selectedId = -1;
        tableView.selectRow( 0 );
    }

    // The engine reported "song stopped" by itself (the end of the song): leave the playing lock, so the pads
    // and the table work again, and make sure the transport does not keep looping the last sequence.
    void KickSongContent::songEnded()
    {
        if( !song_logic::unlockAfterSongEnd( isPlaying ) )
            return;
        isPlaying = false;
        del.setEnabled( true );
        tableView.setInterceptsMouseClicks( true, true );
        pads->resetBorderColor( selectedId );
        pads->clearAllBreathing();
        selectedId = -1;
        if( ae.isSequencePlaying() )
            ae.stopSong();
        pads->setSelected( 0, juce::dontSendNotification );
        pads->setBreathing( 0 );
        tableView.selectRow( 0 );
    }

    void KickSongContent::refreshSequenceList()
    {
        ae.getSequenceDataList( seqList );

        pads->clearAllPadData();
        pads->setBank( "A" );
        for( int index = 0; index < seqList.size(); ++index )
        {
            pads->setActive( index, true );
            pads->setRow1Text( index, seqList[index]->name, juce::dontSendNotification );
            pads->setRow2Text( index, juce::String( (int)seqList[index]->length.get() ) + " Bar", juce::dontSendNotification );
            pads->setRow3Text( index, isSeqSong ? juce::String( bpmValue ) + " BPM" : juce::String( seqList[index]->tempo ) + " BPM", juce::dontSendNotification );
            setPadLED( index % 16, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
        }
        pads->refreshAllPads();
    }

    void KickSongContent::Header::resized()
    {
        sequenceLabel.setBoundsRelative( .018f, .09f, .18f, .82f );
        repeatLabel.setBoundsRelative( .272f, .09f, .18f, .82f );
        bpmLabel.setBoundsRelative( .523f, .09f, .18f, .82f );
        barLabel.setBoundsRelative( .748f, .09f, .18f, .82f );
    }

    void KickSongContent::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, tableView.getBounds(), ui::Theme::backgroundColour( 3 ) );
        mt::utils::renderRoundedRect( g, tableView.getBounds(), ui::Theme::outlineColour( 3 ) );
    }

    void KickSongContent::resized()
    {
        tableView.setBoundsRelative( 0.f, 0.f, .54f, .887f );
        pads->setBoundsRelative( 0.55f, 0.f, 0.45f, 0.9f );

        clr.setBoundsRelative( 0.f, 0.91f, 0.1174f, 0.09f );

        // No "Convert >SEQ" button: converting a song into one sequence needs an engine function that does not exist
        // (the song is a list of sequence steps played in order); the unlaid 0x0 stub was removed.
        insert.setBoundsRelative( 0.7534f, 0.91f, 0.1174f, 0.09f );
        del.setBoundsRelative( 0.8747f, 0.91f, 0.1174f, 0.09f );
    }

    void KickSongContent::visibilityChanged()
    {
        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );
        if( !visible )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );

            contracts::DclSignals::refreshPadLeds();
        }
        else
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
            refreshSequenceList();
        }
    }

    void KickSongContent::setSequenceToPlay( int CurrentPlaying, int NextSequence )
    {
        if( CurrentPlaying >= 0 )
        {
            pads->resetBorderColor( selectedId );
            setPadLED( selectedId, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourIds::PadColourOrange );
            pads->removeBreathing( CurrentPlaying );
            setPadLED( CurrentPlaying, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 );
            pads->setBorderColor( CurrentPlaying, ui::Theme::selectionColour( 2 ), 2, juce::dontSendNotification );
            if( NextSequence != CurrentPlaying && NextSequence >= 0 )
            {
                pads->removeBreathing( selectedId );
                pads->setBreathing( NextSequence );
                setPadLED( NextSequence, contracts::DeviceControlContracts::PadButtonState::StateOn, contracts::DeviceControlContracts::PadColourIds::padColourlightGreen1 );
            }
            selectedId = CurrentPlaying;
        }
    }

    void KickSongContent::setPadLED( const int id, int state, int color )
    {
        int idx = id - pads->getBankOffset();
        if( juce::isPositiveAndBelow( idx, 16 ) )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadDataMessage( idx, state, color ) );
        }
    }

    void KickSongContent::seqSongState( bool state )
    {
        isSeqSong = state;
    }

    void KickSongContent::setBpmValue( float bpm )
    {
        bpmValue = bpm;
        if( isSeqSong )
        {
            refreshSequenceList();
            updateSong( songName );
        }
    }

    ///
    /// Kick Song Mode...
    ///

    KickSongMode::KickSongMode()
        : kick_ui::IScreen( kick_ui::Screen::MenuSong )
    {
        setLookAndFeel( ui::KickLooknFeel::getLnf() );

        mt::utils::addChildComponents( this, header, content, songListBox, bpmNumber, exportt, seqSongBpm, renameSong, deleteSongBtn );
        mt::utils::setValueFontHeight( 18, renameSong, deleteSongBtn );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), renameSong, deleteSongBtn );

        // header.showSettingsButton( true );
        songListBox.setJustificationType( juce::Justification::centredLeft );
        songListBox.setBackGroundColours( ui::Theme::backgroundColour( 13 ), ui::Theme::backgroundColour( 2 ) );
        bpmNumber.setColours( ui::Theme::textColour( 2 ), ui::Theme::textColour( 1 ) );
        bpmNumber.setValueBackgroundColour( ui::Theme::backgroundColour( 13 ) );
        bpmNumber.setTitleProportion( .35f );
        bpmNumber.setTitleFontHeight( 16 );
        songListBox.setTitleFontHeight( 16 );

        seqSongBpm.setValueColour( ui::Theme::textColour( 1 ) );

        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        // Touch Play / Stop in the header (no hardware needed).
        header.showPlayButton( true );
        header.onPlayStopClick = [this]( bool Play ) {
            if( Play )
            {
                if( !content.playOrResume( false ) )
                    header.setPlayState( false, juce::dontSendNotification );
            }
            else
                content.stopOrPause();
        };

        mt::signals::onSongStopped.connect( [&]() {
            juce::MessageManager::callAsync( [this] {
                content.songEnded();
                header.setPlayState( false, juce::dontSendNotification );
            } );
        } );

        songListBox.onSelectionChanged = [&]( int Index, const juce::String& SongName ) {
            if( SongName.equalsIgnoreCase( "create new song" ) )
                askSongName( "CREATE NEW SONG", {}, [this]( const juce::String& N ) {
                    mt::getAudioInterface().createSong( N );
                    pendingSelect = N;
                    triggerAsyncUpdate();
                } );
            else
                content.selectSong( SongName );
        };

        renameSong.onClick = [&]() {
            const auto current = songListBox.getSelected();
            renameSong.setToggleState( false, juce::dontSendNotification );
            if( current.isEmpty() || current.equalsIgnoreCase( "create new song" ) )
                return;
            askSongName( "RENAME SONG", current, [this, current]( const juce::String& N ) {
                ae.renameSong( current, N );
                pendingSelect = N;
                triggerAsyncUpdate();
            } );
        };

        deleteSongBtn.onClick = [&]() {
            const auto current = songListBox.getSelected();
            deleteSongBtn.setToggleState( false, juce::dontSendNotification );
            if( current.isEmpty() || current.equalsIgnoreCase( "create new song" ) )
                return;
            if( content.isSongPlaying() )
                content.stopOrPause();
            ae.deleteSong( current ); // undoable (UNDO brings the song back)
            pendingSelect = {};
            triggerAsyncUpdate();
        };

        exportt.onClick = [&]() {
            auto savePopUp = new SaveFile( songListBox.getSelected() );
            mt::utils::showOptions( "EXPORT", savePopUp, [this, savePopUp]( int R ) {
                if( savePopUp )
                {
                    if( R == UI::APPLY )
                    {
                    }
                }
                delete savePopUp;
            } );
            exportt.setToggleState( false, juce::dontSendNotification );
        };
        seqSongBpm.onClick = [&]() {
            content.seqSongState( seqSongBpm.getToggleState() );
            content.updateSong( songListBox.getSelected() );
            content.refreshSequenceList();
        };

        bpmNumber.onValueChanged = [this]( juce::int64 Value ) {
            bpmValue = bpmNumber.getValue();
            content.setBpmValue( bpmValue );
        };

        ui::themeChanged.connect( [&]() {
            exportt.setImages( "Song_Export", "Song_Export_Active" );
            header.setIcon( "Icon_SongEdit" );
        } );
    }

    void KickSongMode::handleAsyncUpdate()
    {
        auto list     = mt::getAudioInterface().getSongsList();
        auto selected = songListBox.getSelectedIndex();

        // Which song is shown afterwards: a just created / renamed one, else the same row, else the first.
        int show = -1;
        for( size_t i = 0; i < list.size(); ++i )
            if( pendingSelect.isNotEmpty() && list[i].equalsIgnoreCase( pendingSelect ) )
                show = (int)i;
        if( show < 0 )
            show = list.empty() ? -1 : juce::jlimit( 0, (int)list.size() - 1, selected );
        pendingSelect = {};

        list.push_back( "CREATE NEW SONG" );
        songListBox.setListOptions( list, show >= 0 ? show : (int)list.size() - 1 );
        content.selectSong( show >= 0 ? list[(size_t)show] : juce::String() );
    }

    // Name entry for a new / renamed song: not empty, not the reserved entry, not a duplicate.
    void KickSongMode::askSongName( const juce::String& Title, const juce::String& Initial, std::function<void( const juce::String& )> OnName )
    {
        auto& _cns          = ui::KickVirtualKeyBoard::getWithText( Initial );
        _cns.onEnterPressed = [&_cns, this]( int v ) {
            mt::utils::exitDialog( _cns.getCurrentlyFocusedComponent(), v );
        };
        _cns.show(
            Title, ui::KickVirtualKeyBoard::Type::KEYS_LOWERCASE_ALPHABETIC,
            [&_cns, this, Initial, OnName]( int R ) {
                if( R == UI::APPLY )
                {
                    const juce::String name = _cns.getInputText().trim();
                    std::vector<std::string> existing;
                    for( auto& n : ae.getSongsList() )
                        existing.push_back( n.toStdString() );

                    switch( song_logic::checkName( name.toStdString(), existing, Initial.toStdString() ) )
                    {
                        case song_logic::NameCheck::Ok: OnName( name ); return;
                        case song_logic::NameCheck::Duplicate: warningPopup(); return;
                        case song_logic::NameCheck::Empty:
                        case song_logic::NameCheck::Reserved:
                            mt::utils::showNotification( "Enter a song name" );
                            break;
                    }
                }
                // Cancelled / refused: back to the song that was shown.
                triggerAsyncUpdate();
            },
            ui::layout::keyBoardPopup );
    }

    void KickSongMode::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( ae.isProjectLoaded() && contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            setPlayState( buttonCode );
        }
        else if( ae.isProjectLoaded() && contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
                knobCommand( knob, value );
        }
    }

    void KickSongMode::knobCommand( int knob, int value )
    {
        applySequenceKnob( ae, metronome, knob, value );
    }

    void KickSongMode::resized()
    {
        auto b = getLocalBounds();

        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );

        songListBox.setBoundsRelative( 0.225f, .029f, 0.1956f, .082f );
        bpmNumber.setBoundsRelative( 0.43f, .029f, 0.0724f, 0.082f );
        seqSongBpm.setBoundsRelative( .513f, .029f, .049f, .082f );

        exportt.setBoundsRelative( .738f, .029f, .049f, .082f );
        renameSong.setBoundsRelative( .440f, .029f, .115f, .082f );
        deleteSongBtn.setBoundsRelative( .565f, .029f, .135f, .082f );

        content.setBounds( b.reduced( 10 ) );
    }

    void KickSongMode::setPlayState( int Type )
    {
        switch( Type )
        {
            case contracts::DeviceControlContracts::EN_PLAY: header.setPlayState( content.playOrResume( false ), juce::dontSendNotification ); break;
            case contracts::DeviceControlContracts::EN_PLAYSTART: header.setPlayState( content.playOrResume( true ), juce::dontSendNotification ); break;
            case contracts::DeviceControlContracts::EN_STOP:
                content.stopOrPause();
                header.setPlayState( false, juce::dontSendNotification );
                break;
        }
    }

    void KickSongMode::warningPopup()
    {
        auto p = new ui::FilePopup( "A song with the same name already exists", false, false );
        mt::utils::showDialog(
            "WARNING!", p, [&, p]( int R ) {
                delete p;
            },
            { ui::layout::warningPopup }, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        songListBox.setSelected( 0 );
    }

    void KickSongMode::visibilityChanged()
    {
        bool visible = isVisible();
        songListBox.setFocus( visible );
        for( auto it : getChildren() )
            it->setVisible( visible );
        bpmNumber.setVisible( visible && kSongTempoOverrideAvailable );
        seqSongBpm.setVisible( visible && kSongTempoOverrideAvailable );
        exportt.setVisible( visible && kSongExportAvailable );
        header.setPlayState( false, juce::dontSendNotification );
        if( visible )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( mt::PadColorMode::DISABLE_PAD_COLOUR_MODE ) );
            triggerAsyncUpdate();
        }
        else
        {
            auto MTIProg = ae.getProgramData( ae.getSelectedProgram() );
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                         setPadColorModeMessage( MTIProg ? MTIProg->colourMode.get() : mt::OFF ) );
        }
    }

    ///
    /// KickSongTableDisplay
    ///

    KickSongTableDisplay::KickSongTableDisplay()
    {
        mt::utils::setValueFontHeight( 16, seqName, repeat, bpm, bars );
        mt::utils::setValueJustification( juce::Justification::centred, seqName, repeat, bpm, bars );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), seqName, repeat, bpm, bars );
        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), seqName, repeat, bpm, bars );
        mt::utils::addAndMakeVisible( this, seqName, repeat, bpm, bars );
        mt::utils::setEnabled( false, bpm, bars );

        repeat.onValueChanged = [&]( const int value ) {
            step.repeatCount = value;
            if( onRepeatChanged )
                onRepeatChanged( value );
        };
    }

    void KickSongTableDisplay::refreshStep( const mt::MTSongStepInfo& Step )
    {
        step = Step;
        seqName.setText( step.dispName, juce::dontSendNotification );
        // No notification: only the user's own change may write to the engine (onRepeatChanged).
        repeat.setValue( step.repeatCount, juce::dontSendNotification );
        seqName.setTextColour( song_logic::isMissingStep( step.seqNo ) ? juce::Colours::red : ui::Theme::textColour( 1 ) );
        bpm.setValue( step.bpm, juce::dontSendNotification );
        bars.setValue( step.bars, juce::dontSendNotification );
    }

    void KickSongTableDisplay::resized()
    {
        seqName.setBoundsRelative( .018f, .09f, .18f, .82f );
        repeat.setBoundsRelative( .272f, .09f, .18f, .82f );
        bpm.setBoundsRelative( .523f, .09f, .18f, .82f );
        bars.setBoundsRelative( .748f, .09f, .18f, .82f );
    }
}
