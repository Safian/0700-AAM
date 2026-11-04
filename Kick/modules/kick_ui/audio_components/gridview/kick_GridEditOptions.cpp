
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

}
