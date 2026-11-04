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

    AppProgramOptions::AppProgramOptions( void )
        : prg( 0 )
    {
        setName( "Program Options" );
        setVisible( false );

        mt::utils::addAndMakeVisible( this, srNo, prg, opts, close );
        srNo.setInterceptsMouseClicks( false, false );

        close.onClick = [&]() {
            if( onClose )
                onClose();
        };
        prg.onProgChange = [&]( int no ) {
            if( onprogChanged )
                onprogChanged();
        };

        opts.onMenuOptionClicked = [&]( const juce::String& /*option*/, int Index, const juce::String& /*group*/ ) {
            auto& ae = mt::getAudioInterface();

            switch( Index )
            {
                break;
                case 0: // copy pads
                    openCopyPadsPopup();
                    break;
                case 1: // Duplicate Program
                {
                    ae.duplicateProgram( ae.getSelectedProgram() );
                    if( onClose )
                        onClose();
                }
                break;
                case 2:
                {
                    duplicateProgramToTrack();
                    if( onClose )
                        onClose();
                }
                break;
                case 3: // Pad Colour
                    signals::sigScreenOpen( Screen::MenuColorEdit );
                    break;
                case 4: // MIDI Settings
                    openMidiSettingsPopup();
                    break;
            }
        };

        ui::themeChanged.connect( [&]() {
            srNo.setImage( "ID_Program" );
            close.setImages( "Edit_Close" );
        } );
    }
    void AppProgramOptions::duplicateProgramToTrack()
    {
        auto& ae = mt::getAudioInterface();
        ae.duplicateProgram( ae.getSelectedProgram() );
        if( !ae.getNumSequences() )
            ae.createSequence();
        auto track = ae.createTrack( ae.getSelectedSequence() );
        ae.setSelectedTrack( track );
        auto          pProg = ae.getSelectedProgramData();
        juce ::String sel   = pProg ? pProg->name.get() : "";
        ae.setProgramToTrack( sel, ae.getSelectedSequence(), track );
    }
    //==============================================================================
    void AppProgramOptions::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 5 ) );
        mt::utils::fillRoundedRect( g, { 0, 0, 1000, 386 }, ui::Theme::backgroundColour( 1 ) );
        mt::utils::fillRoundedRect( g, { 956, 0, 43, 386 }, ui::Theme::backgroundColour( 12 ) );
    }

    void AppProgramOptions::resized()
    {
        srNo.setBoundsRelative( .0f, .0f, .0429f, .9698f );
        prg.setBoundsRelative( .0f, .0f, .9765f, .2814f );
        opts.setBoundsRelative( .0585f, .1414f, .8544f, .4924f );
        close.setBoundsRelative( .9531f, .0234f, 0.023f, 0.05633f );
    }

    void AppProgramOptions::handleCommand( CPktMgr::HWMsg_t& HwMsg )
    {
        auto fc = ui::getComponentInFocus();
        if( nullptr == fc || &opts == fc )
            opts.handleCommand( HwMsg );
    }

    void AppProgramOptions::visibilityChanged()
    {
        if( isVisible() )
        {
            prg.setProgram( mt::getAudioInterface().getSelectedProgram() );
            srNo.setText( juce::String( prg.getProgNumber() + 1 ) );
            srNo.setImage( getEditProgramIcon( prg.getProgramType() ) );
        }
        opts.visibilityChanged();
        prg.triggerAsyncUpdate();
    }

    void AppProgramOptions::copyPadDataToPad()
    {
        auto sourcePad          = copyPads.getSourcePad();
        auto destinationPad     = copyPads.getDestinationPad();
        auto sourceProgram      = copyPads.getSourceProgram();
        auto destinationProgram = copyPads.getDestinationProgram();

        if( !( sourcePad == destinationPad && sourceProgram == destinationProgram ) )
        {
            auto& ae = mt::getAudioInterface();
            ae.copyPadDataToPad( ae.getProgramIndexByName( sourceProgram ), ae.getProgramIndexByName( destinationProgram ), sourcePad, destinationPad );
        }
        else
            openCopyPadsPopup( true, "You Cannot Copy The Same Pad Over Itself" );
    }

    void AppProgramOptions::copyPadDataToBank()
    {
        auto sourceBank         = copyPads.getSourceBank();
        auto destinationBank    = copyPads.getDestinationBank();
        auto sourceProgram      = copyPads.getSourceProgram();
        auto destinationProgram = copyPads.getDestinationProgram();
        if( !( sourceBank == destinationBank && sourceProgram == destinationProgram ) )
        {
            auto& ae = mt::getAudioInterface();
            ae.copyPadsDataToBank( ae.getProgramIndexByName( sourceProgram ), ae.getProgramIndexByName( destinationProgram ), sourceBank, destinationBank );
        }
        else
            openCopyPadsPopup( true, "You Cannot Copy The Same Bank Over Itself" );
    }

    void AppProgramOptions::copyPadDataToRange()
    {
        auto sourceProgram       = copyPads.getSourceProgram();
        auto destinationProgram  = copyPads.getDestinationProgram();
        auto sourceStartPad      = copyPads.getSourceStartPad();
        auto sourceEndPad        = copyPads.getSourceEndPad();
        auto destinationStartPad = copyPads.getDestinationStartPad();
        if( !( sourceStartPad == destinationStartPad && sourceProgram == destinationProgram ) )
        {
            auto& ae = mt::getAudioInterface();
            ae.copyPadsDataToRange( ae.getProgramIndexByName( sourceProgram ), ae.getProgramIndexByName( destinationProgram ), sourceStartPad, sourceEndPad, destinationStartPad );
        }
        else
            openCopyPadsPopup( true, "You Cannot Copy The Same Pad Over Itself" );
    }

    void AppProgramOptions::openCopyPadsPopup( bool isWarning, juce::String WarningInfo )
    {
        if( isWarning )
        {
            auto p = new WarningPopUp( WarningInfo );
            mt::utils::showDialog(
                "COPY PADS ERROR", p, [this, p]( int R ) {
                    delete p;
                    openCopyPadsPopup();
                },
                ui::layout::warningPopup );
        }
        else
        {
            mt::utils::showOptions( "COPY PADS", &copyPads, [this]( int R ) {
                if( R == UI::APPLY )
                {
                    auto copyTypeIndex = copyPads.getCopyTypeIndex();
                    if( copyTypeIndex == 0 )
                        copyPadDataToPad();
                    else if( copyTypeIndex == 1 )
                        copyPadDataToBank();
                    else if( copyTypeIndex == 2 )
                        copyPadDataToRange();
                    if( onClose )
                        onClose();
                }
            } );
        }
    }

    void AppProgramOptions::openMidiSettingsPopup()
    {
        auto& ae = mt::getAudioInterface();
        auto progIdx = ae.getSelectedProgram();
        auto prog = ae.getProgram( progIdx );
        if( !prog || prog->type.get() != mt::MTProgramType::Midi )
        {
            auto p = new WarningPopUp( "Selected program is not a MIDI program" );
            mt::utils::showDialog(
                "MIDI SETTINGS", p, [p]( int ) { delete p; },
                ui::layout::warningPopup );
            return;
        }

        midiSettings.setProgram( progIdx );
        mt::utils::showOptions( "MIDI PROGRAM SETTINGS", &midiSettings, [this]( int R ) {
            if( R == UI::APPLY )
            {
                midiSettings.applySettings();
                if( onClose )
                    onClose();
            }
        } );
    }

    AppProgramOptionsMenu::AppProgramOptionsMenu()
        : KickMenuComponent( "ProgramOptions", 5 )
    {
        addOption( { "Copy Pads", "COPY Pads" }, "" );
        addOption( { "Duplicate Program", "Duplicate_Program" }, "" );
        addOption( { "Duplicate Program To Track", "Duplicate Program To Track" }, "" );
        addOption( { "Pad Colour", "Pad Colour" }, "" );
        addOption( { "MIDI Settings", "MIDI Settings" }, "" );

        setVisible( true );
        setSelectionEnabled( false );
    }

    AppProgramOptionsMenu::~AppProgramOptionsMenu()
    {
        setVisible( false );
    }

    CopyPads::CopyPads()
    {
        mt::utils::addAndMakeVisible( this, copyType, fromProgram, toProgram, fromPads, toPads, fromBank, toBank, fromPadsStart, fromPadsEnd, toPadsStart );
        mt::utils::setTitleFontHeight( 16, copyType, fromProgram, toProgram, fromPads, toPads, fromBank, toBank, fromPadsStart, fromPadsEnd, toPadsStart );
        mt::utils::setValueFontHeight( 18, copyType, fromProgram, toProgram, fromPads, toPads, fromBank, toBank, fromPadsStart, fromPadsEnd, toPadsStart );
        mt::utils::makeHidden( fromBank, toBank, fromPadsStart, fromPadsEnd, toPadsStart );
        fromProgram.setJustificationType( juce::Justification::centred );
        toProgram.setJustificationType( juce::Justification::centred );
        fromPads.setJustificationType( juce::Justification::centred );
        toPads.setJustificationType( juce::Justification::centred );
        copyType.setJustificationType( juce::Justification::centred );
        fromBank.setJustificationType( juce::Justification::centred );
        toBank.setJustificationType( juce::Justification::centred );
        fromPadsStart.setJustificationType( juce::Justification::centred );
        fromPadsEnd.setJustificationType( juce::Justification::centred );
        toPadsStart.setJustificationType( juce::Justification::centred );
        fromProgram.setEnabled( false );

        fromBank.setListOptions( mt::Kick::padBankNames );
        toBank.setListOptions( mt::Kick::padBankNames );
        int letterAsciiCode = 65;

        std::vector<juce::String> padData;
        for( int i = 0; i <= kick_Constants::totalPianoKeys; i += 16 )
        {
            for( int j = 0; j < 16; j++ )
            {
                juce::String noteLabel;
                noteLabel = noteLabel.formatted( "%c%02d", letterAsciiCode, ( j + 1 ) );
                padData.push_back( noteLabel );
            }
            letterAsciiCode++;
        }
        fromPads.setListOptions( padData );
        toPads.setListOptions( padData );
        fromPadsStart.setListOptions( padData );
        fromPadsEnd.setListOptions( padData );
        toPadsStart.setListOptions( padData );

        copyType.onSelectionChanged = [&]( int Index, const juce::String& ) {
            if( Index == 0 )
            {
                mt::utils::makeVisible( fromPads, toPads );
                mt::utils::makeHidden( fromBank, toBank, fromPadsStart, fromPadsEnd, toPadsStart );
            }
            else if( Index == 1 )
            {
                mt::utils::makeVisible( fromBank, toBank );
                mt::utils::makeHidden( fromPads, toPads, fromPadsStart, fromPadsEnd, toPadsStart );
            }
            else if( Index == 2 )
            {
                mt::utils::makeVisible( fromPadsStart, fromPadsEnd, toPadsStart );
                mt::utils::makeHidden( fromBank, toBank, fromPads, toPads );
            }
        };
    }

    void CopyPads::resized()
    {
        copyType.setBoundsRelative( .1f, .1f, .8f, .2f );

        fromProgram.setBoundsRelative( .1f, .4f, .35f, .2f );
        toProgram.setBoundsRelative( .55f, .4f, .35f, .2f );
        //Pads
        fromPads.setBoundsRelative( .1f, .7f, .35f, .2f );
        toPads.setBoundsRelative( .55f, .7f, .35f, .2f );
        //Banks
        fromBank.setBoundsRelative( .1f, .7f, .35f, .2f );
        toBank.setBoundsRelative( .55f, .7f, .35f, .2f );
        //Range
        fromPadsStart.setBoundsRelative( .1f, .7f, .15f, .2f );
        fromPadsEnd.setBoundsRelative( .3f, .7f, .15f, .2f );
        toPadsStart.setBoundsRelative( .55f, .7f, .35f, .2f );
    }

    void CopyPads::visibilityChanged()
    {
        if( isVisible() )
        {
            auto&                     ae          = mt::getAudioInterface();
            auto                      programList = ae.getProgramList();
            std::vector<juce::String> samplePrograms;
            for( auto it = 0; it < programList.size(); ++it )
            {
                auto programType = ae.getProgramType( programList[it] );
                if( programType == mt::MTProgramType::Drum || programType == mt::MTProgramType::Keygroup || programType == mt::MTProgramType::Clip || programType == mt::MTProgramType::Audio )
                    samplePrograms.push_back( programList[it] );
            }
            fromProgram.setListOptions( samplePrograms );
            toProgram.setListOptions( samplePrograms );
        }
    }

    //==============================================================================
    // MidiProgramSettings
    //==============================================================================
    MidiProgramSettings::MidiProgramSettings()
    {
        mt::utils::addAndMakeVisible( this, noteMin, noteMax, learnBtn, velMin, velMax, transpose,
                                      filterModWheel, filterPitchBend, filterAftertouch, filterProgChange, filterSustain, filterAllCC );

        mt::utils::setTitleFontHeight( 16, noteMin, noteMax, velMin, velMax, transpose );
        mt::utils::setValueFontHeight( 18, noteMin, noteMax, velMin, velMax, transpose );

        learnBtn.setValueFontHeight( 16 );
        mt::utils::setValueFontHeight( 14, filterModWheel, filterPitchBend, filterAftertouch, filterProgChange, filterSustain, filterAllCC );

        learnBtn.onClick = [this]() {
            auto& ae = mt::getAudioInterface();
            auto prog = ae.getProgram( currentProgIndex );
            if( prog )
            {
                if( learnBtn.getToggleState() )
                    prog->startNoteLearn();
                else
                    prog->stopNoteLearn();
            }
        };
    }

    void MidiProgramSettings::setProgram( int progIndex )
    {
        currentProgIndex = progIndex;
        auto& ae = mt::getAudioInterface();
        auto prog = ae.getProgram( currentProgIndex );
        auto midiData = dynamic_cast<mt::MTIMidiProgramData*>( prog );
        if( prog && midiData )
        {
            noteMin.setSelected( midiData->noteMin.get(), juce::dontSendNotification );
            noteMax.setSelected( midiData->noteMax.get(), juce::dontSendNotification );
            velMin.setSelected( midiData->velMin.get() - 1, juce::dontSendNotification );
            velMax.setSelected( midiData->velMax.get() - 1, juce::dontSendNotification );
            transpose.setSelected( prog->transpose.get() + 36, juce::dontSendNotification );

            filterModWheel.setToggleState( midiData->filterModWheel.get(), juce::dontSendNotification );
            filterPitchBend.setToggleState( midiData->filterPitchBend.get(), juce::dontSendNotification );
            filterAftertouch.setToggleState( midiData->filterAftertouch.get(), juce::dontSendNotification );
            filterProgChange.setToggleState( midiData->filterProgramChange.get(), juce::dontSendNotification );
            filterSustain.setToggleState( midiData->filterSustain.get(), juce::dontSendNotification );
            filterAllCC.setToggleState( midiData->filterAllCC.get(), juce::dontSendNotification );

            learnBtn.setToggleState( false, juce::dontSendNotification );
            prog->stopNoteLearn();
        }
    }

    void MidiProgramSettings::applySettings()
    {
        auto& ae = mt::getAudioInterface();
        auto prog = ae.getProgram( currentProgIndex );
        auto midiData = dynamic_cast<mt::MTIMidiProgramData*>( prog );
        if( prog && midiData )
        {
            prog->stopNoteLearn();
            midiData->noteMin = noteMin.getSelectedIndex();
            midiData->noteMax = noteMax.getSelectedIndex();
            midiData->velMin = velMin.getSelectedIndex() + 1;
            midiData->velMax = velMax.getSelectedIndex() + 1;
            prog->transpose = transpose.getSelectedIndex() - 36;

            midiData->filterModWheel = filterModWheel.getToggleState();
            midiData->filterPitchBend = filterPitchBend.getToggleState();
            midiData->filterAftertouch = filterAftertouch.getToggleState();
            midiData->filterProgramChange = filterProgChange.getToggleState();
            midiData->filterSustain = filterSustain.getToggleState();
            midiData->filterAllCC = filterAllCC.getToggleState();
        }
    }

    void MidiProgramSettings::visibilityChanged()
    {
        if( isVisible() && currentProgIndex >= 0 )
        {
            setProgram( currentProgIndex );
        }
    }

    void MidiProgramSettings::resized()
    {
        noteMin.setBoundsRelative( .05f, .06f, .26f, .24f );
        noteMax.setBoundsRelative( .37f, .06f, .26f, .24f );
        learnBtn.setBoundsRelative( .69f, .10f, .26f, .16f );

        velMin.setBoundsRelative( .05f, .34f, .26f, .24f );
        velMax.setBoundsRelative( .37f, .34f, .26f, .24f );
        transpose.setBoundsRelative( .69f, .34f, .26f, .24f );

        filterPitchBend.setBoundsRelative( .05f, .64f, .26f, .14f );
        filterModWheel.setBoundsRelative( .37f, .64f, .26f, .14f );
        filterAftertouch.setBoundsRelative( .69f, .64f, .26f, .14f );

        filterProgChange.setBoundsRelative( .05f, .81f, .26f, .14f );
        filterSustain.setBoundsRelative( .37f, .81f, .26f, .14f );
        filterAllCC.setBoundsRelative( .69f, .81f, .26f, .14f );
    }
}
