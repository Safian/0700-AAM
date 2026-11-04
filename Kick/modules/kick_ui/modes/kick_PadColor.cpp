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
    // The pure mapping (kick_PadColorLogic.h) uses the same numbers as the engine's enums.
    static_assert( pad_color_logic::Off == mt::OFF && pad_color_logic::OffVelocity == mt::OFF_VELOCITY && pad_color_logic::OffFull == mt::OFF_FULL, "pad colour mode numbering" );
    static_assert( pad_color_logic::DimOff == mt::DIM_OFF && pad_color_logic::DimVelocity == mt::DIM_VELOCITY && pad_color_logic::DimFull == mt::DIM_FULL, "pad colour mode numbering" );
    static_assert( pad_color_logic::OffDimOff == mt::OFF_DIM_OFF && pad_color_logic::OffDimVelocity == mt::OFF_DIM_VELOCITY && pad_color_logic::OffDimFull == mt::OFF_DIM_FULL, "pad colour mode numbering" );
    static_assert( pad_color_logic::kPadOffDim == PAD_OFF_DIM && pad_color_logic::kPadOffOff == PAD_OFF_OFF, "pad off numbering" );
    static_assert( pad_color_logic::kOnVelocity == PAD_ON_VELOCITY && pad_color_logic::kOnFull == PAD_ON_FULL && pad_color_logic::kOnOff == PAD_ON_OFF, "pad on numbering" );
    static_assert( pad_color_logic::kPadOffDim == mt::PAD_DIM_STATE && pad_color_logic::kPadOffOff == mt::PAD_OFF_STATE, "pad off state numbering" );

    KickPadColorMode::KickPadColorMode()
        : kick_ui::IScreen( kick_ui::Screen::MenuColorEdit )
    {
        left.selectedColor = &transparent;
        mt::utils::addAndMakeVisible( this, left, right, hdrComp, warningLabel, bankLabel, leftNav, rightNav );
        warningLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        warningLabel.setValueFontHeight( 25 );
        setLookAndFeel( ui::KickLooknFeel::getLnf() );
        mt::utils::addChildComponents( this, header, hdrComp );
        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        header.showSettingsButton( true );

        bankLabel.setText( right.pads->getBank(), juce::dontSendNotification );
        bankLabel.setTextColour( ui::Theme::textColour( 1 ) );
        bankLabel.setValueFontHeight( 24 );
        bankLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 3 ) );
        bankLabel.setValueJustification( juce::Justification::centred );

        left.emptyPadMode.onClick = [&]() {
            ae.setProgramEmptyPadColorMode( ae.getSelectedProgram(), left.emptyPadMode.getToggleState() ? mt::EmptyPadColorMode::Dim : mt::EmptyPadColorMode::Off );
            sendColourModeToHardware( left.padOff.getCurrentSelection(), left.padOn.getCurrentSelection() );
        };

        left.padOff.onClick = [&]() {
            left.padOff.incrementSelection();
            setPadOffModeSelection( left.padOff.getCurrentSelection() );
            sendColourModeToHardware( left.padOff.getCurrentSelection(), left.padOn.getCurrentSelection() );
        };

        left.padOn.onClick = [&]() {
            left.padOn.incrementSelection();
            setPadOnModeSelection( left.padOn.getCurrentSelection() );
            sendColourModeToHardware( left.padOff.getCurrentSelection(), left.padOn.getCurrentSelection() );
        };

        hdrComp.onSelectionChanged = [&]( const juce::String& Program, const mt::MTProgramType Type ) {
            ae.setSelectedProgram( Program );
            refresh();
            refreshLEDs();
        };

        // Add listener function for pad button clicks...
        right.pads->OnButtonClicked = [this]( int Id ) {
            buttonClicked( Id );
        };

        leftNav.onClick = [&]() {
            auto index = right.pads->getBankIndex();
            if( index == 0 )
                right.pads->setBank( 0 );
            else
                right.pads->setBank( index - 1 );
            ae.setBank( right.pads->getBank() );
        };

        rightNav.onClick = [&] {
            auto index = right.pads->getBankIndex();
            if( index == 7 )
                right.pads->setBank( 7 );
            else
                right.pads->setBank( index + 1 );
            ae.setBank( right.pads->getBank() );
        };

        mt::signals::bankChanged.connect( [&]( int Index, juce::String Bank ) {
            setBank( Index, Bank );
        } );

        // Set default states...
        left.selectedColor->setToggleState( true, juce::NotificationType::dontSendNotification );
        hdrComp.toFront( true );
        leftNav.toFront( true );
        rightNav.toFront( true );
        bankLabel.toFront( true );

        ui::themeChanged.connect( [&]() {
            header.setIcon( "Icon_ColourEdit" );
        } );

        // Shift + tap on a colour button: every pad takes that colour.
        left.onColourAll = [this]() { colourAllPads(); };

        // Make Default (the manual): the current colours and pad colour modes of this program become the defaults of every
        // program of the same type (new programs start with them); asks first (Do It / Cancel).
        right.defaultBtn.onClick = [&]() {
            right.defaultBtn.setToggleState( false, juce::dontSendNotification );
            if( !ae.getNumPrograms() )
                return;
            auto* p = new ui::FilePopup{ "Make the current pad colours the default of all programs of this type?", false, false };
            mt::utils::showOptions(
                "MAKE DEFAULT", p, [this, p]( int R ) {
                    if( R == UI::APPLY && ae.makeProgramPadColoursDefault( ae.getSelectedProgram() ) == 0 )
                        mt::utils::showNotification( "Default pad colours saved for this program type" );
                    delete p;
                },
                ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };

        // Reset: every pad of this program back to the default of its type (the stored Make Default, else the factory
        // colours), one undo step; asks first, UNDO stays one tap away in the status bar.
        right.resetBtn.onClick = [&]() {
            right.resetBtn.setToggleState( false, juce::dontSendNotification );
            if( !ae.getNumPrograms() )
                return;
            auto* p = new ui::FilePopup{ "Set all pad colours of this program to the default of its type?", false, false };
            mt::utils::showOptions(
                "RESET PAD COLOURS", p, [this, p]( int R ) {
                    if( R == UI::APPLY )
                    {
                        {
                            mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Default pad colours" );
                            ae.resetProgramPadColours( ae.getSelectedProgram() );
                        }
                        refresh();
                        refreshLEDs();
                        offerUndo( "Pad colours reset to the default" );
                    }
                    delete p;
                },
                ui::layout::warningPopup, ui::Theme::backgroundColour( 1 ), ui::Theme::titleColour( 1 ), ui::Theme::backgroundColour( 2 ) );
        };
    }

    void KickPadColorMode::setPadOnModeSelection( int selection )
    {
        if( selection == PAD_ON_STATE::PAD_ON_VELOCITY )
            left.padOn.setButtonText( "Velocity" );
        else if( selection == PAD_ON_STATE::PAD_ON_FULL )
            left.padOn.setButtonText( "Full" );
        else if( selection == PAD_ON_STATE::PAD_ON_OFF )
            left.padOn.setButtonText( "Off" );
    }

    void KickPadColorMode::setPadOffModeSelection( int selection )
    {
        if( selection == PAD_OFF_STATE::PAD_OFF_DIM )
            left.padOff.setButtonText( "Dim" );
        else if( selection == PAD_OFF_STATE::PAD_OFF_OFF )
            left.padOff.setButtonText( "Off" );
    }
    void KickPadColorMode::sendColourModeToHardware( int padOff, int padOn )
    {
        // Always one of the nine real modes (it used to send INVALID for an unexpected selection).
        const auto padColorMode = (mt::PadColorMode)pad_color_logic::encode( padOff, padOn, left.emptyPadMode.getToggleState() );
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                     setPadColorModeMessage( padColorMode ) );
        ae.setProgramColourMode( ae.getSelectedProgram(), padColorMode );
    }

    int KickPadColorMode::getPadOffColourMode( mt::PadColorMode mode ) { return pad_color_logic::decodePadOff( mode ); }
    int KickPadColorMode::getPadOnColourMode( mt::PadColorMode mode ) { return pad_color_logic::decodePadOn( mode ); }
    bool KickPadColorMode::getEmptyColourMode( mt::PadColorMode mode ) { return pad_color_logic::decodeEmptyDim( mode ); }

    // One pad's border: the cheap update after a single pad was coloured.
    void KickPadColorMode::refreshPad( int Id )
    {
        auto progData = ae.getSelectedProgramData();
        if( juce::isPositiveAndBelow( Id, 128 ) )
            right.pads->setBorderColor( Id, ui::Theme::padButtonColour( progData ? progData->padColour[Id].get() : 0 ), 2.0f, juce::dontSendNotification );
    }

    void KickPadColorMode::refresh()
    {
        right.pads->clearAllPadData();

        right.pads->setActive( -1, true );
        for( auto i = 0; i < 128; i++ )
            refreshPad( i );

        samples.clear();
        ae.getProgramSamples( ae.getSelectedProgram(), samples );
        if( samples.size() )
        {
            if( ae.getProgram( ae.getSelectedProgram() )->getType() == mt::MTProgramType::Keygroup )
            {
                auto text  = juce::File( samples[0]->getSource() ).getFileNameWithoutExtension();
                bankOffset = right.pads->getBankOffset();
                for( auto index = bankOffset; index < bankOffset + _NUM_PADS_IN_COMPONENT; ++index )
                {
                    if( !text.isEmpty() )
                    {
                        right.pads->setRow1Text( index, text, juce::dontSendNotification );
                    }
                }
            }
            else
            {
                for( const auto& sample : samples )
                {
                    if( sample )
                    {
                        auto text  = juce::File( sample->getSource() ).getFileNameWithoutExtension();
                        bankOffset = right.pads->getBankOffset();
                        for( auto index = bankOffset; index < bankOffset + _NUM_PADS_IN_COMPONENT; ++index )
                        {
                            if( !text.isEmpty() )
                            {
                                if( index == (int)sample->keyNote.get() )
                                {
                                    right.pads->setRow1Text( index, text, juce::dontSendNotification );
                                    break;
                                }
                            }
                        }
                    }
                }
            }
        }

        //refreshLEDs();
    }

    void KickPadColorMode::refreshLEDs()
    {
        contracts::DclSignals::refreshPadLeds();
    }

    void KickPadColorMode::offerUndo( const juce::String& Message )
    {
        AppStatusbar::Instance().offerUndo( Message, [this]() {
            ae.getUndoManager().undo();
            refresh();
            refreshLEDs();
        } );
    }

    // Shift + tap on a colour button: every pad of the program takes the colour (one undo step).
    void KickPadColorMode::colourAllPads()
    {
        if( !ae.getNumPrograms() || !left.selectedColor )
            return;
        {
            mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), "Colour all pads" );
            ae.setProgramPadColour( ae.getSelectedProgram(), left.selectedColourId, -1 );
        }
        refresh();
        refreshLEDs();
        offerUndo( "All pads coloured" );
    }

    void KickPadColorMode::buttonClicked( int Id )
    {
        // Shift + pad (hardware press or touch): that pad's colour becomes the selected colour, nothing is painted.
        if( contracts::DclContractsHelper::isShiftPressed() )
        {
            auto progData = ae.getSelectedProgramData();
            if( progData && juce::isPositiveAndBelow( Id, 128 ) )
                left.selectColour( progData->padColour[Id].get() );
            return;
        }
        if( left.selectedColor )
        {
            // One undo step for the touch / the pad press (All Pads rewrites 128 pads).
            mt::ScopedUndoTransaction undoScope( ae.getUndoManager(), left.padType.getToggleState() ? "Colour all pads" : "Pad colour" );
            if( !left.padType.getToggleState() )
            {
                ae.setProgramPadColour( ae.getSelectedProgram(), left.selectedColourId, Id );
                refreshPad( Id );
            }
            else
            {
                ae.setProgramPadColour( ae.getSelectedProgram(), left.selectedColourId, -1 );
                refresh();
            }

            // The hardware pads follow through the supported colour path (programme colours), at once.
            refreshLEDs();
            if( left.padType.getToggleState() )
                offerUndo( "All pads coloured" );
        }
    }

    void KickPadColorMode::setBank( int Index, juce::String Bank )
    {
        if( kick_ui ::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuColorEdit )
        {
            juce::MessageManager::callAsync( [this, Index, Bank] {
                right.pads->setBank( Index );
                bankLabel.setText( right.pads->getBank(), juce::dontSendNotification );
                refresh();
                refreshLEDs();
            } );
        }
    }

    void KickPadColorMode::resized()
    {
        header.setBounds( getLocalBounds().removeFromTop( ui::layout::header.getHeight() ) );
        hdrComp.setBoundsRelative( .225f, 0.0299f, .195f, 0.0824f );
        left.setBoundsRelative( 0.009f, 0.161f, 0.3346f, 0.8239f );
        right.setBoundsRelative( 0.359f, 0.17f, 0.63f, 0.816f );
        // Proportional to the 1024 x 600 design size, so other layouts (mobile T layout) scale too.
        const auto sx = getWidth() / 1024.0f, sy = getHeight() / 600.0f;
        auto       scaled = [sx, sy]( int X, int Y, int W, int H ) { return juce::Rectangle<int>( juce::roundToInt( X * sx ), juce::roundToInt( Y * sy ), juce::roundToInt( W * sx ), juce::roundToInt( H * sy ) ); };
        warningLabel.setBounds( scaled( 400, 200, 400, 100 ) );
        leftNav.setBounds( scaled( 604, 16, 44, 44 ) );
        bankLabel.setBounds( scaled( 648, 16, 112, 44 ) );
        rightNav.setBounds( scaled( 760, 16, 44, 44 ) );
    }

    void KickPadColorMode::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        DBG( "Inside HwCmd Handel Padcolor" );
        if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
                // +/- step the bank (as the on-screen arrows do).
                case contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS: rightNav.onClick(); break;
                case contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS: leftNav.onClick(); break;
                default: break;
            }
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            // The Data Dial steps through the 18 colours.
            if( contracts::DclContractsHelper::getKnobCode( f_HwMsg ) == contracts::DeviceControlContracts::EN_KNOB_CENTER )
            {
                const auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
                if( value != 0 )
                    left.stepColour( value );
            }
        }
        else if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            bankOffset                = ae.getBankOffset();
            auto             padNo    = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            auto             padState = contracts::DclContractsHelper::getPadState( f_HwMsg );
            auto             velocity = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );

            padNo += bankOffset;

            // The pad plays its note (audition) and takes the selected colour: press the pad you want to colour.
            juce::MidiMessage Msg;
            if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
            {
                Msg = juce::MidiMessage::noteOn( 1, padNo, (juce::uint8)velocity );
                buttonClicked( padNo );
            }
            else if( padState == contracts::DeviceControlContracts::BTN_RELEASED )
                Msg = juce::MidiMessage::noteOff( 1, padNo );
            else
                return; // no other state carries a note: never send an empty message

            ae.addLiveMidiMsg( ae.getSelected(), Msg );
        }
    }

    void KickPadColorMode::visibilityChanged()
    {
        bool visible = isVisible();
        if( visible )
        {
            right.pads->setBank( ae.getBank() );
            bankLabel.setText( ae.getBank(), juce::dontSendNotification );
            refresh();
            refreshLEDs();
            auto idx = ae.getSelectedProgram();
            hdrComp.title.setText( ae.getProgramNameByIndex( idx ), juce::dontSendNotification );

            auto progData = ae.getProgramData( ae.getSelectedProgram() );

            auto padOffSel = getPadOffColourMode( progData ? progData->colourMode.get() : mt::OFF );
            auto padOnSel  = getPadOnColourMode( progData ? progData->colourMode.get() : mt::OFF );
            setPadOffModeSelection( padOffSel );
            setPadOnModeSelection( padOnSel );
            left.padOff.setCurrentSelection( padOffSel );
            left.padOn.setCurrentSelection( padOnSel );
            left.emptyPadMode.setToggleState( getEmptyColourMode( progData ? progData->colourMode.get() : mt::OFF ), juce::dontSendNotification );
            // Keep the stored empty-pad setting consistent with the colour mode; written only when it differs
            // (showing the screen must not change the project otherwise).
            const auto wantedEmpty = left.emptyPadMode.getToggleState() ? mt::EmptyPadColorMode::Dim : mt::EmptyPadColorMode::Off;
            if( progData && progData->emptyPadColourMode.get() != wantedEmpty )
                ae.setProgramEmptyPadColorMode( ae.getSelectedProgram(), wantedEmpty );
        }
        if( !ae.getNumPrograms() && visible )
        {
            left.setVisible( false );
            right.setVisible( false );
            hdrComp.setVisible( true );
            header.setVisible( true );
            warningLabel.setVisible( true );
            hdrComp.title.setText( "None", juce::dontSendNotification );
        }
        else
        {
            for( auto it : getChildren() )
                it->setVisible( visible );
            warningLabel.setVisible( false );
        }
    }

    //////////////////////////////
    ////KickColorEditorLeftPanel
    //////////////////////////////

    KickColorEditorLeftPanel::KickColorEditorLeftPanel()
        : TabbedComponent( juce::TabbedButtonBar::Orientation::TabsAtBottom )
    {
        mt::utils::addAndMakeVisible( this, emptyPadMode, padType, padOn, padOff, emptyPadModeLabel, padTypeLabel, padOffLabel, padOnLabel, assignedPadLabel );

        padOff.setValueColour( ui::Theme::textColour( 2 ) );
        padOn.setValueColour( ui::Theme::textColour( 2 ) );

        assignedPadLabel.setValueJustification( juce::Justification::centred );
        assignedPadLabel.setValueFontHeight( 14.0f );
        assignedPadLabel.setValueColour( ui::Theme::textColour( 2 ) );

        padOffLabel.setValueJustification( juce::Justification::centred );
        padOffLabel.setTitleFontHeight( 14.0f );
        padOffLabel.setValueColour( ui::Theme::textColour( 1 ) );

        padOnLabel.setValueJustification( juce::Justification::centred );
        padOnLabel.setValueFontHeight( 14.0f );
        padOnLabel.setValueColour( ui::Theme::textColour( 1 ) );

        padTypeLabel.setValueJustification( juce::Justification::centred );
        padTypeLabel.setValueFontHeight( 14.0f );
        padTypeLabel.setValueColour( ui::Theme::textColour( 1 ) );

        emptyPadModeLabel.setValueJustification( juce::Justification::centred );
        emptyPadModeLabel.setValueFontHeight( 14.0f );
        emptyPadModeLabel.setValueColour( ui::Theme::textColour( 1 ) );

        // Add all colour buttons...

        for( auto i = 0; i < 18; i++ )
        {
            addAndMakeVisible( padButtons[i] );
            padButtons[i].addListener( this );
            padButtons[i].setRadioGroupId( (int)kick_ui::RadioGroupId::ColourEdit );
            padButtons[i].setID( i );
            padButtons[i].setColor( ui::Theme::padButtonColour( i ), ui::Theme::padButtonColour( i ) );
        }

        ui::themeChanged.connect( [&]() {
        } );
    }

    // Data Dial: select the previous / next colour of the palette.
    void KickColorEditorLeftPanel::stepColour( int Delta )
    {
        selectColour( pad_color_logic::stepColour( (int)selectedColourId, Delta, 18 ) );
    }

    void KickColorEditorLeftPanel::selectColour( int ColourId )
    {
        const int next = juce::jlimit( 0, 17, ColourId );
        padButtons[next].setToggleState( true, juce::dontSendNotification ); // radio group: the other colours switch off
        selecting = true;                                                    // not a touch: Shift must not paint every pad
        buttonClicked( &padButtons[next] );                                  // take it as the selected colour
        selecting = false;
    }

    void KickColorEditorLeftPanel::resized()
    {
        padOff.setBoundsRelative( .0292f, .0863f, .4649f, .0909f );
        padOn.setBoundsRelative( .5058f, .0863f, .4649f, .0909f );

        padType.setBoundsRelative( .0292f, .2636f, .4649f, .0909f );
        emptyPadMode.setBoundsRelative( .5058f, .2636f, .4649f, .0909f );

        padOffLabel.setBoundsRelative( .0292f, .0454f, .4649f, .0318f );
        padOnLabel.setBoundsRelative( .5058f, .0454f, .4649f, .0318f );

        padTypeLabel.setBoundsRelative( .0292f, .2227f, .4649f, .0318f );
        emptyPadModeLabel.setBoundsRelative( .5058f, .2227f, .4649f, .0318f );

        for( auto row = 0; row < numRows; ++row )
        {
            x = 0.029f;
            y = row * padButtonHeight + 0.398f;
            for( auto col = 0; col < numCols; ++col )
            {
                auto idx = ( numRows - row - 1 ) * numCols + col;
                padButtons[idx].setBoundsRelative( x, y, padButtonWidth, padButtonHeight );
                x += padButtonWidth;
            }
        }
    }
    void KickColorEditorLeftPanel::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds();
        mt::utils::renderRoundedRect( g, { b.getX(), b.getY(), b.getWidth(), b.getHeight() }, ui::Theme::outlineColour( 3 ), 1, 12.0f );
        mt::utils::fillRoundedRect( g, { b.getX(), b.getY(), b.getWidth(), b.getHeight() }, ui::Theme::backgroundColour( 3 ), 7.0f );
    }
    void KickColorEditorLeftPanel::buttonClicked( juce::Button* ButtonClicked )
    {
        if( (int)kick_ui::RadioGroupId::ColourEdit == ButtonClicked->getRadioGroupId() )
        {
            selectedColor = dynamic_cast<ui::KickColorButton*>( ButtonClicked );
            selectedColor->showBorder( selectedColor->getToggleState() );

            selectedColourId = magic_enum::enum_cast<contracts::DeviceControlContracts::PadColourIds>( selectedColor->getID() ).value();
            // Shift + tap on a colour button colours every pad with it.
            if( !selecting && contracts::DclContractsHelper::isShiftPressed() && onColourAll )
                onColourAll();
        }
    }
    //////////////////////////////
    ////KickColorEditorRightPanel
    //////////////////////////////

    KickColorEditorRightPanel::KickColorEditorRightPanel()
        : pads( std::make_unique<ui::KickPadButtonGroupComponent>( ui::KickPadButton::PadButtonType::colorMode ) )
    {
        mt::utils::addAndMakeVisible( this, pads.get(), defaultBtn, resetBtn );

        pads->setActive( -1, false );
        pads->setPadButtonSpacer( padButtonSpacer );

        defaultBtn.setValueColour( ui::Theme::textColour( 1 ) );
        defaultBtn.setValueFontHeight( 18 );
        resetBtn.setValueColour( ui::Theme::textColour( 1 ) );
        resetBtn.setValueFontHeight( 18 );

        ui::themeChanged.connect( [&]() {
            defaultBtn.setToggleButtonText( "Make Default", "Make Default" );
            resetBtn.setToggleButtonText( "Reset", "Reset" );
        } );
    }

    void KickColorEditorRightPanel::resized()
    {
        pads->setBoundsRelative( 0.f, 0.f, 1.f, 0.8899f );
        defaultBtn.setBoundsRelative( 0.8136f, 0.922f, 0.186f, 0.078f );
        resetBtn.setBoundsRelative( 0.6f, 0.922f, 0.2f, 0.078f );
    }
    void KickColorEditorRightPanel::paint( juce::Graphics& g )
    {
    }

    void KickColorEditorRightPanel::visibilityChanged()
    {
        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );
    }
}
