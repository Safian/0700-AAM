
namespace kick_ui
{
    APPTrackOptionPitchQuantize::APPTrackOptionPitchQuantize()
    {
        setName( "PITCH QUANTIZE" );
        mt::utils::addAndMakeVisible( this, setupLabel, rootNote, scale, noteInputFilter, selectedEvents, noteInputFilter2, noteInputFilter3, startNote, endNote );

        setupLabel.setTextColour( ui::Theme::textColour( 1 ) );
        noteInputFilter.setTextColour( ui::Theme::textColour( 1 ) );
        noteInputFilter2.setTextColour( ui::Theme::textColour( 1 ) );
        noteInputFilter3.setTextColour( ui::Theme::textColour( 2 ) );

        startNote.showTitle( true );
        endNote.showTitle( true );
        startNote.showDropdown( true );
        endNote.showDropdown( true );
        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), startNote, endNote );

        startNote.onNoteChanged = [&]( int NoteNumber ) {
            auto end = endNote.getNoteNumber();
            if( NoteNumber > end )
                endNote.setNote( NoteNumber );
        };

        endNote.onNoteChanged = [&]( int NoteNumber ) {
            auto start = startNote.getNoteNumber();
            if( NoteNumber < start )
                startNote.setNote( NoteNumber );
        };
    }

    void APPTrackOptionPitchQuantize::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 1 ) );
    }

    void APPTrackOptionPitchQuantize::resized()
    {
        setupLabel.setBoundsRelative( .05f, .03f, .90f, 0.07f );
        rootNote.setBoundsRelative( .05f, .13f, .40f, 0.2f );
        scale.setBoundsRelative( .50f, .13f, .40f, 0.2f );
        noteInputFilter.setBoundsRelative( .05f, .36f, .90f, 0.07f );
        selectedEvents.setBoundsRelative( .05f, .46f, .90f, 0.07f );
        noteInputFilter2.setBoundsRelative( .05f, .56f, .90f, 0.07f );
        noteInputFilter3.setBoundsRelative( .05f, .66f, .90f, 0.07f );
        startNote.setBoundsRelative( .05f, .76f, .30f, 0.2f );
        endNote.setBoundsRelative( .50f, .76f, .30f, 0.2f );
    }

    APPTrackOptionHumanize::APPTrackOptionHumanize()
    {
        setName( "HUMANIZE" );
        mt::utils::addAndMakeVisible( this, onlySelectedCheckBox, timeCheckBox, pulses, beatLabel, eagerness, noteLabel, lengthCheckBox, strength, velocityCheckBox, velocity );
        mt::utils::setValueJustification( juce::Justification::centred, beatLabel, noteLabel );
        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 3 ), beatLabel, noteLabel );
        pulses.setNumDecimalPlacesToDisplay( 0 );
        eagerness.setNumDecimalPlacesToDisplay( 0 );
        strength.setNumDecimalPlacesToDisplay( 0 );
        velocity.setNumDecimalPlacesToDisplay( 0 );
        pulses.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        eagerness.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        strength.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        velocity.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        pulses.setRange( 1, 240 );
        eagerness.setRange( -50, 50 );
        strength.setRange( 1, 100 );
        velocity.setRange( 1, 100 );
        pulses.setValue( 30, juce::dontSendNotification );
        velocity.setValue( 20, juce::dontSendNotification );
        strength.setEnabled( false );
        velocity.setEnabled( false );

        timeCheckBox.onClick = [&]() {
            mt::utils::setEnabled( timeCheckBox.getToggleState(), pulses, eagerness );
        };
        pulses.onValueChange    = [this]() { updateLabels(); };
        eagerness.onValueChange = [this]() { updateLabels(); };

        lengthCheckBox.onClick = [&]() {
            strength.setEnabled( lengthCheckBox.getToggleState() );
        };

        velocityCheckBox.onClick = [&]() {
            velocity.setEnabled( velocityCheckBox.getToggleState() );
        };

        timeCheckBox.triggerClick();
        updateLabels();
    }

    void APPTrackOptionHumanize::updateLabels()
    {
        // 96 ticks = a quarter note, so a 16th note = 24 ticks.
        const double ticks = pulses.getValue();
        beatLabel.setText( juce::String( juce::roundToInt( ticks ) ) + " ticks = " + juce::String( ticks / 24.0, 2 ) + " x 16th note", juce::dontSendNotification );
        const int e = juce::roundToInt( eagerness.getValue() );
        noteLabel.setText( e == 0 ? "Early and late" : ( e > 0 ? "Late: " + juce::String( e * 2 ) + "% of the amount" : "Early: " + juce::String( -e * 2 ) + "% of the amount" ), juce::dontSendNotification );
    }

    void APPTrackOptionHumanize::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 1 ) );
    }

    void APPTrackOptionHumanize::resized()
    {
        onlySelectedCheckBox.setBoundsRelative( 0.5f, 0.02f, 0.5f, 0.07f );
        timeCheckBox.setBoundsRelative( 0.0f, 0.02f, 0.5f, 0.07f );
        pulses.setBoundsRelative( 0.1f, 0.10f, 0.38f, 0.18f );
        eagerness.setBoundsRelative( 0.52f, 0.10f, 0.38f, 0.18f );
        beatLabel.setBoundsRelative( 0.1f, 0.29f, 0.38f, 0.06f );
        noteLabel.setBoundsRelative( 0.52f, 0.29f, 0.38f, 0.06f );

        lengthCheckBox.setBoundsRelative( 0.0f, 0.38f, 0.9f, 0.07f );
        strength.setBoundsRelative( 0.1f, 0.46f, 0.8f, 0.18f );

        velocityCheckBox.setBoundsRelative( 0.0f, 0.67f, 0.9f, 0.07f );
        velocity.setBoundsRelative( 0.1f, 0.75f, 0.8f, 0.18f );
    }

    APPTrackOptionRandomEvents::APPTrackOptionRandomEvents()
    {
        setName( "GENERATE RANDOM EVENTS" );
        mt::utils::addAndMakeVisible( this, eventType, replace, noteLength, patternSize, label,
                                      Bank, startPad, endPad,
                                      startNote, endNote, rootNote, scale,
                                      density, rhythm );

        patternSize.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        density.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        rhythm.showTitleAndValue( true, juce::Slider::TextBoxBelow, juce::Justification::centred );
        patternSize.setNumDecimalPlacesToDisplay( 0 );
        density.setNumDecimalPlacesToDisplay( 0 );
        rhythm.setNumDecimalPlacesToDisplay( 0 );

        auto& ae     = mt::getAudioInterface();
        auto  length = ae.getTrackLength( ae.getSelectedSequence(), ae.getSelectedTrack() );
        patternSize.setRange( 0, length, 1 );
        patternSize.setValue( length, juce::dontSendNotification );
        mt::utils::setValueFontHeight( 18.f, patternSize, density, rhythm );

        label.setColours( ui::Theme::textColour( 2 ), juce::Colours::transparentBlack );
        density.setRange( 20, 50, 1 );
        rhythm.setRange( 20, 100, 1 );
        density.setValue( 30, juce::dontSendNotification );
        rhythm.setValue( 40, juce::dontSendNotification );

        patternSize.onValueChanged = [&]( int Value ) {
            if( Value == 0 )
                patternSize.setValue( 1, juce::dontSendNotification );
        };

        auto updatePadNames = [&]( std::vector<juce::String>& list, const juce::String& prefix, ui::KickListBox& S, ui::KickListBox& E ) {
            list.clear();
            for( auto i = 0; i < NUM_PADS_IN_BANK; ++i )
                list.push_back( prefix + juce::String( i + 1 ) );

            S.setListOptions( list, 0 );
            E.setListOptions( list, (int)( list.size() - 1 ) );
        };

        startPad.onSelectionChanged = [&]( int Sel, const juce::String& ) {
            auto end = endPad.getSelectedIndex();
            if( Sel > end )
                endPad.setSelected( Sel, juce::dontSendNotification );
        };
        endPad.onSelectionChanged = [&]( int Sel, const juce::String& ) {
            auto start = startPad.getSelectedIndex();
            if( Sel < start )
                startPad.setSelected( Sel, juce::dontSendNotification );
        };

        updatePadNames( padNames, Bank.getSelectedValue(), startPad, endPad );
        eventType.onValueChanged = [&]( const juce::String& ) { resized(); };
        Bank.onValueChanged      = [&]( const juce::String& S ) { updatePadNames( padNames, S, startPad, endPad ); };
    }

    void APPTrackOptionRandomEvents::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 1 ) );
    }

    void APPTrackOptionRandomEvents::resized()
    {
        eventType.setBoundsRelative( .05f, .05f, .40f, .15f );
        replace.setBoundsRelative( .55f, .05f, .40f, .15f );
        noteLength.setBoundsRelative( .05f, .22f, .40f, .15f );
        patternSize.setBoundsRelative( .55f, .22f, .40f, .15f );
        label.setBoundsRelative( .05f, .39f, .90f, .07f );

        if( !eventType.getSelectedValue().compareIgnoreCase( "Drum events" ) )
        {
            label.setText( "Drum event options", juce::dontSendNotification );
            Bank.setBoundsRelative( .05f, .48f, .28f, .15f );
            startPad.setBoundsRelative( .35f, .48f, .28f, .15f );
            endPad.setBoundsRelative( .65f, .48f, .28f, .15f );
            mt::utils::makeVisible( Bank, startPad, endPad );
            mt::utils::makeHidden( endNote, startNote, rootNote, scale );
        }
        else
        {
            label.setText( "Melodic event options", juce::dontSendNotification );
            rootNote.setBoundsRelative( .05f, .48f, .15f, .15f );
            scale.setBoundsRelative( .22f, .48f, .24f, .15f );
            startNote.setBoundsRelative( .48f, .48f, .20f, .15f );
            endNote.setBoundsRelative( .70f, .48f, .20f, .15f );
            mt::utils::makeHidden( Bank, startPad, endPad );
            mt::utils::makeVisible( endNote, startNote, rootNote, scale );
        }
        density.setBoundsRelative( .05f, .70f, .90f, .12f );
        rhythm.setBoundsRelative( .05f, .83f, .90f, .12f );
    }

    void GridNotePropertiesDialog::StepRow::attachTo( juce::Component& parent,
                                                    const juce::String& labelText,
                                                    const juce::String& coarseMinusText,
                                                    const juce::String& fineMinusText,
                                                    const juce::String& finePlusText,
                                                    const juce::String& coarsePlusText,
                                                    bool                useCoarse )
    {
        hasCoarse = useCoarse;
        parent.addAndMakeVisible( title );
        parent.addAndMakeVisible( value );
        parent.addAndMakeVisible( fineMinus );
        parent.addAndMakeVisible( finePlus );

        title.setText( labelText, juce::dontSendNotification );
        title.setJustificationType( juce::Justification::centredLeft );
        title.setColour( juce::Label::textColourId, ui::Theme::textColour( 1 ) );
        mt::utils::setValueJustification( juce::Justification::centred, value );
        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 3 ), value );
        mt::utils::setValueColour( ui::Theme::textColour( 2 ), value );
        mt::utils::setValueFontHeight( 17, value );

        auto configureBtn = []( ui::KickButton& btn, const juce::String& txt ) {
            btn.setClickingTogglesState( false );
            btn.setButtonText( txt );
            btn.setValueColour( ui::Theme::textColour( 1 ) );
            btn.setValueFontHeight( 16 );
        };

        configureBtn( fineMinus, fineMinusText );
        configureBtn( finePlus, finePlusText );

        if( hasCoarse )
        {
            parent.addAndMakeVisible( coarseMinus );
            parent.addAndMakeVisible( coarsePlus );
            configureBtn( coarseMinus, coarseMinusText );
            configureBtn( coarsePlus, coarsePlusText );
        }
    }

    void GridNotePropertiesDialog::StepRow::layout( juce::Rectangle<int> bounds )
    {
        auto b = bounds.reduced( 0, 3 );
        title.setBounds( b.removeFromLeft( 125 ) );
        b.removeFromLeft( 6 );

        constexpr int kBtnW = 62;
        constexpr int kGap  = 6;
        if( hasCoarse )
        {
            coarseMinus.setBounds( b.removeFromLeft( kBtnW ) );
            b.removeFromLeft( kGap );
        }
        fineMinus.setBounds( b.removeFromLeft( kBtnW ) );
        b.removeFromLeft( kGap );

        if( hasCoarse )
        {
            coarsePlus.setBounds( b.removeFromRight( kBtnW ) );
            b.removeFromRight( kGap );
        }
        finePlus.setBounds( b.removeFromRight( kBtnW ) );
        b.removeFromRight( kGap );

        value.setBounds( b );
    }

    GridNotePropertiesDialog::GridNotePropertiesDialog( const grid_logic::NotePropertiesState& initialState,
                                                        bool                                   isDrumTrack,
                                                        int                                    gridStepTicks,
                                                        int                                    maxStartTicks,
                                                        std::function<juce::String( int )>     formatStartBbt )
        : state( grid_logic::clampNoteProperties( initialState, maxStartTicks ) )
        , drumTrack( isDrumTrack )
        , stepTicks( juce::jmax( 1, gridStepTicks ) )
        , maxStart( juce::jmax( 0, maxStartTicks ) )
        , bbtFormatter( std::move( formatStartBbt ) )
    {
        setName( "NOTE PROPERTIES" );

        const int          octaveOrBank = drumTrack ? 16 : 12;
        const juce::String pitchCoarseM = drumTrack ? "-Bnk" : "-Oct";
        const juce::String pitchCoarseP = drumTrack ? "+Bnk" : "+Oct";

        pitchRow.attachTo( *this, drumTrack ? "Pad / Pitch" : "Pitch", pitchCoarseM, "-1", "+1", pitchCoarseP, true );
        startRow.attachTo( *this, "Start", "-Step", "-1t", "+1t", "+Step", true );
        lengthRow.attachTo( *this, "Length", "-Step", "-1t", "+1t", "+Step", true );
        velocityRow.attachTo( *this, "Velocity", "-10", "-1", "+1", "+10", true );
        probRow.attachTo( *this, "Probability", "-10%", "-1%", "+1%", "+10%", true );
        ratchetRow.attachTo( *this, "Ratchet", "", "-1", "+1", "", false );

        auto applyStep = [this]( int& field, int delta, int minVal, int maxVal ) {
            field = grid_logic::stepNoteProperty( field, delta, minVal, maxVal );
            refreshLabels();
        };

        pitchRow.coarseMinus.onClick = [&, octaveOrBank]() { applyStep( state.note, -octaveOrBank, 0, 127 ); };
        pitchRow.fineMinus.onClick   = [&]() { applyStep( state.note, -1, 0, 127 ); };
        pitchRow.finePlus.onClick    = [&]() { applyStep( state.note, +1, 0, 127 ); };
        pitchRow.coarsePlus.onClick  = [&, octaveOrBank]() { applyStep( state.note, +octaveOrBank, 0, 127 ); };

        startRow.coarseMinus.onClick = [&]() { applyStep( state.start, -stepTicks, 0, maxStart ); };
        startRow.fineMinus.onClick   = [&]() { applyStep( state.start, -1, 0, maxStart ); };
        startRow.finePlus.onClick    = [&]() { applyStep( state.start, +1, 0, maxStart ); };
        startRow.coarsePlus.onClick  = [&]() { applyStep( state.start, +stepTicks, 0, maxStart ); };

        constexpr int kMaxLen = 1920 * 64;
        lengthRow.coarseMinus.onClick = [&]() { applyStep( state.length, -stepTicks, 1, kMaxLen ); };
        lengthRow.fineMinus.onClick   = [&]() { applyStep( state.length, -1, 1, kMaxLen ); };
        lengthRow.finePlus.onClick    = [&]() { applyStep( state.length, +1, 1, kMaxLen ); };
        lengthRow.coarsePlus.onClick  = [&]() { applyStep( state.length, +stepTicks, 1, kMaxLen ); };

        velocityRow.coarseMinus.onClick = [&]() { applyStep( state.velocity, -10, 1, 127 ); };
        velocityRow.fineMinus.onClick   = [&]() { applyStep( state.velocity, -1, 1, 127 ); };
        velocityRow.finePlus.onClick    = [&]() { applyStep( state.velocity, +1, 1, 127 ); };
        velocityRow.coarsePlus.onClick  = [&]() { applyStep( state.velocity, +10, 1, 127 ); };

        probRow.coarseMinus.onClick = [&]() { applyStep( state.probability, -10, 0, 100 ); };
        probRow.fineMinus.onClick   = [&]() { applyStep( state.probability, -1, 0, 100 ); };
        probRow.finePlus.onClick    = [&]() { applyStep( state.probability, +1, 0, 100 ); };
        probRow.coarsePlus.onClick  = [&]() { applyStep( state.probability, +10, 0, 100 ); };

        ratchetRow.fineMinus.onClick = [&]() { applyStep( state.ratchet, -1, 1, 8 ); };
        ratchetRow.finePlus.onClick  = [&]() { applyStep( state.ratchet, +1, 1, 8 ); };

        addAndMakeVisible( muteBtn );
        muteBtn.setValueColour( ui::Theme::textColour( 1 ) );
        muteBtn.setValueFontHeight( 17 );
        muteBtn.setToggleButtonText( "Mute: ON", "Mute: OFF" );
        muteBtn.setToggleState( state.mute, juce::dontSendNotification );
        muteBtn.onClick = [&]() {
            state.mute = muteBtn.getToggleState();
            refreshLabels();
        };

        addAndMakeVisible( deleteBtn );
        deleteBtn.setClickingTogglesState( false );
        deleteBtn.setValueColour( juce::Colours::white );
        deleteBtn.setValueBackgroundColour( juce::Colour( 0xff9e2a2b ) );
        deleteBtn.setValueFontHeight( 17 );
        deleteBtn.onClick = [&]() {
            deleteRequested = true;
            mt::utils::exitDialog( this, UI::APPLY );
        };

        refreshLabels();
    }

    void GridNotePropertiesDialog::refreshLabels()
    {
        const juce::String noteLabel = drumTrack
                                           ? ( mt::Kick::getPadName( state.note ) + " (" + juce::String( state.note ) + ")" )
                                           : ( mt::ScaleAndNotes::getNoteName( state.note ) + " (" + juce::String( state.note ) + ")" );
        pitchRow.value.setText( noteLabel, juce::dontSendNotification );

        const juce::String bbtText = bbtFormatter ? bbtFormatter( state.start ) : juce::String( state.start );
        startRow.value.setText( bbtText + "  [" + juce::String( state.start ) + " t]", juce::dontSendNotification );
        lengthRow.value.setText( juce::String( state.length ) + " ticks", juce::dontSendNotification );
        velocityRow.value.setText( juce::String( state.velocity ), juce::dontSendNotification );
        probRow.value.setText( juce::String( state.probability ) + " %", juce::dontSendNotification );
        ratchetRow.value.setText( juce::String( state.ratchet ) + "x", juce::dontSendNotification );
    }

    void GridNotePropertiesDialog::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 1 ) );
    }

    void GridNotePropertiesDialog::resized()
    {
        auto b    = getLocalBounds().reduced( 14, 8 );
        auto rows = b.getHeight() / 7;
        pitchRow.layout( b.removeFromTop( rows ) );
        startRow.layout( b.removeFromTop( rows ) );
        lengthRow.layout( b.removeFromTop( rows ) );
        velocityRow.layout( b.removeFromTop( rows ) );
        probRow.layout( b.removeFromTop( rows ) );
        ratchetRow.layout( b.removeFromTop( rows ) );

        auto actionRow = b.reduced( 0, 4 );
        muteBtn.setBounds( actionRow.removeFromLeft( actionRow.getWidth() / 2 ).reduced( 0, 0 ).withTrimmedRight( 6 ) );
        deleteBtn.setBounds( actionRow.withTrimmedLeft( 6 ) );
    }

}
