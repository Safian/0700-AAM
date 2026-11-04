#include <engine/audio_engine/engine/mt_automation.h>

namespace kick_ui
{
    namespace
    {
        constexpr const char* kTouchFxTree = "TOUCHFX";
    }

    XyFxSetupScreen::XyFxSetupScreen()
    {
        attack.showTitleAndValue( true, juce::Slider::TextBoxBelow );
        release.showTitleAndValue( true, juce::Slider::TextBoxBelow );
        wetDry.showTitleAndValue( true, juce::Slider::TextBoxBelow );

        mt::utils::addAndMakeVisible( this, xAxisLabel, yAxisLabel, presets, attack, release, wetDry, xCoordinateValue, yCoordinateValue );
        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), xAxisLabel, yAxisLabel );
        mt::utils::setValueJustification( juce::Justification::centred, xAxisLabel, yAxisLabel, xCoordinateValue, yCoordinateValue );
        mt::utils::setValueJustification( juce::Justification::centredBottom, attack, release, wetDry );
        mt::utils::setTitleJustification( juce::Justification::centredTop, attack, release, wetDry, xAxisLabel, yAxisLabel );
        mt::utils::setValueFontHeight( 18, attack, release, wetDry, xAxisLabel, yAxisLabel, xCoordinateValue, yCoordinateValue, xCoordinateValue, yCoordinateValue );
        mt::utils::setTitleFontHeight( 16, attack, release, wetDry, presets, xAxisLabel, yAxisLabel );
        mt::utils::setValueProportion( .35f, xAxisLabel, yAxisLabel );
        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 13 ), xAxisLabel, yAxisLabel, xCoordinateValue, yCoordinateValue );

        std::vector<juce::String> names;
        for( int i = 0; i < xyfx::kNumPresets; ++i )
            names.emplace_back( xyfx::preset( i ).name );
        presets.setListOptions( names, 0, juce::dontSendNotification );

        attack.setRange( 0, 100, 1 );
        release.setRange( 0, 100, 1 );
        wetDry.setRange( 0, 100, 1 );
        attack.setValue( 5, juce::dontSendNotification );
        release.setValue( 30, juce::dontSendNotification );
        wetDry.setValue( 100, juce::dontSendNotification );
        // The sliders run 0..100 %; the text is the time they stand for.
        attack.textFromValueFunction  = []( double V ) { return juce::String( xyfx::envelopeText( (float)V ) ); };
        release.textFromValueFunction = []( double V ) { return juce::String( xyfx::envelopeText( (float)V ) ); };
        wetDry.setTextValueSuffix( " %" );
        attack.updateText();
        release.updateText();
        refreshAxes();
        setXYCoordinateValues( 0, 0 );

        presets.setJustificationType( juce::Justification::centred );
        presets.setTitleColour( ui::Theme::textColour( 1 ) );

        presets.onSelectionChanged = [&]( int, const juce::String& ) {
            refreshAxes();
            if( onPresetChange )
                onPresetChange();
            if( onChanged )
                onChanged();
        };
        attack.onValueChanged  = [&]( double ) { if( onChanged ) onChanged(); };
        release.onValueChanged = [&]( double ) { if( onChanged ) onChanged(); };
        wetDry.onValueChanged  = [&]( double ) { if( onChanged ) onChanged(); };
    }

    void XyFxSetupScreen::refreshAxes()
    {
        const auto& p = xyfx::preset( getPreset() );
        xAxisLabel.setText( xyfx::axisName( p.x ), juce::dontSendNotification );
        yAxisLabel.setText( xyfx::axisName( p.y ), juce::dontSendNotification );
    }

    void XyFxSetupScreen::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds();
        g.setColour( ui::Theme::backgroundColour() );
        g.fillRoundedRectangle( getLocalBounds().toFloat(), 3 );
        g.setColour( ui::Theme::outlineColour( 3 ) );
        g.drawRoundedRectangle( b.getX(), b.getY(), b.getWidth(), b.getHeight(), cornerSizeX, lineThickness );
    }

    void XyFxSetupScreen::resized()
    {
        presets.setBoundsRelative( .0584f, .0454f, .8830f, .1409f );
        attack.setBoundsRelative( .0584f, .209f, .8830f, .1409f );
        release.setBoundsRelative( .0584f, .3727f, .8830f, .1409f );
        wetDry.setBoundsRelative( .0584f, .5363f, .8830f, .1409f );

        xAxisLabel.setBoundsRelative( .0584f, .7f, .4269f, .1409f );
        yAxisLabel.setBoundsRelative( .5146f, .7f, .4269f, .1409f );
        xCoordinateValue.setBoundsRelative( .0584f, .8545f, .4269f, .1f );
        yCoordinateValue.setBoundsRelative( .5146f, .8545f, .4269f, .1f );
    }

    void XyFxSetupScreen::setValues( int Preset, float AttackPct, float ReleasePct, float WetPct )
    {
        presets.setSelected( juce::jlimit( 0, xyfx::kNumPresets - 1, Preset ), juce::dontSendNotification );
        attack.setValue( AttackPct, juce::dontSendNotification );
        release.setValue( ReleasePct, juce::dontSendNotification );
        wetDry.setValue( WetPct, juce::dontSendNotification );
        attack.updateText();
        release.updateText();
        refreshAxes();
    }

    void XyFxSetupScreen::stepPreset( int Delta )
    {
        presets.setSelected( juce::jlimit( 0, xyfx::kNumPresets - 1, getPreset() + ( Delta > 0 ? 1 : -1 ) ), juce::sendNotification );
    }

    void XyFxSetupScreen::setXYCoordinateValues( const float xCoord, const float yCoord )
    {
        const auto& p = xyfx::preset( getPreset() );
        xCoordinateValue.setText( xyfx::axisText( p.x, xCoord ), juce::dontSendNotification );
        yCoordinateValue.setText( xyfx::axisText( p.y, yCoord ), juce::dontSendNotification );
    }

    juce::String XyFxSetupScreen::summary()
    {
        return juce::String( xyfx::preset( getPreset() ).name ) + " | Attack " + xyfx::envelopeText( getAttackPercent() ) + " | Release " + xyfx::envelopeText( getReleasePercent() ) + " | Wet " + juce::String( (int)getWetPercent() ) + " %";
    }

    void XyFxSetupScreen::knobCommand( int knob, int value )
    {
        // One tick = one step of the slider (the raw tick count worked on a 0..100 range only by luck of the step size).
        switch( knob )
        {
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1: stepPreset( value ); break;
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_2: attack.knobRotated( mt::mixlogic::qlinkDelta( value, attack.getIncrement() ) ); break;
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_3: release.knobRotated( mt::mixlogic::qlinkDelta( value, release.getIncrement() ) ); break;
            case contracts::DeviceControlContracts::KnobSequence::EN_KNOB_4: wetDry.knobRotated( mt::mixlogic::qlinkDelta( value, wetDry.getIncrement() ) ); break;
        }
    }

    XyFxView::XyFxView()
        : kick_ui::IScreen( kick_ui::Screen::HomeXYView )
    {
        mt::utils::addAndMakeVisible( this, setupScreen, latch, setup, xyFx, header, on_off, qlinkModeBadge, output, hdrComp );
        mt::utils::addListener( this, latch, setup );
        xyFx.setEnabled( false );

        hdrComp.setValueBackgroundColor( ui::Theme::backgroundColour( 3 ) );
        header.showOptions( true, true ); // XY-13: the Automation icon sets the mode that records / plays the gesture lanes
        header.onBackButtonPressed = [this]( bool ) { closeScreen( Screen::HomeListView ); };
        output.setJustificationType( juce::Justification::centredLeft );
        output.setTitleColour( ui::Theme::textColour( 1 ) );
        rebuildTargets();

        mt::utils::setValueColour( ui::Theme::textColour( 1 ), latch, setup, on_off );
        mt::utils::setValueFontHeight( 18, latch, setup, on_off );
        mt::utils::setValueColour( ui::Theme::textColour( 2 ), qlinkModeBadge );
        mt::utils::setValueFontHeight( 12, qlinkModeBadge );
        output.setTitleFontHeight( 16 );

        const auto& p = xyfx::preset( setupScreen.getPreset() );
        xyFx.setAxes( p.x, p.y );

        on_off.onClick = [&]() {
            gate.setEnabled( on_off.getToggleState() );
            xyFx.setEnabled( on_off.getToggleState() );
            if( !on_off.getToggleState() )
            {
                xyFx.clearHighlight();
                closeEffect();
            }
            xyFx.repaint();
            saveState();
        };

        setupScreen.onPresetChange = [&]() {
            const auto& pr = xyfx::preset( setupScreen.getPreset() );
            xyFx.setAxes( pr.x, pr.y );
            setupScreen.setXYCoordinateValues( xyFx.getXCoordinate(), xyFx.getYCoordinate() );
        };
        setupScreen.onChanged = [&]() {
            if( restoring )
                return;
            push();
            saveState();
        };

        output.onSelectionChanged = [&]( int, const juce::String& ) {
            if( restoring )
                return;
            closeEffect(); // the effect leaves the old destination, a new touch opens it on the new one
            saveState();
        };

        // Finger down opens the gate, finger up closes it (Latch holds it); the position drives the effect while it is down.
        xyFx.onTouch = [&]( bool Down, const float, const float ) {
            if( Down ) gate.touchDown();
            else gate.touchUp();
            push();
        };
        xyFx.onValueChanged = [&]( const float xCoord, const float yCoord ) {
            setupScreen.setXYCoordinateValues( xCoord, yCoord );
            push();
        };

        // XY-11 / XY-12: Touch strip Touch FX mode & Shift+TOUCH_FX setup toggle
        signals::sigTouchFxStripPos.connect( [this]( float y ) {
            xyFx.setNormalizedY( y );
        } );
        signals::sigTouchFxStripGate.connect( [this]( bool down ) {
            if( down && !on_off.getToggleState() )
                on_off.setToggleState( true, juce::sendNotification );
            if( down ) gate.touchDown();
            else gate.touchUp();
            push();
        } );
        signals::sigTouchFxToggleSetup.connect( [this]() {
            setup.setToggleState( !setup.getToggleState(), juce::sendNotification );
        } );

        ui::themeChanged.connect( [&]() {
            latch.setToggleButtonText( "Latch", "Latch" );
            setup.setToggleButtonText( "Setup", "Setup" );
            on_off.setToggleButtonText( "Off", "On" );
            header.setIcon( "Icon_XY" );
        } );
    }

    XyFxView::~XyFxView() {}

    // ---- destinations: the selected track's program, the return / submix / output buses (the old list, "Output 1 / 2 / 1, 2",
    // had no consumer at all).
    void XyFxView::rebuildTargets()
    {
        auto& ae = mt::getAudioInterface();
        targets.clear();
        targets.push_back( { "Track", true, {}, false } );
        targets.push_back( { "MIDI CC", false, {}, true } );
        for( const auto& n : ae.getBusManager().getReturnBusNames() ) targets.push_back( { juce::String( n ), false, juce::String( n ), false } );
        for( const auto& n : ae.getBusManager().getSubmixBusNames() ) targets.push_back( { juce::String( n ), false, juce::String( n ), false } );
        for( const auto& n : ae.getBusManager().getOutputBusNames() ) targets.push_back( { juce::String( n ), false, juce::String( n ), false } );

        std::vector<juce::String> labels;
        for( const auto& t : targets ) labels.push_back( t.label );
        const int keep = output.getSelectedIndex() < 0 ? 0 : output.getSelectedIndex();
        const bool wasRestoring = restoring;
        restoring               = true; // the selection handler must not treat this as a user change
        output.setListOptions( labels, juce::jmin( keep, (int)labels.size() - 1 ), juce::sendNotification );
        restoring = wasRestoring;
    }

    const XyFxView::Target* XyFxView::selectedTarget()
    {
        const int i = output.getSelectedIndex();
        return ( i >= 0 && i < (int)targets.size() ) ? &targets[(size_t)i] : nullptr;
    }

    mt::MTIInsertsInterface* XyFxView::interfaceFor( const Target& T )
    {
        if( T.isMidiCc )
            return nullptr;
        auto& ae = mt::getAudioInterface();
        if( T.isTrack )
        {
            auto& sel   = ae.getSelected();
            auto* pProg = ae.getProgramDataForTrack( sel.sequence, sel.track );
            if( pProg == nullptr )
                return nullptr;
            return ae.getInsertsInterface( ae.getProgramIndexByName( pProg->name.get() ) );
        }
        if( auto* bus = ae.getBusManager().getBus( T.bus ) )
            return bus->getInsertsInterface();
        return nullptr;
    }

    double XyFxView::bpm()
    {
        auto& ae = mt::getAudioInterface();
        if( auto* tempo = ae.getTempoMgr( ae.getSelectedSequence() ) )
        {
            const double v = tempo->getTempoSettings().getBPM();
            if( v > 1.0 )
                return v;
        }
        return 120.0;
    }

    // The whole state goes to the engine in one lock-free write; the audio thread picks it up with its next block.
    void XyFxView::push()
    {
        const auto* target = selectedTarget();
        if( target == nullptr )
            return;

        auto& ae = mt::getAudioInterface();

        // XY-14: the "MIDI CC" location sends the position through the MIDI Control XY engine (its CCs, ranges, output port).
        if( target->isMidiCc )
        {
            if( auto* mc = dynamic_cast<MidiControl*>( KickHome::Instance().findScreen( Screen::MenuMidiControl ) ) )
            {
                if( gate.open() )
                    mc->sendXy( xyFx.getXCoordinate(), xyFx.getYCoordinate() );
                else
                    mc->resetXy(); // the next touch sends its first position again
            }
            applied    = *target;
            hasApplied = true;
            return;
        }

        // XY-13: Write mode records the gesture (position while the gate is open, the gate itself) as FX lanes of the selected track.
        if( target->isTrack )
        {
            const auto sel = ae.getSelected();
            if( gate.open() )
            {
                if( !gateRecorded )
                    ae.recordAutomationValue( sel, mt::automation::TouchGate, mt::automation::kNormScale );
                ae.recordAutomationValue( sel, mt::automation::TouchX, mt::automation::normToValue( xyFx.getXCoordinate() ) );
                ae.recordAutomationValue( sel, mt::automation::TouchY, mt::automation::normToValue( xyFx.getYCoordinate() ) );
            }
            else if( gateRecorded )
                ae.recordAutomationValue( sel, mt::automation::TouchGate, 0 );
            gateRecorded = gate.open();
        }

        // A destination change (or the selected track changing) moves the effect: the old one is closed first.
        if( hasApplied && ( applied.isTrack != target->isTrack || applied.bus != target->bus || target->isTrack ) )
            if( auto* old = interfaceFor( applied ) )
                if( old != interfaceFor( *target ) )
                    old->closeTouchFx();

        auto* engine = interfaceFor( *target );
        if( engine == nullptr || !engine->isAvailable() )
        {
            hasApplied = false;
            return;
        }

        const auto& p = xyfx::preset( setupScreen.getPreset() );
        mt::MTTouchFxSettings s;
        s.mode      = p.mode;
        s.filter    = p.filter;
        s.division  = ( p.x == xyfx::Axis::BeatDivision ) ? xyfx::divisionOfColumn( (int)xyFx.getXCoordinate() ) : 0;
        s.x         = ( p.x == xyfx::Axis::BeatDivision ) ? 0.f : xyFx.getXCoordinate();
        s.y         = xyFx.getYCoordinate();
        s.gate      = gate.open();
        s.attackMs  = xyfx::envelopeMs( setupScreen.getAttackPercent() );
        s.releaseMs = xyfx::envelopeMs( setupScreen.getReleasePercent() );
        s.wet       = setupScreen.getWetPercent() / 100.f;
        s.bpm       = bpm();
        engine->setTouchFx( s );
        applied    = *target;
        hasApplied = true;
    }

    void XyFxView::closeEffect()
    {
        gate.closeAll();
        if( gateRecorded )
            mt::getAudioInterface().recordAutomationValue( mt::getAudioInterface().getSelected(), mt::automation::TouchGate, 0 );
        gateRecorded = false;
        if( hasApplied )
            if( auto* engine = interfaceFor( applied ) )
                engine->closeTouchFx();
        hasApplied = false;
    }

    // ---- state in the project tree (TOUCHFX node)
    void XyFxView::saveState()
    {
        if( restoring )
            return;
        auto tree = mt::getAudioInterface().getValueTree().getOrCreateChildWithName( kTouchFxTree, nullptr );
        tree.setProperty( "preset", setupScreen.getPreset(), nullptr );
        tree.setProperty( "attack", setupScreen.getAttackPercent(), nullptr );
        tree.setProperty( "release", setupScreen.getReleasePercent(), nullptr );
        tree.setProperty( "wet", setupScreen.getWetPercent(), nullptr );
        tree.setProperty( "latch", latch.getToggleState(), nullptr );
        tree.setProperty( "destination", output.getSelected(), nullptr );
    }

    void XyFxView::loadState()
    {
        auto tree = mt::getAudioInterface().getValueTree().getChildWithName( kTouchFxTree );
        if( !tree.isValid() )
            return;
        restoring = true;
        setupScreen.setValues( tree.getProperty( "preset", 0 ), tree.getProperty( "attack", 5.0 ), tree.getProperty( "release", 30.0 ), tree.getProperty( "wet", 100.0 ) );
        latch.setToggleState( tree.getProperty( "latch", false ), juce::dontSendNotification );
        gate.setLatch( latch.getToggleState() );
        xyFx.setLatch( latch.getToggleState() );
        const auto dest = tree.getProperty( "destination", "Track" ).toString();
        for( int i = 0; i < (int)targets.size(); ++i )
            if( targets[(size_t)i].label == dest )
                output.setSelected( i, juce::dontSendNotification );
        const auto& p = xyfx::preset( setupScreen.getPreset() );
        xyFx.setAxes( p.x, p.y );
        restoring = false;
    }

    void XyFxView::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        on_off.setBoundsRelative( .8082f, .0299f, .0880f, .0823f );
        qlinkModeBadge.setBoundsRelative( .6350f, .0299f, .1650f, .0823f );
        latch.setBoundsRelative( .0097f, .9213f, .1174f, .0636f );
        xyFx.setBoundsRelative( .0097f, .1610f, .9823f, .7391f );
        output.setBoundsRelative( .2250f, .0299f, .1956f, .0823f );
        hdrComp.setBoundsRelative( .4305f, .0299f, .1956f, .0823f );
        if( setup.getToggleState() )
        {
            xyFx.setBoundsRelative( .0097f, .1610f, .6330f, .7391f );
            setupScreen.setBoundsRelative( .6575f, .1610f, .3346f, .8243f );
            setup.setBoundsRelative( .5273f, .9213f, .1174f, .0636f );
        }
        else
        {
            setupScreen.setBoundsRelative( .0f, .0f, .0f, .0f );
            setup.setBoundsRelative( .8747f, .9213f, .1174f, .0636f );
        }
    }

    void XyFxView::visibilityChanged()
    {
        if( isVisible() )
        {
            rebuildTargets();
            loadState();
            const int qmode = mt::getSettings().knobControl.getMode();
            qlinkModeBadge.setText( qmode == 0 ? "Q-LINK: CUSTOM" : "Q-LINK: SCREEN", juce::dontSendNotification );
            qlinkModeBadge.setValueColour( qmode == 0 ? ui::Theme::textColour( 3 ) : ui::Theme::textColour( 1 ) );
        }
        else
        {
            // Leaving the screen: nothing stays held (a touch gate left open would keep the effect on with no screen to close it).
            xyFx.clearHighlight();
            closeEffect();
        }
    }

    void XyFxView::buttonClicked( juce::Button* button )
    {
        if( button == &setup )
        {
            resized();
        }
        else if( button == &latch )
        {
            gate.setLatch( latch.getToggleState() );
            xyFx.setLatch( latch.getToggleState() );
            push();
            saveState();
        }
    }

    void XyFxView::showSummary()
    {
        // Also with the Setup panel closed: the Q-Link used to change values nobody could see.
        const int qmode = mt::getSettings().knobControl.getMode();
        qlinkModeBadge.setText( qmode == 0 ? "Q-LINK: CUSTOM" : "Q-LINK: SCREEN", juce::dontSendNotification );
        qlinkModeBadge.setValueColour( qmode == 0 ? ui::Theme::textColour( 3 ) : ui::Theme::textColour( 1 ) );
        AppStatusbar::Instance().setBoxInfo( setupScreen.summary() );
    }

    void XyFxView::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knobID    = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto knobValue = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knobID == contracts::DeviceControlContracts::EN_KNOB_CENTER )
                setupScreen.stepPreset( knobValue ); // Data Dial: previous / next preset
            else
                setupScreen.knobCommand( knobID, knobValue );
            showSummary();
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto  buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            auto& ae         = mt::getAudioInterface();
            auto& aeSel      = ae.getSelected();
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_PLAY: ae.setSequencePlay( aeSel.sequence, false, true ); return;
                case contracts::DeviceControlContracts::EN_PLAYSTART: ae.setSequencePlay( aeSel.sequence, true, true ); return;
                case contracts::DeviceControlContracts::EN_STOP: ae.setSequencePause(); return;
                case contracts::DeviceControlContracts::EN_PLUS: setupScreen.stepPreset( 1 ); showSummary(); return;
                case contracts::DeviceControlContracts::EN_MINUS: setupScreen.stepPreset( -1 ); showSummary(); return;
                case contracts::DeviceControlContracts::EN_KNOB_ENTER: latch.setToggleState( !latch.getToggleState(), juce::sendNotification ); return; // ENTER = Latch
            }
        }
    }

}
