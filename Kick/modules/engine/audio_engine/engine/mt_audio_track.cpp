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
    static int gTrackCtr = 0;

    MTTrack::MTTrack( MTAudioManager& M, MTSequence& Seq, juce::UndoManager* UM, juce::ValueTree& VT )
        : MTITrackData( VT, UM )
        , MTNoteList( VT, UM, Seq )
        , manager( M )
        , seq( Seq )
        , undoManager( UM )
        , vTree( VT )
    {
        ++gTrackCtr;

        if( 0 >= vTree.getProperty( IDs::id ).toString().length() )
            vTree.setProperty( IDs::id, CommonUtils::createID(), nullptr );

        if( 0 >= vTree.getProperty( IDs::name ).toString().length() )
            vTree.setProperty( IDs::name, ( "Track " + juce::String( gTrackCtr ) ), nullptr );

        vTree.addListener( this );
        MTNoteList::setReaderLock( &lock ); // G2: foreign note-map edits (undo/redo) exclude the tick thread; order lock -> MTNoteList::lock

        // Locate midi file for this track...
        /*
        {
            auto       projDir  = manager.getAssetManager()->getProjDir();
            auto       seqData  = Seq.getData();
            auto       n        = seqData.Name + juce::File().getSeparatorString() + data.Name + ".mid";
            juce::File midiFile = projDir.getChildFile( n );
            if( !midiFile.exists() )
            {
                midiFile.create();
            }
            {
                // Read Midifile...
                juce::MidiFile mFile;
                auto           stream = juce::FileInputStream( midiFile );
                mFile.readFrom( stream );
                if( auto trk = mFile.getTrack( 0 ) )
                    midiSeq = *trk;

                MTNoteList::setMidiList( &midiSeq );
            }
        }
        */

        // Setup handler for program being deleted
        // (scoped: a destroyed track - e.g. after an undo of its creation - must not stay connected to the static signal)
        programDeletedConn = programBeingDeleted.connect( [this]( MTIProgram* MTIProg ) {
            if( program.load() == MTIProg )
            {
                setProgram( nullptr );
            }
        } );

        // Restore a persisted automation lane (written by commitLane).
        reloadLaneFromState();

        // Restore program.
        // Connect handler to length change handler.
        lengthChangeHandler();
        progChangeHandler();
    }

    void MTTrack::setName( const juce::String& Name )
    {
        if( Name.isNotEmpty() )
            MTITrackData::name = Name;
    }

    void MTTrack::setLevel( int Level ) { MTITrackData::level = Level; }
    void MTTrack::setPan( const float Pan ) { MTITrackData::pan = Pan; }
    void MTTrack::setLength( const int Length )
    {
        auto len             = juce::jmin<int>( seq.length.get(), Length );
        MTITrackData::length = len;
    }
    void MTTrack::setUsingSeqLength( bool UseSeqLength ) { MTITrackData::isSeqLength = UseSeqLength; }
    void MTTrack::setMute( bool Muted )
    {
        MTITrackData::mute = Muted;
        if( 0 < muteGroup.get() )
        {
            seq.forAllTracks( [&, Muted]( MTTrack* t ) {
                if( muteGroup == t->muteGroup )
                {
                    if( t->isMute() != Muted )
                        t->setMute( Muted );
                }
            } );
        }
    }

    void MTTrack::setTranspose( int Transpose ) { MTITrackData::transpose = Transpose; }
    void MTTrack::setMidiPort( const juce::String& MidiPort ) { MTITrackData::midiPort.setValue( MidiPort, undoManager ); }
    void MTTrack::setMidiChannel( int MidiChannel ) { MTITrackData::midiChannel = MidiChannel; }
    void MTTrack::setMidiOutChannel( int MidiChannel ) { MTITrackData::midiOutChannel = MidiChannel; }
    void MTTrack::setMuteGroup( int Group ) { MTITrackData::muteGroup = Group; }

    bool MTTrack::isMidiInAllowed( const juce::String& Source, int Channel ) const noexcept
    {
        auto midiInCh   = MTITrackData::midiChannel.get();
        auto midiInPort = MTITrackData::midiPort.get();

        auto isSrcAllowed     = ( 0 == midiInPort.compareIgnoreCase( "all" ) ) || ( 0 == midiInPort.compareIgnoreCase( Source ) );
        auto isChannelAllowed = ( 0 >= midiInCh ) || ( Channel == midiInCh );
        return ( isSrcAllowed && isChannelAllowed );
    }

    MTIProgram* MTTrack::getProgram() const { return program.load(); }

    int MTTrack::setProgram( MTIProgram* p )
    {
        MTITrackData::programId = p ? p->id.get() : "";
        return 0;
    };

    void MTTrack::progChangeHandler()
    {
        auto newProgram = manager.getProgramById( programId.get() );
        if( newProgram != program.load() )
        {
            stopAllMidiNotes();
            program.store( newProgram );
            bindProgramTree( newProgram );
            manager.triggerTrackProgramChanged( getIndex(), program.load() );
        }
    }

    void MTTrack::lengthChangeHandler()
    {
        auto trackLength = getLength();
        lengthInTicks    = ( (unsigned int)( seq.getTicksUpto( trackLength ) ) );
        lengthFactor     = ( 1.0 / lengthInTicks );
    }

    void MTTrack::valueTreePropertyChanged( juce::ValueTree& TreeWhosePropertyHasChanged, const juce::Identifier& Property )
    {
        if( vTree == TreeWhosePropertyHasChanged )
        {
            if( Property == IDs::name )
                trackNameChanged( this );
            else if( Property == IDs::level )
            {
                levelScaler = ( level.get() * MIDI::SCALER );
                laneApplied.store( -1 ); // in Read mode the lane wins again on the next tick
                recordLevelMove();
            }
            else if( Property == IDs::solo )
                seq.updateTrackSolo( this );
            else if( Property == IDs::length || Property == IDs::usingSequenceLength )
                lengthChangeHandler();
            else if( Property == IDs::program )
                progChangeHandler();
        }
        else if( progTree.isValid() && progTree == TreeWhosePropertyHasChanged && Property == IDs::pan )
            recordPanMove(); // program pan moved (mixer knob, Q-Link): Write mode records it
        else if( TreeWhosePropertyHasChanged.hasType( IDs::AUTOLANE ) && ( Property == IDs::autoPoints || Property == IDs::autoProgram || Property == IDs::autoTarget ) )
            reloadLaneFromState(); // undo/redo of a clear, or project load
    }

    ///
    /// Automation (track level lane)
    ///
    void MTTrack::automationModeChanged( int Mode )
    {
        {
            const juce::ScopedLock sl( laneWriteLock );
            laneRec.reset();
            panRec.reset();
            for( auto& f : fxLanes )
                f.rec.reset();
        }
        for( auto& f : fxLanes )
        {
            f.applied.store( automation::kNoValue );
            f.pending.store( automation::kNoValue );
        }
        laneApplied.store( -1 );
        panApplied.store( automation::kNoValue );
        panTarget.store( automation::kNoValue ); // drop a value published before the mode changed
        // Leaving Read: the level falls back to the stored level. The program pan (which Read wrote into the program's
        // pan property) is restored by MTAudioManager::setAutomationMode, because a program can be shared by tracks.
        if( automation::Read != Mode )
            levelScaler = ( MTITrackData::level.get() * MIDI::SCALER );
    }

    bool MTTrack::hasAutomation() const
    {
        const juce::ScopedLock sl( lock );
        if( !laneLevel.empty() || !lanePan.empty() )
            return true;
        for( const auto& f : fxLanes )
            if( !f.lane.empty() )
                return true;
        return false;
    }

    std::vector<automation::Point> MTTrack::getLanePoints( int Param ) const
    {
        const auto node = findLaneNode( Param );
        if( !node.isValid() )
            return {};
        return automation::fromString( node[IDs::autoPoints].toString().toStdString() );
    }

    bool MTTrack::setLanePoint( int Param, int OldTick, int NewTick, int Value )
    {
        if( Param != automation::Level && Param != automation::Pan && !automation::isFxLane( Param ) )
            return false;
        const int          tick   = NewTick < 0 ? 0 : NewTick;
        const int          value  = automation::isFxLane( Param ) ? juce::jlimit( 0, automation::kNormScale, Value )
                                    : Param == automation::Level ? juce::jlimit( 0, 127, Value ) : juce::jlimit( -automation::kPanScale, automation::kPanScale, Value );
        const juce::String progId = MTITrackData::programId.get();
        auto               node   = findLaneNode( Param );
        automation::Lane   lane   = node.isValid() ? automation::fromString( node[IDs::autoPoints].toString().toStdString() ) : automation::Lane();
        if( Param == automation::Pan )
        {
            if( progId.isEmpty() )
                return false;
            if( node.isValid() && !lane.empty() && node[IDs::autoProgram].toString() != progId )
                return false; // the pan lane belongs to another program: it is not edited under this one
        }
        if( OldTick >= 0 )
            automation::eraseRange( lane, OldTick, OldTick );
        if( !automation::setPoint( lane, tick, value ) )
            return false;

        if( !node.isValid() )
        {
            if( ( Param == automation::Pan || automation::isFxLane( Param ) ) && !findLaneNode( automation::Level ).isValid() )
            { // a Level node is always created first (readers that know only the Level lane take the first AUTOLANE child)
                auto lv = juce::ValueTree( IDs::AUTOLANE );
                lv.setProperty( IDs::autoParam, (int)automation::Level, nullptr );
                lv.setProperty( IDs::autoPoints, "", nullptr );
                vTree.addChild( lv, -1, undoManager );
            }
            node = juce::ValueTree( IDs::AUTOLANE );
            node.setProperty( IDs::autoParam, Param, nullptr );
            node.setProperty( IDs::autoPoints, "", nullptr );
            vTree.addChild( node, -1, undoManager );
        }
        if( Param == automation::Pan )
            node.setProperty( IDs::autoProgram, progId, undoManager );
        node.setProperty( IDs::autoPoints, juce::String( automation::toString( lane ) ), undoManager ); // the listener reloads the lane
        return true;
    }

    bool MTTrack::eraseLanePoint( int Param, int Tick )
    {
        auto node = findLaneNode( Param );
        if( !node.isValid() )
            return false;
        auto lane = automation::fromString( node[IDs::autoPoints].toString().toStdString() );
        if( automation::eraseRange( lane, Tick, Tick ) == 0 )
            return false;
        node.setProperty( IDs::autoPoints, juce::String( automation::toString( lane ) ), undoManager );
        return true;
    }

    // The AUTOLANE child of the track holding Param. A node without `autoParam` (saved before the PAN lane existed)
    // is the Level lane.
    juce::ValueTree MTTrack::findLaneNode( int Param ) const
    {
        for( int i = 0; i < vTree.getNumChildren(); ++i )
        {
            auto c = vTree.getChild( i );
            if( c.hasType( IDs::AUTOLANE ) && Param == automation::paramFromStored( c.hasProperty( IDs::autoParam ), (int)c[IDs::autoParam] ) )
                return c;
        }
        return {};
    }

    // One undo step together with the caller's transaction (MTAudioManager::clearAutomation opens it): both lanes are
    // cleared through the UndoManager, the ValueTree listener reloads (empties) the in-memory lanes.
    void MTTrack::clearAutomation( bool Undoable )
    {
        bool levelDone = false, panDone = false;
        bool fxDone[automation::kNumFxLanes] = {};
        juce::String clearedPanProg; // F7: program whose pan lane is emptied here (its pan must go back to the stored value)
        {
            const juce::ScopedLock w( laneWriteLock );
            if( !lanePan.empty() )
                clearedPanProg = lanePanProgId;
        }
        if( Undoable )
        {
            auto ln = findLaneNode( automation::Level );
            if( ln.isValid() && ln[IDs::autoPoints].toString().isNotEmpty() )
            {
                ln.setProperty( IDs::autoPoints, "", undoManager );
                levelDone = true;
            }
            auto pn = findLaneNode( automation::Pan );
            if( pn.isValid() && pn[IDs::autoPoints].toString().isNotEmpty() )
            {
                pn.setProperty( IDs::autoPoints, "", undoManager );
                panDone = true;
            }
            for( int i = 0; i < automation::kNumFxLanes; ++i )
            {
                auto fn = findLaneNode( automation::kFirstFxLane + i );
                if( fn.isValid() && fn[IDs::autoPoints].toString().isNotEmpty() )
                {
                    fn.setProperty( IDs::autoPoints, "", undoManager );
                    fxDone[i] = true;
                }
            }
        }
        {
            automation::Lane       emptyL, emptyP;
            const juce::ScopedLock w( laneWriteLock );
            {
                const juce::ScopedLock sl( lock );
                if( !levelDone )
                {
                    laneLevel.swap( emptyL );
                    levelScaler = ( MTITrackData::level.get() * MIDI::SCALER ); // F7: back to the stored level
                }
                if( !panDone ) lanePan.swap( emptyP );
                for( int i = 0; i < automation::kNumFxLanes; ++i )
                    if( !fxDone[i] )
                    {
                        automation::Lane emptyF;
                        fxLanes[i].lane.swap( emptyF );
                    }
            }
            for( int i = 0; i < automation::kNumFxLanes; ++i )
            {
                if( !fxDone[i] ) fxLanes[i].committed = "";
                fxLanes[i].rec.reset();
            }
            if( !levelDone ) laneCommitted = "";
            if( !panDone ) lanePanCommitted = "";
            laneRec.reset();
            panRec.reset();
            refreshPanActive();
            if( !panDone && clearedPanProg.isNotEmpty() )
            {
                panRestoreId = clearedPanProg; // undoable path: reloadLaneFromState requests it
                panRestorePending.store( true );
            }
        }
        laneApplied.store( -1 );
        panApplied.store( automation::kNoValue );
        for( auto& f : fxLanes )
        {
            f.applied.store( automation::kNoValue );
            f.pending.store( automation::kNoValue );
        }
    }

    juce::String MTTrack::takePanRestore()
    {
        if( !panRestorePending.exchange( false ) )
            return {};
        const juce::ScopedLock w( laneWriteLock );
        return panRestoreId;
    }

    bool MTTrack::panLaneDrives( const juce::String& Id )
    {
        const juce::ScopedLock w( laneWriteLock );
        return !lanePan.empty() && lanePanProgId == Id;
    }

    // Applies the lane values for the (post-wrap) track position. Tick thread, called with `lock` held. No allocation;
    // only acts when the interpolated value changed. Level goes to levelScaler; Pan is handed to the message thread.
    void MTTrack::applyLaneAt( unsigned int Pos )
    {
        if( automation::Read != manager.getAutomationMode() )
            return;
        if( !laneLevel.empty() )
        {
            const int v = juce::jlimit( 0, 127, (int)automation::valueAt( laneLevel, (int32_t)Pos, -1 ) );
            if( v != laneApplied.load( std::memory_order_relaxed ) )
            {
                laneApplied.store( v, std::memory_order_relaxed );
                levelScaler.store( v * MIDI::SCALER, std::memory_order_relaxed );
            }
        }
        if( !lanePan.empty() && lanePanActive.load( std::memory_order_relaxed ) )
        {
            const int v = juce::jlimit( -automation::kPanScale, automation::kPanScale, (int)automation::valueAt( lanePan, (int32_t)Pos, 0 ) );
            if( v != panApplied.load( std::memory_order_relaxed ) )
            {
                panApplied.store( v, std::memory_order_relaxed );
                panTarget.store( v, std::memory_order_relaxed ); // message thread applies it (MTAudioManager::applyAutomatedPans)
            }
        }
        for( auto& f : fxLanes )
        {
            if( f.lane.empty() )
                continue;
            const int v = juce::jlimit( 0, automation::kNormScale, (int)automation::valueAt( f.lane, (int32_t)Pos, 0 ) );
            if( v != f.applied.load( std::memory_order_relaxed ) )
            {
                f.applied.store( v, std::memory_order_relaxed );
                f.pending.store( v, std::memory_order_relaxed ); // MTAudioManager::applyAutomatedFx (message thread) applies it
            }
        }
    }

    // Called (message or MIDI-input thread) when the track level property changed. Records a breakpoint at the
    // current track position in Write mode while this track plays. All level changes funnel through the ValueTree
    // property (mixer slider, Q-Link via setTrackLevel, track view), so this is the single recording hook.
    void MTTrack::recordLevelMove()
    {
        if( automation::Write != manager.getAutomationMode() )
            return;
        const juce::ScopedLock w( laneWriteLock );
        auto                   p    = program.load();
        const int              tick = livePlayTick.load( std::memory_order_relaxed );
        if( !isPlaying.load() || tick < 0 || !p || p->isAudio() )
        {
            laneRec.reset(); // not playing: the next move starts a new touch
            return;
        }
        automation::Lane next = laneLevel; // allocation on the writer thread only; the tick thread never sees a partial lane
        if( automation::RecResult::Recorded != automation::recordMove( next, laneRec, tick, juce::jlimit( 0, 127, (int)vTree.getProperty( IDs::level ) ) ) )
            return;
        {
            const juce::ScopedLock sl( lock );
            laneLevel.swap( next );
        }
        commitLane( juce::String( automation::toString( laneLevel ) ) ); // laneLevel only changes under laneWriteLock
    }

    // Program pan moved (the played program's tree). Write mode only. When several tracks play the same program only
    // the lowest (sequence, track) playing one records, so exactly one lane owns the take (MTAudioManager::isPanLaneOwner;
    // same ordering that decides playback in applyAutomatedPans).
    void MTTrack::recordPanMove()
    {
        if( automation::Write != manager.getAutomationMode() )
            return;
        auto      p    = program.load();
        const int tick = livePlayTick.load( std::memory_order_relaxed );
        if( !isPlaying.load() || tick < 0 || !p || p->isAudio() || !progTree.isValid() )
        {
            const juce::ScopedLock w( laneWriteLock );
            panRec.reset();
            return;
        }
        if( !manager.isPanLaneOwner( this, p ) )
            return;
        const juce::String progId = p->id.get();
        const int          value  = automation::panToValue( (double)progTree.getProperty( IDs::pan, PAN::DEFAULT ) );

        const juce::ScopedLock w( laneWriteLock );
        const bool             sameProgram = ( lanePanProgId == progId );
        automation::Lane       next        = sameProgram ? lanePan : automation::Lane(); // a new program starts a fresh lane
        if( !sameProgram )
            panRec.reset();
        if( automation::RecResult::Recorded != automation::recordMove( next, panRec, tick, value ) )
            return;
        {
            const juce::ScopedLock sl( lock );
            lanePan.swap( next );
        }
        commitPanLane( juce::String( automation::toString( lanePan ) ), progId );
        refreshPanActive();
    }

    void MTTrack::refreshPanActive()
    {
        lanePanActive.store( lanePanProgId.isNotEmpty() && lanePanProgId == MTITrackData::programId.get(), std::memory_order_relaxed );
    }

    // Follows the played program: its tree is listened to for pan moves (Write recording).
    void MTTrack::bindProgramTree( MTIProgram* P )
    {
        if( progTree.isValid() )
            progTree.removeListener( this );
        progTree = P ? P->vTree : juce::ValueTree();
        if( progTree.isValid() )
            progTree.addListener( this );
        {
            const juce::ScopedLock w( laneWriteLock );
            panRec.reset();
            refreshPanActive();
        }
        panApplied.store( automation::kNoValue );
    }

    // Stores the lane text in the track's ValueTree (not undoable: a take is not an edit step) so it saves/loads
    // with the project. Caller holds laneWriteLock.
    void MTTrack::commitLane( const juce::String& Points )
    {
        if( Points == laneCommitted )
            return;
        laneCommitted = Points;
        auto node     = findLaneNode( automation::Level );
        if( !node.isValid() )
        {
            node = juce::ValueTree( IDs::AUTOLANE );
            vTree.addChild( node, -1, nullptr );
        }
        node.setProperty( IDs::autoParam, (int)automation::Level, nullptr );
        node.setProperty( IDs::autoPoints, Points, nullptr );
    }

    // PAN lane node: a second AUTOLANE child (autoParam = Pan, autoProgram = program id, autoPoints). A Level node is
    // always created first (empty if needed) so that a reader which only knows the Level lane and takes the first
    // AUTOLANE child still sees a harmless Level node. Caller holds laneWriteLock. The committed text and program id
    // are set BEFORE the ValueTree writes so that the listener's reloadLaneFromState sees our own commit and skips.
    void MTTrack::commitPanLane( const juce::String& Points, const juce::String& ProgramId )
    {
        struct Guard // G14: our own ValueTree writes below must not bounce back into reloadLaneFromState (laneWriteLock held)
        {
            explicit Guard( bool& F ) : f( F ) { f = true; }
            ~Guard() { f = false; }
            bool& f;
        } inCommit( inPanCommit );
        lanePanCommitted = Points;
        lanePanProgId    = ProgramId;
        if( !findLaneNode( automation::Level ).isValid() )
        {
            auto lv = juce::ValueTree( IDs::AUTOLANE );
            lv.setProperty( IDs::autoParam, (int)automation::Level, nullptr );
            lv.setProperty( IDs::autoPoints, "", nullptr );
            vTree.addChild( lv, -1, nullptr );
        }
        auto node = findLaneNode( automation::Pan );
        if( !node.isValid() )
        {
            node = juce::ValueTree( IDs::AUTOLANE );
            node.setProperty( IDs::autoParam, (int)automation::Pan, nullptr );
            vTree.addChild( node, -1, nullptr );
        }
        node.setProperty( IDs::autoProgram, ProgramId, nullptr );
        node.setProperty( IDs::autoPoints, Points, nullptr );
    }

    // Reads both lanes from the ValueTree (construction, project load, undo/redo). Skips lanes that are our own commit.
    void MTTrack::reloadLaneFromState()
    {
        const juce::ScopedLock w( laneWriteLock );
        if( inPanCommit ) // G14: the listener fired from our own commitPanLane
            return;
        {
            const auto node   = findLaneNode( automation::Level );
            const auto points = node.isValid() ? node[IDs::autoPoints].toString() : juce::String();
            if( points != laneCommitted )
            {
                laneCommitted = points;
                auto next     = automation::fromString( points.toStdString() );
                {
                    const juce::ScopedLock sl( lock );
                    laneLevel.swap( next );
                    if( laneLevel.empty() ) // F7: no lane: do not stay on the last automated level
                        levelScaler = ( MTITrackData::level.get() * MIDI::SCALER );
                }
                laneRec.reset();
                laneApplied.store( -1 );
            }
        }
        {
            const auto node   = findLaneNode( automation::Pan );
            const auto points = node.isValid() ? node[IDs::autoPoints].toString() : juce::String();
            const auto progId = node.isValid() ? node[IDs::autoProgram].toString() : juce::String();
            if( points != lanePanCommitted || progId != lanePanProgId )
            {
                const juce::String oldProg = lanePanProgId;
                lanePanCommitted = points;
                lanePanProgId    = progId;
                auto next        = automation::fromString( points.toStdString() );
                bool nowEmpty    = false;
                {
                    const juce::ScopedLock sl( lock );
                    lanePan.swap( next );
                    nowEmpty = lanePan.empty();
                }
                if( nowEmpty && oldProg.isNotEmpty() ) // F7: lane gone (undo of a take, clear, empty project): restore the stored pan
                {
                    panRestoreId = oldProg;
                    panRestorePending.store( true );
                }
                panRec.reset();
                panApplied.store( automation::kNoValue );
            }
            refreshPanActive();
        }
        for( int i = 0; i < automation::kNumFxLanes; ++i )
        {
            auto&      f      = fxLanes[i];
            const auto node   = findLaneNode( automation::kFirstFxLane + i );
            const auto points = node.isValid() ? node[IDs::autoPoints].toString() : juce::String();
            const int  target = node.isValid() ? (int)node[IDs::autoTarget] : 0;
            if( points != f.committed || target != f.target )
            {
                f.committed = points;
                f.target    = target;
                auto next   = automation::fromString( points.toStdString() );
                {
                    const juce::ScopedLock sl( lock );
                    f.lane.swap( next );
                }
                f.rec.reset();
                f.applied.store( automation::kNoValue );
            }
        }
    }

    // ---- FX lanes
    bool MTTrack::hasFxLane( int Param ) const
    {
        const int i = automation::fxLaneIndex( Param );
        if( i < 0 )
            return false;
        const juce::ScopedLock sl( lock );
        return !fxLanes[i].lane.empty();
    }

    int MTTrack::getFxLaneTarget( int Param ) const
    {
        const int i = automation::fxLaneIndex( Param );
        if( i < 0 )
            return -1;
        const juce::ScopedLock w( laneWriteLock );
        return fxLanes[i].lane.empty() ? -1 : fxLanes[i].target;
    }

    // Write mode: records Value (0..1000) at the track's play position while it plays; message thread.
    void MTTrack::recordFxValue( int Param, int Value, int Target )
    {
        const int i = automation::fxLaneIndex( Param );
        if( i < 0 || automation::Write != manager.getAutomationMode() )
            return;
        auto&                  f    = fxLanes[i];
        const juce::ScopedLock w( laneWriteLock );
        const int              tick = livePlayTick.load( std::memory_order_relaxed );
        if( !isPlaying.load() || tick < 0 )
        {
            f.rec.reset(); // not playing: the next move starts a new touch
            return;
        }
        const int          t       = automation::isInsertLane( Param ) ? juce::jmax( 0, Target ) : 0;
        automation::Lane   next    = ( t == f.target ) ? f.lane : automation::Lane(); // another parameter: a fresh lane
        if( t != f.target )
            f.rec.reset();
        if( automation::RecResult::Recorded != automation::recordMove( next, f.rec, tick, juce::jlimit( 0, automation::kNormScale, Value ) ) )
            return;
        f.target = t;
        {
            const juce::ScopedLock sl( lock );
            f.lane.swap( next );
        }
        commitFxLane( Param );
    }

    // Stores the lane text (and the insert target) in the track's tree, not undoable (a take is not an edit step). laneWriteLock held.
    void MTTrack::commitFxLane( int Param )
    {
        const int i = automation::fxLaneIndex( Param );
        auto&     f = fxLanes[i];
        f.committed = juce::String( automation::toString( f.lane ) );
        struct Guard // our own tree writes must not bounce back into reloadLaneFromState (laneWriteLock held)
        {
            explicit Guard( bool& F ) : flag( F ) { flag = true; }
            ~Guard() { flag = false; }
            bool& flag;
        } inCommit( inPanCommit );
        if( !findLaneNode( automation::Level ).isValid() )
        {
            auto lv = juce::ValueTree( IDs::AUTOLANE );
            lv.setProperty( IDs::autoParam, (int)automation::Level, nullptr );
            lv.setProperty( IDs::autoPoints, "", nullptr );
            vTree.addChild( lv, -1, nullptr );
        }
        auto node = findLaneNode( Param );
        if( !node.isValid() )
        {
            node = juce::ValueTree( IDs::AUTOLANE );
            node.setProperty( IDs::autoParam, Param, nullptr );
            vTree.addChild( node, -1, nullptr );
        }
        node.setProperty( IDs::autoTarget, f.target, nullptr );
        node.setProperty( IDs::autoPoints, f.committed, nullptr );
    }

    void MTTrack::handleAsyncUpdate()
    {
        manager.triggerTrackNotesChanged( manager.getSequenceIndex( &seq ), getIndex() );

        /*
        auto projDir = manager.getAssetManager()->getProjDir();
        auto n = seq.getName() + juce::File().getSeparatorString() + getName() + ".mid";
        juce::File midiFile = projDir.getChildFile( n );
        if( !midiFile.exists() )
        {
            midiFile.create();
        }
        {
            midiFile.replaceWithData( nullptr, 0 );
            juce::MidiFile mFile;
            mFile.setTicksPerQuarterNote( defaultPPQN );
            mFile.addTrack( midiSeq );
            auto stream = juce::FileOutputStream( midiFile );
            mFile.writeTo( stream );
        }
        */
    }

    void MTTrack::play()
    {
        isPlaying = true;
        // The variables are cleared here and in stop as the workflow may be pause->playstart. if Ui calls stop and resume then it may lead to issues.
        trackPlayhead.clear();
        trackPlayhead.bnb.bar = seq.isLooping.get() ? juce::jmax<int>( 0, seq.getLoopStart() - 1 ) : 0;
        prevTick              = 0;
        tickCtr               = 0;
        tickPos               = 0;
    }

    void MTTrack::resume()
    {
        isPlaying = true;
    }

    void MTTrack::pause()
    {
        isPlaying = false;
        if (auto* p = program.load()) p->updatePluginTransport(seq.getBPM(), double(tickPos) / defaultPPQN, false);
        arpPanic();
        stopAllMidiNotes();
    }

    void MTTrack::stop()
    {
        if (auto* p = program.load()) p->updatePluginTransport(seq.getBPM(), 0, false);
        arpPanic();
        stopAllMidiNotes();
        const bool bendWasMoved = liveBendActive.exchange( false ); // G12: centre only if a live bend was actually moved
        if( auto* p = program.load() ) // F16: all-notes-off no longer centres the pitch bend; only a real stop does
        {
            if( bendWasMoved )
            {
                juce::Array<juce::MidiMessage> centre;
                centre.add( juce::MidiMessage::pitchWheel( getMidiOutChannel(), 8192 ) );
                p->noteProcess( centre ); // forwarded by MIDI/CLAP programs; ignored by the sample programs (not live)
            }
            p->resetPerformance(); // bend centre + sustain release also for muted programs
        }
        isPlaying = false;
        trackPlayhead.clear();
        trackPlayhead.bnb.bar = seq.isLooping.get() ? juce::jmax<int>( 0, seq.getLoopStart() - 1 ) : 0;
        prevTick              = 0;
        tickCtr               = 0;
        tickPos               = 0;
    }

    void MTTrack::seek( int posInTicks )
    {
        tickPos = posInTicks;
        if( seq.isLooping.get() )
            while( tickPos > lengthInTicks.load() ) // This is cheap modulo
                tickPos -= lengthInTicks.load();

        //// restrict the position between loop start and sequence length.
        //tickPos  = juce::jlimit<int>( 0, seq.getTicksUpto( getLength() ), posInTicks );
        //auto bnb = seq.toBnB( tickPos );

        //const juce::ScopedLock sl( lock );

        //trackPlayhead.bnb.tick = bnb.tick;
        //trackPlayhead.bnb.bar  = bnb.bar;
        //trackPlayhead.bnb.beat = bnb.beat;

        recalculateIndex( tickPos );
        updateTrackPlayhead( tickPos );
    }

    void MTTrack::seekToTick( unsigned int TargetTick )
    {
        // Same lock as onTick(); the sequence lock is already held by the caller (lock order: sequence, track).
        const juce::ScopedLock sl( lock );

        // Hanging notes: all-notes-off and drop the queued note-offs, as pause()/stop() do.
        stopAllMidiNotes();
        midiMsgs.clearQuick();

        // fillMidiMessages() plays (prevTick, Tick]; queued note-offs are keyed by absolute (shifted) ticks that
        // are compared with tickCtr, so tickCtr must follow prevTick (== shifted tick).
        prevTick = bar_seek::prevTickFor( TargetTick );
        tickCtr  = prevTick;
        tickPos  = bar_seek::wrapToLength( TargetTick, lengthInTicks.load() );

        trackPlayhead.bnb = seq.toBnB( tickPos );
        recalculateIndex( tickPos );
        updateTrackPlayhead( tickPos );
    }

    void MTTrack::recalculateIndex( unsigned int BeginTick )
    {
        noteIdx = 0;

        // If zero
        if( 0 == BeginTick )
            return;

        // Loop to find correct index...
        const auto& vt = getNoteListVT();
        while( noteIdx < vt.getNumChildren() )
        {
            auto     tmp = vt.getChild( noteIdx );
            MTNoteRO note( tmp );
            if( note.start() < (int)BeginTick )
                noteIdx++;
            else
                break;
        }
    }

    void MTTrack::onBeat()
    {
        trackPlayhead.bnb.tick = 0;
        trackPlayhead.bnb.beat++;
    };

    void MTTrack::onBar()
    {
        trackPlayhead.bnb.beat = 0;
        int last               = seq.isLooping.get() ? juce::jmin<int>( getLength(), seq.getLoopEnd() ) : juce::jmin<int>( getLength(), seq.getLength() );
        if( ++trackPlayhead.bnb.bar % last == 0 )
        {
            trackPlayhead.bnb.bar = trackPlayhead.bnb.bar = seq.isLooping.get() ? juce::jmax<int>( 0, seq.getLoopStart() - 1 ) : 0;
            trackPlayhead.loopCount++;
        }
    }

    void MTTrack::fillMidiMessages( unsigned int tick, unsigned int PrevTick, int MidiChannel )
    {
        //juce::Logger::writeToLog( "prevTick, " + juce::String( PrevTick ) + "PresentTick, " + juce::String(tick));
        juce::HashMap<int, MTNote*>* map = nullptr;
        for( auto pos = PrevTick + 1; pos <= tick; pos++ )
        {
            int posOnTrack = pos % lengthInTicks.load();
            map            = getNotesAtPos( posOnTrack );
            if( !map ) continue;
            for( const auto& it : *map )
            {
                int noteNo = getEffectiveNoteNo( it->number.get() );
                if( !isTrackEffectivelyMuted() && CommonUtils::isValidMidiNote( noteNo ) )
                {
                    // Check probability of playback.
                    // random() includes both ends. Roll 1..MAX so a probability of 0 never plays,
                    // MAX always plays, and any other value plays exactly that many times in MAX.
                    auto rand = AudIO::utils::random<int>( PROBABILITY::MIN + 1, PROBABILITY::MAX );
                    if( rand <= it->probability.get() )
                    {
                        int noteVelocity = getEffectiveNoteVelocity( it->velocity.get() );

                        // Trigger note now.
                        midiMsgs.insert( -1, juce::MidiMessage::noteOn( MidiChannel, noteNo, (juce::uint8)noteVelocity ) );

                        // Check Ratchet
                        auto parts = (int)( it->ratchet.get() );
                        if( 1 < parts )
                        {
                            auto time      = tickCtr;
                            auto partTicks = ( it->length.get() / parts );
                            // Queue Retriggers...
                            for( auto i = 2; i <= parts; ++i )
                            {
                                queuedNotes.insert( { time - 1, juce::MidiMessage::noteOff( MidiChannel, noteNo, (juce::uint8)noteVelocity ) } );
                                time += partTicks;
                                queuedNotes.insert( { time, juce::MidiMessage::noteOn( MidiChannel, noteNo, (juce::uint8)noteVelocity ) } );
                            }
                        }
                        // Queue last note off.
                        queuedNotes.insert( { ( pos + it->length.get() - 1 ), juce::MidiMessage::noteOff( MidiChannel, noteNo, (juce::uint8)noteVelocity ) } ); // Queue note-off
                    }
                }
            }
        }
    }

    void MTTrack::onTick( unsigned int Tick )
    {
        auto pProgram = program.load();
        if( !pProgram || ( 0 == lengthInTicks.load() ) )
            return;

        if( pProgram->isAudio() )
            return;

        unsigned int diffTick = Tick - prevTick;

        tickCtr += diffTick;
        tickPos += diffTick;
        trackPlayhead.bnb.tick += diffTick;
        auto trackMidiOutChannel = getMidiOutChannel();

        const juce::ScopedLock sl( lock );

        // Check if any pending queued note off's are to be sent...
        for( auto p = queuedNotes.begin(); p != queuedNotes.end(); )
        {
            if( p->first <= tickCtr )
            {
                midiMsgs.insert( -1, p->second );
                p = queuedNotes.erase( p );
            }
            else
                ++p;
        }

        // Automation playback (Read mode): lane value for this tick's position, before the notes use levelScaler.
        applyLaneAt( tickPos >= lengthInTicks.load() ? tickPos - lengthInTicks.load() : tickPos );

        fillMidiMessages( Tick, prevTick, trackMidiOutChannel );

        prevTick = Tick;

        if( tickPos >= lengthInTicks.load() )
        {
            tickPos -= lengthInTicks.load();
            noteIdx = 0;
        }

        updateTrackPlayhead( tickPos );
        if( arpOn.load( std::memory_order_relaxed ) ) // arpeggiator step (tick thread); events go out with this tick's messages
        {
            arp::Result ar;
            {
                arp::SpinGuard g( arpFlag );
                arpeg.tick( (int64_t)tickCtr, ar );
            }
            arpEmit( ar, midiMsgs );
        }
        livePlayTick.store( (int)tickPos, std::memory_order_relaxed );
        if( recCapturing.load( std::memory_order_relaxed ) ) // sequence recording: where the take stands (closes held notes)
            recLastTick.store( (int)tickPos, std::memory_order_relaxed );
        // add logic to compute normalized playhead position using lengthInTicks
        pProgram->updatePluginTransport(seq.getBPM(), double(tickPos) / defaultPPQN, true);
        if( !midiMsgs.isEmpty() )
        {
            for( const auto& m : midiMsgs ) // G8: only note-ons move the meter
                if( m.isNoteOn() )
                {
                    reloadMidiMeter( levelScaler );
                    break;
                }
            recordNoteActivity( midiMsgs );
            pProgram->noteProcess( midiMsgs ); // sequence + arp output: not live (decision 7)
            midiMsgs.clearQuick();
        }
    }

    bool MTTrack::isInputMonitored( const juce::MidiMessage& Msg ) const noexcept
    {
        const auto* raw = Msg.getRawData();
        const int   n   = Msg.getRawDataSize();
        return trackmonitor::passes( getMonitor(), recArmed.load( std::memory_order_relaxed ), isPlaying.load(),
                                         n > 0 ? raw[0] : 0, n > 1 ? raw[1] : 0, n > 2 ? raw[2] : 0 );
    }

    int MTTrack::processIncomingMidiMessage( const juce::MidiMessage& Msg, bool Monitored )
    {
        if( Msg.isNoteOnOrOff() ) // RECALL buffer: raw (pre-transpose) note, no allocation, even if the track is muted
        {
            live::Event e;
            e.timeMs   = juce::Time::currentTimeMillis();
            e.tick     = isPlaying.load() ? livePlayTick.load( std::memory_order_relaxed ) : -1;
            e.note     = (uint8_t)Msg.getNoteNumber();
            e.velocity = (uint8_t)( Msg.isNoteOn() ? Msg.getVelocity() : 0 );
            e.channel  = (uint8_t)juce::jlimit( 0, 15, Msg.getChannel() - 1 );
            e.on       = Msg.isNoteOn();
            liveRing.push( e );
            // SEQUENCE RECORDING: an armed track in a running take keeps the raw note at the playhead tick. Lock free and
            // allocation free (own spin flag, O(1)); only while the track plays, so a stopped transport records nothing.
            if( recCapturing.load( std::memory_order_relaxed ) && isPlaying.load() )
            {
                const int t = livePlayTick.load( std::memory_order_relaxed );
                if( t >= 0 )
                {
                    seqrec::Event r;
                    r.tick     = t;
                    r.note     = e.note;
                    r.velocity = e.velocity;
                    r.channel  = e.channel;
                    r.on       = e.on;
                    recCap.push( r );
                }
            }
        }
        if( !Monitored ) // Monitor Off / Auto: captured above, but not handed to the program
            return 0;
        if( Msg.isNoteOnOrOff() && arpFilterLive( Msg ) ) // arpeggiator takes the note; its output is emitted by onTick
            return 0;
        if( Msg.isPitchWheel() ) // G12: remember a moved live bend so stop() centres only then
            liveBendActive.store( Msg.getPitchWheelValue() != 8192, std::memory_order_relaxed );
        auto       pProgram = program.load();
        const bool muted    = isTrackEffectivelyMuted();
        // G5: a muted track starts nothing new but still lets go (note-off, pitch wheel, CC64, CC123)
        const bool pass = !muted || mutefilter::passesWhileMuted( Msg.getRawData()[0], Msg.getRawDataSize() > 1 ? Msg.getRawData()[1] : 0,
                                                                       Msg.getRawDataSize() > 2 ? Msg.getRawData()[2] : 0 );
        if( pProgram && pass )
        {
            juce::Array<juce::MidiMessage> list;

            if( Msg.isNoteOnOrOff() )
            {
                auto effCh = getMidiOutChannel();
                auto effNo = getEffectiveNoteNo( Msg.getNoteNumber() );

                if( Msg.isNoteOn() )
                {
                    auto effVel = getEffectiveNoteVelocity( Msg.getVelocity() );
                    list.insert( -1, juce::MidiMessage::noteOn( effCh, effNo, (juce::uint8)effVel ) );
                }
                else
                    list.insert( -1, juce::MidiMessage::noteOff( effCh, effNo ) );
            }
            else if( Msg.isChannelPressure() ) // keyboard / pad aftertouch follows the track's output channel like the notes
                list.insert( -1, juce::MidiMessage::channelPressureChange( getMidiOutChannel(), Msg.getChannelPressureValue() ) );
            else
                list.insert( -1, Msg );

            recordNoteActivity( list );
            pProgram->noteProcess( list, true ); // live input
        }
        if( !muted && Msg.isNoteOn() ) // G8
            reloadMidiMeter( levelScaler );

        return 0;
    }

    bool MTTrack::arpFilterLive( const juce::MidiMessage& Msg )
    {
        if( !arpOn.load( std::memory_order_relaxed ) )
            return false;
        auto       p      = program.load();
        const bool active = isPlaying.load() && p && !p->isAudio() && 0 != lengthInTicks.load(); // same conditions as onTick stepping
        const int  note   = Msg.getNoteNumber();
        arp::SpinGuard g( arpFlag );
        if( Msg.isNoteOn() )
        {
            if( !active )
                return false; // transport stopped: play the note directly
            arpeg.noteOn( note, Msg.getVelocity() );
            return true;
        }
        const bool ours = arpeg.noteOff( note ); // always keep key state in sync
        return active && ours;
    }

    void MTTrack::arpEmit( const arp::Result& R, juce::Array<juce::MidiMessage>& Out )
    {
        for( int i = 0; i < R.n; ++i )
        {
            const auto& e = R.ev[(size_t)i];
            if( e.on )
            {
                if( isTrackEffectivelyMuted() )
                    continue; // keep stepping while muted, just stay silent
                if( getEffectiveNoteVelocity( e.velocity ) <= 0 )
                    continue; // track level 0 (or automated to 0): silent, like the sequence notes (no jlimit up to 1)
                const int eff = juce::jlimit( 0, 127, getEffectiveNoteNo( e.note ) );
                const int vel = juce::jlimit( 1, 127, getEffectiveNoteVelocity( e.velocity ) );
                Out.insert( -1, juce::MidiMessage::noteOn( getMidiOutChannel(), eff, (juce::uint8)vel ) );
                arpSentNote.store( eff );
            }
            else if( const int n = arpSentNote.exchange( -1 ); n >= 0 )
                Out.insert( -1, juce::MidiMessage::noteOff( getMidiOutChannel(), n ) );
        }
    }

    // Non-tick-thread output (enable/disable/panic): straight to the program, like the other live input paths.
    void MTTrack::arpFlush( const arp::Result& R )
    {
        juce::Array<juce::MidiMessage> list;
        arpEmit( R, list );
        if( auto p = program.load(); p && !list.isEmpty() )
        {
            recordNoteActivity( list );
            p->noteProcess( list, false ); // decision 7: the pedal does not act on arp output
        }
    }

    void MTTrack::arpPanic()
    {
        arp::Result r;
        {
            arp::SpinGuard g( arpFlag );
            arpeg.panic( r );
        }
        arpFlush( r );
    }

    void MTTrack::setArpEnabled( bool On )
    {
        arp::Result r;
        {
            arp::SpinGuard g( arpFlag );
            arpeg.setEnabled( On, r );
            arpOn.store( On );
        }
        arpFlush( r );
    }
    void MTTrack::setArpMode( int Mode ) { arp::SpinGuard g( arpFlag ); arpeg.setMode( Mode ); }
    void MTTrack::setArpRate( int Rate ) { arp::SpinGuard g( arpFlag ); arpeg.setRate( Rate ); }
    void MTTrack::setArpOctaves( int Octaves ) { arp::SpinGuard g( arpFlag ); arpeg.setOctaves( Octaves ); }
    void MTTrack::setArpLatch( bool On ) { arp::SpinGuard g( arpFlag ); arpeg.setLatch( On ); }
    void MTTrack::setArpGate( int Percent ) { arp::SpinGuard g( arpFlag ); arpeg.setGatePercent( Percent ); }
    int  MTTrack::getArpMode() { arp::SpinGuard g( arpFlag ); return arpeg.getMode(); }
    int  MTTrack::getArpRate() { arp::SpinGuard g( arpFlag ); return arpeg.getRate(); }
    int  MTTrack::getArpOctaves() { arp::SpinGuard g( arpFlag ); return arpeg.getOctaves(); }
    bool MTTrack::isArpLatch() { arp::SpinGuard g( arpFlag ); return arpeg.isLatch(); }

    int MTTrack::recallLiveNotes( int WindowMs )
    {
        const int len = (int)lengthInTicks.load();
        if( len <= 0 )
            return 0;
        const double ticksPerMs = double( seq.getBPM() ) * defaultPPQN / 60000.0;
        auto         notes      = live::pairEvents( liveRing.snapshot(), juce::Time::currentTimeMillis(), WindowMs, ticksPerMs, len );
        int          committed  = 0;
        {
            // F2: the tick thread walks the note HashMap under `lock`; the edits below rebuild that map. Lock order:
            // MTTrack::lock, then MTNoteList::lock (see timingCorrectLocked).
            const juce::ScopedLock sl( lock );
            for( const auto& n : notes )
                if( 0 == addOrModifyNote( n.note, n.startTick, n.lengthTicks, n.velocity ) )
                    ++committed;
        }
        if( committed ) liveRing.clear(); // each captured note is committed once
        return committed;
    }

    void MTTrack::recBegin()
    {
        recCap.clear();
        recLastTick.store( 0, std::memory_order_relaxed );
        recCapturing.store( true );
    }

    int MTTrack::recFlush( bool CloseOpen, std::vector<seqrec::Note>& Scratch )
    {
        Scratch.clear();
        if( CloseOpen )
            recCapturing.store( false ); // end of the take: no new events after this (a push in flight is cleared by the next recBegin)
        const int len = (int)lengthInTicks.load();
        if( 0 == recCap.drain( Scratch, len, recLastTick.load( std::memory_order_relaxed ), CloseOpen ) )
            return 0;
        int committed = 0;
        {
            // Same rule as recallLiveNotes: the tick thread walks the note HashMap under `lock`; the edits rebuild it.
            const juce::ScopedLock sl( lock );
            for( const auto& n : Scratch )
                if( 0 == addOrModifyNote( n.note, n.startTick, n.lengthTicks, n.velocity ) )
                    ++committed;
        }
        return committed;
    }

    int MTTrack::timingCorrectLocked( const track::timingCorrectInfo& Info, const std::set<int>* OnlyIds )
    {
        const juce::ScopedLock sl( lock ); // see the lock order in the header
        return MTNoteList::timingCorrect( Info, OnlyIds, (int)lengthInTicks.load() );
    }

    int MTTrack::processIncomingMidiMessage( const MTMidiMessage& Msg )
    {
        if( isMidiInAllowed( Msg.source->getName(), Msg.message.getChannel() ) )
            return processIncomingMidiMessage( Msg.message, isInputMonitored( Msg.message ) );
        return -1;
    }

    /*inline void MTTrack::addMidiNote( int NoteNumber, int Velocity )
    {
        auto msg = juce::MidiMessage::noteOn( getMidiOutChannel(), NoteNumber, (juce::uint8)Velocity );
        processIncomingMidiMessage( msg );
    }

    inline void MTTrack::stopMidiNote( int NoteNumber )
    {
        auto msg = juce::MidiMessage::noteOff( getMidiOutChannel(), NoteNumber );
        processIncomingMidiMessage( msg );
    }*/

    void MTTrack::recordNoteActivity( const juce::Array<juce::MidiMessage>& Msgs ) noexcept
    {
        for( const auto& m : Msgs )
        {
            if( m.isNoteOnOrOff() )
            {
                const int n = m.getNoteNumber();
                if( n < 0 || n > 127 ) continue;
                const uint64_t bit = uint64_t( 1 ) << ( n & 63 );
                auto&          w   = activeNotes[n >> 6];
                if( m.isNoteOn() ) w.fetch_or( bit, std::memory_order_relaxed );
                else w.fetch_and( ~bit, std::memory_order_relaxed );
            }
            else if( m.isAllNotesOff() || m.isAllSoundOff() )
            {
                activeNotes[0].store( 0, std::memory_order_relaxed );
                activeNotes[1].store( 0, std::memory_order_relaxed );
            }
        }
    }

    inline void MTTrack::stopAllMidiNotes()
    {
        auto msg = juce::MidiMessage::allNotesOff( getMidiOutChannel() );
        processIncomingMidiMessage( msg );
        queuedNotes.clear();
    }

