namespace kick_ui
{
    PadPerform::PadPerform()
        : kick_ui::IScreen( kick_ui::Screen::MenuPadPerform )
    {
        mt::utils::addChildComponents( this, header, content );
        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        ui::themeChanged.connect( [&]() {
            header.setIcon( "Icon_PadPerform" );
        } );
    }

    void PadPerform::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        content.setBounds( b );
    }

    void PadPerform::visibilityChanged()
    {
        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );
    }

    // Hardware input of the screen. Q-Link 1-4 (Screen mode, the default; in Custom mode the KnobEdit takes the Q-Links
    // before they get here, kick_AppHome.cpp): 1 Scale, 2 Root, 3 Octave, 4 Chord list (Chords / Progression) or the mode.
    void PadPerform::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            const auto padNo    = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            const auto padState = contracts::DclContractsHelper::getPadState( f_HwMsg );
            const auto velocity = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );

            if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
                content.hardwarePad( padNo, true, velocity );
            else if( padState == contracts::DeviceControlContracts::BTN_RELEASED )
                content.hardwarePad( padNo, false, 0 );
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
                content.knobCommand( knob, value );
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); break;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); break;
            }
        }
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////

    PadPerformContent::PadPerformContent()
        : pads( std::make_unique<ui::KickPadButtonGroupComponent>( ui::KickPadButton::PadButtonType::with1Row ) )
    {
        mt::utils::addChildComponents( this, rootNote, scale, startOn, octave, bankInc, bankDec, bank, chromaticBtn, notesBtn, chordsBtn, progressionBtn, levelBtn, levelParamBtn, heldLabel, chord, scale_octave, pads.get() );
        mt::utils::setRadioGroupID( (int)kick_ui::RadioGroupId::Padperform, chromaticBtn, notesBtn, chordsBtn, progressionBtn );
        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), chord, startOn );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), chromaticBtn, notesBtn, chordsBtn, progressionBtn, levelBtn, levelParamBtn, bank );
        mt::utils::setValueFontHeight( 18, chromaticBtn, notesBtn, chordsBtn, progressionBtn, levelBtn, levelParamBtn, bank );
        mt::utils::setTitleFontHeight( 16, scale, rootNote, octave, chord, startOn );
        mt::utils::setValueFontHeight( 20, scale, rootNote, octave, chord, startOn );
        mt::utils::setJustificationType( juce::Justification::centred, scale, rootNote, octave, chord, startOn );

        heldLabel.setInterceptsMouseClicks( false, false );
        heldLabel.setValueColour( ui::Theme::textColour( 1 ) );
        heldLabel.setValueFontHeight( 18 );
        heldLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 3 ) );
        heldLabel.setValueJustification( juce::Justification::centred );

        pads->setRow1Justification( -1, juce::Justification::centred );
        // The hardware pads are handled by hardwarePad(); the group would also click its own pad on every hardware hit.
        pads->setSelectionEnabled( false );
        bank.setInterceptsMouseClicks( false, false );
        bank.setValueBackgroundColour( ui::Theme::backgroundColour( 3 ) );
        bank.setValueJustification( juce::Justification::centred );
        bankInc.toFront( true );
        bankDec.toFront( true );

        levelParamBtn.setToggleState( true, juce::dontSendNotification ); // on = Velocity, off = Tune
        levelParamBtn.setVisible( false );

        // One bank truth: the engine's. The screen follows it (hardware bank buttons, KickHome) and writes to it (± buttons,
        // the group's bank list), so the displayed bank, the pad LEDs and the notes the hardware pads play always agree.
        bankConn_ = mt::signals::bankChanged.connect( [&]( int index, juce::String ) {
            if( kick_ui ::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuPadPerform )
            {
                bankIndex = index;
                if( pads->getBankIndex() != index ) pads->setBank( index );
            }
        } );

        pads->OnBankChanged = [&]( const juce::String& bankName ) {
            bank.setText( bankName, juce::dontSendNotification );
            bankIndex = pads->getBankIndex();
            if( ae.getBank().toUpperCase() != bankName.toUpperCase() )
                ae.setBank( bankName );
            refreshData();
        };

        auto selectBank = [&]( int index ) {
            index = juce::jlimit( 0, 7, index );
            if( pads->getBankIndex() != index )
                pads->setBank( index ); // -> OnBankChanged -> ae.setBank
        };
        bankInc.onClick = [&, selectBank]() {
            selectBank( pads->getBankIndex() + 1 );
            bankInc.setToggleState( false, juce::dontSendNotification );
        };
        bankDec.onClick = [&, selectBank]() {
            selectBank( pads->getBankIndex() - 1 );
            bankDec.setToggleState( false, juce::dontSendNotification );
        };

        // On-screen pads: one note-on per finger on touch-down (velocity from where on the pad), the matching note-off on lift.
        pads->onPadTouchDown = [this]( int padId, int finger, float yFrac ) {
            padDown( padmap::fingerKey( finger ), padId, ui::piano::velocityFromPosition( yFrac, 24 ) );
        };
        pads->onPadTouchUp = [this]( int, int finger ) { padUp( padmap::fingerKey( finger ) ); };

        chromaticBtn.onClick = [&]() { if( chromaticBtn.getToggleState() ) setMode( padmap::Chromatic ); };
        notesBtn.onClick     = [&]() { if( notesBtn.getToggleState() ) setMode( padmap::Notes ); };
        chordsBtn.onClick    = [&]() { if( chordsBtn.getToggleState() ) setMode( padmap::Chords ); };
        progressionBtn.onClick = [&]() { if( progressionBtn.getToggleState() ) setMode( padmap::Progression ); };

        levelBtn.onClick = [&]() {
            if( levelBtn.getToggleState() != level16_ ) toggleSixteenLevels();
        };
        levelParamBtn.onClick = [&]() {
            levelParamTune_ = ! levelParamBtn.getToggleState();
            updateLevelUi();
            pushToUI();
            pushToHW();
        };

        rootNote.onSelectionChanged = [&]( int, const juce::String& RootNote ) {
            if( pPadPerform )
            {
                pPadPerform->performRootNote = RootNote;
                requestRefresh();
            }
        };

        // No bank gate: the octave applies in every bank (H bank used to keep the old octave while the list moved on).
        octave.onSelectionChanged = [&]( int, const juce::String& Octave ) {
            if( pPadPerform )
            {
                pPadPerform->performOctave = Octave;
                requestRefresh();
            }
        };

        scale.onSelectionChanged = [&]( int, const juce::String& Scale ) {
            if( pPadPerform )
            {
                pPadPerform->performScale = Scale;
                requestRefresh();
            }
        };

        startOn.onSelectionChanged = [&]( int, const juce::String& StartOnType ) {
            if( pPadPerform )
            {
                pPadPerform->performBanks = StartOnType;
                requestRefresh();
            }
        };

        chord.onSelectionChanged = [&]( int, const juce::String& Chord ) {
            if( pPadPerform )
            {
                pPadPerform->performChords = Chord;
                requestRefresh();
            }
        };

        ui::themeChanged.connect( [&]() {
            chromaticBtn.setToggleButtonText( "Chromatic", "Chromatic" );
            notesBtn.setToggleButtonText( "Notes", "Notes" );
            chordsBtn.setToggleButtonText( "Chords", "Chords" );
            progressionBtn.setToggleButtonText( "Progressions", "Progressions" );
            levelBtn.setToggleButtonText( "16 LEVELS", "16 LEVELS" );
            levelParamBtn.setToggleButtonText( "Velocity", "Tune" );
        } );
    }

    PadPerformContent::~PadPerformContent()
    {
        stopTimer();
        bankConn_.disconnect();
        pads->onPadTouchDown = nullptr;
        pads->onPadTouchUp   = nullptr;
    }

    void PadPerformContent::resized()
    {
        // Left Side...
        scale.setBoundsRelative( .02f, .0456f, .315f, .14f );
        rootNote.setBoundsRelative( .02f, .2065f, .315f, .14f );
        octave.setBoundsRelative( .02f, .369f, .315f, .14f );
        chord.setBoundsRelative( .02f, .532f, .315f, .14f );
        startOn.setBoundsRelative( .02f, .695f, .315f, .14f );

        // Right Side...
        pads->setBoundsRelative( .358f, .02f, .63f, .87f );

        // Bottom Row...
        chromaticBtn.setBoundsRelative( 0.0097f, .912f, .117f, .0742f );
        notesBtn.setBoundsRelative( .1310f, .912f, .117f, .0742f );
        chordsBtn.setBoundsRelative( .2523f, .912f, .117f, .0742f );
        progressionBtn.setBoundsRelative( .3736f, .912f, .117f, .0742f );
        levelBtn.setBoundsRelative( .4949f, .912f, .105f, .0742f );
        levelParamBtn.setBoundsRelative( .6049f, .912f, .105f, .0742f );
        heldLabel.setBoundsRelative( .7149f, .912f, .105f, .0742f );
        bank.setBoundsRelative( .8317f, .91f, .1565f, .0782f );
        bankDec.setBoundsRelative( .8317f, .912f, .0332f, .0742f );
        bankInc.setBoundsRelative( .9549f, .912f, .0332f, .0742f );
    }

    void PadPerformContent::knobCommand( int knob, int value )
    {
        const bool chordsShown = padmap::normaliseMode( selectedType ) == padmap::Chords || padmap::normaliseMode( selectedType ) == padmap::Progression;
        switch( knob )
        {
            case contracts::DeviceControlContracts::EN_KNOB_1: scale.knobRotated( value ); break;
            case contracts::DeviceControlContracts::EN_KNOB_2: rootNote.knobRotated( value ); break;
            case contracts::DeviceControlContracts::EN_KNOB_3: octave.knobRotated( value ); break;
            case contracts::DeviceControlContracts::EN_KNOB_4:
                if( chordsShown ) chord.knobRotated( value );
                else stepMode( value > 0 ? 1 : -1 );
                break;
            case contracts::DeviceControlContracts::EN_KNOB_5: chord.knobRotated( value ); break;
            case contracts::DeviceControlContracts::EN_KNOB_6: startOn.knobRotated( value ); break;
            case contracts::DeviceControlContracts::EN_KNOB_7: stepMode( value > 0 ? 1 : -1 ); break;
            default: break;
        }
    }

    void PadPerformContent::visibilityChanged()
    {
        bool visible = isVisible();
        for( auto it : getChildren() )
            it->setVisible( visible );

        if( isVisible() || isShowing() )
        {
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
            lastHwColours_.clear();
            kgColoursValid_ = false;
            pPadPerform     = ae.getPadPerformanceData();

            // Open on the engine's bank (it may have changed while another screen was in front).
            bankIndex = juce::jlimit( 0, 7, ae.getBankOffset() / 16 );
            if( pads->getBankIndex() != bankIndex ) pads->setBank( bankIndex );
            else bank.setText( mt::Kick::padBankNames[(size_t)bankIndex], juce::dontSendNotification );

            refresh();
            if( level16_ )
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setLEDONMessage( contracts::DeviceControlContracts::EN_16LEVELS ) );
        }
        else
        {
            stopTimer();
            releaseAllNotes();
            if( level16_ )
            {
                level16_ = false;
                levelBtn.setToggleState( false, juce::dontSendNotification );
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setLEDOFFMessage( contracts::DeviceControlContracts::EN_16LEVELS ) );
            }
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );

            contracts::DclSignals::refreshPadLeds();
        }
    }

    // Stored Off / Chromatic Chords modes (no button, no behaviour) are normalised to Chromatic here, so a project that
    // holds one of them shows and plays a normal pad map.
    void PadPerformContent::refresh()
    {
        if( ! pPadPerform ) return;

        const int stored = (int)pPadPerform->performMode.get();
        const int mode   = padmap::normaliseMode( stored );
        if( mode != stored ) pPadPerform->performMode = (mt::PerformanceMode)mode;
        selectedType = mode;

        scale.setSelected( pPadPerform->performScale.get(), juce::dontSendNotification );
        octave.setSelected( pPadPerform->performOctave.get(), juce::dontSendNotification );
        rootNote.setSelected( pPadPerform->performRootNote.get(), juce::dontSendNotification );
        chord.setSelected( pPadPerform->performChords.get(), juce::dontSendNotification );
        startOn.setSelected( pPadPerform->performBanks.get(), juce::dontSendNotification );
        chromaticBtn.setToggleState( mode == padmap::Chromatic, juce::dontSendNotification );
        notesBtn.setToggleState( mode == padmap::Notes, juce::dontSendNotification );
        chordsBtn.setToggleState( mode == padmap::Chords, juce::dontSendNotification );
        progressionBtn.setToggleState( mode == padmap::Progression, juce::dontSendNotification );
        levelBtn.setToggleState( level16_, juce::dontSendNotification );
        refreshData();
    }

    void PadPerformContent::setMode( int mode )
    {
        if( ! pPadPerform ) return;
        mode         = padmap::normaliseMode( mode );
        selectedType = mode;
        pPadPerform->performMode = (mt::PerformanceMode)mode;
        requestRefresh();
    }

    void PadPerformContent::stepMode( int direction )
    {
        const int cur  = padmap::normaliseMode( selectedType );
        const int next = juce::jlimit( (int)padmap::Chromatic, (int)padmap::Progression, cur + direction );
        if( next == cur ) return;
        ui::KickToggleButton* btns[] = { &chromaticBtn, &notesBtn, &chordsBtn, &progressionBtn };
        btns[next - padmap::Chromatic]->setToggleState( true, juce::dontSendNotification );
        setMode( next );
    }

    // Q-Link ticks and list taps arrive in bursts; the (DCL pad colour + engine colour + 16 label) recalculation waits for
    // the burst to end.
    void PadPerformContent::requestRefresh() { startTimer( 40 ); }

    void PadPerformContent::timerCallback()
    {
        stopTimer();
        refreshData();
    }

    padmap::Params PadPerformContent::params()
    {
        padmap::Params p;
        p.mode      = padmap::normaliseMode( selectedType );
        p.rootPc    = juce::jlimit( 0, 11, rootNote.getSelectedIndex() );
        p.octaveIdx = juce::jlimit( 0, 10, octave.getSelectedIndex() );
        auto f      = mt::ScaleAndNotes::scaleFormulaMap.find( (mt::ScaleAndNotes::scale_type)juce::jmax( 0, scale.getSelectedIndex() ) );
        if( f != mt::ScaleAndNotes::scaleFormulaMap.end() ) p.scale = f->second;
        p.startOnRoot = startOn.getSelectedIndex() == 1;
        p.chord       = chord.getSelected().toStdString();
        return p;
    }

    void PadPerformContent::refreshData()
    {
        stopTimer();
        cur_    = params();
        layout_ = padmap::layout( cur_ );

        const bool chordsShown = cur_.mode == padmap::Chords || cur_.mode == padmap::Progression;
        chord.setVisible( chordsShown && isVisible() );

        pads->refreshAllPads();
        pads->clearAllPadData();
        pushToUI();
        pushToHW();
        pushToEngineColours();
        updateLevelUi();
        for( auto& [key, slot] : keySlot_ )
            pads->setPadHighlight( slot, true );
    }

    int PadPerformContent::hwColour( padmap::Mark mark )
    {
        switch( mark )
        {
            case padmap::Mark::Root: return (int)contracts::DeviceControlContracts::PadColourIds::PadColourRed;
            case padmap::Mark::Active: return (int)contracts::DeviceControlContracts::PadColourIds::padColourDarkYellow;
            default: return (int)contracts::DeviceControlContracts::PadColourIds::PadColourTrans;
        }
    }

    // The 16 pads of the shown bank: note names + border colour (root / scale / out of scale) from the layout; in 16 LEVELS
    // the stepped value of each pad.
    void PadPerformContent::pushToUI()
    {
        const auto& names = mt::ScaleAndNotes::noteNames();
        const int   first = juce::jlimit( 0, 112, bankIndex * 16 );
        for( int i = 0; i < 16; ++i )
        {
            const int    slot = first + i;
            padmap::Mark mark = padmap::Mark::NotAvailable;
            juce::String text;
            if( level16_ )
            {
                mark = padmap::Mark::Active;
                if( levelParamTune_ )
                {
                    const int off = i - 8;
                    text          = off == 0 ? juce::String( "0" ) : ( off > 0 ? "+" : "" ) + juce::String( off );
                    if( off == 0 ) mark = padmap::Mark::Root;
                }
                else
                    text = juce::String( padmap::levelVelocity( i ) );
            }
            else
            {
                const auto& pad = layout_[(size_t)slot];
                mark            = pad.mark;
                if( pad.note >= 0 && pad.note < (int)names.size() ) text = names[(size_t)pad.note];
            }
            const auto border = mark == padmap::Mark::Root   ? ui::Theme::highlightColour( 2 ) :
                                mark == padmap::Mark::Active ? ui::Theme::highlightColour( 1 ) :
                                                               ui::Theme::highlightColour( 0 );
            pads->setRow1Text( slot, text, juce::dontSendNotification );
            pads->setBorderColor( slot, border, 3.0f, juce::dontSendNotification );
            pads->setActive( slot, mark == padmap::Mark::Root || mark == padmap::Mark::Active );
        }
    }

    // Pad LEDs of the shown bank. Only sent when they changed (a Q-Link burst used to send the same packet again and again).
    void PadPerformContent::pushToHW()
    {
        const int        first = juce::jlimit( 0, 112, bankIndex * 16 );
        std::vector<int> colours;
        for( int i = 0; i < 16; ++i )
        {
            padmap::Mark mark = layout_[(size_t)( first + i )].mark;
            if( level16_ ) mark = ( levelParamTune_ && i == 8 ) ? padmap::Mark::Root : padmap::Mark::Active;
            colours.push_back( hwColour( mark ) );
        }
        if( colours == lastHwColours_ ) return;
        lastHwColours_ = colours;
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setAllPadDataColourInfoMessage( contracts::DeviceControlContracts::PadButtonState::StateOn, colours ) );
    }

    // The engine keeps the colours of all 128 pad slots for the LEDs of the other screens (Keygroup / MIDI / Plugin / CV).
    void PadPerformContent::pushToEngineColours()
    {
        std::array<int, 128> colours{};
        for( size_t i = 0; i < 128; ++i )
            colours[i] = hwColour( layout_[i].mark );
        if( kgColoursValid_ && colours == lastKgColours_ ) return;
        kgColoursValid_ = true;
        lastKgColours_  = colours;
        mt::MTPadColors padData = { mt::EmptyPadColorMode::Off, mt::PadColorMode::OFF, { 0 } };
        padData.Colours         = colours;
        ae.setKeyGroupPadColour( padData );
    }

    void PadPerformContent::updateLevelUi()
    {
        levelBtn.setToggleState( level16_, juce::dontSendNotification );
        levelParamBtn.setVisible( level16_ && isVisible() );
    }

    void PadPerformContent::toggleSixteenLevels()
    {
        level16_ = ! level16_;
        if( level16_ )
        {
            // The pad you hit last (its mapped note) is what all 16 pads play; before any hit: the first pad of the bank.
            levelBase_ = lastBaseNote_ >= 0 ? lastBaseNote_ : layout_[(size_t)juce::jlimit( 0, 112, bankIndex * 16 )].note;
            if( levelBase_ < 0 ) levelBase_ = 60;
        }
        contracts::DclSignals::sendCommandToDcl( level16_ ? contracts::DclContractsHelper::setLEDONMessage( contracts::DeviceControlContracts::EN_16LEVELS ) :
                                                            contracts::DclContractsHelper::setLEDOFFMessage( contracts::DeviceControlContracts::EN_16LEVELS ) );
        updateLevelUi();
        pads->clearAllPadData();
        pushToUI();
        pushToHW();
        for( auto& [key, slot] : keySlot_ )
            pads->setPadHighlight( slot, true );
        kick_log( "padperform", "16 LEVELS %s base note %d param %s\n", level16_ ? "on" : "off", levelBase_, levelParamTune_ ? "Tune" : "Velocity" );
    }

    ///////////////////////////////////////////////////////////////////////////////////////////////////////////////////
    // Playing

    void PadPerformContent::sendNote( const mt::MTSelected& target, bool on, int note, int velocity )
    {
        const auto msg = on ? juce::MidiMessage::noteOn( 1, note, (juce::uint8)juce::jlimit( 1, 127, velocity ) ) : juce::MidiMessage::noteOff( 1, note );
        ae.addLiveMidiMsg( target, msg );
    }

    void PadPerformContent::hardwarePad( int padNo, bool pressed, int velocity )
    {
        const int slot = juce::jlimit( 0, 127, ae.getBankOffset() + padNo );
        if( pressed ) padDown( slot, slot, velocity );
        else padUp( slot );
    }

    void PadPerformContent::padDown( int key, int slot, int velocity )
    {
        std::vector<int> notes;
        int              vel = juce::jlimit( 1, 127, velocity );
        if( level16_ )
        {
            const auto lv = padmap::levelFor( levelParamTune_ ? padmap::LevelParam::Tune : padmap::LevelParam::Velocity, levelBase_, slot % 16 );
            notes         = { lv.note };
            if( lv.velocity > 0 ) vel = lv.velocity;
        }
        else
        {
            notes = padmap::notesForPad( layout_, cur_, slot );
            if( ! notes.empty() ) lastBaseNote_ = notes.front();
        }
        if( notes.empty() ) return; // slot beyond note 127

        const mt::MTSelected target = ae.getSelected();
        const auto           old    = keyTarget_.find( key );
        const mt::MTSelected oldTarget = old != keyTarget_.end() ? old->second : target;
        for( int n : held_.press( key, notes ) ) // a lost release of the same key: end its old notes first
            sendNote( oldTarget, false, n, 0 );
        keyTarget_[key] = target;
        keySlot_[key]   = slot;
        for( int n : notes )
            sendNote( target, true, n, vel );

        pads->setPadHighlight( slot, true );
        updateHeldDisplay();
    }

    // The note-off ends exactly the notes the press started, on the track it started on, whatever bank / scale / root /
    // chord / selected track the screen has moved on to since.
    void PadPerformContent::padUp( int key )
    {
        const auto off = held_.release( key );
        const auto it  = keyTarget_.find( key );
        const auto tgt = it != keyTarget_.end() ? it->second : ae.getSelected();
        for( int n : off )
            sendNote( tgt, false, n, 0 );
        if( it != keyTarget_.end() ) keyTarget_.erase( it );

        const auto sl = keySlot_.find( key );
        if( sl != keySlot_.end() )
        {
            const int slot = sl->second;
            keySlot_.erase( sl );
            bool stillHeld = false;
            for( auto& kv : keySlot_ )
                if( kv.second == slot ) stillHeld = true;
            if( ! stillHeld ) pads->setPadHighlight( slot, false );
        }
        updateHeldDisplay();
    }

    void PadPerformContent::releaseAllNotes()
    {
        std::vector<int> keys;
        for( auto& kv : keyTarget_ ) keys.push_back( kv.first );
        for( int k : keys ) padUp( k );
        for( int n : held_.releaseAll() )
            sendNote( ae.getSelected(), false, n, 0 );
        keyTarget_.clear();
        for( auto& kv : keySlot_ ) pads->setPadHighlight( kv.second, false );
        keySlot_.clear();
        updateHeldDisplay();
    }

    // Note name / chord name of what is held ("Cmaj7", "G7", "C3 E3").
    void PadPerformContent::updateHeldDisplay()
    {
        const auto  notes = held_.soundingNotes();
        const auto& names = mt::ScaleAndNotes::noteNames();
        juce::String text;
        if( ! notes.empty() )
        {
            text = juce::String( padmap::chordName( notes, mt::_preferFlatNames.load() ) );
            if( text.isEmpty() && notes.size() <= 3 )
                for( int n : notes )
                    text += ( text.isEmpty() ? "" : " " ) + names[(size_t)juce::jlimit( 0, 127, n )];
        }
        heldLabel.setText( text, juce::dontSendNotification );
    }

    void PadPerformContent::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 1 ) );
        g.fillRoundedRectangle( { 10.0f, 10.0f, 342.0f, 396.0f }, 5.0f );
    }
}
