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

namespace mt
{
    class MTTrack;
    class MTSequence;

    static sigslot::signal<MTSequence*> sequenceNameChanged;

    using seqOnBarFn  = std::function<void()>;
    using seqOnBeatFn = std::function<void()>;

    using seqPlaybackReachedEnd = std::function<void()>;
    using seqPlayHeadChangedFn  = std::function<void( MTBarNBeats&, double )>;
    using seqTempoChangedFn     = std::function<void( float, int, int, int )>;

    class MTSequence
        : public MTISequenceData
        , public MTTempoMgr
        , private juce::ValueTree::Listener
    {
    public:
        explicit MTSequence( MTAudioManager& M, juce::ValueTree& VT, juce::UndoManager* UM, MTPlayhead& GlobalPlayhead );
        virtual ~MTSequence();

        // Callback functions...
        seqPlayHeadChangedFn onPlayPositionChangedFn      = []( MTBarNBeats&, double ) {};
        seqTempoChangedFn    onTempoTimeSignatureChangeFn = []( float, int, int, int ) {};

        inline juce::ValueTree& getVT() { return vTree; }
        inline juce::ValueTree& getState() { return vTree; }

        // Properties
        inline juce::String getID() const { return id.get(); }
        inline juce::String getName() const { return name.get(); }
        inline int          getLength() const { return length.get(); }
        inline int          getLoopStart() const { return loopStart.get(); }
        inline int          getLoopEnd() const { return loopEnd.get(); }
        inline int          getTranspose() const { return transpose.get(); }
        inline bool         isUsingMasterBPM() const { return isMasterBPM.get(); }
        inline MTBarNBeats  getLengthBnB() { return MTBarNBeats{ getLength() }; }
        const int           getBPM();
        MTTimeBase          getTimeBase();

        // Index API's
        void       setIndex( int Index ) { MTISequenceData::index = Index; };
        inline int getIndex() const { return MTISequenceData::index; }

        int setName( const juce::String& Name );
        int setBPM( int BPM );
        int setLength( int Bars );
        int setLength( float Multiplier );

        int setLoopStart( int Start );
        int setLoopEnd( int End );
        int setLoopPoints( int Start, int End );
        int setTranspose( int Transpose );
        int useMasterBPM( bool UseMasterBPM );
        int setLooping( bool IsLooping );

        bool getLoopingState() { return isLooping.get(); }
        int  modifySequenceLength( int ToModifyBars, bool Insert, int AtPosition = -1 );
        int  copyTrackBars( int trackIndex, const SequenceCopyBars& copyBars, std::vector<MTNote>& noteList );

        // Time related API's
        unsigned int getLengthInTicks();
        const double getLengthInSeconds();
        const double getLengthInSeconds( const MTBarNBeats& Time );
        const double getLengthInMS();

        inline void     setSelectedTrack( const int Index ) { selectedTrack = juce::jlimit<int>( -1, (int)tracks.size() - 1, Index ); }
        inline int      getSelectedTrack() const { return selectedTrack; }
        inline MTTrack* getSelectedTrackPtr() { return getTrack( selectedTrack ); }

        // Record state API's
        RecordState getRecordState() { return MTISequenceData::rState; };
        void        setRecordState( RecordState State ) { MTISequenceData::rState = State; };
        inline bool isArmed() { return RecordState::Armed == getRecordState(); };
        inline bool isRecording() { return RecordState::Recording == getRecordState(); };

        // Playhead...
        inline MTPlayhead& playhead() { return sequencePlayhead; }
        inline void        playheadGlobalToLocal() { sequencePlayhead = globalPlayhead; }
        inline void        playheadLocalToGlobal() { globalPlayhead = sequencePlayhead; }

        // Positioning
        void setBBT( int knob, int value );
        void seek( int posInTicks );
        void seek( const MTBarNBeats& BnB );

        // Bar seek that the listener HEARS (unlike seek(), which only moves the counters). Bars are 0-based in the
        // engine, the UI shows bar + 1. Both return the resulting 0-based bar (clamped, no wrap).
        //  - playing: only records the target; the tick-processing thread applies it at the start of its next onTick.
        //  - stopped: parks the position; play() starts from it (discardSeek() = "play from beginning").
        //  - paused: parks the position; resume() continues from it.
        int seekToBar( int Bar0 );
        int seekBars( int DeltaBars ); // relative to the current (or already requested) bar
        // Start of the last bar of the playable range (loopEnd - 1 when looping, else length - 1), 0-based.
        int  getLastBar();
        void discardSeek() { seekTargetBar.store( -1 ); }
        // shifted tick = clock tick + shift; see applySeek().
        inline unsigned int getTickShift() const noexcept { return tickShift.load(); }

        // Track API's...
        int createTrack( const juce::String& Name );
        int createTrack( juce::ValueTree& VT );

        int  deleteTrack( int Index );
        int  deleteTrack( const juce::String& Name );
        void deleteAllTracks();

        int              getTrackIndex( const juce::String& Name );
        int              getTrackIndex( const MTTrack* pTrack );
        MTTrack*         getTrack( int Index );
        MTTrack*         getTrack( const juce::String& Name );
        juce::ValueTree& getTrackState( int Index );
        juce::ValueTree& getTrackState( const juce::String& Name );

        const std::vector<juce::String>& getTrackList() const noexcept { return trackList; };
        const juce::Array<juce::String>  getValidRoutableTrackList( int Track );
        const int                        getNumMidiTracks();
        const juce::Array<int>&          getMidiTrackIndexes();

        int  setTrackSolo( const juce::String& Name, bool Solo );
        int  setTrackSolo( int Index, bool Solo );
        void updateTrackSolo( MTTrack* PTrk );

        inline bool             isTrackSolo( int Index ) const { return soloingTracks[Index]; }
        inline bool             isAnyTrackSolo() const { return !soloingTracks.isZero(); }
        inline bool             isTrackMutedDueToSolo( int Index ) const { return !soloingTracks.isZero() && !soloingTracks[Index]; }
        const juce::BigInteger& getSoloingTracks() const { return soloingTracks; }
        int                     getNumSoloTracks();
        int                     resetTrackSolos();

        int getNumTracks();
        int forAllTracks( std::function<void( MTTrack* T )> func );
        int forAllMidiTracks( std::function<void( MTTrack* T )> func );

        // Playback API's
        void prepareToPlay();
        void play();
        void resume();
        void pause();
        void stop();

        void onStart( unsigned int Ticks );
        void onTick( unsigned int Ticks );
        void onBeat();
        void onBar();
        void onLoopBegin( unsigned int );
        void onLoopEnd( unsigned int, unsigned int );
        void clearCounters();

    private:
        void                          seekSeqenceInternal( int posInTicks );
        void                          showBar( int bar0 );
        void                          applySeek( int bar0, unsigned int rawTick );
        void                          updateTrackSoloList();
        void                          updateTrackList();
        void                          lengthChangeHandler();
        void                          valueTreePropertyChanged( juce::ValueTree& treeWhosePropertyHasChanged, const juce::Identifier& property ) override;
        void                          setupMidiClock( unsigned int bar );
        void                          initializeTickDataFromTempoSettings( const MTTempoSettings& tempoSettings );
        inline const MTTempoSettings& getSequenceTempoSettings( unsigned int bar )
        {
            return ( isUsingMasterBPM() ? manager.getTempoSettingsAtBar( bar ) : getTempoSettingsAtBar( bar ) );
        };
        //void clearCounters();
        void isCircularRouted( const juce::String&, const MTTrack*, bool& );

        juce::Array<int> midiIndexes;

    private:
        MTAudioManager&    manager;
        juce::UndoManager* undoManager{ nullptr };

        MTPlayhead& globalPlayhead;
        MTPlayhead  sequencePlayhead;

        std::vector<std::unique_ptr<MTTrack>> tracks;
        std::vector<juce::String>             trackList;

        juce::CriticalSection lock;
        juce::ValueTree       vTree{ IDs::SEQUENCE };
        juce::MidiFile        mFile;
        juce::File            seqMidiFile;

        bool             initialized{ false };
        bool             isPlaying{ false };
        std::atomic<bool> isPaused{ false };
        int              selectedTrack{ 0 };
        juce::BigInteger soloingTracks;

        std::atomic<unsigned int> ticksPerBeat{ 0 };
        std::atomic<unsigned int> ticksPerBar{ 0 };
        std::atomic<unsigned int> ticksPer1_2{ 0 };
        std::atomic<unsigned int> ticksPer1_4{ 0 };
        std::atomic<unsigned int> ticksPer1_8{ 0 };
        std::atomic<unsigned int> ticksPer1_16{ 0 };

        std::atomic<unsigned int> barPos{ 0 };
        std::atomic<unsigned int> beatPos{ 0 };
        std::atomic<unsigned int> tickPos{ 0 };
        std::atomic<unsigned int> pos1_2{ 0 };
        std::atomic<unsigned int> pos1_4{ 0 };
        std::atomic<unsigned int> pos1_8{ 0 };
        std::atomic<unsigned int> pos1_16{ 0 };
        std::atomic<unsigned int> prevTicks{ 0 };

        // Bar seek state. The clock (CMidiClock) is not touched: the sequence maps the raw clock tick to its own
        // timeline, shifted = raw + tickShift (mod 2^32); tracks and the sequence only ever see shifted ticks.
        std::atomic<unsigned int> tickShift{ 0 };
        std::atomic<unsigned int> lastRawTick{ 0 }; // raw clock tick of the last processed onTick (origin for a seek while paused)
        std::atomic<int>          seekTargetBar{ -1 }; // requested/parked 0-based bar, -1 = none

        double lengthFactor{ 1 };
        int    lengthInTicks{ 1 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( MTSequence )
    };

} // namespace MT
