#include "kick_Sampler.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/dcl/dcl_signals.h"

namespace kick_ui
{
    int Sampler::lastPressedPad = -1;
    Sampler::Sampler()
        : kick_ui::IScreen( kick_ui::Screen::MenuSampler )
    {
        mt::utils::addChildComponents( this, header, options, recorder, clear, exportt, sample, slice, padTap, padHold, cancel, slicePlus, undoMarker, save, play, discard, edit, inputTrim, deviceStatusLabel );
        mt::utils::makeHidden( cancel, slicePlus, undoMarker, save, play, discard, edit );
        mt::utils::setRadioGroupID( (int)kick_ui::RadioGroupId::SampleActions, sample, slice, padTap, padHold );
        mt::utils::setValueFontHeight( 18, sample, slice, padTap, padHold, cancel, slicePlus, undoMarker, save, play, discard, edit );
        mt::utils::setValueColour( ui::Theme::textColour( 1 ), sample, slice, padTap, padHold, cancel, slicePlus, undoMarker, save, play, discard, edit );

        options.showPanSlider( false );
        options.showRecordButton( false );
        options.showOutputs( false );

        options.setInputs( { "IN L - OUT MONO", "IN R - OUT MONO", "IN L+R - OUT MONO", "IN L - OUT STEREO", "IN R - OUT STEREO", "IN L+R - OUT STEREO" }, 5 );
        recorder.signalStrengthChanged.connect( [this]( mt::MTMeterData& values ) {
            options.signalStrengthChanged( { values[0].rms, values[1].rms, values[0].peak, values[1].peak } );
        } );

        recorder.recordModeChanged.connect( [&]( SamplerRecorder::RecordMode Mode ) {
            recordModeChanged( Mode );
        } );

        options.inputChanged.connect( [this]( int numChannels, const juce::String& ) {
            recorder.setRecordChannels( ( SamplerRecorder::RecordChannels )( numChannels - 1 ) );
        } );

        options.gainChanged.connect( [this]( float gain ) {
            recorder.setRecordThresholdDB( gain );
        } );

        options.outputChanged.connect( [this]( int numChannels, const juce::String& str ) {
        } );

        options.monitorStateChanged.connect( [this]( bool on ) {
            recorder.setMonitorOn( on );
        } );

        header.onBackButtonPressed = [this]( bool ) { closeScreen(); };

        inputTrim.setRange( -36.0, 12.0, 0.5 );
        inputTrim.setValue( 0.0, juce::dontSendNotification );
        inputTrim.onValueChanged = [this]( double v ) {
            ae.setRecorderGainDB( (float)v );
        };

        deviceStatusLabel.setValueJustification( juce::Justification::centred );
        deviceStatusLabel.setValueFontHeight( 13 );
        deviceStatusLabel.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );

        recorder.recordingStarted.connect( [this]( bool on ) {
            if( on )
                clear.setEnabled( true );
        } );

        exportt.setEnabled( false );
        exportt.onClick = [&]() { exportt.setToggleState( false, juce::dontSendNotification ); };
        clear.setEnabled( false );
        clear.onClick = [&]() {
            clear.setToggleState( false, juce::dontSendNotification );
            discardRecordingWithConfirm();
        };

        recorder.onPreviewStopped = [this]() { play.setToggleState( false, juce::dontSendNotification ); };

        // Monitor level reaches the recorder (default 80 % like the slider; the engine default of 100 % invited feedback).
        options.monitorLevelChanged.connect( [this]( double level ) { ae.setRecorderMonitorGain( (float)level ); } );
        ae.setRecorderMonitorGain( (float)options.getMonitorLevelNormalized() );
        options.showInserts( false ); // no Sampler insert chain in the engine (Looper only)

        /// Sampler Actions

        cancel.onClick = [&]() { discardRecordingWithConfirm(); };

        slicePlus.onClick = [&]() {
            if( samplePoints.size() == 0 )
                samplePoints.push_back( 0 );

            samplePoints.push_back( recorder.getCurrentRecordTime() );
            syncMarkersToRecorder();
        };

        undoMarker.onClick = [&]() {
            if( !samplePoints.empty() )
            {
                samplePoints.pop_back();
                syncMarkersToRecorder();
            }
        };

        save.onClick = [&]() {
            openSaveFilePopUp( false );
        };

        play.onClick = [&]() {
            if( play.getToggleState() )
                ae.startRecorderPreview( 0.0 );
            else
                ae.stopRecorderPreview();
        };

