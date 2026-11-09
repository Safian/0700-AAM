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

namespace mt
{

    ///
    /// MTAudioManager
    ///

    MTAudioManager::MTAudioManager( const char* PrjName, const char* PrjManufacturer, const char* PrjVersion )
        : MTAudioRecorderManager( engine )
        , MTAudioProgramManager( selected, engine )
        , MTAudioTransportManager( selected, *this )
        , MTIPadPerformanceData( juce::ValueTree{ IDs::PERFORMANCE }, nullptr )
    {
        juce::ignoreUnused( PrjName, PrjManufacturer, PrjVersion );
        prepareSave = [this] {
            juce::String failures;
            forAllPrograms([&](MTIProgram* program) {
                if (!program->prepareToSave()) {
                    const auto plugin = program->getState().getChildWithName("CLAP").getProperty("name").toString();
                    failures += "Program '" + program->getName() + "'" +
                                (plugin.isEmpty() ? juce::String{} : " (" + plugin + ")") + ": " + program->saveError() + "\n";
                }
            });
            return failures.isEmpty() ? juce::Result::ok() : juce::Result::fail(failures);
        };
        // G10: in Read mode the programs' pan holds the automated value; the project must store the pre-automation one.
        // Only the serialised copy is patched (message thread, where panBases lives); the live program tree, which the audio
        // thread reads, is never touched by a save. Programs are matched by id, as panBases is keyed.
        patchSnapshot = [this]( juce::ValueTree& Snapshot ) {
            if( panBases.empty() )
                return;
            auto programs = Snapshot.getChildWithName( IDs::PROGRAMS );
            for( int i = 0; i < programs.getNumChildren(); ++i )
            {
                auto prog = programs.getChild( i );
                const auto it = panBases.find( prog.getProperty( IDs::id ).toString() );
                if( it != panBases.end() )
                    prog.setProperty( IDs::pan, it->second, nullptr );
            }
        };
        onSaveFailure = [this](const juce::String& reason) {
            listeners.call([&](MTAudioInterfaceListener& listener) { listener.onProjectSaveFailed(reason); });
        };
        onDirtyChanged = [this](bool dirty) {
            listeners.call([&](MTAudioInterfaceListener& listener) { listener.onProjectDirtyChanged(dirty); });
        };

        //juce::MessageManager::callAsync( [&]() {
        // Audio previwer object...
        if( auto pPreview = std::make_unique<AudioPreview>( engine ) )
        {
            _previewer = pPreview.get();
            engine.addMeter( engine.createObject( std::move( pPreview ) ) );
            _previewer->initAudioRouting();
            assert( _previewer != nullptr );
        }

        // Recorder object...
        MTAudioRecorderManager::initRecorder();
        //} );

        // Metronome object...
        _metronome = std::make_unique<MTMetronome>( engine, getSettings().metronome.vTree, [this]() {
            const auto c = getSeqRecControl();
            return c.phase == seqrec::Phase::Recording || c.phase == seqrec::Phase::Armed;
        } );

        // Initialize Audio device...
        juce::MessageManager::callAsync( [&]() {
            dm = std::make_unique<AudioDevice>( AUDIO_NUM_INPUTS, AUDIO_NUM_OUTPUTS, juce::String() );

            if( dm.get() )
            {
                engine.addAudioDevice( "Master", dm.get(), true );
                engine.startAudioDevice( AUDIO_SAMPLE_RATE, AUDIO_BLOCK_SIZE );

                // Startup state: the device error used to be stored and never read (no sound, no message).
                auto&      startup   = kick_startup::state();
                const auto devError  = dm->lastError();
                auto*      current   = dm->getJuceAudioDeviceMgr().getCurrentAudioDevice();
                if( juce::SystemStats::getEnvironmentVariable( "KICK_NO_AUDIO_DEVICE", {} ) == "1" )
                {
                    startup.set( kick_startup::Step::AudioEngine, kick_startup::StepState::Skipped, "disabled" );
                }
                else if( devError.isNotEmpty() )
                {
                    const auto p = startup.fail( kick_startup::Fault::AudioDevice, devError.toStdString() );
                    kick_log( "error", "[ERROR] [AUDIO] %s\n", p.cause.c_str() );
                }
                else if( current == nullptr )
                {
                    const auto p = startup.fail( kick_startup::Fault::AudioDeviceMissing, "" );
                    kick_log( "error", "[ERROR] [AUDIO] %s\n", p.cause.c_str() );
                }
                else
                {
                    const auto name = current->getName() + " " + juce::String( (int)current->getCurrentSampleRate() ) + " Hz";
                    startup.set( kick_startup::Step::AudioEngine, kick_startup::StepState::Ok, name.toStdString() );
                    kick_log( "info", "[INFO] [AUDIO] device ready: %s\n", name.toRawUTF8() );
                }
            }
        } );

        // MIDI messages handler

        auto midiMesageHandler = [&]( const MTMidiMessage& Msg ) {
            //String messageText;
            //messageText << "[" << Msg.source->getName() << "] : " << Msg.message.getDescription() << "\n";
            //DBG( messageText );
            auto& msg = Msg.message;

            if( msg.isMidiClock() ) {}
            else if( msg.isMidiStart() ) {}
            else if( msg.isMidiStop() ) {}
            else if( msg.isMidiContinue() ) {}
            else if( msg.isTempoMetaEvent() ) {}
            else if( msg.isTimeSignatureMetaEvent() ) {}
            else
            {
                if( auto seq = getSequence( selected.sequence ) )
                {
                    seq->forAllTracks( [&]( MTTrack* T ) {
                        T->processIncomingMidiMessage( Msg );
                    } );
                }
            }
        };

        MTMidiDeviceManager::onMidiMessage = midiMesageHandler;
        //MTMidiDeviceManager::onMidiMessages = [&]( juce::Array<MTMidiMessage>& midiMessages ) {
        //    for( auto& m : midiMessages )
        //        midiMesageHandler( m );
        //};

        transportStateChanged = [&]( TransportState State ) {
            if( TransportState::Playing == State )
                stopPreview();

            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onStateChanged( State ); } );
            signals::playStateChanged( State );
        };

        trackSelectionChanged = [&]() { // Trigger views to update...
            selectedStr = sseq + "|" + strack + "|" + sprog;
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onTrackSelectionChange( selected ); } );
            signals::onTrackSelectionChange( selected );
        };

        seqSelectionChanged = [&]() { // Trigger views to update...
            selectedStr = sseq + "|" + strack + "|" + sprog;
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onSequenceSelectionChange( selected ); } );
            signals::onSequenceSelectionChange( selected );
        };

        progSelectionChanged = [&]() { // Trigger views to update...
            selectedStr = sseq + "|" + strack + "|" + sprog;
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onProgramSelectionChange( selected ); } );
            signals::onProgramSelectionChange( selected );
        };

        trackListChanged = [&]( int Sequence ) { // Trigger views to update...
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onTrackListChanged( Sequence ); } );
        };

        seqListChanged = [&]() { // Trigger views to update...
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onSequenceListChanged(); } );
        };

        progListChanged = [&]() { // Trigger views to update...
            listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onProgramListChanged(); } );
        };

        seqTimeSignatureChanged = [&]( float BPM, int Bars, int Num, int Den ) { // Trigger views to update...
            listeners.call( [&, BPM, Bars, Num, Den]( MTAudioInterfaceListener& l ) { l.onBPMAndTimeSignatureChanged( BPM, Bars, Num, Den ); } );
        };
    }

    MTAudioManager::~MTAudioManager()
    {
        // No callback may retain recorder/program objects while they are freed.
        engine.stopAudioDevice();
        internalTimer.stopTimer();
        // The application's views have already gone away. Destruction must not
        // publish transport notifications to their signal callbacks.
        listeners.clear();
        transportStateChanged = {};
        MTAudioTransportManager::stop();
        closeProject();
        MTAudioRecorderManager::closeRecorder();
        _previewer = nullptr;
        closeProject();
        MTAudioRecorderManager::closeRecorder();
        _previewer = nullptr;
        // Base destruction happens after the live program manager is gone.
        // A failed shutdown save must not be replaced with stale XML later.
        detachWithoutSaving();
        prepareSave = {}; patchSnapshot = {}; onSaveFailure = {}; onDirtyChanged = {};
    }

    ////////////////////////////
    //// Project
    ////////////////////////////
    void MTAudioManager::settleAudioThread( int callbacks, int timeoutMs )
    {
        // The audio device runs its callbacks on its own thread; after the engine objects were replaced, `callbacks` completed
        // callbacks mean it holds no reference to the old ones. Nothing to wait for when no device is running (headless).
        if( !dm || !dm->isRunning() )
            return;
        const auto target   = dm->completedCallbacks() + static_cast<uint64_t>( callbacks );
        const auto deadline = juce::Time::getMillisecondCounter() + static_cast<juce::uint32>( timeoutMs );
        while( dm->completedCallbacks() < target && juce::Time::getMillisecondCounter() < deadline )
            std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
    }

    MTAudioManager::SaveAsResult MTAudioManager::saveProjectAs( const juce::File& newDir, bool switchToNew, const CopyFolderFn& copyFolder, const std::atomic<bool>& cancel )
    {
        if( !projectLoaded )
        {
            SaveAsResult r;
            r.error = "No project is open";
            return r;
        }
        const auto r = projectManager::saveAs( newDir, switchToNew, copyFolder, cancel );
        kick_log( r.ok ? "info" : "warn", "[%s] [PROJ] Save as '%s': %s\n", r.ok ? "INFO" : "WARN", newDir.getFullPathName().toRawUTF8(), r.ok ? "ok" : r.error.toRawUTF8() );
        return r;
    }

    bool MTAudioManager::loadProject( juce::File& ProjectFile, bool createIfMissing )
    {
        const auto start_ms = kick_logger::get_uptime_ms();
        const auto projName = ProjectFile.getFullPathName();
        kick_log( "info", "[INFO] [PROJ] Project load begin: '%s'\n", projName.toRawUTF8() );
        lastLoadError.clear();

        // An existing project is opened, never made up: a missing / deleted one is reported and the open project stays as it is.
        if( !createIfMissing )
        {
            const bool exists = ProjectFile.existsAsFile() || ( ProjectFile.isDirectory() && ProjectFile.getChildFile( ProjectFile.getFileName() + __projectFileExtension ).existsAsFile() );
            if( !exists )
            {
                lastLoadError = "The project was not found: " + ProjectFile.getFullPathName();
                kick_log( "info", "[INFO] [PROJ] Project load end: '%s' (not found)\n", projName.toRawUTF8() );
                projectManager::clearDiscard();
                return false;
            }
        }

        if (!closeProject())
        {
            lastLoadError = "The open project could not be saved first: " + projectManager::getLastSaveError();
            projectManager::clearDiscard();
            kick_log( "info", "[INFO] [PROJ] Project load end: '%s' (failed to close previous, took %llu ms)\n",
                      projName.toRawUTF8(), static_cast<unsigned long long>( kick_logger::get_uptime_ms() - start_ms ) );
            return false;
        }
        projectManager::clearDiscard(); // "Don't save" applies to that one close only
        undoManager.clearUndoHistory();
        vTree = projectManager::loadOrCreate( ProjectFile );
        if (!vTree.isValid())
        {
            lastLoadError = projectManager::getLastSaveError().isNotEmpty() ? projectManager::getLastSaveError() : juce::String( "The project file is not valid" );
            kick_log( "info", "[INFO] [PROJ] Project load end: '%s' (invalid project, took %llu ms)\n",
                      projName.toRawUTF8(), static_cast<unsigned long long>( kick_logger::get_uptime_ms() - start_ms ) );
            return false;
        }

#if defined( USE_VALUE_TREE_DEBUGGER ) && ( USE_VALUE_TREE_DEBUGGER > 0 )
        debugger.setSource( vTree );
#endif

        // Initialize bus manager...
        busMgr.setup( vTree.getOrCreateChildWithName( IDs::BUSES, nullptr ), &undoManager );

        // Initialize recorder...
        MTAudioRecorderManager::setup( vTree, &undoManager );

        // Initialize Asset manager...
        MTAudioAssetManager::setup( vTree.getOrCreateChildWithName( IDs::FILEMANAGER, nullptr ), &undoManager, projectManager::getProjectDir() );

        // Initialize master tempo settings...
        MTTempoMgr::setup( vTree.getOrCreateChildWithName( IDs::TEMPOS, nullptr ), &undoManager );

        // Initialize transport state...
        MTAudioTransportManager::setup();

        // Initialize Programs....
        MTAudioProgramManager::setup( vTree.getOrCreateChildWithName( IDs::PROGRAMS, nullptr ), &undoManager );
        //DBG( "LP: FM3: " << engine.getFeeMemory() );

        settleAudioThread( 3, 300 ); // the programs are live on the audio thread before songs / sequences refer to them

        // Initialize Song Manager...
        MTSongManager::init( vTree.getOrCreateChildWithName( IDs::SONGS, nullptr ), &undoManager );

        // Initialize Sequences....
        MTAudioSequenceManager::setup( vTree.getOrCreateChildWithName( IDs::SEQUENCES, nullptr ), &undoManager );

        projectLoaded = true;

        //DBG( "LP: Exit FM: " << engine.getFeeMemory() );
        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onProjectLoaded( ProjectFile ); } );

        // Reset Selection.
        setSelectedSequenceAndTrack( 0, 0 );
        setSelectedProgram( 0 );

        // Start timer for metering and playhead...
        internalTimer.setCallbackFunction( [&]() { interTimerCallbackHandler(); } );
        internalTimer.startTimerHz( 25 );
        kick_log( "info", "[INFO] [PROJ] Project load end: '%s' (success, took %llu ms, %zu programs)\n",
                  projName.toRawUTF8(), static_cast<unsigned long long>( kick_logger::get_uptime_ms() - start_ms ),
                  getPrograms().size() );
        return true;
    }

    bool MTAudioManager::closeProject()
    {
        if (!projectLoaded) return true;
        const auto start_ms = kick_logger::get_uptime_ms();
        const auto projName = vTree.getProperty( IDs::name, "project" ).toString();
        kick_log( "info", "[INFO] [PROJ] Project close begin: '%s'\n", projName.toRawUTF8() );

        // Persist while the programs still exist. On failure the active project,
        // playback and selection remain available for recovery and retry.
        if (!projectManager::close())
        {
            kick_log( "info", "[INFO] [PROJ] Project close end: '%s' (failed to persist, took %llu ms)\n",
                      projName.toRawUTF8(), static_cast<unsigned long long>( kick_logger::get_uptime_ms() - start_ms ) );
            return false;
        }
        // loadProject runs on a worker (LoadProjectAndAnimation::run). The 25 Hz timer callback (applyAutomatedPans) mutates
        // panBases on the message thread, so stop the timer and clear panBases there too. No lock is held here, and
        // projectManager::close() above already marshals to the message thread from this same worker, so this adds no new
        // blocking dependency.
        {
            auto stopAndClear = [this] {
                internalTimer.stopTimer();
                panBases.clear(); // the project (and its programs) goes away; nothing to restore
            };
            auto* mm = juce::MessageManager::getInstanceWithoutCreating();
            if( !mm || mm->isThisTheMessageThread() )
                stopAndClear();
            else
                mm->callFunctionOnMessageThread( []( void* ctx ) -> void* {
                    ( *static_cast<decltype( stopAndClear )*>( ctx ) )();
                    return nullptr;
                }, &stopAndClear );
        }
        projectLoaded = false;
        MTAudioTransportManager::close();

        // Call listeners
        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onProjectClose(); } );

        //DBG( "CP: Enter FM: " << engine.getFeeMemory() );
        selected = { -1, -1, -1 };

        // Save and close project first...
        vTree = juce::ValueTree{ IDs::ROOT };

        // Now close rest of the things...
        MTAudioAssetManager::close();
        MTSongManager::close();

        //DBG( "CP: FM2: " << engine.getFeeMemory() );
        engine.reset();
        settleAudioThread( 2, 200 );
        //DBG( "CP: FM3: " << engine.getFeeMemory() );

        MTAudioSequenceManager::close();
        MTAudioProgramManager::close();

        settleAudioThread( 3, 300 ); // the audio thread no longer touches the closed programs
        //DBG( "CP: End FM: " << engine.getFeeMemory() );

        // Reset Selection.
        setSelectedSequenceAndTrack( -1, -1 );
        setSelectedProgram( -1 );
        kick_log( "info", "[INFO] [PROJ] Project close end: '%s' (took %llu ms)\n",
                  projName.toRawUTF8(), static_cast<unsigned long long>( kick_logger::get_uptime_ms() - start_ms ) );
        return true;
    }

    bool MTAudioManager::saveProject()
    {
        return projectManager::save(); // G10 pan write-back lives in patchSnapshot (all save paths)
    }
    bool MTAudioManager::autosaveNow()
    {
        return projectManager::save( projectManager::SaveReason::Autosave );
    }
    ////////////////////////////
    //// Undo/Redo..
    ////////////////////////////
    // Undo / redo change the ValueTree only. The engine objects (programs, sequences, tracks) are built from tree nodes, so
    // an undone/redone create or delete leaves them stale; this brings them back in line, then re-reads what depends on the
    // bar grid (a time signature change is undone through the tree too). Done here, after the undo call returned, and not
    // from valueTreeChildAdded/Removed: building objects inside the undo callback would write to the UndoManager while it
    // is performing, which JUCE forbids (the write is dropped and asserts).
    void MTAudioManager::syncEngineWithTree()
    {
        // Programs first: the tracks resolve their program from the program list.
        MTAudioProgramManager::reconcileWithTree();
        MTAudioSequenceManager::reconcileWithTree();
        forAllSequences( [&]( MTSequence* S ) {
            S->forAllTracks( []( MTTrack* T ) { T->refreshProgram(); } );
            S->refreshTimeGrid();
        } );
    }

    void MTAudioManager::performUndo()
    {
        if( undoManager.canUndo() )
        {
            undoManager.undo();
            syncEngineWithTree();
        }
    }

    void MTAudioManager::performRedo()
    {
        if( undoManager.canRedo() )
        {
            undoManager.redo();
            syncEngineWithTree();
        }
    }

    ////////////////////////////
    //// Automation..
    ////////////////////////////
    void MTAudioManager::setAutomationMode( int Mode )
    {
        Mode = automation::clampMode( Mode );
        const int old = automationMode.load();
        if( Mode == old )
            return;
        if( automation::Read == old )
        {
            // Leaving Read: the programs get their stored pan back. Done with the mode momentarily Off so that the
            // restore itself is never recorded when the new mode is Write.
            automationMode.store( automation::Off );
            restoreAutomatedPans();
            restoreAutomatedFx();
        }
        automationMode.store( Mode );
        forAllSequences( [Mode]( MTSequence* S ) {
            S->forAllTracks( [Mode]( MTTrack* T ) { T->automationModeChanged( Mode ); } );
        } );
    }

    bool MTAudioManager::isPanLaneOwner( const MTTrack* Track, const MTIProgram* Prog )
    {
        auto* mm = juce::MessageManager::getInstanceWithoutCreating();
        if( !mm || !mm->isThisTheMessageThread() )
            return true;
        int mine = -1, lowestOther = -1;
        forAllSequences( [&]( MTSequence* S ) {
            const int si = getSequenceIndex( S );
            S->forAllTracks( [&]( MTTrack* T ) {
                const int order = si * 1000 + T->getIndex();
                if( T == Track )
                    mine = order;
                else if( T->isPlayingProgram( Prog ) && ( lowestOther < 0 || order < lowestOther ) )
                    lowestOther = order;
            } );
        } );
        return lowestOther < 0 || mine <= lowestOther;
    }

    void MTAudioManager::applyAutomatedPans()
    {
        struct Claim
        {
            MTIProgram* prog;
            int         value;
            int         order;
        };
        std::vector<Claim> claims; // message thread: allocation is fine here
        forAllSequences( [&]( MTSequence* S ) {
            const int si = getSequenceIndex( S );
            S->forAllTracks( [&]( MTTrack* T ) {
                const int v = T->takePanTarget(); // always consumed, so nothing stale survives a mode change
                if( automation::kNoValue != v )
                    if( auto* p = T->getProgram() )
                        claims.push_back( { p, v, si * 1000 + T->getIndex() } );
            } );
        } );
        std::vector<juce::String> restores; // F7: pan lanes that became empty since the last pass
        forAllSequences( [&]( MTSequence* S ) {
            S->forAllTracks( [&]( MTTrack* T ) {
                if( auto id = T->takePanRestore(); id.isNotEmpty() )
                    restores.push_back( id );
            } );
        } );
        for( const auto& id : restores )
            restoreAutomatedPan( id );
        if( automation::Read != getAutomationMode() )
            return;
        for( const auto& c : claims )
        {
            bool wins = true;
            for( const auto& o : claims )
                if( o.prog == c.prog && o.order < c.order )
                    wins = false;
            if( !wins )
                continue;
            const double target = automation::valueToPan( c.value );
            if( std::abs( (double)c.prog->getPan() - target ) < 1e-6 )
                continue;
            panBases.emplace( c.prog->id.get(), (double)c.prog->getPan() ); // keeps the first (pre-automation) value
            c.prog->vTree.setProperty( IDs::pan, target, nullptr ); // same property the mixer knob writes; not undoable
        }
    }

    void MTAudioManager::applyAutomatedFx()
    {
        const bool read = automation::Read == getAutomationMode();
        forAllSequences( [&]( MTSequence* S ) {
            S->forAllTracks( [&]( MTTrack* T ) {
                // The pending values are always consumed, so nothing stale survives a mode change.
                int  v[automation::kNumFxLanes];
                bool any = false;
                for( int i = 0; i < automation::kNumFxLanes; ++i )
                {
                    v[i] = T->takeFxTarget( automation::kFirstFxLane + i );
                    any  = any || v[i] != automation::kNoValue;
                }
                if( !any || !read )
                    return;
                auto* p = T->getProgram();
                auto* f = p ? dynamic_cast<MTIInsertsInterface*>( p ) : nullptr;
                if( !f || !f->isAvailable() )
                    return;
                const auto id = p->id.get();
                if( v[automation::TouchX - automation::kFirstFxLane] != automation::kNoValue || v[automation::TouchY - automation::kFirstFxLane] != automation::kNoValue
                    || v[automation::TouchGate - automation::kFirstFxLane] != automation::kNoValue )
                {
                    auto last = [&]( int Param, int Fallback ) { const int a = T->lastFxValue( Param ); return a == automation::kNoValue ? Fallback : a; };
                    // A gesture without a gate lane (edited by hand) keeps the gate open.
                    const bool gate = T->hasFxLane( automation::TouchGate ) ? automation::gateOpen( last( automation::TouchGate, 0 ) ) : true;
                    f->setTouchFxGesture( (float)automation::valueToNorm( last( automation::TouchX, 0 ) ), (float)automation::valueToNorm( last( automation::TouchY, 0 ) ), gate );
                    fxDriven[id] |= 256u;
                }
                for( int slot = 0; slot < automation::kNumInsertLanes; ++slot )
                {
                    const int value = v[automation::InsertParam0 + slot - automation::kFirstFxLane];
                    const int param = T->getFxLaneTarget( automation::InsertParam0 + slot );
                    if( value == automation::kNoValue || param < 0 )
                        continue;
                    f->setParameterAutomated( (unsigned)slot, param, (float)automation::valueToNorm( value ) );
                    fxDriven[id] |= ( 1u << slot );
                }
            } );
        } );
    }

    // Leaving Read: the Touch FX gate closes and the driven insert parameters go back to the values stored in the tree.
    void MTAudioManager::restoreAutomatedFx()
    {
        for( const auto& d : fxDriven )
        {
            auto* p = getProgramById( d.first );
            auto* f = p ? dynamic_cast<MTIInsertsInterface*>( p ) : nullptr;
            if( !f )
                continue;
            if( d.second & 256u )
                f->closeTouchFx();
            for( int slot = 0; slot < automation::kNumInsertLanes; ++slot )
                if( d.second & ( 1u << slot ) )
                    f->restoreInsertFromState( (unsigned)slot );
        }
        fxDriven.clear();
    }

    // Message thread. Puts back the stored pan of one program, unless another track's pan lane still drives it.
    void MTAudioManager::restoreAutomatedPan( const juce::String& ProgramId )
    {
        const auto it = panBases.find( ProgramId );
        if( it == panBases.end() )
            return;
        bool driven = false;
        forAllSequences( [&]( MTSequence* S ) {
            S->forAllTracks( [&]( MTTrack* T ) { driven = driven || T->panLaneDrives( ProgramId ); } );
        } );
        if( driven )
            return;
        if( auto* p = getProgramById( ProgramId ) )
            p->vTree.setProperty( IDs::pan, it->second, nullptr );
        panBases.erase( it );
    }

    void MTAudioManager::restoreAutomatedPans()
    {
        for( const auto& b : panBases )
            if( auto* p = getProgramById( b.first ) )
                p->vTree.setProperty( IDs::pan, b.second, nullptr );
        panBases.clear();
    }

    void MTAudioManager::clearAutomation( int Scope )
    {
        undoManager.beginNewTransaction( "Clear Automation" );
        if( 0 == Scope )
        {
            if( auto* t = getTrack() )
                t->clearAutomation( true );
        }
        else if( 1 == Scope )
        {
            if( auto* s = getSequence( selected.sequence ) )
                s->forAllTracks( []( MTTrack* T ) { T->clearAutomation( true ); } );
        }
        else
            forAllSequences( []( MTSequence* S ) {
                S->forAllTracks( []( MTTrack* T ) { T->clearAutomation( true ); } );
            } );
    }

    ////////////////////////////
    //// Listeners..
    ////////////////////////////
    void MTAudioManager::addListener( MTAudioInterfaceListener* l )
    {
        listeners.add( l );
        if( l )
        {
            l->onSequenceListChanged();
            l->onTrackListChanged( selected.sequence );
            l->onProgramListChanged();
            l->onProgramSelectionChange( selected );
            l->onSequenceSelectionChange( selected );
            l->onTrackSelectionChange( selected );
        }
    }

    void MTAudioManager::removeListener( MTAudioInterfaceListener* l ) { listeners.remove( l ); }

    ////////////////////////////
    //// Selection
    ////////////////////////////
    void MTAudioManager::setSelected( MTSelected Selected )
    {
        setSelectedProgram( Selected.program );
        setSelectedSequenceAndTrack( Selected.sequence, Selected.track );
    }

    ////////////////////////////
    //// Creators
    ////////////////////////////
    MTSampleEditorInterface* MTAudioManager::createSampleEditor()
    {
        MTSampleEditorInterface* sei = new MTSampleEditorInterface;
        return sei;
    }

    juce::AudioDeviceManager& MTAudioManager::getAudioDeviceManager()
    {
        return dm->getJuceAudioDeviceMgr();
    }

    void MTAudioManager::triggerTrackNotesChanged( int Sequence, int Track )
    {
        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onTrackNoteChanged( Sequence, Track ); } );
    }

    void MTAudioManager::triggerTrackProgramChanged( int Track, MTIProgram* Program )
    {
        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onTrackProgramChanged( Track, getProgramIndex( Program ) ); } );
    }

    ////////////////////////////
    //// Preview Utilities
    ////////////////////////////
    void MTAudioManager::createPreview( const juce::File& F, float Gain )
    {
        _previewer->setGain( (double)Gain );
        _previewer->previewOn( F.getFullPathName() );
    }

    void MTAudioManager::stopPreview() { _previewer->previewOff(); }
    void MTAudioManager::loopPreview( bool Loop ) { _previewer->setLooping( Loop ); }
    void MTAudioManager::setPreviewMute( bool Mute ) { _previewer->setMute( Mute ); }
    bool MTAudioManager::isPreviewRunning() { return _previewer->isRunning(); }
    bool MTAudioManager::isPreviewLooping() { return _previewer->isLooping(); }
    bool MTAudioManager::isPreviewMuted() { return _previewer->isMuted(); }

    void MTAudioManager::setPreviewGain( float Gain ) { _previewer->setGain( (double)Gain ); }

    MTMeterData MTAudioManager::readPreviewMeter()
    {
        return engine.readMeter( _previewer->getId() );
    }

    ////////////////////////////
    //// Bank
    ////////////////////////////
    void MTAudioManager::setBank( const juce::String& Bank )
    {
        auto b     = Bank.toUpperCase();
        auto index = (int)( *b.getCharPointer() - 'A' );
        if( 0 <= index && index < 8 )
        {
            bank = Bank;
            signals::bankChanged( index, bank );
        }
    }

    ////////////////////////////
    //// Sequence recording (REC / OVERDUB + PLAY), see mt_seq_record.h and the declaration in mt_audio_manager.h
    ////////////////////////////
    seqrec::Control MTAudioManager::getSeqRecControl() const
    {
        const juce::ScopedLock sl( seqRecLock );
        return seqRec;
    }

    void MTAudioManager::seqRecSyncState( MTSequence* S )
    {
        if( !S )
            return;
        const auto c = getSeqRecControl();
        S->setRecordState( seqrec::Phase::Recording == c.phase ? RecordState::Recording : ( seqrec::Phase::Armed == c.phase ? RecordState::Armed : RecordState::NotArmed ) );
    }

    void MTAudioManager::seqRecPressed()
    {
        seqrec::Control before, after;
        int             idx;
        bool            punchIn = false;
        {
            const juce::ScopedLock sl( seqRecLock );
            before = seqRec;
            if( seqrec::Phase::Off == before.phase )
                seqRecSeq = getSelectedSequence(); // REC arms the sequence that is selected now

            if( isPlaying() )
            {
                if( seqrec::Phase::Recording == before.phase )
                {
                    // Punch-out: stop recording, continue playing
                    seqRec = seqrec::Control{};
                }
                else
                {
                    // Punch-in: start recording immediately without erasing existing notes
                    seqRec.phase = seqrec::Phase::Recording;
                    seqRec.overdub = true;
                    punchIn = true;
                }
            }
            else
            {
                seqRec = seqrec::onRecPressed( before );
            }
            after  = seqRec;
            idx    = seqRecSeq;
        }
        auto* S = getSequence( idx );
        if( punchIn && S )
        {
            seqRecBeginTake( S, false );
        }
        else if( seqrec::Phase::Recording == before.phase && seqrec::Phase::Off == after.phase )
        {
            seqRecEnd( S ); // punch out: playback goes on
        }
        seqRecSyncState( S );
    }

    void MTAudioManager::seqOverdubPressed()
    {
        int  idx;
        bool punchIn = false;
        {
            const juce::ScopedLock sl( seqRecLock );
            if( seqrec::Phase::Off == seqRec.phase )
                seqRecSeq = getSelectedSequence();

            if( isPlaying() )
            {
                if( seqrec::Phase::Recording == seqRec.phase && seqRec.overdub )
                {
                    seqRec = seqrec::Control{};
                }
                else
                {
                    seqRec.phase = seqrec::Phase::Recording;
                    seqRec.overdub = true;
                    punchIn = true;
                }
            }
            else
            {
                seqRec = seqrec::onOverdubPressed( seqRec );
            }
            idx = seqRecSeq;
        }
        auto* S = getSequence( idx );
        if( punchIn && S )
        {
            seqRecBeginTake( S, false );
        }
        seqRecSyncState( S );
    }

    bool MTAudioManager::getAudioLoad( double& Cpu, int& XRuns ) const
    {
        if( !dm )
            return false;
        auto* device = dm->getJuceAudioDeviceMgr().getCurrentAudioDevice();
        if( device == nullptr || !device->isPlaying() )
            return false;
        Cpu    = dm->getJuceAudioDeviceMgr().getCpuUsage();
        XRuns  = device->getXRunCount();
        return true;
    }

    void MTAudioManager::seqRecBeforePlay( int Seq )
    {
        // The Looper's sync record also calls setSequencePlay(Now) (LoopRecorder::record, after setLooperRecord): that play
        // must not start a sequence take (it would erase the armed tracks' notes while the user records the Looper).
        // The overdub flag alone (it used to outlive the recording) must not block sequence takes: busy = writing now.
        const bool looperBusy = getLooper()->isRecording();
        bool       clear      = false;
        {
            const juce::ScopedLock sl( seqRecLock );
            if( seqrec::Phase::Armed != seqRec.phase || Seq != seqRecSeq )
                return;
            const seqrec::Control next = seqrec::onPlayStartedUnlessLooper( seqRec, looperBusy, clear );
            if( next.phase == seqRec.phase )
                return; // Looper recording: stay Armed, no take
            seqRec          = next;
            seqRecIdlePolls = 0;
        }
        // Count-in: the metronome setting only offers "Off" (kick_modeHeader.cpp), so a take starts at once.
        seqRecBeginTake( getSequence( Seq ), clear );
    }

    // Start of a take: one undo transaction, a plain REC take erases the notes of the armed tracks, then their capture starts.
    // deleteAllNotes() takes the track's reader lock first (the tick thread walks the notes under it), so it is safe while playing.
    void MTAudioManager::seqRecBeginTake( MTSequence* S, bool Clear )
    {
        if( !S )
            return;
        undoManager.beginNewTransaction( "Record" );

        // Ensure the active/selected track is armed so playing into it records
        if( auto* selTrack = getTrack( getSelectedSequence(), getSelectedTrack() ) )
            selTrack->setRecArmed( true );
        else if( S->getNumTracks() > 0 )
            if( auto* t0 = S->getTrack( 0 ) )
                t0->setRecArmed( true );

        S->forAllTracks( [&]( MTTrack* T ) {
            if( !T->isRecArmed() )
                return;
            if( Clear )
                T->deleteAllNotes();
            T->recBegin();
        } );
        seqRecSyncState( S );
    }

    void MTAudioManager::seqRecFlush( MTSequence* S, bool CloseOpen )
    {
        if( !S )
            return;
        S->forAllTracks( [&]( MTTrack* T ) {
            if( !T->isRecCapturing() )
                return;
            if( !T->recHasPending() )
            {
                if( CloseOpen )
                    T->recStop();
                return;
            }
            std::set<int> before;
            const bool    tc = static_cast<bool>( recordedNotesHook );
            if( tc )
                for( auto vt : T->getNoteListVT() )
                    if( vt.hasType( IDs::NOTE ) )
                        before.insert( (int)vt.getProperty( IDs::id ) );
            const int committed = T->recFlush( CloseOpen, seqRecScratch );
            if( committed > 0 && tc )
            {
                std::set<int> added;
                for( auto vt : T->getNoteListVT() )
                    if( vt.hasType( IDs::NOTE ) && !before.count( (int)vt.getProperty( IDs::id ) ) )
                        added.insert( (int)vt.getProperty( IDs::id ) );
                recordedNotesHook( T, added ); // global timing correct, only the new notes
            }
        } );
    }

    void MTAudioManager::seqRecEnd( MTSequence* S )
    {
        seqRecFlush( S, true );
        if( S )
            S->forAllTracks( [&]( MTTrack* T ) { T->recStop(); } );
        {
            const juce::ScopedLock sl( seqRecLock );
            seqRec = seqrec::onTransportStopped( seqRec );
        }
        seqRecSyncState( S );
    }

    // 25 Hz, message thread. While a take runs: commit the finished notes; when the transport is no longer playing
    // (STOP, pause, end of a non-looping sequence) for two polls in a row, end the take. The two-poll debounce covers the
    // gap between seqRecBeforePlay() and the transport state becoming Playing.
    void MTAudioManager::pollSequenceRecording()
    {
        seqrec::Control c;
        int             idx;
        {
            const juce::ScopedLock sl( seqRecLock );
            c   = seqRec;
            idx = seqRecSeq;
        }
        if( seqrec::Phase::Recording != c.phase )
            return;
        auto* S = getSequence( idx );
        if( !S )
        {
            const juce::ScopedLock sl( seqRecLock );
            seqRec = seqrec::Control{};
            return;
        }
        if( isPlaying() )
        {
            seqRecIdlePolls = 0;
            seqRecFlush( S, false );
        }
        else if( ++seqRecIdlePolls >= 2 )
        {
            seqRecIdlePolls = 0;
            seqRecEnd( S );
        }
    }

    ////////////////////////////
    //// Bank
    ////////////////////////////
    void MTAudioManager::interTimerCallbackHandler()
    {
        MTAudioTransportManager::updatePlayheads();
        MTAudioSequenceManager::processMidiMeters();
        MTAudioProgramManager::processMidiMeters();
        applyAutomatedPans();
        applyAutomatedFx();
        pollSequenceRecording();

        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onPlayHeadChanged( MTAudioSequenceManager::getPlayhead().bnb ); } );
        listeners.call( [&]( MTAudioInterfaceListener& l ) { l.onTrackMetersUpdated(); } );
    }

    //////////////////////////
    //// Song Utilities
    /////////////////////////

    void MTAudioManager::playSong( const juce::String& SongName, int SequenceIndexToPlay )
    {
        auto songInfo = getSongInfo( SongName );
        // A step whose sequence was deleted (seqNo < 0) cannot be played: setupSong would dereference a missing sequence.
        for( auto& step : songInfo.steps )
            if( step.seqNo < 0 )
                return;
        if( songInfo.steps.size() && SequenceIndexToPlay >= 0 && SequenceIndexToPlay < (int)songInfo.steps.size() )
            setupSong( songInfo, SequenceIndexToPlay ); // Setting up the queue
    }

    void MTAudioManager::resumeSong()
    {
        resume( getSelectedSequence() );
    }

    void MTAudioManager::stopSong()
    {
        stop();
    }
}
