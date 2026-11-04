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
#define loopRecorderScaleHeight 20

    LoopRecorder::LoopRecorder( ui::KickIOComponent& Opts )
        : options( Opts )
    {
        progressBar.setTextToDisplay( "" );
        progressBar.setColour( juce::ProgressBar::backgroundColourId, ui::Theme::backgroundColour( 12 ) );
        progressBar.setColour( juce::ProgressBar::foregroundColourId, ui::Theme::foregroundColour( 2 ) );

        ae.addListener( this );

        mt::utils::addAndMakeVisible( this, bars, syncB, stopB, progressBar, wav );
        mt::utils::addAndMakeVisible( this, reverseB, replaceB, playB, recordB, ph, outGain, outPan );

        outGain.setRange( 0.0, 1.0, 0.001 );
        outPan.setRange( -1.0, 1.0, 0.01 );
        outPan.setCentered( true );

        mt::utils::setShowTitle( true, bars, syncB, stopB );
        mt::utils::setTitleColour( ui::Theme::textColour( 1 ), bars, syncB, stopB );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), bars, syncB, stopB );
        mt::utils::setTitleBackgroundColour( ui::Theme::backgroundColour( 9 ), bars, syncB, stopB );
        mt::utils::setValueBackgroundColour( ui::Theme::backgroundColour( 13 ), bars, syncB, stopB );
        mt::utils::setTitleFontHeight( 18, syncB, stopB );
        mt::utils::setValueFontHeight( 20, bars, syncB, stopB );
        mt::utils::setTitleProportion( .42f, syncB, stopB );

        ui::themeChanged.connect( [&]() {
            reverseB.setImages( "Looper_reverse", "Looper_reverse_active" );
            recordB.setImages( "Looper_rec", "Looper_rec_active" );
            replaceB.setImages( "Looper_repeat", "Looper_repeat_active" );
            playB.setImages( "Looper_play", "Looper_play_active" );
        } );

        setupCallbackHandlers();
    }

    LoopRecorder::~LoopRecorder()
    {
        stopTimer();
        ae.removeListener( this );
    }

    void LoopRecorder::onSequenceSelectionChange( const mt::MTSelected& Selected )
    {
        tempoMgr = ae.getTempoMgr( Selected.sequence );
    }

    void LoopRecorder::setupCallbackHandlers()
    {
        /// Sidebar, options panel.
        options.inputChanged.connect( [this]( int selectedIndex, const juce::String& ) { ae.setLooperRecordingMode( ( mt::Recorder::Mode )( selectedIndex - 1 ) ); } );
        options.gainChanged.connect( [this]( float dbVal ) { ae.setLooperThresholdDB( dbVal ); } );
        options.monitorStateChanged.connect( [this]( bool Value ) { ae.setLooperMonitorOn( Value ); } );
        options.monitorLevelChanged.connect( [&]( double Value ) { ae.setLooperMonitorGain( Value ); } );

        /// Controls...
        outGain.onValueChanged = [&]( double Gain ) { ae.setLooperGain( Gain ); };
        outPan.onValueChanged  = [&]( double Pan ) { ae.setLooperPan( Pan ); };

        // Setup length. A new length clears the loop (the engine reallocates): ask first when there is a recording.
        bars.onValueChanged = [&]( int Value ) {
            if( looper_logic::needsClearConfirm( ae.isLooperEmpty(), shownBars, Value ) )
            {
                const int previous = shownBars;
                bars.setValue( previous, juce::dontSendNotification ); // stays until the user confirms
                auto* p = new ui::FilePopup( "Changing the length clears the recorded loop. Continue?" );
                mt::utils::showDialog(
                    "WARNING!", p, [this, p, Value]( int R ) {
                        if( R == UI::APPLY )
                        {
                            bars.setValue( Value, juce::dontSendNotification );
                            applyLength( Value );
                        }
                        delete p;
                    },
                    ui::layout::warningPopup );
                return;
            }
            applyLength( Value );
        };
        // Direction
        reverseB.onClick = [&]() { ae.setLooperDirection( reverseB.getToggleState() ? mt::Recorder::REVERSE : mt::Recorder::FORWARD ); };

        // Record Mode
        recordB.onClick = [&]() {
            if( recordB.getToggleState() ) record( false );
            else
            {
                ae.setLooperOverdub( false );
                ae.setLooperPlay();
            }
        };
        // Overdub Mode: leaving it also leaves the overdub flag (it used to stay set for good: every later REC became an
        // overdub, the REC LED showed the overdub colour and sequence recording stayed disarmed).
        replaceB.onClick = [&]() {
            if( replaceB.getToggleState() ) record( true );
            else
            {
                ae.setLooperOverdub( false );
                ae.setLooperPlay();
            }
        };
        // Play Mode
        playB.onClick = [&]() { ae.setLooperPlay(); };

        // Stop
        stopB.onClick = [&]() {
            if( !stopB.getToggleState() ) stop();
            else ae.setLooperPlay();
        };
    }

    void LoopRecorder::play( bool start )
    {
        stopB.setToggleState( true, juce::dontSendNotification );
        if( syncB.getToggleState() )
        {
            // looperPlayback() restarts the loop at 0. A plain PLAY on a sequence that already plays is a no-op in the
            // engine (guard against the doubled hardware PLAY), so with SYNC the sequence has to be restarted
            // explicitly, or loop and sequence run out of phase. Stopped: unchanged (a parked seek survives PLAY).
            ae.setSequencePlay( ae.getSelectedSequence(), start || ae.isSequencePlaying(), true );
        }

        ae.looperPlayback();
    }

    void LoopRecorder::stop( bool PauseSequence )
    {
        recordB.setToggleState( false, juce::dontSendNotification );
        stopB.setToggleState( false, juce::dontSendNotification );
        if( PauseSequence && syncB.getToggleState() )
            ae.setSequencePause();
        ae.looperStop();
        ae.setLooperOverdub( false );
    }

    void LoopRecorder::record( bool Overdub )
    {
        // Sampled before the flags below are set: a looper that is already recording keeps writing from its
        // current position, so the sequence must not be restarted under it (phase would split).
        const bool wasRecording = ae.isLooperRecording();

        // One start only: looperStart() below begins the recording (a second setLooperRecord(true) made the undo
        // snapshot twice, the second time of the already started take). REC replaces, OVERDUB adds.
        ae.setLooperOverdub( Overdub );

        stopB.setToggleState( true, juce::dontSendNotification );
        auto sync = syncB.getToggleState();
        if( sync )
        {
            // Same as play(): a playing sequence ignores a plain PLAY (engine guard), so restart it to align the
            // sequence start with the recording start. Stopped: unchanged (plain PLAY keeps a parked seek).
            // Already recording (OVERDUB / second RECORD press): no restart, the loop is not at its start.
            ae.setSequencePlay( ae.getSelectedSequence(), ae.isSequencePlaying() && !wasRecording, true );
        }

        ae.setLooperDirection( mt::Recorder::FORWARD );
        reverseB.setToggleState( false, juce::dontSendNotification ); // recording always runs forward: the button shows it
        ae.looperStart( false, sync );
    }

    void LoopRecorder::handleCommand( CPktMgr::HWMsg_t& hwMsg )
    {
        if( contracts::DclContractsHelper::isButtonMessage( hwMsg ) && ( contracts::DeviceControlContracts::BTN_PRESSED == contracts::DclContractsHelper::getButtonState( hwMsg ) ) )
        {
            auto buttonCode = contracts::DclContractsHelper::getButtonCode( hwMsg );
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_STOP: stop(); break;
                case contracts::DeviceControlContracts::EN_PLAY: play( false ); break;
                case contracts::DeviceControlContracts::EN_PLAYSTART: play( true ); break;
                case contracts::DeviceControlContracts::EN_RECORD: record( false ); break;
                case contracts::DeviceControlContracts::EN_OVERDUB: record( true ); break;
            }
        }
        else if( contracts::DclContractsHelper::isKnobMessage( hwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( hwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( hwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
            {
                // Key 61 has Q-Link 1..4: threshold, monitor level, output gain, output pan. The threshold goes to the
                // engine too (options.knobRotated() only moved the slider display).
                switch( knob )
                {
                    case contracts::DeviceControlContracts::EN_KNOB_1:
                    {
                        const float db = juce::jlimit( (float)DB::MIN, (float)DB::MAX, ae.getLooperThresholdDB() + (float)value );
                        ae.setLooperThresholdDB( db );
                        options.setGain( db );
                        break;
                    }
                    case contracts::DeviceControlContracts::EN_KNOB_2:
                        options.setMonitorLevelNormalized( juce::jlimit( 0.0, 1.0, options.getMonitorLevelNormalized() + value * 0.01 ), juce::sendNotification );
                        break;
                    case contracts::DeviceControlContracts::EN_KNOB_3: outGain.knobRotated( mt::mixlogic::qlinkDelta( value, outGain.getIncrement() ) ); break;
                    case contracts::DeviceControlContracts::EN_KNOB_4: outPan.knobRotated( mt::mixlogic::qlinkDelta( value, outPan.getIncrement() ) ); break;
                    default: options.knobCommand( knob, value ); break; // 5: input mode (not on the Key 61)
                }
            }
        }
    }

    void LoopRecorder::setThumbnail()
    {
        auto& source = ae.getLooperSampleBuffer();
        wav.setSource( { const_cast<float**>( source.getArrayOfReadPointers() ), source.getNumChannels(), source.getNumSamples() }, ae.getLooperSampleRate() );
    }

    void LoopRecorder::clearRecording()
    {
        stop();
        wav.clearCache();
        ae.looperClear();
        ph.reDraw();
    }

    void LoopRecorder::exportRecording( juce::String& FilePath )
    {
        stop();
        ae.looperSave( FilePath );
    }

    // Looper-level undo: the engine keeps one snapshot (before the last REC / OVERDUB); a second press redoes.
    void LoopRecorder::undoRecording()
    {
        stop();
        ae.looperUndo();
        wav.clearCache();
        setThumbnail();
        ph.reDraw();
    }

    // The tempo manager of the selected sequence, asked each time (the cached raw pointer outlived sequence / project changes).
    mt::MTTempoMgr* LoopRecorder::currentTempo()
    {
        tempoMgr = ae.getTempoMgr( ae.getSelectedSequence() );
        return tempoMgr;
    }

    // 0 = unknown (no tempo): the caller leaves the loop alone.
    double LoopRecorder::getRecordingLength( int Bars )
    {
        auto* tempo = currentTempo();
        return looper_logic::loopSeconds( tempo != nullptr, tempo ? tempo->getSecondsUpto( Bars ) : 0.0 );
    }

    void LoopRecorder::setRecordingLength( int Bars )
    {
        const auto timeInSeconds = getRecordingLength( Bars );
        if( timeInSeconds > 0.0 )
            ae.setLooperLength( timeInSeconds );
    }

    void LoopRecorder::applyLength( int Bars )
    {
        stop();
        setRecordingLength( Bars );
        shownBars = Bars;
        setThumbnail();
    }

    void LoopRecorder::getDisplayText( juce::String& markerText, int bar, int beat, int tick, int numDigits )
    {
        if( numDigits == 1 )
            markerText = markerText.formatted( "%d", bar );
        else if( numDigits == 2 )
            markerText = markerText.formatted( "%d.%d", bar, beat );
        else if( numDigits == 3 )
            markerText = markerText.formatted( "%d.%d.%d", bar, beat, tick );
    }

    void LoopRecorder::paint( juce::Graphics& g )
    {
        mt::utils::fillRoundedRect( g, wav.getBounds(), ui::Theme::backgroundColour( 11 ) );

        // draw scale
        g.setColour( ui::Theme::backgroundColour( 8 ) );
        juce::Font f( 9 );
        g.setFont( f );

        int num = 4, denom = 4;
        if( !ae.getTimeSignature( ae.getSelectedSequence(), num, denom ) || num <= 0 || denom <= 0 )
        {
            num   = settings.projects.numerator().getIntValue();
            denom = settings.projects.denominator().getIntValue();
        }
        if( num <= 0 ) num = 4;
        if( denom <= 0 ) denom = 4;

        int   numBeats  = looper_logic::calculateTotalBeats( bars.getValue(), num );
        float beatWidth = looper_logic::calculateBeatWidth( static_cast<float>( getWidth() ), numBeats );
        paintBeats( g, numBeats, beatWidth, scaleArea );
    }

    void LoopRecorder::paintBeats( juce::Graphics& g, int numBeats, float beatWidth, const juce::Rectangle<int>& displayRect )
    {
        juce::String maxTick;
        int          totalBeats        = numBeats;
        int          maxTickDisplayLen = g.getCurrentFont().getStringWidth( juce::String().formatted( "%d.%d.%d", totalBeats, 4, 960 ) ) + seSamplesSpacing + seSpaceFromMarker;
        int          maxBeatDisplayLen = g.getCurrentFont().getStringWidth( juce::String().formatted( "%d.%d.%d", totalBeats, 4, 0 ) );

        int totalDigits      = 1;
        int noOfBeatMarkings = 0;

        // total beat markings from sequence time signature (LO-09)
        int num = 4, denom = 4;
        if( !ae.getTimeSignature( ae.getSelectedSequence(), num, denom ) || num <= 0 || denom <= 0 )
        {
            num   = settings.projects.numerator().getIntValue();
            denom = settings.projects.denominator().getIntValue();
        }
        noOfBeatMarkings = ( num > 0 ) ? num : 4;
        auto denominator = ( denom > 0 ) ? denom : 4;

        // total tick markings
        int noOfTickMarkings = 0;

        if( std::floor( ( beatWidth - maxBeatDisplayLen ) / maxTickDisplayLen ) > 0 )
            noOfTickMarkings = static_cast<int>( std::floor( beatWidth / maxTickDisplayLen ) - 1 );

        if( noOfBeatMarkings > 0 )
            totalDigits += 1;
        if( noOfTickMarkings > 0 )
            totalDigits += 1;

        // Safe ppq resolution calculation preventing ARM SIGFPE
        int ppqResolution = 960;
        if( noOfTickMarkings > 0 )
        {
            ppqResolution = looper_logic::safePpqResolution( denominator );
        }

        float currentPosX = 0;
        float tickWidth   = beatWidth / ( noOfTickMarkings + 1 );

        int skipCount = 0;
        int skipIndex = 0;

        if( beatWidth <= ( maxBeatDisplayLen * 2 ) )
        {
            skipCount        = 2;
            int newBeatWidth = beatWidth * 2;
            if( newBeatWidth <= ( maxBeatDisplayLen * 2 ) )
                skipCount = 4;
        }
        else if( !noOfTickMarkings )
            skipCount = 1;

        int barInc = 4;
        if( noOfBeatMarkings <= 3 )
            barInc = noOfBeatMarkings;

        if( noOfBeatMarkings <= skipCount )
            totalDigits = 1;

        juce::String markingText;
        bool         tickDisplayed{ false };
        for( int i = 1; i <= totalBeats; i += barInc )
        {
            int bar;
            if( barInc == 1 )
                bar = static_cast<int>( std::ceil( i / barInc ) );
            else
                bar = static_cast<int>( std::ceil( i / barInc ) + 1 );

            juce::Colour scaleColur = ui::Theme::textColour( 1 );

            for( int j = 1; j <= noOfBeatMarkings + 1; j++ )
            {
                int beat = j;
                if( j != 1 )
                {
                    for( int k = 0; k < noOfTickMarkings; k++ )
                    {
                        markingText = juce::String().formatted( "%d.%d.%d", bar, beat - 1, ( k + 1 ) * ppqResolution );
                        drawMarkingText( g, markingText, currentPosX, displayRect );
                        currentPosX += tickWidth;
                        tickDisplayed = true;
                    }
                }
                if( j != noOfBeatMarkings + 1 )
                {
                    if( ( skipCount <= 1 ) || !( skipIndex++ % skipCount ) )
                    {
                        if( tickDisplayed || j == 1 || totalDigits == 1 )
                            scaleColur = ui::Theme::textColour( 1 );
                        else
                            scaleColur = ui::Theme::textColour( 3 );

                        getDisplayText( markingText, bar, beat, 0, totalDigits );
                        drawMarkingText( g, markingText, currentPosX, displayRect, scaleColur );

                        tickDisplayed = false;
                    }

                    if( noOfTickMarkings )
                        currentPosX += tickWidth;
                    else
                        currentPosX += beatWidth;
                }
            }
        }
    }

    void LoopRecorder::drawMarkingText( juce::Graphics& g, juce::String& markerText, float xpos, const juce::Rectangle<int>& displayRect, juce::Colour lineColour )
    {
        g.setColour( lineColour );
        g.drawLine( xpos + seSpaceFromMarker, displayRect.getTopLeft().getY() + 7.0f, xpos + seSpaceFromMarker, static_cast<float>( displayRect.getHeight() ) );
        g.drawText( markerText, static_cast<int>( xpos ), static_cast<int>( displayRect.getTopLeft().getY() + static_cast<float>( displayRect.getHeight() ) ), g.getCurrentFont().getStringWidth( markerText ), displayRect.getHeight() - 5,
                    juce::Justification::horizontallyCentred );
    }

    void LoopRecorder::timerCallback()
    {
        // Update play button state.
        reverseB.setEnabled( ae.isLooperPlay() );
        reverseB.setToggleState( ae.isLooperReverse(), juce::dontSendNotification );
        // Update toggle states...
        playB.setToggleState( ae.isLooperPlay(), juce::dontSendNotification );
        recordB.setToggleState( stopB.getToggleState() ? ae.isLooperRecording() : false, juce::dontSendNotification );
        replaceB.setToggleState( ae.isLooperOverdubbing(), juce::dontSendNotification );
        stopB.setEnabled( !ae.isLooperEmpty() );

        // Export / Clear follow the engine (a loop recorded with the hardware REC counts too).
        const bool exportable = looper_logic::canExportOrClear( ae.isLooperEmpty(), ae.isLooperRecording() );
        if( exportable != lastExportable )
        {
            lastExportable = exportable;
            recordingStarted( exportable );
        }

        // Update level meters...
        {
            auto values = ae.getLooperInputMeter();
            // the meter of the engine always has two entries (left, right)
            if( 2 <= values.size() )
                options.signalStrengthChanged( { values[0].rms, values[1].rms, values[0].peak, values[1].peak } );
            else
                options.signalStrengthChanged( {} );
        }

        // Update waveform.
        if( ae.isLooperRunning() )
        {
            setThumbnail();
            ph.reDraw();
        }
        return;
    }

    void LoopRecorder::resized()
    {
        bars.setBoundsRelative( .0306f, .8067f, .1073f, .1274f );

        reverseB.setBoundsRelative( .184f, .8466f, .095f, .0876f );

        replaceB.setBoundsRelative( .2852f, .8466f, .095f, .0876f );
        //playB.setBoundsRelative( .524f, .8466f, .095f, .0876f);
        recordB.setBoundsRelative( .3865f, .8466f, .095f, .0876f );

        outGain.setBoundsRelative( .4969f, .7986f, .1226f, .1394f );
        outPan.setBoundsRelative( .6042f, .7986f, .1226f, .1394f );

        syncB.setBoundsRelative( .7438f, .7928f, .0981f, .1394f );
        stopB.setBoundsRelative( .8481f, .7928f, .1226f, .1394f );

        auto b         = getLocalBounds();
        auto panelArea = b.withBottom( b.proportionOfHeight( 0.75f ) );

        progressBar.setBounds( panelArea.removeFromTop( 6 ) );
        ph.setBounds( panelArea );
        scaleArea = panelArea.removeFromTop( loopRecorderScaleHeight );
        wav.setBounds( panelArea );
    }

    void LoopRecorder::visibilityChanged()
    {
        auto visible = isVisible();
        if( visible )
        {
            options.setInputSelection( ae.getLooperRecordingMode(), juce::dontSendNotification );
            options.setGain( ae.getLooperThresholdDB() );
            options.setMonitor( ae.isLooperMonitorOn() );
            options.setMonitorLevelNormalized( ae.getLooperMonitorGain() );

            outGain.setValue( ae.getLooperGain(), juce::dontSendNotification );
            outPan.setValue( ae.getLooperPan(), juce::dontSendNotification );

            // Compared in whole frames, and never when the tempo is unknown: a float / double mismatch used to clear the
            // loop at every visit (100 BPM, 4 bars).
            auto       numBars = bars.getValue();
            const auto wanted  = getRecordingLength( numBars );
            if( wanted > 0.0 && !looper_logic::sameLoopLength( wanted, ae.getLooperLength(), ae.getLooperSampleRate() ) )
                setRecordingLength( numBars );
            shownBars = numBars;

            lastExportable = !lastExportable; // force the Export / Clear state to be sent
            startTimerHz( 30 );
        }
        else
        {
            stopTimer();
            // Leaving the screen stops the looper but does not pause a sequence the user may have started elsewhere.
            stop( false );
        }
        // Enable or Disable looper backend.
        ae.setLooperEnable( visible );

        for( auto it : getChildren() )
            it->setVisible( isVisible() );
    }

    ///////////////////////////////////////////////////////////////////////
    // Looper Playhead
    ///////////////////////////////////////////////////////////////////////
    LooperPlayHead::LooperPlayHead()
    {
        setName( "LooperPlayHead-Canvas" );
        setInterceptsMouseClicks( false, false );
    }

    void LooperPlayHead::paint( juce::Graphics& g )
    {
        auto b = getLocalBounds().toFloat();
        if( b.isEmpty() )
            return;

        auto cX = normPos * b.getWidth();
        mt::utils::renderPlayhead( g, b, cX, ui::Theme::foregroundColour( 2 ) );
    }

#undef loopRecorderScaleHeight
}
