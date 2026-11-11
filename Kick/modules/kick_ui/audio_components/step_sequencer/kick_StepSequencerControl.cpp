

namespace kick_ui
{
    StepSequencerControl::StepSequencerControl()
    {
        mt::utils::addAndMakeVisible( this, leftNudgeButton, rightNudgeButton, nudgeText, clearbar, clearpad, padSelect, settingsBtn, barLabel, padLabel, barMinus, barPlus, padMinus, padPlus );
        nudgeText.setInterceptsMouseClicks( false, false );

        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 3 ), barLabel, padLabel, nudgeText );
        mt::utils::setValueFontHeight( 18, barLabel, padLabel, clearpad, clearbar, padSelect, settingsBtn, nudgeText );
        mt::utils::setValueJustification( juce::Justification::centred, padLabel, barLabel, nudgeText );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), barLabel, padLabel, clearpad, clearbar, padSelect, settingsBtn, nudgeText );

        ui::themeChanged.connect( [&]() {
            leftNudgeButton.setImages( "Minus", "Minus_Active" );
            rightNudgeButton.setImages( "Plus", "Plus_Active" );
            barMinus.setImages( "Minus", "Minus_Active" );
            padMinus.setImages( "Minus", "Minus_Active" );
            barPlus.setImages( "Plus", "Plus_Active" );
            padPlus.setImages( "Plus", "Plus_Active" );
            clearpad.setToggleButtonText( "Clear Pad", "Clear Pad" );
            clearbar.setToggleButtonText( "Clear Bar", "Clear Bar" );
            padSelect.setToggleButtonText( "Pad Select", "Pad Select" );
            settingsBtn.setToggleButtonText( "Settings", "Settings" );
        } );
        leftNudgeButton.toFront( true );
        rightNudgeButton.toFront( true );
    }

    void StepSequencerControl::resized()
    {
        clearpad.setBoundsRelative( .0097f, .0185f, .1174f, .6296f );
        clearbar.setBoundsRelative( .1360f, .0185f, .1174f, .6296f );
        settingsBtn.setBoundsRelative( .2547f, .0185f, .1174f, .6296f );
        padSelect.setBoundsRelative( .6203f, .0185f, .1174f, .6296f );
        leftNudgeButton.setBoundsRelative( .8747f, .0185f, .0332f, .6296f );
        nudgeText.setBoundsRelative( .8747f, .0f, .1174f, .6666f );
        rightNudgeButton.setBoundsRelative( .9589f, .0185f, .0332f, .6296f );
        barLabel.setBoundsRelative( .4931f, .0f, .1174f, .6666f );
        padLabel.setBoundsRelative( .7475f, .0f, .1174f, .6666f );
        barMinus.setBoundsRelative( .4931f, .0185f, .0332f, .6296f );
        barPlus.setBoundsRelative( .57729, .0185f, .0332f, .6296f );
        padMinus.setBoundsRelative( .7475, .0185f, .0332f, .6296f );
        padPlus.setBoundsRelative( .8317f, .0185f, .0332f, .6296f );
    }

    ///
    /// Class StepSequencerTopControl
    ///

    StepSequencerTopControl::StepSequencerTopControl()
    {
        mt::utils::addAndMakeVisible( this, paramValueListBox, padValueListBox, timeDivValueListBox, barValueListBox, pad, timediv, bar, parameter, page, pageLabel, pageLeft, pageRight );

        int letterAsciiCode = 65;

        padData.clear();
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
        padValueListBox.setListOptions( padData );

        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 9 ), pad, parameter, bar, timediv, pageLabel );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), pad, parameter, bar, timediv, pageLabel );
        barValueListBox.setJustificationType( juce::Justification::centred );
        timeDivValueListBox.setJustificationType( juce::Justification::centred );
        page.setJustificationType( juce::Justification::centred );
        mt::utils::setValueJustification( juce::Justification::centred, parameter, pad, bar, timediv, pageLabel );
        mt::utils::setValueFontHeight( 16, parameter, pad, bar, timediv, pageLabel );

        pageLeft.onClick  = [&]() { page.knobRotated( -1.0 ); };
        pageRight.onClick = [&]() { page.knobRotated( 1.0 ); };

        page.onValueChanged            = [&]( const juce::String& ) { if( pageChanged ) pageChanged(); };
        barValueListBox.onValueChanged = [&]( const juce::String& ) { if( barChanged ) barChanged(); };
        padValueListBox.onValueChanged = [&]( const juce::String& ) { if( padChanged ) padChanged(); };
    }

    void StepSequencerTopControl::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, { 0, 0, 1003, 44 }, ui::Theme::backgroundColour( 1 ), 5.0f );
    }

    void StepSequencerTopControl::resized()
    {
        pad.setBoundsRelative( .012f, .2727f, .044f, .4545f );
        padValueListBox.setBoundsRelative( .058f, .1363f, .076f, .7272f );

        parameter.setBoundsRelative( .142f, .2727f, .110f, .4545f );
        paramValueListBox.setBoundsRelative( .255f, .1363f, .145f, .7272f );

        pageLabel.setBoundsRelative( .565f, .2727f, .054f, .4545f );
        page.setBoundsRelative( .622f, .1363f, .054f, .7272f );

        bar.setBoundsRelative( .686f, .2727f, .046f, .4545f );
        barValueListBox.setBoundsRelative( .735f, .1363f, .065f, .7272f );

        timediv.setBoundsRelative( .810f, .2727f, .090f, .4545f );
        timeDivValueListBox.setBoundsRelative( .904f, .1363f, .084f, .7272f );
    }

    void StepSequencerTopControl::setTotalBars( int bars )
    {
        if( barsData.size() != (size_t)bars )
        {
            barsData.clear();
            for( int i = 1; i <= bars; i++ )
            {
                barsData.push_back( juce::String( i ) );
            }
            barValueListBox.setListOptions( barsData );
        }
    }

    void StepSequencerTopControl::setMaxPages( int pages )
    {
        std::vector<juce::String> pagesData;
        for( int i = 1; i <= pages; i++ )
            pagesData.push_back( juce::String( i ) );

        page.setListOptions( pagesData );
    }

    int StepSequencerTopControl::getTimeDivision()
    {
        auto value = timeDivValueListBox.getSelectedIndex();
        return mt::Kick::TimeDivisionValues[value];
    }

    void StepSequencerTopControl::setSelectedPad( int index )
    {
        padValueListBox.setSelected( index, juce::dontSendNotification );
    }

    void StepSequencerTopControl::setSelectedPage( int index )
    {
        page.setSelected( index, juce::dontSendNotification );
    }

}
