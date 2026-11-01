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

#pragma once

#include "mt_audio_bar_seek.h"
#include "mt_live_capture.h"
#include "mt_arpeggiator.h"
#include "mt_automation.h"

namespace mt
{
    class MTNoteList;
    class MTTrack;

    static sigslot::signal<MTTrack*> trackNameChanged;

    class MTTrack
        : public MTITrackData
        , public MTNoteList
        , public ICMidiClockListener
        , public juce::ValueTree::Listener
    {
    public:
        explicit MTTrack( MTAudioManager& M, MTSequence& Seq, juce::UndoManager* UM, juce::ValueTree& VT );
        ~MTTrack()
        {
            trackPlayhead.clear();
            if( progTree.isValid() )
                progTree.removeListener( this );
        };

        juce::ValueTree& getVT() { return vTree; };
        juce::ValueTree& getState() { return vTree; };

        // Index API's
        void       setIndex( int Index ) { MTITrackData::index = Index; };
        inline int getIndex() const { return MTITrackData::index; }

        juce::String getBank() const { return bank; }
        void         setBank( const juce::String& Bank )
        {
            auto b   = Bank.toUpperCase();
            auto idx = (int)( *b.getCharPointer() - 'A' );
            if( 0 <= idx && idx < 8 )
                bank = Bank;
        }

        void setName( const juce::String& Name );
        void setLevel( int Velocity );
        void setPan( const float Pan );
        void setLength( int Length );
        void setUsingSeqLength( bool UseSeqLength );
        void setMute( bool Muted );
        void setTranspose( int Transpose );
        void setMidiPort( const juce::String& MidiPort );
        void setMidiChannel( int MidiChannel );
        void setMidiOutChannel( int MidiChannel );
        void setMuteGroup( int Group );

        inline juce::String getId() const noexcept { return MTITrackData::id.get(); }
        inline juce::String getName() const noexcept { return MTITrackData::name.get(); }
        inline juce::String getMidiPort() const noexcept { return MTITrackData::midiPort.get(); }
        inline bool         isUsingSeqLength() const noexcept { return MTITrackData::isSeqLength.get(); };
        inline bool         isMute() const noexcept { return MTITrackData::mute.get(); };
        inline bool         isSolo() const noexcept { return MTITrackData::solo.get(); };
        inline int          getTranspose() const noexcept { return MTITrackData::transpose.get(); };
        inline int          getMidiChannel() const noexcept { return MTITrackData::midiChannel.get(); };
        inline int          getMidiOutChannel() const noexcept { return MTITrackData::midiOutChannel.get(); };
        inline int          getMuteGroup() const noexcept { return MTITrackData::muteGroup.get(); }
        inline int          getLength() const noexcept { return MTITrackData::isSeqLength.get() ? seq.getLength() : (int)MTITrackData::length.get(); };
        inline int          getLevel() const noexcept { return MTITrackData::level.get(); }
        inline float        getPan() const noexcept { return MTITrackData::pan.get(); }

        bool isMidiInAllowed( const juce::String& Source, int Channel ) const noexcept;

        MTIProgram* getProgram() const;
        int         setProgram( MTIProgram* p );

        void play();
        void resume();
        void pause();
        void stop();
        void seek( int posInTicks );
        // Re-align playback to a bar start: TargetTick is in the sequence's shifted tick domain (see MTSequence::applySeek).
        // Flushes sounding/queued notes and resyncs prevTick/tickCtr/tickPos so the next onTick plays from TargetTick.
        void seekToTick( unsigned int TargetTick );

        void onTick( unsigned int Tick ) override;
        void onBeat();
        void onBar();

        /*void addMidiNote( int NoteNumber, int Velocity );
        void stopMidiNote( int NoteNumber );*/
        void stopAllMidiNotes();
        int  processIncomingMidiMessage( const juce::MidiMessage& Msg );
        int  processIncomingMidiMessage( const MTMidiMessage& Msg );
        // RECALL: commit the last WindowMs of captured live notes into this track's note list. Returns the number of
        // notes committed (0 if nothing was captured). Non-realtime (message thread); the caller owns the undo transaction.
        int recallLiveNotes( int WindowMs );
        // Timing correct on this track's notes. LOCK ORDER (F2): takes MTTrack::lock (the lock onTick() holds while it walks
        // the note HashMap) and only then MTNoteList::lock (taken inside the note-list edits, e.g. add/erase via the
        // ValueTree listeners). Nothing may take MTNoteList::lock first and then MTTrack::lock. Passes the track length
        // (not the sequence length) so Start mode wraps inside the track (F6). Message thread only.
        int timingCorrectLocked( const track::timingCorrectInfo& Info, const std::set<int>* OnlyIds = nullptr );
        // Track length in ticks (0 = not set yet); thread safe.
        int getLengthTicks() const { return (int)lengthInTicks.load(); }

        // Arpeggiator (per track). Acts on live input only (processIncomingMidiMessage) and is stepped by onTick, so it
        // runs while the transport plays; with the transport stopped, live notes pass through unchanged. Setters may be
        // called from any non-realtime thread. Mode/Rate/Octaves: see mt::arp::Mode / Rate / kMaxOct.
        void setArpEnabled( bool On );
        bool isArpEnabled() const { return arpOn.load( std::memory_order_relaxed ); }
        void setArpMode( int Mode );
        void setArpRate( int Rate );
        void setArpOctaves( int Octaves );
        void setArpLatch( bool On );
        void setArpGate( int Percent );
        int  getArpMode();
        int  getArpRate();
        int  getArpOctaves();
        bool isArpLatch();

        const MTMeterData& getMeter() const { return midiMeter.getMeter(); }
        inline void        processMidiMeter() { midiMeter.process(); }

        // Status
        inline bool              isTrackEffectivelyMuted() { return ( !MTITrackData::solo.get() && MTITrackData::mute.get() || soloMuted.load() ); }
        inline int               getEffectiveNoteNo( int Note ) { return Note + seq.getTranspose() + getTranspose(); };
        inline int               getEffectiveNoteVelocity( int Velocity ) { return (int)( Velocity * levelScaler ); };
        inline const MTPlayhead& playhead() { return trackPlayhead; }

        // Automation (track LEVEL lane). Mode is global (MTAudioManager::getAutomationMode).
        //  Threading: laneLevel is read by onTick under `lock` (no allocation, no extra locking there) and replaced
        //  only by swapping a fully built lane under `lock`; writers (message / MIDI-input thread: recording, clear,
        //  undo reload) are serialised by laneWriteLock and never hold `lock` while allocating.
        //  Called by MTAudioManager::setAutomationMode on the message thread: re-arms/restores the level scaler.
        void automationModeChanged( int Mode );
        // Drops the recorded lane. Undoable through the track's UndoManager when Undoable (the caller opens the transaction).
        void clearAutomation( bool Undoable );
        bool hasAutomation() const;
        // PAN lane (see mt_automation.h): keyed to the PROGRAM the track plays; applies only while the track's program
        // id equals the id the lane was recorded for. Playback never touches the ValueTree from the tick thread: onTick
        // publishes the lane value in panTarget, and MTAudioManager::applyAutomatedPans (message thread, 25 Hz timer)
        // collects it with takePanTarget() and writes the program's pan property - the same property the mixer knob
        // writes, so drum/clip/keygroup voices and the MIDI program's CC10 path react exactly as to a manual move.
        bool isPlayingProgram( const MTIProgram* P ) const { return isPlaying.load() && livePlayTick.load( std::memory_order_relaxed ) >= 0 && program.load() == P; }
        int takePanTarget(){ return panTarget.exchange( automation::kNoValue ); }
        // F7 (pan): set when the pan lane of a program became empty (clear / reload); the message thread collects the
        // program id (MTAudioManager::applyAutomatedPans) and restores the stored pan. Empty string: nothing pending.
        juce::String takePanRestore();
        // True while this track's non-empty pan lane belongs to program Id (message thread).
        bool panLaneDrives( const juce::String& Id );

        // Solo Mute; don't call this API's directly without careful analysis.
        inline void setSoloMute( bool Mute ) { soloMuted = Mute; }

    protected:
        inline void reloadMidiMeter( float Level ) { midiMeter.reload( Level ); };

    private:
        // Arpeggiator plumbing. arpFilterLive: true = the live note was taken by the arpeggiator (do not forward).
        bool arpFilterLive( const juce::MidiMessage& Msg );
        void arpEmit( const arp::Result& R, juce::Array<juce::MidiMessage>& Out );
        void arpFlush( const arp::Result& R );
        void arpPanic();

        void        progChangeHandler();
        void        lengthChangeHandler();
        void        recalculateIndex( unsigned int Tick );
        void        valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property ) override;
        void        handleAsyncUpdate() override;
        inline void updateTrackPlayhead( unsigned tickpos ) { trackPlayhead.normized = lengthFactor * tickPos; };
        void        fillMidiMessages( unsigned int tick, unsigned int prevTick, int midiChannel );
        void        recordLevelMove();
        void        commitLane( const juce::String& Points );
        void        reloadLaneFromState();
        void        applyLaneAt( unsigned int Pos );
        void        recordPanMove();
        void        bindProgramTree( MTIProgram* P );
        void        refreshPanActive(); // caller holds laneWriteLock
        juce::ValueTree findLaneNode( int Param ) const;
        void            commitPanLane( const juce::String& Points, const juce::String& ProgramId );

