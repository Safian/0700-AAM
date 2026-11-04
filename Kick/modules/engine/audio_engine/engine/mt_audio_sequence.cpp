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
    static int gSeqCtr = 0;

    MTSequence::MTSequence( MTAudioManager& M, juce::ValueTree& VT, juce::UndoManager* UM, MTPlayhead& GlobalPlayhead )
        : MTISequenceData( VT, UM )
        , manager( M )
        , globalPlayhead{ GlobalPlayhead }
        , undoManager( UM )
        , vTree( VT )
    {
        tracks.reserve( 64 );

        // Unique ID
        if( 0 >= vTree.getProperty( IDs::id ).toString().length() )
            vTree.setProperty( IDs::id, CommonUtils::createID(), nullptr );

        // Add listener to the value tree.
        vTree.addListener( this );

        // Setup defaults, if they don't exist.
        if( !vTree.hasProperty( IDs::name ) )
            setName( "Sequence " + juce::String( ++gSeqCtr ) );

        // Setup tempo manager.
        MTTempoMgr::setup( vTree.getOrCreateChildWithName( IDs::TEMPOS, nullptr ), UM );

        // Locate midi file for this sequence...
        {
            auto projDir = manager.getProjDir();
            auto seqName = getName();
            auto n       = seqName + juce::File().getSeparatorString() + seqName + ".mid";
            seqMidiFile  = projDir.getChildFile( n );
            if( !seqMidiFile.exists() )
            {
                seqMidiFile.create();
            }
            auto stream = juce::FileInputStream( seqMidiFile );
            mFile.readFrom( stream );
        }

        auto tracksVT = vTree.getChildWithName( IDs::TRACKS );
        auto targetTree = tracksVT.isValid() ? tracksVT : vTree;
        for( auto it : targetTree )
        {
            if( it.hasType( IDs::TRACK ) )
            {
                createTrack( it );
                std::this_thread::sleep_for( std::chrono::milliseconds( 50 ) );
            }
        }

        trackNameChanged.connect( [&]( MTTrack* T ) { updateTrackList(); } );
        updateTrackList();

        getBPM();
        lengthChangeHandler();
        initializeTickDataFromTempoSettings( getSequenceTempoSettings( 0 ) );
        initialized = true;
    }

    MTSequence::~MTSequence()
    {
        vTree.removeListener( this );
        stop();

        tracks.clear();

        // Write MIDI data...
        {
            mFile.setTicksPerQuarterNote( defaultPPQN );
            auto stream = juce::FileOutputStream( seqMidiFile );
            mFile.writeTo( stream );
        }
    }

    const int MTSequence::getBPM()
    {
        auto t = isUsingMasterBPM() ? (int)manager.getBPM() : (int)MTTempoMgr::getBPM();
        if( t != (int)tempo.get() )
            tempo = t;
        return t;
    }

    //int MTSequence::getIndex()
    //{
    //    auto parent = vTree.getParent();
    //    return parent.indexOf( vTree );
    //}

    MTTimeBase MTSequence::getTimeBase()
    {
        return isMasterBPM.get() ? manager.getTimeBase() : MTTempoMgr::getTimeBase();
    }

    int MTSequence::setName( const juce::String& SeqName )
    {
        // Also, rename MIDI file ...
        auto newMidiFileName = SeqName + ".mid";
        seqMidiFile.moveFileTo( seqMidiFile.getSiblingFile( newMidiFileName ) );
        seqMidiFile = manager.getProjDir().getChildFile( newMidiFileName );

        name = SeqName;
        sequenceNameChanged( this );
        return 0;
    }

    int MTSequence::setBPM( int Bpm )
    {
        if( isMasterBPM.get() )
            manager.setBPM( Bpm );
        else
            MTTempoMgr::setBPM( Bpm );
        return getBPM();
    }

    int MTSequence::setLength( int Bars )
    {
        auto bars       = juce::jlimit<int>( BAR::MIN, BAR::MAX, Bars );
        auto prevLength = length.get();
        length          = bars;

        // Update loopStart accordingly
        if( !loopStart.isUsingDefault() && bars <= loopStart.get() )
            loopStart = ( bars - 1 );

        // Update loopEnd accordingly...
        if( bars < loopEnd.get() )
            loopEnd = bars;
        else if( ( loopEnd.get() == bars - 1 ) && ( loopEnd.get() + 2 != prevLength ) )
            loopEnd = bars;

        return getLength();
    }

    int MTSequence::setLength( float Multiplier )
    {
        return setLength( (int)( getLength() * Multiplier ) );
    }

    int MTSequence::setLoopStart( int Start )
    {
        auto start = juce::jlimit<int>( ( BAR::MIN - 1 ), ( getLoopEnd() - 1 ), Start );
        loopStart  = start;
        return getLoopStart();
    }

    int MTSequence::setLoopEnd( int End )
    {
        auto end = juce::jlimit<int>( getLoopStart(), getLength(), End );
        loopEnd  = end;
        return getLoopEnd();
    }

    int MTSequence::setLoopPoints( int Start, int End )
    {
        setLoopStart( Start );
        return setLoopEnd( End );
    }

    int MTSequence::setTranspose( int Tranpose )
    {
        transpose = Tranpose;
        return getTranspose();
    }

    int MTSequence::useMasterBPM( bool UseMasterBPM )
    {
        isMasterBPM = UseMasterBPM;
        return getBPM();
    }

    int MTSequence::setLooping( bool IsLooping )
    {
        isLooping = IsLooping;
        return isLooping.get();
    }

    int MTSequence::modifySequenceLength( int ToModifyBars, bool Insert, int AtPosition )
    {
        auto& tempoSettings = getTempoSettingsAtBar( ( 0 > AtPosition ? getLength() : AtPosition ) );
        auto  timeToEdit    = ToModifyBars * tempoSettings.getTicksPerBar();
        auto  atTime        = getTicksUpto( ( 0 > AtPosition ? getLength() : AtPosition ) );

        auto lenToModify = Insert ? ToModifyBars : -ToModifyBars;
        for( const auto& it : tracks )
        {
            it->modifyLength( timeToEdit, Insert, atTime );
        }
        setLength( getLength() + lenToModify );
        return -1;
    }

    unsigned int MTSequence::getLengthInTicks() { return getTicksUpto( getLength() ); }

    const double MTSequence::getLengthInSeconds()
    {
        return isUsingMasterBPM() ? manager.getSecondsUpto( getLength() ) : MTTempoMgr::getSecondsUpto( getLength() );
    }

    const double MTSequence::getLengthInSeconds( const MTBarNBeats& Time )
    {
        return isUsingMasterBPM() ? manager.getSecondsUpto( Time.bar, Time.beat, Time.tick ) : getSecondsUpto( Time.bar, Time.beat, Time.tick );
    }

    const double MTSequence::getLengthInMS() { return 1000.0 * getLengthInSeconds(); }

    void MTSequence::lengthChangeHandler()
    {
        lengthInTicks = getLengthInTicks();
        lengthFactor  = ( 0 >= lengthInTicks ) ? 1.0 : ( 1.0 / lengthInTicks ); // Avoid divide by zero
    }

    void MTSequence::valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property )
    {
        if( vTree == treeWhosePropertyHasChanged )
        {
            if( ( property == IDs::isMasterBPM ) || ( property == IDs::bpm ) || ( property == IDs::den ) || ( property == IDs::num ) )
            {
                setupMidiClock( sequencePlayhead.bnb.bar );
                getBPM();
            }
            if( ( property == IDs::length ) )
            {
                lengthChangeHandler();
            }
        }
    }

    void MTSequence::valueTreeChildAdded( juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenAdded )
    {
        if( parentTree == vTree && childWhichHasBeenAdded.hasType( IDs::TRACK ) )
        {
            auto id = childWhichHasBeenAdded.getProperty( IDs::id ).toString();
            for( auto& t : tracks )
            {
                if( t->getId() == id )
                    return;
            }
            createTrack( childWhichHasBeenAdded );
        }
    }

    void MTSequence::valueTreeChildRemoved( juce::ValueTree& parentTree, juce::ValueTree& childWhichHasBeenRemoved, int /*index*/ )
    {
        if( parentTree == vTree && childWhichHasBeenRemoved.hasType( IDs::TRACK ) )
        {
            auto id = childWhichHasBeenRemoved.getProperty( IDs::id ).toString();
            const juce::ScopedLock sl( lock );
            for( size_t i = 0; i < tracks.size(); ++i )
            {
                if( tracks[i]->getId() == id )
                {
                    tracks.erase( tracks.begin() + static_cast<std::ptrdiff_t>( i ) );
                    updateTrackList();
                    break;
                }
            }
        }
    }

    ///
    /// Track API's.
    ///
    int MTSequence::createTrack( const juce::String& TrkName )
    {
        if( getTrack( TrkName ) || ( tracks.size() >= MAX_TRACKS ) )
            return -1;

        juce::ValueTree vt = createVTree( IDs::TRACK,
                                          IDs::name, TrkName );
        vTree.appendChild( vt, undoManager );
        return createTrack( vt );
    }

    int MTSequence::createTrack( juce::ValueTree& VT )
    {
        static unsigned int _trkNo = 0;

        if( !VT.isValid() )
            VT = juce::ValueTree( IDs::TRACK );

        juce::String nameStr = VT.getProperty( IDs::name );
        if( nameStr.isEmpty() )
        {
            nameStr = "Track " + juce::String( _trkNo + 1 );
            VT.setProperty( IDs::name, nameStr, nullptr );
        }

        if( auto& t = tracks.emplace_back( std::make_unique<MTTrack>( manager, *this, undoManager, VT ) ) )
        {
            ++_trkNo;
            updateTrackList();
            return ( (int)tracks.size() - 1 );
        }
        return -1;
    }

    int MTSequence::deleteTrack( int Index )
    {
        if( juce::isPositiveAndBelow( Index, tracks.size() ) )
        {
            const juce::ScopedLock sl( lock );
            auto tVT = tracks[Index]->getVT();
            tracks.erase( tracks.begin() + Index );
            vTree.removeChild( tVT, undoManager );
            updateTrackList();
            return 0;
        }
        return -1;
    }

    int MTSequence::deleteTrack( const juce::String& TrkName )
    {
        return deleteTrack( getTrackIndex( TrkName ) );
    }

    void MTSequence::deleteAllTracks()
    {
        while( 0 < getNumTracks() )
            deleteTrack( 0 );
    }

    int MTSequence::getTrackIndex( const juce::String& TrkName )
    {
        int idx = 0;
        for( auto& itr : tracks )
        {
            if( itr->getName() == TrkName )
            {
                return idx;
            }
            ++idx;
        }
        return -1;
    }

    int MTSequence::getTrackIndex( const MTTrack* pTrack )
    {
        int idx = 0;
        for( auto& itr : tracks )
        {
            if( itr.get() == pTrack )
            {
                return idx;
            }
            ++idx;
        }
        return -1;
    }

    MTTrack* MTSequence::getTrack( int Index )
    {
        if( juce::isPositiveAndBelow( Index, tracks.size() ) )
            return tracks[(size_t)Index].get();
        return nullptr;
    }

    MTTrack* MTSequence::getTrack( const juce::String& TrkName )
    {
        for( auto& itr : tracks )
            if( itr->name.get() == TrkName )
                return itr.get();

        return nullptr;
    }

    juce::ValueTree& MTSequence::getTrackState( int Index )
    {
        if( juce::isPositiveAndBelow( Index, tracks.size() ) )
            return tracks[(size_t)Index]->getState();

        return (juce::ValueTree&)EMPTY::VTState;
    }

    juce::ValueTree& MTSequence::getTrackState( const juce::String& TrkName )
    {
        return getTrackState( getTrackIndex( TrkName ) );
    }

    int MTSequence::setTrackSolo( const juce::String& TrkName, bool Solo )
    {
        auto pTrack = getTrack( TrkName );
        if( pTrack )
        {
            pTrack->solo = Solo;
            updateTrackSoloList();
        }

        return pTrack ? 0 : -1;
    }

    int MTSequence::setTrackSolo( int Index, bool Solo )
    {
        auto pTrack = getTrack( Index );
        if( pTrack )
        {
            pTrack->solo = Solo;
            updateTrackSoloList();
        }

        return pTrack ? 0 : -1;
    }

    void MTSequence::updateTrackSolo( MTTrack* PTrk )
    {
        if( PTrk )
        {
            auto idx = PTrk->getIndex();
            soloingTracks.setBit( idx, PTrk->isSolo() );
        }
    }

    void MTSequence::updateTrackSoloList()
    {
        // Update solo mute list.
        for( const auto& it : tracks )
        {
            updateTrackSolo( it.get() );
        }
        // Update solo mute state in track.
        for( const auto& it : tracks )
        {
            auto idx = it->getIndex();
            it->setSoloMute( isTrackMutedDueToSolo( idx ) );
        }
    }

    int MTSequence::getNumSoloTracks()
    {
        auto count = 0;
        for( auto index = 0; index < getNumTracks(); index++ )
            if( soloingTracks[index] )
                count++;
        return count;
    }

    int MTSequence::resetTrackSolos()
    {
        if( getNumSoloTracks() != 0 )
        {
            soloingTracks.clear();
            return 0;
        }
        return -1;
    }

    int MTSequence::getNumTracks()
    {
        return (int)tracks.size();
    }

    int MTSequence::copyTrackBars( int trackIndex, const SequenceCopyBars& copyBars, std::vector<MTNote>& noteList )
    {
        auto timeToEdit    = getTicksPerBar() * ( copyBars.startBar - ( copyBars.atPositionBar ) );
        auto newStartValue = getTicksUpto( copyBars.atPositionBar - 1 );
        auto newEndValue   = getTicksUpto( copyBars.atPositionBar - 1 + copyBars.numBars );
        if( getNumTracks() <= trackIndex )
            createTrack( "" );
        auto it = getTrack( trackIndex );
        it->modifyBars( noteList, copyBars.noteRange.startTime, copyBars.noteRange.endTime, copyBars.numBars, timeToEdit, newStartValue, newEndValue, copyBars.isMerge );
        if( copyBars.atPositionBar > getLength() )
            setLength( copyBars.atPositionBar - 1 + copyBars.numBars );
        return 1;
    }

    int MTSequence::forAllTracks( std::function<void( MTTrack* T )> func )
    {
        if( func )
        {
            for( const auto& it : tracks )
            {
                func( it.get() );
            }
        }
        return 0;
    }

    int MTSequence::forAllMidiTracks( std::function<void( MTTrack* T )> func )
    {
        if( func )
        {
            for( const auto& it : tracks )
            {
                auto pPrg = it->getProgram();
                if( pPrg )
                {
                    if( pPrg->getType() != MTProgramType::Audio )
                        func( it.get() );
                }
                else
                    func( it.get() );
            }
        }
        return 0;
    }

    void MTSequence::updateTrackList()
    {
        midiIndexes.clear();
        trackList.clear();
        numTracks = getNumTracks();

        forAllTracks( [&]( MTTrack* T ) {
            auto index = static_cast<int>( trackList.size() );
            T->setIndex( index );
            trackList.push_back( T->getName() );
            auto pPrg = T->getProgram();
            if( pPrg )
            {
                if( pPrg->getType() != MTProgramType::Audio )
                    midiIndexes.addIfNotAlreadyThere( index );
            }
            else
                midiIndexes.addIfNotAlreadyThere( index );
        } );
        updateTrackSoloList();
    }

    const juce::Array<int>& MTSequence::getMidiTrackIndexes()
    {
        midiIndexes.clear();
        forAllMidiTracks( [&]( MTTrack* T ) {
            midiIndexes.addIfNotAlreadyThere( T->getIndex() );
        } );
        return midiIndexes;
    }

    const int MTSequence::getNumMidiTracks()
    {
        auto numMidiTracks{ 0 };

        forAllMidiTracks( [&]( MTTrack* T ) {
            ++numMidiTracks;
        } );
        return numMidiTracks;
    }

    const juce::Array<juce::String> MTSequence::getValidRoutableTrackList( int Track )
    {
        juce::Array<juce::String> validRoutableTrack;
        if( juce::isPositiveAndBelow( Track, numTracks ) )
        {
            validRoutableTrack.addIfNotAlreadyThere( "None" );
            auto sourceTrack = getTrack( Track )->getName();
            forAllMidiTracks( [&]( MTTrack* T ) {
                if( T )
                {
                    if( T->getName() != sourceTrack )
                    {
                        if( T->route.get() != "None" )
                        {
                            bool isCircular{ false };
                            isCircularRouted( sourceTrack, T, isCircular );
                            if( !isCircular ) validRoutableTrack.addIfNotAlreadyThere( T->name.get() );
                        }
                        else
                            validRoutableTrack.addIfNotAlreadyThere( T->getName() );
                    }
                }
            } );
        }
        return validRoutableTrack;
    }

    void MTSequence::isCircularRouted( const juce::String& SourceTrack, const MTTrack* DestTrack, bool& IsCircular )
    {
        if( DestTrack )
        {
            if( DestTrack->route.get() != "None" )
            {
                if( DestTrack->route.get() == SourceTrack )
                {
                    IsCircular = true;
                    return;
                }
                else
                    isCircularRouted( SourceTrack, getTrack( DestTrack->route.get() ), IsCircular );
            }
            else
                IsCircular = false;
        }
    }

    ///
    /// Position control
    ///
    void MTSequence::seekSeqenceInternal( int posInTicks )
    {
        tickPos = ( (unsigned int)posInTicks % lengthInTicks );
        beatPos = ( (unsigned int)posInTicks % ticksPerBeat );
        barPos  = ( (unsigned int)posInTicks % ticksPerBar );

        pos1_2  = ( (unsigned int)posInTicks % ticksPer1_2 );
        pos1_4  = ( (unsigned int)posInTicks % ticksPer1_4 );
        pos1_8  = ( (unsigned int)posInTicks % ticksPer1_8 );
        pos1_16 = ( (unsigned int)posInTicks % ticksPer1_16 );

        // Update BnB
        sequencePlayhead.bnb      = toBnB( tickPos );
        sequencePlayhead.normized = ( lengthFactor * (double)tickPos );
        //DBG( "tPos " << juce::String( tickPos ) << ", beatPos " << juce::String( beatPos ) << ", barPos " << juce::String( barPos ) << ", Norm " << juce::String( sequencePlayhead.normized ) );
    }

    void MTSequence::seek( int posInTicks )
    {
        seekSeqenceInternal( posInTicks );

        for( const auto& track : tracks )
            track->seek( tickPos );
    }

    void MTSequence::seek( const MTBarNBeats& BnB )
    {
        auto pos = getTicksUpto( BnB.bar, BnB.beat, BnB.tick );
        seek( pos );
    }

    void MTSequence::setBBT( int knob, int value )
    {
        auto bbt = sequencePlayhead.bnb;
        switch( knob )
        {
            case 0: bbt.bar += ( value ); break;
            case 1: bbt.beat += ( value ); break;
            case 2: bbt.tick += ( value ); break;
        }
        seek( bbt );
    }

    ///
    /// Bar seek (audible). Engine bars are 0-based, the UI shows bar + 1.
    ///
    int MTSequence::seekToBar( int Bar0 )
    {
        const auto bar = bar_seek::clampBar( Bar0, getLength() );

        // Always record the target: while playing, the tick-processing thread picks it up in onTick(); while stopped or
        // paused, play()/resume() consume it. No lock and no work on the caller's thread in the playing case.
        seekTargetTick.store( -1 );
        seekTargetBar.store( bar );

        if( !isPlaying )
        {
            // No tick thread activity: show the parked position right away (UI polls the playhead).
            const juce::ScopedLock sl( lock );
            showBar( bar );
        }
        return bar;
    }

    int MTSequence::seekBars( int DeltaBars )
    {
        // A request that has not been applied yet is the reference, so quick repeated presses accumulate.
        auto current = seekTargetBar.load();
        if( 0 > current && 0 <= seekTargetTick.load() )
        {
            const juce::ScopedLock sl( lock );
            current = (int)toBnB( (unsigned int)seekTargetTick.load() ).bar; // bar a pending tick seek falls in
        }
        if( 0 > current )
            current = ( isPlaying || isPaused.load() ) ? (int)sequencePlayhead.bnb.bar : 0; // stopped: play() starts at bar 0
        return seekToBar( bar_seek::relativeBar( current, DeltaBars, getLength() ) );
    }

    int MTSequence::seekToTick( int Tick )
    {
        const auto tick = event_seek::clampTick( Tick, (long long)getLengthInTicks() );

        // Same hand-over as seekToBar(): newest request wins, applied by onTick() / play() / resume().
        seekTargetBar.store( -1 );
        seekTargetTick.store( tick );

        if( !isPlaying )
        {
            const juce::ScopedLock sl( lock );
            showTick( (int)toBnB( (unsigned int)tick ).bar, tick );
        }
        return tick;
    }

    int MTSequence::getSeekTick()
    {
        const auto tick = seekTargetTick.load();
        if( 0 <= tick )
            return tick;

        const auto bar = seekTargetBar.load();
        if( 0 <= bar )
        {
            const juce::ScopedLock sl( lock );
            return (int)getTicksUpto( (unsigned int)bar_seek::clampBar( bar, getLength() ) );
        }
        return ( isPlaying || isPaused.load() ) ? (int)tickPos.load() : 0;
    }

    int MTSequence::getLastBar()
    {
        return bar_seek::lastBar( isLooping.get(), loopEnd.get(), getLength() );
    }

    // Counters/playhead of the sequence itself for the start of Bar0 (no tracks, no clock shift).
    void MTSequence::showBar( int Bar0 )
    {
        const auto bar = (unsigned int)bar_seek::clampBar( Bar0, getLength() );

        // Tick data first: seekSeqenceInternal() and the bar/beat counters depend on the bar's time signature.
        initializeTickDataFromTempoSettings( getSequenceTempoSettings( bar ) );
        seekSeqenceInternal( (int)getTicksUpto( bar ) );

        // A bar start is aligned to every grid below it, whatever the tempo map before it looks like.
        barPos  = 0;
        beatPos = 0;
        pos1_2  = 0;
        pos1_4  = 0;
        pos1_8  = 0;
        pos1_16 = 0;
    }

    // Like showBar() for a tick inside Bar0: the beat/bar/grid counters keep the phase of the tick (modulo the
    // bar's own time signature, the same approximation seek() uses).
    void MTSequence::showTick( int Bar0, int Tick )
    {
        const auto bar = (unsigned int)bar_seek::clampBar( Bar0, getLength() );
        initializeTickDataFromTempoSettings( getSequenceTempoSettings( bar ) );
        seekSeqenceInternal( Tick );
    }

    // Moves the heard position to the start of Bar0. RawTick is the clock tick the shift is anchored to: after this
    // call, shifted(RawTick) == start tick of the bar.
    //  - onTick() (tick thread): RawTick is the tick being processed.
    //  - play(): RawTick 0 (the clock restarts at 0), resume(): lastRawTick (the clock continues from there).
    // Lock order: sequence lock, then track locks (same as onTick/onBar).
    void MTSequence::applySeek( int Bar0, unsigned int RawTick )
    {
        const juce::ScopedLock sl( lock );
        const auto             bar = bar_seek::clampBar( Bar0, getLength() );
        applySeekAt( getTicksUpto( (unsigned int)bar ), bar, RawTick );
    }

    // Tick variant of applySeek(): the bar is the one the tick falls in.
    void MTSequence::applySeekTick( int Tick, unsigned int RawTick )
    {
        const juce::ScopedLock sl( lock );
        const auto             tick = event_seek::clampTick( Tick, (long long)getLengthInTicks() );
        applySeekAt( (unsigned int)tick, bar_seek::clampBar( (int)toBnB( (unsigned int)tick ).bar, getLength() ), RawTick );
    }

    // Common part. The sequence lock is held by the caller (recursive CriticalSection).
    void MTSequence::applySeekAt( unsigned int Target, int Bar0, unsigned int RawTick )
    {
        const juce::ScopedLock sl( lock );

        tickShift.store( bar_seek::shiftFor( Target, RawTick ) );
        prevTicks.store( Target ); // onTick() computes diff = shifted - prevTicks: 0 for the tick that applied the seek

        if( Target == getTicksUpto( (unsigned int)Bar0 ) )
            showBar( Bar0 );
        else
            showTick( Bar0, (int)Target );

        // Flushes hanging notes and resyncs prevTick/tickCtr/tickPos/noteIdx/playhead of every track.
        for( const auto& it : tracks )
            it->seekToTick( Target );

        setupMidiClock( (unsigned int)Bar0 ); // no-op unless playing
    }

    void MTSequence::applySeekRequest( int Tick, int Bar0, unsigned int RawTick )
    {
        if( 0 <= Tick )
            applySeekTick( Tick, RawTick );
        else if( 0 <= Bar0 )
            applySeek( Bar0, RawTick );
    }

    ///
    /// Playback control.
    ///
    void MTSequence::clearCounters()
    {
        beatPos   = 0;
        barPos    = 0;
        tickPos   = 0;
        prevTicks = 0;

        // Update local.
        sequencePlayhead.clear();
    }

    void MTSequence::prepareToPlay()
    {
    }

    void MTSequence::play()
    {
        // A bar parked by seekToBar() while stopped. "Play from beginning" discards it before calling play().
        const auto parkedBar  = seekTargetBar.exchange( -1 );
        const auto parkedTick = seekTargetTick.exchange( -1 );

        // The clock restarts at tick 0 (start( true ) below): forget the previous run's tick mapping.
        tickShift.store( 0 );
        prevTicks.store( 0 );
        isPaused = false;

        // Setup starting tick.
        sequencePlayhead.bnb.bar = isLooping.get() ? juce::jmax<int>( 0, loopStart.get() - 1 ) : 0;

        // Initialize
        initializeTickDataFromTempoSettings( getSequenceTempoSettings( sequencePlayhead.bnb.bar ) );

        // Seek to play position.
        seek( sequencePlayhead.bnb );

        // Set all tracks to play.
        for( const auto& it : tracks )
            it->play();

        // Start at the parked bar (after the tracks were reset above); the clock origin is tick 0.
        applySeekRequest( parkedTick, parkedBar, 0 );

        // Emit signals...
        onStart( tickPos );
        if( isLooping.get() ) onLoopBegin( 0 );

        // Start Midi clock...
        isPlaying = true;
        setupMidiClock( sequencePlayhead.bnb.bar );
        getMidiClock().start( true );
    }

    void MTSequence::resume()
    {
        isPaused = false;

        // A bar parked while paused: the clock was stopped, not reset, so it continues from lastRawTick.
        const auto parkedBar  = seekTargetBar.exchange( -1 );
        const auto parkedTick = seekTargetTick.exchange( -1 );
        applySeekRequest( parkedTick, parkedBar, lastRawTick.load() );

        for( const auto& it : tracks )
            it->resume();

        // Start Midi clock...
        isPlaying = true;
        setupMidiClock( sequencePlayhead.bnb.bar );
        getMidiClock().start( false );
    }

    void MTSequence::pause()
    {
        isPlaying = false;
        isPaused  = true;
        getMidiClock().stop();

        for( const auto& it : tracks )
            it->pause();
    }

    void MTSequence::stop()
    {
        isPlaying = false;
        isPaused  = false;
        seekTargetTick.store( -1 );
        seekTargetBar.store( -1 ); // stopped means "back to the start": a parked bar is only valid until stop
        getMidiClock().stop();

        for( const auto& it : tracks )
            it->stop();
    }

    void MTSequence::onStart( unsigned int Ticks )
    {
    }

    void MTSequence::onLoopBegin( unsigned int BeginTick )
    {
        signals::onLoopBegin( sequencePlayhead.loopCount );
    }

    void MTSequence::onLoopEnd( unsigned int EndTick, unsigned int LoopCount )
    {
        signals::onLoopEnd( LoopCount );
    }

    void MTSequence::onTick( unsigned int RawTicks )
    {
        //double start = juce::Time::getMillisecondCounterHiRes();

        // Apply a bar seek requested from another thread. Done here, on the tick-processing thread, so that nothing
        // else touches the tick bookkeeping (prevTicks, track prevTick/tickCtr/queued notes) at the same time.
        lastRawTick.store( RawTicks );
        if( isPlaying && ( 0 <= seekTargetBar.load() || 0 <= seekTargetTick.load() ) )
        {
            const auto tick = seekTargetTick.exchange( -1 );
            const auto bar  = seekTargetBar.exchange( -1 );
            applySeekRequest( tick, bar, RawTicks );
        }

        // Everything below works on the sequence's own (shifted) timeline.
        const unsigned int Ticks = bar_seek::shifted( RawTicks, tickShift.load() );

        auto diff = Ticks - prevTicks;
        prevTicks.store( Ticks );

        tickPos += diff;
        beatPos += diff;
        barPos += diff;

        pos1_2 += diff;
        pos1_4 += diff;
        pos1_8 += diff;
        pos1_16 += diff;

        sequencePlayhead.bnb.tick += diff;

        if( barPos > ticksPerBar )
        {
            barPos -= ticksPerBar; // Clear bar counter
            beatPos = 0; // Also clear beat counter
            onBar();
        }

        if( beatPos > ticksPerBeat )
        {
            beatPos -= ticksPerBeat; // Clear beat counter
            onBeat();
        }

        // Update playhead
        sequencePlayhead.normized = ( lengthFactor * (double)tickPos );

        {
            const juce::ScopedLock sl( lock );
            for( const auto& it : tracks )
                it->onTick( Ticks );
        }

        if( pos1_2 > ticksPer1_2 )
        {
            pos1_2 -= ticksPer1_2;
            signals::on1_2();
        }
        if( pos1_4 > ticksPer1_4 )
        {
            pos1_4 -= ticksPer1_4;
            signals::on1_4();
        }
        if( pos1_8 > ticksPer1_8 )
        {
            pos1_8 -= ticksPer1_8;
            signals::on1_8();
        }
        if( pos1_16 > ticksPer1_16 )
        {
            pos1_16 -= ticksPer1_16;
            signals::on1_16();
        }
        ///To Emit or Not...
        //signals::onTick( playhead.bnb.tick );

        //juce::Logger::writeToLog( "Sequence execution Time, " + juce::String( juce::Time::getMillisecondCounterHiRes() - start ) );
    }

    void MTSequence::onBeat()
    {
        sequencePlayhead.bnb.tick = 0;
        ++sequencePlayhead.bnb.beat;

        const juce::ScopedLock sl( lock );
        for( const auto& it : tracks )
            it->onBeat();

        signals::onBeat( sequencePlayhead.bnb.beat );
    }

    void MTSequence::onBar()
    {
        sequencePlayhead.bnb.beat = 0;
        sequencePlayhead.bnb.tick = 0;
        ++sequencePlayhead.bnb.bar;

        const juce::ScopedLock sl( lock );
        for( const auto& it : tracks )
            it->onBar();

        auto lEnd = juce::jmax<int>( 0, loopEnd.get() );
        if( isLooping.get() && ( sequencePlayhead.bnb.bar >= lEnd ) )
        {
            ++sequencePlayhead.loopCount;

            // Trigger onLoopEnd callback...
            onLoopEnd( tickPos, sequencePlayhead.loopCount );

            // Get sequence starting position.
            auto loopStartBar = isLooping.get() ? juce::jmax<int>( 0, loopStart.get() ) : 0;
            seekSeqenceInternal( getTicksUpto( loopStartBar ) );

            // Trigger onLoopBegin callback...
            onLoopBegin( tickPos );
        }

        setupMidiClock( sequencePlayhead.bnb.bar );
        signals::onBar( sequencePlayhead.bnb.bar );
    }

    ///
    /// MIDI Clock.
    ///
    void MTSequence::setupMidiClock( unsigned int Bar )
    {
        if( !isPlaying )
            return;

        const auto& tempoSettings = getSequenceTempoSettings( Bar );

        auto prevTempo = getMidiClock().getTempo();
        auto newTempo  = tempoSettings.getBPM();
        if( !juce::approximatelyEqual( prevTempo, newTempo ) )
        {
            initializeTickDataFromTempoSettings( tempoSettings );

            /// Setup clock.
            getMidiClock().setTempo( newTempo );

            /// Notify of tempo and time signature change.
            onTempoTimeSignatureChangeFn( newTempo, Bar, tempoSettings.getNumerator(), tempoSettings.getDenominator() );
        }
    }

    void MTSequence::initializeTickDataFromTempoSettings( const MTTempoSettings& tempoSettings )
    {
        auto tPBar  = tempoSettings.getTicksPerBar();
        auto tPBeat = tempoSettings.getTicksPerBeat();
        auto tP1_2  = tempoSettings.getTicksPerTimebase( MTTimeBase::_2 );
        auto tP1_4  = tempoSettings.getTicksPerTimebase( MTTimeBase::_4 );
        auto tP1_8  = tempoSettings.getTicksPerTimebase( MTTimeBase::_8 );
        auto tP1_16 = tempoSettings.getTicksPerTimebase( MTTimeBase::_16 );

        /// Synchronously update all automic.
        ticksPerBeat = tPBeat;
        ticksPerBar  = tPBar;
        ticksPer1_2  = tP1_2;
        ticksPer1_4  = tP1_4;
        ticksPer1_8  = tP1_8;
        ticksPer1_16 = tP1_16;
    }
}