        discard.onClick = [&]() { discardRecordingWithConfirm(); };

        edit.onClick = [&]() {
            openSaveFilePopUp( true );
        };

        sample.onClick = [&]() {
            buttonState = SAMPLE;
        };

        slice.onClick = [&]() {
            buttonState = SLICE;
        };

        padTap.onClick = [&]() {
            buttonState = PADTAP;
            refreshPadsLED();
            lastPressedPad = -1;
        };

        padHold.onClick = [&]() {
            buttonState = PADHOLD;
            refreshPadsLED();
        };

        sample.triggerClick();

        ui::themeChanged.connect( [&]() {
            exportt.setImages( "Song_Export", "Song_Export_Active" );
            clear.setImages( "Song_Clear", "Song_Clear_Active" );
            header.setIcon( "Icon_Sampler" );
            sample.setToggleButtonText( "Sample", "Sample" );
            slice.setToggleButtonText( "Slice", "Slice" );
            play.setToggleButtonText( "Play", "Stop" );
        } );

        mt::signals::bankChanged.connect( [&]( int Index, juce::String Bank ) {
            if( kick_ui ::KickHome::Instance().getCurrentScreen() == kick_ui::Screen::MenuSampler )
            {
                contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::setBankMessage( Index ) );
                if( padTap.getToggleState() || padHold.getToggleState() )
                {
                    refreshPadsLED();
                    lastPressedPad = -1;
                }
            }
        } );
    }

    void Sampler::recordModeChanged( SamplerRecorder::RecordMode newMode )
    {
        mode = newMode;
        clear.setEnabled( mode != SamplerRecorder::RecordMode::recordModeNone );
        if( mode == SamplerRecorder::RecordMode::recordModeNone )
        {
            startSamplePoints.clear();
            endSamplePoints.clear();
            samplePoints.clear();
            recorder.setMarkers( {} );
            lastPressedPad = -1;
            mt::utils::makeVisible( sample, slice, padTap, padHold );
            mt::utils::makeHidden( cancel, slicePlus, undoMarker, save, play, discard, edit );
        }
        else if( mode == SamplerRecorder::RecordMode::recordModeRecord )
        {
            mt::utils::makeVisible( sample, slice, padTap, padHold, cancel );
            mt::utils::makeHidden( slicePlus, undoMarker, save, play, discard, edit );
        }
        else if( mode == SamplerRecorder::RecordMode::recordModeRecording )
        {
            if( slice.getToggleState() )
                mt::utils::makeVisible( slicePlus, undoMarker );
            else
                mt::utils::makeHidden( slicePlus, undoMarker );
            mt::utils::makeHidden( sample, slice, padTap, padHold, cancel, save, play, discard, edit );
        }
        else if( mode == SamplerRecorder::RecordMode::recordModeStopped )
        {
            mt::utils::makeVisible( save, play, discard, edit );
            mt::utils::makeHidden( sample, slice, padTap, padHold, cancel, slicePlus, undoMarker );
        }
    }

    void Sampler::discardRecordingWithConfirm()
    {
        auto p = new ui::FilePopup( "Do you want to discard this recording?" );
        mt::utils::showDialog(
            "WARNING!", p, [p, this]( int R ) {
                if( R == UI::APPLY )
                {
                    recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeNone );
                    recorder.clearRecording();
                }
                delete p;
            },
            ui::layout::warningPopup );
    }

    void Sampler::showSaveError( const juce::String& message )
    {
        kick_log( "Sampler", "save failed: %s", message.toRawUTF8() );
        ui::KickAlertWindow::showCancelAlert( ui::KickAlertWindow::KickAlertWindowType::Warning, message, "Sampler", []( int ) {} );
    }

    // Recordings live in the project: <project>/samples (the project saver already treats that folder as the project's own).
    juce::File Sampler::getSamplesDir() const { return ae.getProjectDir().getChildFile( "samples" ); }

    bool Sampler::programAcceptsPadSamples( int program ) const
    {
        auto* p = ae.getProgram( program );
        if( p == nullptr )
            return false;
        const auto t = p->getType();
        return t == mt::MTProgramType::Drum || t == mt::MTProgramType::Clip || t == mt::MTProgramType::Keygroup;
    }

    void Sampler::openSaveFilePopUp( bool isEditing )
    {
        KeepDiscardSampleComponent* process = new KeepDiscardSampleComponent( buttonState );
        mt::utils::showOptions( "KEEP OR DISCARD SAMPLES", process, [process, this, isEditing]( int r ) {
            if( r == UI::BUTTONS::APPLY )
            {
                auto fileName  = process->getSampleName();
                auto padIndex  = process->getPadIndex();
                auto selPrg    = process->getSelectedProgram();
                auto sliceSave = process->getSliceSaveTypeIndex();

                delete process;
                auto sampleFileName = saveAndLoadRecordedSample( fileName, padIndex, selPrg, sliceSave );
                if( isEditing && sampleFileName.isNotEmpty() )
                {
                    if( juce::File( sampleFileName ).exists() )
                    {
                        kick_ui::sigSampleEditLoadFileForEditing( sampleFileName, SampleEditorMode::modeChop );
                        for( auto marker : lastMarkerSeconds )
                            kick_ui::sigSampleEditAddRegionAt( marker );
                        kick_ui::signals::sigScreenOpen( kick_ui::Screen::MenuSampleEdit );
                    }
                }
            }
            else
                delete process;
        } );
    }

    juce::String Sampler::saveAndLoadRecordedSample( const juce::String& fileName, int PadIndex, int SelProgram, int SliceSaveType )
    {
        // 1. Where: <project>/samples/<unique name>.wav
        const auto samplesDir = getSamplesDir();
        if( !samplesDir.createDirectory() )
        {
            showSaveError( "Cannot create the folder " + samplesDir.getFullPathName() );
            return {};
        }
        const auto uniqueName = sampler_logic::uniqueFileName( fileName.toStdString(), ".wav", [&]( const std::string& n ) { return samplesDir.getChildFile( n ).existsAsFile(); } );
        const auto file       = samplesDir.getChildFile( uniqueName );

        // 2. Write, then verify what landed on disk (the engine call has no result): the saved file gives the real rate and length.
        ae.saveRecording( file.getFullPathName().toStdString() );
        double rate        = 0.0;
        int    totalFrames = 0;
        {
            juce::WavAudioFormat                    wav;
            std::unique_ptr<juce::AudioFormatReader> reader( file.existsAsFile() ? wav.createReaderFor( file.createInputStream().release(), true ) : nullptr );
            if( reader != nullptr )
            {
                rate        = reader->sampleRate;
                totalFrames = (int)juce::jmin<juce::int64>( reader->lengthInSamples, 0x7fffffff );
            }
        }
        if( rate <= 0.0 || totalFrames <= 0 )
        {
            file.deleteFile();
            showSaveError( "The recording could not be saved to " + file.getFullPathName() );
            return {};
        }
        const auto sampleFile = file.getFullPathName();

        // 3. Markers were taken against the recording; capture them before the recorder is reset.
        const std::vector<double> markers( samplePoints.begin(), samplePoints.end() );
        lastMarkerSeconds = markers;

        recorder.clearRecording();
        ae.addAsset( sampleFile, mt::AssetTypes::Samples );

        if( SelProgram != 0 )
            ae.setSelectedProgram( SelProgram - 1 ); // only now: choosing a program in the dialog and cancelling changes nothing

        if( buttonState == SAMPLE )
        {
            if( SelProgram != 0 && programAcceptsPadSamples( ae.getSelectedProgram() ) )
                ae.addSampleToProgram( ae.getSelectedProgram(), PadIndex, sampleFile );
        }
        else if( buttonState == SLICE )
        {
            const auto slices = sampler_logic::sliceModeSlices( markers, totalFrames / rate, rate, totalFrames );
            for( size_t i = 0; i < slices.size(); ++i )
                addAssetSlice( sampleFile, slices[i].start, slices[i].end, (int)i + 1 );

            if( SliceSaveType != 0 )
            {
                // "Slices To Pads": a new Drum program, slice i on pad i (whole sample on pad 0 when nothing was sliced).
                auto progIndex = ae.createProgram( mt::MTProgramType::Drum, juce::File( sampleFile ).getFileNameWithoutExtension() );
                if( slices.empty() )
                    ae.addSampleToProgram( progIndex, 0, sampleFile );
                else
                    for( size_t i = 0; i < slices.size(); ++i )
                        ae.addSampleToProgram( progIndex, slices[i].pad, sampleFile, (int)i + 1 );
            }
        }
        else // PADTAP, PADHOLD
        {
            addSlicesToPads( sampleFile, rate, totalFrames );
        }

        recordModeChanged( SamplerRecorder::RecordMode::recordModeNone );
        samplePoints.clear();
        return sampleFile;
    }

    void Sampler::addSlicesToPads( const juce::String& SampleFile, double sampleRate, int totalFrames )
    {
        const auto slices = sampler_logic::padSlices( startSamplePoints, endSamplePoints, totalFrames / sampleRate, sampleRate, totalFrames );
        const bool assign = programAcceptsPadSamples( ae.getSelectedProgram() );
        for( size_t i = 0; i < slices.size(); ++i )
        {
            addAssetSlice( SampleFile, slices[i].start, slices[i].end, (int)i + 1 );
            if( assign )
                ae.addSampleToProgram( ae.getSelectedProgram(), slices[i].pad, SampleFile, (int)i + 1 );
        }
        if( !assign )
            kick_log( "Sampler", "pad slices stored as sample slices only: the selected program takes no pad samples" );
    }

    juce::String Sampler::getValidFileName( const std::string& filePath )
    {
        const juce::File f( filePath );
        const auto       dir = f.getParentDirectory();
        return juce::String( sampler_logic::uniqueFileName( f.getFileNameWithoutExtension().toStdString(), f.getFileExtension().toStdString(),
                                                            [&]( const std::string& n ) { return dir.getChildFile( n ).existsAsFile(); } ) );
    }

    void Sampler::addAssetSlice( const juce::String& SampleFile, int LoopStart, int LoopEnd, int Index )
    {
        auto sliceTree = createVTree( mt::IDs::SLICE, mt::IDs::id, CommonUtils::createID(),
                                      mt::IDs::loopStart, LoopStart,
                                      mt::IDs::loopEnd, LoopEnd );

        mt::ASoundSlice sliceObj{ sliceTree, &ae.getUndoManager() };
        ae.addAssetSlice( SampleFile, &sliceObj, Index );
    }

    void Sampler::resized()
    {
        auto b = getLocalBounds();
        header.setBounds( b.removeFromTop( ui::layout::header.getHeight() ) );
        sample.setBoundsRelative( .0097f, .919f, .1565f, 0.063f );
        slice.setBoundsRelative( .1702f, .919f, .1565f, 0.063f );
        padTap.setBoundsRelative( .3307f, .919f, .1565f, 0.063f );
        padHold.setBoundsRelative( .491f, .919f, .1565f, 0.063f );
        cancel.setBoundsRelative( .651f, .919f, .1565f, 0.063f );

        slicePlus.setBoundsRelative( .0097f, .919f, .1565f, 0.063f );
        undoMarker.setBoundsRelative( .1702f, .919f, .1565f, 0.063f );

        save.setBoundsRelative( .0097f, .919f, .1565f, 0.063f );
        discard.setBoundsRelative( .1702f, .919f, .1565f, 0.063f );
        play.setBoundsRelative( .3307f, .919f, .1565f, 0.063f );
        edit.setBoundsRelative( .491f, .919f, .1565f, 0.063f );

        deviceStatusLabel.setBounds( 10, 52, 166, 30 );
        inputTrim.setBounds( 186, 52, 166, 30 );

        options.setBoundsRelative( .0097f, .1508f, .3346f, .7522f );
        recorder.setBoundsRelative( .3542f, .1588f, .6379f, .72f );
        exportt.setBoundsRelative( .7318f, .0258f, .0489f, .0823f );
        clear.setBoundsRelative( .7847f, .0258f, .0489f, .0823f );
    }

    void Sampler::syncMarkersToRecorder()
    {
        std::vector<double> m( samplePoints.begin(), samplePoints.end() );
        recorder.setMarkers( m );
    }

    void Sampler::updateInputDeviceStatus()
    {
        auto& devMgr = ae.getAudioDeviceManager();
        auto* dev = devMgr.getCurrentAudioDevice();
        const bool hasDev = ( dev != nullptr );
        const bool isOpen = hasDev && dev->isOpen();
        const int activeIns = hasDev ? dev->getActiveInputChannels().countNumberOfSetBits() : 0;
        const double sr = hasDev ? dev->getCurrentSampleRate() : 0.0;
        const auto status = sampler_logic::formatInputStatus( hasDev, isOpen, activeIns, sr );
        deviceStatusLabel.setText( juce::String( status ), juce::dontSendNotification );
        if( status == "NO INPUT" || status.rfind( "ERROR", 0 ) == 0 )
            deviceStatusLabel.setTextColour( ui::Theme::textColour( 3 ) );
        else
            deviceStatusLabel.setTextColour( ui::Theme::textColour( 1 ) );
    }

    void Sampler::handleCommand( CPktMgr::HWMsg_t& f_HwMsg )
    {
        if( contracts::DclContractsHelper::isPadButtonMessage( f_HwMsg ) )
        {
            int padNum   = contracts::DclContractsHelper::getPadNumber( f_HwMsg );
            int padState = contracts::DclContractsHelper::getPadState( f_HwMsg );
            int padVelocity = contracts::DclContractsHelper::getPadVelocity( f_HwMsg );

            if( sample.getToggleState() )
            {
            }
            else if( slice.getToggleState() )
            {
                if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
                {
                    if( ae.isRecorderRunning() )
                    {
                        samplePoints.push_back( recorder.getCurrentRecordTime() );
                        syncMarkersToRecorder();
                    }
                    else
                    {
                        recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeRecord );
                        if( samplePoints.size() == 0 )
                            samplePoints.push_back( 0 );
                        syncMarkersToRecorder();
                    }
                }
            }
            else if( padTap.getToggleState() ) // pad tap: 1st tap on a pad starts its slice, the next tap (same pad) ends it, another pad ends the previous and starts its own
            {
                if( padState == contracts::DeviceControlContracts::BTN_PRESSED && juce::isPositiveAndBelow( padNum, _NUM_PADS_IN_COMPONENT ) )
                {
                    const auto padIndex = padNum + ae.getBankOffset();
                    if( !ae.isRecorderRunning() )
                        recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeRecord );
                    const auto now = recorder.getCurrentRecordTime();

                    if( lastPressedPad == padNum )
                    {
                        if( !isSliceRecorded )
                        {
                            isSliceRecorded           = true;
                            endSamplePoints[padIndex] = now;
                            padLeds[(size_t)padNum]   = contracts::DeviceControlContracts::padColourlightGreen1;
                        }
                        else
                        {
                            startSamplePoints[padIndex] = now;
                            isSliceRecorded             = false;
                            padLeds[(size_t)padNum]     = contracts::DeviceControlContracts::padColourDarkRed;
                        }
                    }
                    else
                    {
                        if( lastPressedPad >= 0 )
                        {
                            const auto lastIndex = lastPressedPad + ae.getBankOffset();
                            if( !isSliceRecorded )
                                endSamplePoints[lastIndex] = now; // the previous slice ends where this one starts
                            padLeds[(size_t)lastPressedPad] = contracts::DeviceControlContracts::padColourlightGreen1;
                        }
                        startSamplePoints[padIndex] = now;
                        isSliceRecorded             = false;
                        lastPressedPad              = padNum;
                        padLeds[(size_t)padNum]     = contracts::DeviceControlContracts::padColourDarkRed;
                    }
                    sendPadLeds();
                }
            }
            else if( padHold.getToggleState() )
            {
                if( juce::isPositiveAndBelow( padNum, _NUM_PADS_IN_COMPONENT ) )
                {
                    auto padIndex = padNum + ae.getBankOffset();
                    if( padState == contracts::DeviceControlContracts::BTN_PRESSED )
                    {
                        if( !ae.isRecorderRunning() )
                            recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeRecord );
                        startSamplePoints[padIndex] = recorder.getCurrentRecordTime();
                        padLeds[(size_t)padNum]     = contracts::DeviceControlContracts::padColourDarkRed;
                        sendPadLeds();
                    }
                    else if( padState == contracts::DeviceControlContracts::BTN_RELEASED && startSamplePoints.count( padIndex ) )
                    {
                        endSamplePoints[padIndex] = recorder.getCurrentRecordTime();
                        padLeds[(size_t)padNum]   = contracts::DeviceControlContracts::padColourlightGreen1;
                        sendPadLeds();
                    }
                }
            }
        }
        else if( contracts::DclContractsHelper::isKnobMessage( f_HwMsg ) )
        {
            auto knob  = contracts::DclContractsHelper::getKnobCode( f_HwMsg );
            auto value = contracts::DclContractsHelper::getKnobValue( f_HwMsg );
            if( knob != contracts::DeviceControlContracts::EN_KNOB_CENTER )
            {
                switch( knob )
                {
                    case contracts::DeviceControlContracts::EN_KNOB_3:
                        options.knobRotated( value );
                        break;
                    case contracts::DeviceControlContracts::EN_KNOB_4:
                        options.monitorKnobRotated( value );
                        break;
                }
                recorder.knobCommand( knob, value );
                options.knobCommand( knob, value );
            }
        }
        else if( contracts::DclContractsHelper::isButtonMessage( f_HwMsg ) && ( contracts::DclContractsHelper::getButtonState( f_HwMsg ) == contracts::DeviceControlContracts::BTN_PRESSED ) )
        {
            auto buttonCode = contracts::DclContractsHelper::getButtonCode( f_HwMsg );
            switch( buttonCode )
            {
                case contracts::DeviceControlContracts::EN_RECORD:
                case contracts::DeviceControlContracts::EN_OVERDUB:
                {
                    if( ae.isRecorderRunning() || mode == SamplerRecorder::RecordMode::recordModeRecording || mode == SamplerRecorder::RecordMode::recordModeRecord )
                    {
                        recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeStopped );
                    }
                    else
                    {
                        recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeRecord );
                    }
                    return;
                }
                case contracts::DeviceControlContracts::EN_STOP:
                {
                    if( ae.isRecorderRunning() || mode == SamplerRecorder::RecordMode::recordModeRecording || mode == SamplerRecorder::RecordMode::recordModeRecord )
                    {
                        recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeStopped );
                    }
                    ae.stopRecorderPreview();
                    play.setToggleState( false, juce::dontSendNotification );
                    return;
                }
                case contracts::DeviceControlContracts::EN_PLAY:
                case contracts::DeviceControlContracts::EN_PLAYSTART:
                {
                    if( !play.getToggleState() )
                    {
                        play.setToggleState( true, juce::sendNotification );
                    }
                    else
                    {
                        play.setToggleState( false, juce::sendNotification );
                    }
                    return;
                }
            }
        }
    }

    void Sampler::refreshPadsLED()
    {
        std::vector<mt::MTISample*> samples;
        const auto                  bankOffset = ae.getBankOffset();

        ae.getProgramSamples( ae.getSelectedProgram(), samples );
        for( int hw = 0; hw < NUM_PADS_IN_BANK; hw++ )
        {
            bool assigned = false;
            for( auto it : samples )
                if( it && (int)it->keyNote.get() == bankOffset + hw && it->getSource( 0 ).length() )
                    assigned = true;
            padLeds[(size_t)hw] = assigned ? contracts::DeviceControlContracts::padColourDarkYellow : contracts::DeviceControlContracts::PadColourTrans;
        }
        sendPadLeds();
    }

    // One full-bank colour message: the only pad-LED message the MPC bridge processes (single-pad commands are dropped there).
    void Sampler::sendPadLeds()
    {
        std::vector<int> colours( padLeds.begin(), padLeds.end() );
        contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::
                                                     setAllPadDataColourInfoMessage( contracts::DeviceControlContracts::PadButtonState::StateOn, colours ) );
    }

    void Sampler::visibilityChanged()
    {
        auto handle_sampler = [this]( CPktMgr::HWMsg_t& f_HwMsg ) {

        };

        auto visible = isVisible();
        if( visible )
        {
            updateInputDeviceStatus();
            options.setGain( recorder.getRecordThresholdDB() );
            options.setMonitor( recorder.isMonitorOn() );
            options.setInputSelection( recorder.getRecordChannels(), juce::dontSendNotification );
            ae.setRecorderThresholdDB( recorder.getRecordThresholdDB() );
            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
            //Controls::InputEvents::clearAllPads();
            if( padTap.getToggleState() || padHold.getToggleState() )
            {
                refreshPadsLED();
                lastPressedPad = -1;
            }
        }
        else
        {
            ae.stopRecorderPreview();
            // Leaving the screen ends a running recording instead of re-arming (= restarting) it on return: the take is kept (stopped),
            // an armed-but-empty recorder is simply disarmed.
            if( mode == SamplerRecorder::RecordMode::recordModeRecording )
                recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeStopped );
            else if( mode == SamplerRecorder::RecordMode::recordModeRecord )
            {
                recorder.setRecordMode( SamplerRecorder::RecordMode::recordModeNone );
                recorder.clearRecording();
            }

            contracts::DclSignals::sendCommandToDcl( contracts::DclContractsHelper::clearAllPadsMessage() );
            contracts::DclSignals::refreshPadLeds();
        }

        for( auto it : getChildren() )
            it->setVisible( visible );
        exportt.setVisible( kShowExportButton && visible );
        recorder.setRecordMode( mode );
    }

    void Sampler::paint( juce::Graphics& g )
    {
        g.setColour( ui::Theme::backgroundColour( 1 ) );
        g.fillRoundedRectangle( { 10.0f, 86.0f, 342.0f, 396.0f }, 5.0f );
        g.fillRoundedRectangle( { 362.0f, 86.0f, 652.0f, 396.0f }, 5.0f );
    }
}