        MTAudioManager&          manager;
        MTSequence&              seq;
        juce::UndoManager*       undoManager{ nullptr };
        std::atomic<MTIProgram*> program{ nullptr };
        MTPlayhead               trackPlayhead;

        juce::ValueTree                vTree{ IDs::TRACK };
        juce::Array<juce::MidiMessage> midiMsgs;
        juce::MidiMessageSequence      midiSeq;
        juce::String                   bank{ "A" };
        std::atomic<bool>              isPlaying{ false };

        MTMidiMeter midiMeter{ 40 };

        std::multimap<unsigned int, juce::MidiMessage> queuedNotes;

        int                   noteIdx{ 0 };
        unsigned int          prevTick{ 0 };
        unsigned int          tickCtr{ 0 };
        unsigned int          tickPos{ 0 };
        std::atomic<double>   levelScaler{ 1.f }; // written by the UI thread (level change) and by onTick (Read mode)
        // Automation lane state (see automation methods above).
        automation::Lane      laneLevel;               // guarded by `lock` (reader: onTick) + laneWriteLock (writers)
        automation::RecState  laneRec;                 // guarded by laneWriteLock
        juce::CriticalSection laneWriteLock;
        juce::String          laneCommitted;           // last text stored in / read from the ValueTree; laneWriteLock
        std::atomic<int>      laneApplied{ -1 };       // last level pushed to levelScaler by playback (-1 = none)
        // PAN lane state. lanePan: like laneLevel (tick thread reads under `lock`, writers swap under `lock` + laneWriteLock).
        automation::Lane      lanePan;
        automation::RecState  panRec;                  // laneWriteLock
        juce::String          lanePanCommitted;        // laneWriteLock
        std::atomic<bool>     panRestorePending{ false };
        juce::String          panRestoreId;           // laneWriteLock
        juce::String          lanePanProgId;           // program id the lane belongs to; laneWriteLock
        juce::ValueTree       progTree;                // the played program's tree (listened to for pan moves in Write mode)
        std::atomic<bool>     lanePanActive{ false };  // lane non-empty and its program id == the track's program id
        std::atomic<int>      panApplied{ automation::kNoValue }; // last pan value published by playback (tick thread)
        std::atomic<int>      panTarget{ automation::kNoValue };  // tick thread -> message thread hand-off
        mutable juce::CriticalSection lock;

        std::atomic<bool>         soloMuted{ false };
        // Arpeggiator state. arpeg is guarded by arpFlag (input thread vs tick thread); arpOn mirrors enabled for the
        // lock-free fast path; arpSentNote = effective (transposed) pitch currently sounding from the arp, so the
        // note-off always matches the note-on even if transpose changes meanwhile (-1 = none).
        arp::Arpeggiator  arpeg{ 0x9E3779B9u, 96 };
        std::atomic_flag  arpFlag = ATOMIC_FLAG_INIT;
        std::atomic<bool> arpOn{ false };
        std::atomic<int>  arpSentNote{ -1 };
        // RECALL capture: per-track ring of live note events (written on the MIDI/UI input thread, see mt_live_capture.h)
        // and a mirror of the post-wrap playhead tick published by onTick (tickPos itself is tick-thread private).
        live::Ring       liveRing;
        std::atomic<int> livePlayTick{ -1 };
        std::atomic<unsigned int> lengthInTicks{ 0 };
        std::atomic<double>       lengthFactor{ 0 };
        //unsigned int              count;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( MTTrack )
    }; // MTTrack

} // namespace MT
