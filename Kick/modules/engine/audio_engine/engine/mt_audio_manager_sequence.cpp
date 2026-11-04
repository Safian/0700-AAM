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

    MTAudioSequenceManager::MTAudioSequenceManager( MTSelected& Selected, MTAudioManager& Manager )
        : selected( Selected ), manager( Manager )
    {
        // Reserve default storage size.
        sequences.reserve( 64 );

        // Connect callbacks...
        sequenceNameChanged.connect( [&]( MTSequence* ) { updateSequenceList(); } );
    }

    MTAudioSequenceManager::~MTAudioSequenceManager()
    {
        sequences.clear();
    }

    void MTAudioSequenceManager::close()
    {
        vTree       = juce::ValueTree{ IDs::SEQUENCES };
        undoManager = nullptr;

        pSelectedSequence = nullptr;
        sequences.clear();

        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );
        updateSequenceList();
        if( trackListChanged ) trackListChanged( -1 );
    }

    void MTAudioSequenceManager::setup( juce::ValueTree VT, juce::UndoManager* UM )
    {
        vTree       = VT;
        undoManager = UM;

        // Initialize Sequences....
        for( auto it : vTree )
        {
            if( it.hasType( IDs::SEQUENCE ) )
            {
                createSequence( it );
            }
        }
        std::this_thread::sleep_for( std::chrono::milliseconds( 100 ) );

        updateSequenceList();

        // Initialize Tracks....
        // Nothing to do here, as engine initializes this as part of sequences....
        if( trackListChanged ) trackListChanged( -1 );
    }

    ////////////////////////////
    //// Selection ...
    ////////////////////////////
    void MTAudioSequenceManager::setSelectedSequence( int num )
    {
        auto selection = AudIO::utils::limit<int>( -1, (int)sequences.size() - 1, num );
        auto prev      = selected.sequence;

        if( prev != selection )
        {
            auto isValid = juce::isPositiveAndBelow( selection, sequences.size() );

            pSelectedSequence = isValid ? sequences[selection].get() : nullptr;
            selected.sequence = isValid ? selection : -1;
            sseq              = pSelectedSequence ? pSelectedSequence->getName() : "--";

            if( pSelectedSequence )
                setSelectedTrack( pSelectedSequence->getSelectedTrack() );

            if( seqSelectionChanged )
                seqSelectionChanged();
        }
    }

    void MTAudioSequenceManager::setSelectedSequence( const juce::String& Sequence )
    {
        setSelectedSequence( getSequenceIndexByName( Sequence ) );
    }

    void MTAudioSequenceManager::setSelectedTrack( int num )
    {
        auto prev = selected.track;
        strack    = "--";

        if( auto s = getSequence( selected.sequence ) )
        {
            s->setSelectedTrack( num );
            selected.track = s->getSelectedTrack();
            if( auto trk = s->getTrack( selected.track ) )
                strack = trk->getName();
        }
        else
        {
            selected.track = num;
        }

        if( prev != selected.track )
        {
            if( trackSelectionChanged )
                trackSelectionChanged();
        }
    }

    void MTAudioSequenceManager::setSelectedTrack( const juce::String& Track )
    {
        if( auto s = getSequence( selected.sequence ) )
        {
            setSelectedTrack( s->getTrackIndex( Track ) );
        }
    }

    void MTAudioSequenceManager::setSelectedSequenceAndTrack( int Sequence, int Track )
    {
        setSelectedSequence( Sequence );
        setSelectedTrack( Track );
    }

    void MTAudioSequenceManager::setSelectedSequenceAndTrack( const juce::String& Sequence, const juce::String& Track )
    {
        setSelectedSequence( Sequence );
        setSelectedTrack( Track );
    }
    ////////////////////////////
    //// Timing ...
    ////////////////////////////

    int MTAudioSequenceManager::setTimeSignature( int Sequence, int Numerator, int Denominator )
    {
        auto s = getSequence( Sequence );
        if( !s )
            return -1;
        if( !s->isUsingMasterBPM() )
            return s->setOwnTimeSignature( Numerator, Denominator );

        // Global: every sequence that follows the global tempo changes its bar grid with it.
        if( !timesig::isValid( Numerator, Denominator ) )
            return -1;
        if( manager.getNumTempoSettings() != 1 )
            return -2;

        if( undoManager ) undoManager->beginNewTransaction( "Time Signature" );
        std::vector<std::pair<MTSequence*, MTSequence::TimeGrid>> before;
        forAllSequences( [&]( MTSequence* S ) {
            if( S->isUsingMasterBPM() ) before.emplace_back( S, S->captureTimeGrid() );
        } );
        auto tempo = manager.getTempoState( 0 ); // raw tree writes: one undo transaction (see MTSequence::remapTimeGrid)
        tempo.setProperty( IDs::num, Numerator, undoManager );
        tempo.setProperty( IDs::den, Denominator, undoManager );
        for( auto& it : before )
        {
            it.first->mirrorGlobalTimeSignature( undoManager ); // the own tempo map follows (seek / loop / track ticks read it)
            it.first->remapTimeGrid( it.second );
        }
        kick_log( "info", "[INFO] [SEQ] Global time signature set to %d/%d, %d sequence(s) re-mapped\n", Numerator, Denominator, (int)before.size() );
        return 0;
    }

    int MTAudioSequenceManager::setSequenceLength( const int Sequence, const int Bars )
    {
        if( auto s = getSequence( Sequence ) )
        {
            s->setLength( Bars );
            return 0;
        }
        return -1;
    }

    int MTAudioSequenceManager::setSequenceLength( const int Sequence, const float Multiplier )
    {
        if( auto s = getSequence( Sequence ) )
        {
            s->setLength( Multiplier );
            return 0;
        }
        return -1;
    }

    ////////////////////////////
    //// Sequence
    ////////////////////////////

    int MTAudioSequenceManager::createSequence( const juce::String& Name )
    {
        if( getSequenceByName( Name ) || ( sequences.size() >= MAX_SEQUENCE ) )
            return -1;

        if( undoManager ) undoManager->beginNewTransaction( "Create sequence" ); // one undo step per create / delete
        juce::ValueTree vt = createVTree( IDs::SEQUENCE, IDs::name, Name );
        vTree.appendChild( vt, undoManager );

        return createSequence( vt );
    }

    static unsigned int gSeqNo = 0;

    std::unique_ptr<MTSequence> MTAudioSequenceManager::makeSequence( juce::ValueTree& VT )
    {
        if( !VT.isValid() ) VT = juce::ValueTree( IDs::SEQUENCE );

        juce::String name = VT.getProperty( IDs::name );
        if( name.isEmpty() )
        {
            name = "Sequence" + juce::String( gSeqNo + 1 );
            VT.setProperty( IDs::name, name, nullptr );
        }

        auto seqPtr = std::make_unique<MTSequence>( manager, VT, undoManager, globalPlayhead );
        ++gSeqNo;
        // Register callbacks...
        seqPtr->onTempoTimeSignatureChangeFn = [&]( float BPM, int Bars, int Num, int Den ) {
            if( seqTimeSignatureChanged )
                seqTimeSignatureChanged( BPM, Bars, Num, Den );
        };
        return seqPtr;
    }

    void MTAudioSequenceManager::reconcileWithTree()
    {
        std::vector<juce::ValueTree> nodes;
        for( auto it : vTree )
            if( it.hasType( IDs::SEQUENCE ) )
                nodes.push_back( it );

        const auto prevSelected = pSelectedSequence;
        const auto prevIndex    = selected.sequence;

        const bool changed = treesync::reconcile(
            sequences, nodes,
            []( MTSequence& S, const juce::ValueTree& N ) { return S.getVT() == N; },
            [&]( const juce::ValueTree& N ) -> std::unique_ptr<MTSequence> {
                juce::ValueTree vt = N;
                return makeSequence( vt );
            },
            [&]( MTSequence& S ) {
                // Never destroy a sequence that is still playing.
                if( manager.getPlayingSequence() == &S )
                    manager.stop();
                if( pSelectedSequence == &S )
                    pSelectedSequence = nullptr;
            } );

        // The tracks of the sequences that stayed (new sequences built theirs from the tree).
        bool tracksChanged = false;
        for( auto& s : sequences )
            tracksChanged = s->reconcileTracksWithTree() || tracksChanged;

        if( !changed && !tracksChanged )
            return;

        if( changed )
        {
            // Selection: keep the selected sequence if it survived, else the nearest index.
            int sel = -1;
            for( int i = 0; i < (int)sequences.size(); ++i )
                if( sequences[(size_t)i].get() == prevSelected ) sel = i;
            if( sel < 0 && !sequences.empty() )
                sel = juce::jlimit<int>( 0, (int)sequences.size() - 1, prevIndex );
            pSelectedSequence = sel >= 0 ? sequences[(size_t)sel].get() : nullptr;
            selected.sequence = sel;
            sseq              = pSelectedSequence ? pSelectedSequence->getName() : "--";
            updateSequenceList();
        }

        const int trackSel = pSelectedSequence ? pSelectedSequence->getSelectedTrack() : -1;
        selected.track     = trackSel;
        strack             = "--";
        if( pSelectedSequence )
            if( auto trk = pSelectedSequence->getTrack( trackSel ) )
                strack = trk->getName();

        if( changed && seqSelectionChanged )
            seqSelectionChanged();
        if( trackSelectionChanged )
            trackSelectionChanged();
        if( trackListChanged )
            trackListChanged( selected.sequence );
    }

    int MTAudioSequenceManager::createSequence( juce::ValueTree& VT )
    {
        if( auto seqPtr = makeSequence( VT ) )
        {
            auto pSeq = seqPtr.get();
            pSeq->mirrorGlobalTimeSignature( nullptr ); // a new / loaded sequence that follows the global tempo has the global grid
            sequences.push_back( std::move( seqPtr ) );

            if( 1 == sequences.size() )
                setSelectedSequence( 0 );

            // Update sequence lists...
            updateSequenceList();
            return (int)gSeqNo;
        }
        return -1;
    }

    int MTAudioSequenceManager::deleteSequence( int Index )
    {
        if( juce::isPositiveAndBelow( Index, sequences.size() ) )
        {
            if( undoManager ) undoManager->beginNewTransaction( "Delete sequence" );
            auto& ptr = sequences[(size_t)Index];

            // Stop audio playback, if current sequence is playing...
            if( pSelectedSequence == ptr.get() )
            {
                manager.stop();
                pSelectedSequence = nullptr;
            }

            vTree.removeChild( ptr->getVT(), undoManager );
            sequences.erase( sequences.begin() + Index );

            setSelectedSequence( getSelectedSequence() );
            if( 0 == sequences.size() )
            {
                selected.sequence = -1;
                pSelectedSequence = nullptr;
            }

            // Update sequence list and call listeners...
            if( trackListChanged ) trackListChanged( selected.sequence );
            updateSequenceList();
            return 0;
        }
        return -1;
    }

    int MTAudioSequenceManager::deleteSequence( const juce::String& Name )
    {
        return deleteSequence( getSequenceIndexByName( Name ) );
    }
    int MTAudioSequenceManager::copySequence( int Index )
    {
        if( auto s = getSequence( Index ) )
        {
            if( undoManager ) undoManager->beginNewTransaction( "Copy sequence" );
            auto vt = s->getState().createCopy();
            vt.removeProperty( IDs::id, nullptr );
            vt.setProperty( IDs::name, juce::String( s->getName() + "_copy" ), nullptr );
            vTree.appendChild( vt, undoManager );
            createSequence( vt );
        }
        return 0;
    }

    int MTAudioSequenceManager::clearSequence( int Index )
    {
        if( auto s = getSequence( Index ) )
        {
            if( undoManager ) undoManager->beginNewTransaction( "Clear sequence" );
            s->deleteAllTracks();
        }
        return 0;
    }

    void MTAudioSequenceManager::forAllSequences( std::function<void( MTSequence* S )> func )
    {
        if( func )
        {
            for( const auto& it : sequences )
            {
                func( it.get() );
            }
        }
    }

    MTSequence* MTAudioSequenceManager::getSequence( int index )
    {
        if( juce::isPositiveAndBelow( index, sequences.size() ) )
            return (MTSequence*)sequences[(size_t)index].get();
        return nullptr;
    }

    MTSequence* MTAudioSequenceManager::getSequenceByName( const juce::String& Name )
    {
        for( auto& it : sequences )
            if( it->getName().equalsIgnoreCase( Name ) )
                return it.get();
        return nullptr;
    }

    MTSequence* MTAudioSequenceManager::getSequenceByID( const juce::String& ID )
    {
        for( auto& it : sequences )
            if( it->getID().equalsIgnoreCase( ID ) )
                return it.get();
        return nullptr;
    }

    int MTAudioSequenceManager::getSequenceIndex( const MTSequence* pSeq )
    {
        int index = 0;
        for( auto& it : sequences )
        {
            if( it.get() == pSeq )
                return index;
            ++index;
        }
        return -1;
    }

    int MTAudioSequenceManager::getSequenceIndexByName( const juce::String& Name )
    {
        int index = 0;
        for( auto& it : sequences )
        {
            if( Name.equalsIgnoreCase( it->getName() ) )
                return index;
            ++index;
        }
        return -1;
    }

    juce::ValueTree& MTAudioSequenceManager::getSequenceState( int Sequence )
    {
        if( juce::isPositiveAndBelow( Sequence, sequences.size() ) )
            return sequences[(size_t)Sequence]->getState();
        return (juce::ValueTree&)EMPTY::VTState;
    }

    juce::ValueTree& MTAudioSequenceManager::getSequenceState( const juce::String& Name )
    {
        return getSequenceState( getSequenceIndexByName( Name ) );
    }

    juce::ValueTree MTAudioSequenceManager::getSequenceTempoState( int Sequence, int TempoIndex )
    {
        if( juce::isPositiveAndBelow( Sequence, sequences.size() ) )
            return sequences[(size_t)Sequence]->getTempoState( TempoIndex );
        return EMPTY::VTState;
    }

    int MTAudioSequenceManager::getSequenceDataList( std::vector<MTISequenceData*>& List )
    {
        List.clear();
        forAllSequences( [&]( MTSequence* S ) {
            List.push_back( dynamic_cast<MTISequenceData*>( S ) );
        } );
        return 0;
    }

    void MTAudioSequenceManager::updateSequenceList() noexcept
    {
        sequenceList.clear();

        // Update sequence name list...
        forAllSequences( [&]( MTSequence* S ) {
            S->setIndex( (int)sequenceList.size() );
            sequenceList.push_back( S->getName() );
        } );

        // Trigger sequence list updated...
        if( seqListChanged ) seqListChanged();
    }

    ////////////////////////////
    //// Track
    ////////////////////////////

    int MTAudioSequenceManager::createTrack( int Sequence, const juce::String& Name )
    {
        if( auto seq = getSequence( Sequence ) )
        {
            auto trackNum = seq->createTrack( Name );
            if( -1 != trackNum )
            {
                if( 1 == seq->getNumTracks() )
                    setSelectedTrack( 0 );

                if( trackListChanged ) trackListChanged( Sequence );
                return trackNum;
            }
        }
        return -1;
    }

    int MTAudioSequenceManager::deleteTrack( int Sequence, int Index )
    {
        if( auto seq = getSequence( Sequence ) )
        {
            if( 0 == seq->deleteTrack( Index ) )
            {
                setSelectedTrack( getSelectedTrack() );
                if( trackListChanged ) trackListChanged( Sequence );
                return 0;
            }
        }
        return -1;
    }

    int MTAudioSequenceManager::deleteTrack( int Sequence, const juce::String& Name )
    {
        if( auto seq = getSequence( Sequence ) )
        {
            if( 0 == seq->deleteTrack( Name ) )
            {
                setSelectedTrack( getSelectedTrack() );
                if( trackListChanged ) trackListChanged( Sequence );
                return 0;
            }
        }
        return -1;
    }
    MTTrack* MTAudioSequenceManager::getTrack()
    {
        return this->getTrack( selected );
    }

    MTTrack* MTAudioSequenceManager::getTrack( const MTSelected& Selection )
    {
        return this->getTrack( Selection.sequence, Selection.track );
    }

    MTTrack* MTAudioSequenceManager::getTrack( int SequenceIndex, int TrackIndex )
    {
        if( auto s = getSequence( SequenceIndex ) )
            return (MTTrack*)s->getTrack( TrackIndex );
        return nullptr;
    }

    MTTrack* MTAudioSequenceManager::getTrack( int SequenceIndex, const juce::String& TrackName )
    {
        if( auto s = getSequence( SequenceIndex ) )
            return (MTTrack*)s->getTrack( TrackName );
        return nullptr;
    }

    juce::ValueTree& MTAudioSequenceManager::getTrackState( int Sequence, int Track )
    {
        if( auto t = getTrack( Sequence, Track ) )
            return t->getState();
        return (juce::ValueTree&)EMPTY::VTState;
    }

    juce::ValueTree& MTAudioSequenceManager::getTrackState( int Sequence, const juce::String& Name )
    {
        if( auto t = getTrack( Sequence, Name ) )
            return t->getState();
        return (juce::ValueTree&)EMPTY::VTState;
    }

    void MTAudioSequenceManager::forAllTracksInSequence( int Sequence, std::function<void( MTTrack* T )> func )
    {
        if( auto seq = getSequence( Sequence ) )
        {
            seq->forAllTracks( func );
        }
    }

    void MTAudioSequenceManager::processMidiMeters()
    {
        forAllTracksInSequence( getSelectedSequence(), []( MTTrack* T ) {
            T->processMidiMeter();
        } );
    }

    int MTAudioSequenceManager::copyTrack( int Sequence, const juce::String from, const juce::String to )
    {
        juce::Identifier excludeName( "name" ), excludeId( "id" ), pgm( "program" );

        auto fromState = getTrackState( Sequence, from );
        auto toState   = getTrackState( Sequence, to );
        auto fromNotes = fromState.getChildWithName( IDs::NOTELIST );
        auto toNotes   = toState.getChildWithName( IDs::NOTELIST );
        auto track     = getTrack( Sequence, to );

        for( int id = 0; id < fromState.getNumProperties(); ++id )
        {
            auto propertyName = fromState.getPropertyName( id );
            if( propertyName != excludeName && propertyName != excludeId )
                toState.setProperty( propertyName, fromState.getProperty( propertyName ), nullptr );
            juce::String pgmName = fromState.getProperty( pgm );
            auto         p       = getAudioInterface().getProgram( getAudioInterface().getProgramIndexByID( pgmName ) );
            track->setProgram( p );
        }
        toNotes.removeAllChildren( nullptr );
        for( const auto& child : fromNotes )
        {
            toNotes.appendChild( child.createCopy(), nullptr );
        }
        return 0;
    }

}