#if 0
    struct lessNote
    {
        template<typename T>
        bool operator()( const T& l, const T& r ) const
        {
            return l.second < r.second;
        }
    };

    int MTTrack::explode( const MTSelected& /*selection*/, std::vector<MTNoteData>* /*Notes*/ )
    {
        std::multiset < std::pair<std::shared_ptr<MTNoteData>, int>, lessNote > noteList;
        for( auto& it : notes_.getAllNotes() )
        {
            noteList.insert( std::make_pair( it, it->NoteNumber ) );
        }

        int prevNoteNum = -1;
        int trackIndex = -1;
        for( auto& it : noteList )
        {
            if( it.second != prevNoteNum )
            {
                trackIndex = sequence.createTrack( getName() + "_" + std::to_string( it.second ) );
                sequence.getTracks()[(size_t)trackIndex]->setProgram( getProgram() );
                prevNoteNum = it.second;
            }
            sequence.getTracks()[(size_t)trackIndex]->addNote( *it.first );
        }
        return 0;
    }

    int MTTrack::setLengthInBarAndBeats( int Bar, double Beats )
    {
        return setLength( sequence.tempoSequence.barsBeatsToTime( { Bar, Beats } ) );
    }

    double MTTrack::modifyLength( double Length, double Pos, bool Insert )
    {
        if( 0 <= Length )
        {
            // Update new Length...
            auto currLength = getLength();
            length = (Insert ? (currLength + Length) : (currLength - Length));

            for( const auto& itr : track->getClips() )
            {
                if( itr->getName() == KickMidiClipName )
                {
                    auto startBeat = (Insert ? (Pos) : (Pos - Length));
                    auto delta = (Insert ? (Length) : (-Length));

                    for( auto& it : notes_.getAllNotes() )
                    {
                        if( it->StartBeat >= startBeat )
                            notes_.editStartAndLength( &it, (it->StartBeat + delta), it->LengthInBeats );
                    }
                }
                // Set maximum lengths...
                itr->setLength( length, true );
            }
        }
        return getLength();
    }

#endif
}
