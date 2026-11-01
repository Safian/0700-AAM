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
        programBeingDeleted.connect( [this]( MTIProgram* MTIProg ) {
            if( program.load() == MTIProg )
            {
                setProgram( nullptr );
            }
        } );

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
    void MTTrack::setMidiPort( const juce::String& MidiPort ) { MTITrackData::midiPort.setValue( midiPort, undoManager ); }
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
                levelScaler = ( level.get() * MIDI::SCALER );
            else if( Property == IDs::solo )
                seq.updateTrackSolo( this );
            else if( Property == IDs::length || Property == IDs::usingSequenceLength )
                lengthChangeHandler();
            else if( Property == IDs::program )
                progChangeHandler();
        }
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
        auto isEffectivelyMute   = isTrackEffectivelyMuted();
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
        // add logic to compute normalized playhead position using lengthInTicks
        pProgram->updatePluginTransport(seq.getBPM(), double(tickPos) / defaultPPQN, true);
        if( !midiMsgs.isEmpty() )
        {
            reloadMidiMeter( levelScaler );
            pProgram->noteProcess( midiMsgs );
            midiMsgs.clearQuick();
        }
    }

    int MTTrack::processIncomingMidiMessage( const juce::MidiMessage& Msg )
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
        }
        if( Msg.isNoteOnOrOff() && arpFilterLive( Msg ) ) // arpeggiator takes the note; its output is emitted by onTick
            return 0;
        auto pProgram = program.load();
        if( pProgram && !isTrackEffectivelyMuted() )
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
            else
                list.insert( -1, Msg );

            pProgram->noteProcess( list );
        }
        if( !isTrackEffectivelyMuted() )
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
            p->noteProcess( list );
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
        auto         notes      = live::pairEvents( liveRing.snapshot(), juce::Time::currentTimeMillis(), WindowMs, isPlaying.load(), ticksPerMs, len );
        int          committed  = 0;
        for( const auto& n : notes )
            if( 0 == addOrModifyNote( n.note, n.startTick, n.lengthTicks, n.velocity ) )
                ++committed;
        if( committed ) liveRing.clear(); // each captured note is committed once
        return committed;
    }

    int MTTrack::processIncomingMidiMessage( const MTMidiMessage& Msg )
    {
        if( isMidiInAllowed( Msg.source->getName(), Msg.message.getChannel() ) )
            return processIncomingMidiMessage( Msg.message );
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
