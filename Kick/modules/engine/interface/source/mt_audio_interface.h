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

#include "mt_audio_interface_types.h"
#include "mt_audio_inserts_types.h"
#include "mt_audio_timebase_types.h"
#include "mt_midi_interface_types.h"
#include "mt_audio_transport.h"
#include "mt_audio_interface_looper.h"

namespace mt
{
    class MTAudioManager;
    class MTSampleEditorInterface;
    class MTIProgram;
    class MTAudioAssetManager;
    class ASoundSlice;

    struct signals
    {
        static sigslot::signal<>                                     midiInputsChanged;
        static sigslot::signal<>                                     midiOutputsChanged;
        static sigslot::signal<int /*index*/, juce::String /*Bank*/> bankChanged;
        static sigslot::signal<TransportState /*State*/>             playStateChanged;
        static sigslot::signal<int /*PadNum*/, int /*Colour*/>       padColourChanged;

        static sigslot::signal<const MTSelected& /*Selected*/> onProgramSelectionChange;
        static sigslot::signal<const MTSelected& /*Selected*/> onSequenceSelectionChange;
        static sigslot::signal<const MTSelected& /*Selected*/> onTrackSelectionChange;

        static sigslot::signal<>              onSongStopped;
        static sigslot::signal<int>           onNextSeqAboutToPlayInSong;
        static sigslot::signal<int, int, int> onSongSequencePlay;
        static sigslot::signal<int>           onNextSequencePlay;

        static sigslot::signal<int> onBar;
        static sigslot::signal<int> onBeat;
        static sigslot::signal<int> onTick;
        static sigslot::signal<int> onLoopBegin;
        static sigslot::signal<int> onLoopEnd;
        static sigslot::signal<>    onNote;
        static sigslot::signal<>    on1_2;
        static sigslot::signal<>    on1_2T;
        static sigslot::signal<>    on1_4;
        static sigslot::signal<>    on1_4T;
        static sigslot::signal<>    on1_8;
        static sigslot::signal<>    on1_8T;
        static sigslot::signal<>    on1_16;
        static sigslot::signal<>    on1_16T;
        static sigslot::signal<>    on1_32;
        static sigslot::signal<>    on1_32T;
    };

    class MTAudioInterfaceListener
    {
    public:
        /** Destructor. */
        virtual ~MTAudioInterfaceListener() = default;

        virtual void onStateChanged( TransportState ) {}

        // Selection.
        virtual void onProgramSelectionChange( const MTSelected& /*Selected*/ ) {}
        virtual void onSequenceSelectionChange( const MTSelected& /*Selected*/ ) {}
        virtual void onTrackSelectionChange( const MTSelected& /*Selected*/ ) {}

        // Selection List.
        virtual void onSequenceListChanged() {}
        virtual void onProgramListChanged() {}
        virtual void onTrackListChanged( int /*Sequence*/ ) {}

        // Sequence
        virtual void onBPMChange( int /*Bpm*/ ) { ; }
        virtual void onTimeSignatureChange( int /*Num*/, int /*Den*/ ) { ; }
        virtual void onBPMAndTimeSignatureChanged( float /*Bpm*/, int /*NumBars*/, int /*Numerator*/, int /*Denominator*/ ) { ; }

        // Playhead
        virtual void onPlayHeadChanged( MTBarNBeats /*BnB*/ ) { ; }
        // Track Meter.
        virtual void onTrackMetersUpdated() { ; }

        // Track
        virtual void onTrackNoteChanged( int /*Sequence*/, int /*Track*/ ) { ; }
        virtual void onTrackProgramChanged( int /*trackNo*/, int /*programNo*/ ) { ; }

        virtual void onTrackMuteChange( int /*Sequence*/, int /*Track*/, bool /*Mute*/ ) { ; }
        virtual void onTrackSoloChange( int /*Sequence*/, int /*Track*/, bool /*Solo*/ ) { ; }
        virtual void onTrackPlayChange( int /*Sequence*/, int /*Track*/, bool /*Play*/ ) { ; }
        virtual void onTrackVelocityChange( int /*Sequence*/, int /*Track*/, int /*Velocity*/ ) { ; }
        virtual void onTrackTransposeChange( int /*Sequence*/, int /*Track*/, bool /*Transpose*/ ) { ; }
        virtual void onTrackGainChange( int /*Sequence*/, int /*Track*/, double /*Gain*/ ) { ; }

        // Project...
        virtual void onProjectLoaded( juce::File ) { ; }
        virtual void onProjectClose() { ; }
        virtual void onProjectSaveFailed(const juce::String&) {}
        virtual void onProjectDirtyChanged(bool) {}
    };

    class MTAudioInterface
        : public MTLooperInterface, public juce::DeletedAtShutdown
    {
    public:
        explicit MTAudioInterface( const char* Name, const char* Manufacturer, const char* Version );
        ~MTAudioInterface();

        // Listeners
        void addListener( MTAudioInterfaceListener* l );
        void removeListener( MTAudioInterfaceListener* l );

        // Project API's
        bool isProjectLoaded();
        // createIfMissing = true only for New project (creates the folder and an empty project); otherwise a missing project is an
        // error (getProjectLoadError()) and the open one stays untouched.
        bool loadProject( juce::File& ProjectFile, bool createIfMissing = false );
        bool closeProject();
        bool saveProject();
        juce::String getProjectSaveError() const;
        juce::String getProjectLoadError() const;
        bool isProjectDirty() const;
        bool isProjectReadOnly() const; // the open project is a factory demo / template: Save As only
        bool isProjectAutoSave() const; // Preferences auto save switch
        // "Don't save": the next close of the open project (a load, quit) does not write it.
        void discardProjectChangesOnClose();
        // Save As / Save a Copy (any thread; see KickXML::saveAs).
        struct SaveAsOutcome { bool ok{ false }; bool cancelled{ false }; juce::String error; juce::File file; };
        SaveAsOutcome saveProjectAs( const juce::File& newDir, bool switchToNew, const std::function<bool( const juce::File&, const juce::File& )>& copyFolder, const std::atomic<bool>& cancel );

        // Master API's....
        const int         getMasterBPM();
        void              setMasterBPM( const int BPM );
        const MTBarNBeats getMasterLength();
        void              setMasterLength( const int Bars, const double Beats );

        // Selection...
        const MTSelected&   getSelected();
        const juce::String& getSelectedStr();

        int  getSelectedSequence() const;
        int  getSelectedTrack() const;
        int  getSelectedProgram() const;
        void setSelected( MTSelected Selected );
        void setSelectedSequence( int Selected );
        void setSelectedSequence( const juce::String& Selected );
        void setSelectedTrack( int Selected );
        void setSelectedTrack( const juce::String& Selected );
        void setSelectedProgram( int Selected );
        void setSelectedProgram( const juce::String& Selected );
        void setSelectedSequenceAndTrack( int Sequence, int Track );
        void setSelectedSequenceAndTrack( const juce::String& Sequence, const juce::String& Track );

        void performUndo();
        void performRedo();

        void               setBank( const juce::String& );
        const juce::String getBank();
        const int          getBankOffset();

        // Creators
        int createProgram( MTProgramType Type, const juce::String& Name = "" );
        int createSequence( const juce::String& Name = "" );
        int createTrack( int Sequence, const juce::String& Name = "" );

        // Deleters
        int deleteProgram( int Index );
        int deleteProgram( const juce::String& Name );
        int deleteSequence( int Index );
        int deleteSequence( const juce::String& Name );
        int deleteTrack( int Sequence, int Index );
        int deleteTrack( int Sequence, const juce::String& Name );

        // Pad performance
        MTIPadPerformanceData* getPadPerformanceData( const int Program = -1 );

        // Program
        int             getNumPrograms();
        MTProgramType   getProgramType( const int program );
        MTProgramType   getProgramType( const juce::String& program );
        MTIProgramData* getProgramDataForTrack( int Seq, int Track );
        MTIProgram*     getProgramForTrack( int Seq, int Track );
        juce::ValueTree getProgramVT( int Program );
        MTIProgram*     getProgram( int Program );
        MTIProgram*     getProgram( const juce::String& Program );
        MTIProgramData* getProgramData( int Program );
        MTIProgramData* getProgramData( const juce::String& Program );
        MTIProgramData* getProgramDataForProgramID( const juce::String& ProgramID );
        MTIProgramData* getSelectedProgramData();
        int             duplicateProgram( int program );

        int                              getProgramList( std::vector<MTIProgramData*>& List );
        const std::vector<juce::String>& getProgramList() const noexcept;

        int                       setProgramColourMode( int Program, PadColorMode Mode );
        int                       setProgramEmptyPadColorMode( int Program, EmptyPadColorMode Mode );
        int                       setProgramPadColour( int Program, int Colour, int Pad );
        int                       resetProgramPadColours( int Program ); // default colours of the program type (the user's Make Default, else factory)
        int                       makeProgramPadColoursDefault( int Program ); // Make Default: the program's colours + modes become the type's defaults
        std::vector<juce::String> getProgramNames( MTProgramCategory ct = MTProgramCategory::Unused );
        int                       renameProgram( int Program, const juce::String& NewName );
        int                       getProgramIndexByName( const juce::String& ProgramName );
        int                       getProgramIndexByID( const juce::String& ProgramID );
        juce::String              getProgramNameByIndex( int ProgramIndex );

        int setLevelForSelectedProgram( int Index, int Level ); // 0 > Index : Set parameters on Program; 0 < Index, set parameters for Note...
        int setMuteForSelectedProgram( int Index, bool Mute );
        int setPanForSelectedProgram( int Index, double Pan );
        int setPitchForSelectedProgram( int Index, int Semi, int Fine );

        void setSimultaneousNotePlay( int Program, int Note, const std::vector<int>& PlayNotes );
        void setSimultaneousNoteMute( int Program, int Note, const std::vector<int>& MuteTargets );

        // Samples
        void                      addSampleToProgram( int Program, int KeyNote, const juce::String& Sample, int Slice = 0, int Layer = 0 );
        void                      removeSampleFromProgram( int Program, int KeyNote );
        // Clears only one layer's source of the pad (undoable); the pad's other settings stay. Returns false if nothing was cleared.
        bool                      clearSampleLayerInProgram( int Program, int KeyNote, int Layer );
        void                      removeSampleFromProgram( int Program, int KeyNote, const juce::String& Sample );
        int                       getNumProgramSamples( int Program );
        void                      getProgramSamples( int Program, std::vector<MTISample*>& Samples );
        std::vector<juce::String> getProgramSampleNames( int program );
        std::vector<juce::String> getProgramSampleNames( const juce::String& programName );
        SamplePlayType            getSamplePlayType( int Program, int Note );
        SamplePlayType            getSamplePlayType( int Note );
        void                      copyPadDataToPad( int SourceProgram, int DestinationProgram, int SourcePad, int DestinationPad, int Layer = 0 );
        void                      copyPadsDataToBank( int SourceProgram, int DestinationProgram, int SourceBank, int DestinationBank, int Layer = 0 );
        void                      copyPadsDataToRange( int SourceProgram, int DestinationProgram, int SourceStartPad, int SourceEndPad, int DestinationStartPad, int Layer = 0 );

        // Pad Mutes
        bool              togglePadMute( const int Program, const int Pad );
        int               padMute( const int Program, const int Pad, bool Mute );
        juce::Array<int>* getPadMutes( const int Program );
        void              triggerPadMuteStatusRefresh( const int Program );

        int padAddToGroup( const int Program, const int Pad, const int Group );
        int padRemoveFromGroup( const int Program, const int Pad );

        // Sequence
        int                              getNumSequences();
        int                              renameSequence( int Sequence, const juce::String& Name );
        int                              getSequenceDataList( std::vector<MTISequenceData*>& List );
        const std::vector<juce::String>& getSequenceList() const noexcept;
        MTISequenceData*                 getSequenceData( int Sequence );
        int                              copySequence( int Index );
        int                              clearSequence( int Index );

        juce::ValueTree& getSequenceState( int Sequence );
        juce::ValueTree& getSequenceState( const juce::String& Name );
        juce::ValueTree  getSequenceTempoState( int Sequence, int Tempo = 0 );

        MTBarNBeats toBnB( const MTSelected& Selection, unsigned int TimeInTicks );
        int         toTicks( const MTSelected& Selection, MTBarNBeats& Time );
        MTBarNBeats toBnB( const MTSelected& Selection, double TimeInBeats );
        float       toSeconds( int Sequence, MTBarNBeats& Time ); // If Sequence < 0, returns Bar Length for currently selected sequence.
        const int   getBars( int Sequence );

        MTTempoMgr* getTempoMgr( int Sequence );

        int setSequenceBPM( int Sequence, int Bpm );
        int setSequenceLength( int Sequence, int Bars );
        int setSequenceLength( int Sequence, float Multiplier = 1.0f );
        int copyTrackBars( const SequenceCopyBars& copyBars );
        int modifySequenceLength( int Sequence, int ModifyBars, bool Insert = true, int AtBarPosition = -1 );
        // Time signature of the sequence (its own, or the global one while it follows the global tempo). The bar grid
        // changes, the events stay on their ticks, the sequence / loop / track lengths are re-mapped and rounded up to
        // whole bars (nothing is cut off); the metronome follows. One undo step. Returns 0, -1 invalid (numerator 1..16,
        // denominator 4/8/16/32), -2 the tempo map holds several time signatures (not editable).
        int  setTimeSignature( int Sequence, int Numerator, int Denominator );
        bool getTimeSignature( int Sequence, int& Numerator, int& Denominator );
        int setSequenceLoop( int Sequence, bool Loop );
        int setSequenceLoopStart( int Sequence, int StartBar );
        int setSequenceLoopEnd( int Sequence, int EndBar );
        int setSequenceLoopPoints( int Sequence, int StartBar, int EndBar );
        int setSequenceLengthToMaster( int Sequence, bool UseMaster );
        int setSequenceBPMToMaster( int Sequence, bool UseMaster );
        int sequenceBounceToSample( int Sequence, juce::File& file );

        double getSequenceLengthinMs();
        int    getSequenceLength();
        int    getSequenceLength( int Sequence ); // In Bars...
        int    getSequenceLoopEnd();
        int    getSequenceLoopEnd( int Sequence );
        bool   getLoopingState();

        // Playhead...
        MTPlayhead& getPlayhead();
        MTPlayhead& getTrackPlayhead();
        // PosInTicks represents the position w.r.t sequence length, Track is a part of sequence,
        // on passing track position this api will restrict the track position to the length of the sequence.
        void seek( int posInTicks );

        // Bar seek that follows what you hear (notes are re-aligned, hanging notes flushed). NUMBERING: bars are
        // 0-based in the engine (0 = first bar); the UI shows bar + 1. Results are clamped to [0, length - 1], no wrap.
        // Playing: applied on the next tick of the tick-processing thread. Stopped/paused: the position is parked and
        // used by the next PLAY/resume (PLAY START and STOP discard it). Both return the resulting 0-based bar,
        // or -1 for an invalid sequence. Sequence = sequence index, as in setSequencePlay().
        int seekToBar( int Sequence, int Bar0 );
        int seekBars( int Sequence, int DeltaBars ); // relative to the current (or already requested) bar
        int getSequenceLastBar( int Sequence ); // 0-based start of the last bar: loopEnd - 1 when looping, else length - 1
        // Tick-level seek (same hand-over as seekToBar). Returns the clamped tick, or -1 for an invalid sequence.
        int seekToTick( int Sequence, int Tick );
        // Tick a seek is relative to: pending/parked target, else the live playhead (playing/paused), else 0. -1: invalid.
        int getSequenceSeekTick( int Sequence );
        // Previous (Direction < 0, strictly before FromTick) / next (Direction > 0, strictly after) note start tick of
        // the selected track, or -1 if there is none / invalid selection. Reads the track's note list: call it on the
        // message thread (where the UI edits notes), not from the tick/audio thread.
        int findEventTick( const MTSelected& Selection, int FromTick, int Direction );

        void setBBT( int knob, int value );

        // Transport API's
        int  setSequencePlay( int Sequence, bool FromBeginning, bool Now );
        int  setSequencePause();
        int  setSequenceStop();
        bool isSequencePlaying();

        // SEQUENCE RECORDING (hardware REC / OVERDUB + PLAY). The track's Record button arms the track
        // (setTrackRecordArm); hardware REC arms the selected sequence (a plain take erases the armed tracks' notes when
        // PLAY starts it), OVERDUB switches to / arms an overdub take (keeps them); PLAY / PLAY START starts the take,
        // STOP ends it (see MTAudioManager). Message thread. The state getters feed the REC / OVERDUB LEDs.
        void setTrackRecordArm( int Sequence, int Track, bool Armed );
        bool isTrackRecordArmed( int Sequence, int Track );
        void sequenceRecPressed();
        void sequenceOverdubPressed();
        bool isSequenceRecArmed();
        bool isSequenceRecording();
        bool isSequenceOverdub();

        void seqAddTrackToGroup( const int Sequence, const int Track, const int Group );
        void seqRemoveTrackFromGroup( const int Sequence, const int Track );

        // Track
        int                              getNumTracks( int Sequence );
        void                             renameTrack( int Sequence, int Track, const juce::String& Name );
        int                              getTrackListData( int Sequence, std::vector<MTITrackData*>& List );
        int                              getTrackListData( std::vector<MTITrackData*>& List );
        const std::vector<juce::String>& getTrackList( int Sequence ) const noexcept;
        MTITrackData*                    getTrackData( int Sequence, int Track );
        const MTMeterData&               getTrackMeter( int Sequence, int Track );
        int                              getTrackIndexByName( int Sequence, const juce::String& Track );
        const juce::Array<juce::String>  getValidRoutableTracks( int Sequence, const juce::String& Track );
        const juce::Array<juce::String>  getValidRoutableTracks( int Sequence, int Track );
        const int                        getNumMidiTracks( int Sequence );
        const juce::Array<int>&          getMidiTrackIndexes( int Sequence );
        int                              resetTrackSolos();

        juce::ValueTree& getTrackState( int Sequence, int Track );
        juce::ValueTree& getTrackState( int Sequence, const juce::String& Name );

        int setProgramToTrack( int Program, int Sequence, int Track );
        int setProgramToTrack( const juce::String& Program, int Sequence, int Track );
        int copyTrack( int Sequence, int From, int To );
        int copyTrack( int Sequence, const juce::String& From, const juce::String& To );

        void setTrackToUseSeqLength( int Sequence, int Track, bool UseSeqLength );
        int  getTrackLength( int Sequence, int Track );
        void setTrackLength( int Sequence, int Track, int Bars );
        int  getTrackLevel( int Sequence, int Track );
        void setTrackLevel( int Sequence, int Track, int Level );
        void setTrackTranspose( int Sequence, int Track, int Transpose );
        void setTrackMute( int Sequence, int Track, bool Mute );
        bool getTrackMute( int Sequence, int Track );
        int  setTrackSolo( int Sequence, int Track, bool Solo );
        bool getTrackSolo( int Sequence, int Track );
        int  getNumSoloTracks( int Sequence );
        void setTrackMidiPort( int Sequence, int Track, const juce::String& MidiPort );
        void setTrackMidiChannel( int Sequence, int Track, int MidiChannel );
        void setTrackMidiOutChannel( int Sequence, int Track, int MidiChannel );

        int insertTimeSig( const MTSelected& Selection, int beat, int num, int den );
        int deleteTimSig( const MTSelected& Selection, int bar );

        // Note related API's...
        juce::ValueTree               getNoteList();
        juce::ValueTree               getNoteList( int SequenceIndex, int TrackIndex );
        juce::Array<juce::ValueTree>& getSelectedNoteList();

        int    getNumNotes( const MTSelected& Selection );
        int    getNumSelectedNotes( const MTSelected& Selection );
        MTNote getNoteData( const MTSelected& Selection, int NoteId );

        int  addOrModifyNote( int Note, int Start, int Length, int Velocity = 127, int Probability = 100, int Ratchet = 1 );
        int  addOrModifyNotesInTrack( int Sequence, int Track, int Note, int Start, int Length, int Velocity = 127, int Probability = 100, int Ratchet = 1 );
        int  eraseNote( int Note, int Start, int Length ); // If Length < 0, Erase note matching NoteNo, and Start. else, matching all 3 parameters
        int  eraseInvalidNotes();
        bool isInvalidNoteExists();

        MTNote                           getNoteData( int Note, int Start, int Length );
        std::pair<juce::ValueTree, bool> getNoteVT( int Note, int Start, int Length );

        int getNoteID( int Note, int StartTime, int Length );
        int getSelectedNoteID( int Note, int StartTime, int Length );

        int  getNotesInRange( const MTSelected& Selection, std::vector<MTNote>& Notes, const NotesInRange& Range );
        int  deleteNotesInRange( const MTSelected& Selection, const NotesInRange& Range );
        int  deleteNoteInTimeRange( const MTSelected& Selection, int Start, int End );
        int  selectAllNotes( bool Select );
        int  selectAllInNote( const MTSelected& Selection, const int NoteNumber, bool Select );
        int  selectNotesInRange( const NotesInRange& Range, bool Select );
        bool isSelectedNoteInRange( const NotesInRange& Range );
        int  setNoteVelocities( const NotesInRange& Range, int Velocity );
        int  setNoteProbabilities( const NotesInRange& Range, int Probability );
        int  setNoteRatchets( const NotesInRange& Range, int Ratchet );

        int editNote( const int NoteID, double DeltaTimeInBeats, int DeltaNoteNumber, double DeltaLength );
        int editNoteLength( const int NoteID, double DeltaLength, bool LeftEdge );
        int editNote( const MTSelected& Selection, int NoteID, int NoteStart );

        int editNote( const MTSelected& Selection, const std::vector<int>& Notes, double DeltaTimeInBeats, int DeltaNoteNumber, double DeltaLength );
        int editNote( const MTSelected& Selection, const NotesInRange& Range, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength );
        int nudgeNotes( const MTSelected& Selection, int noteNumber, double DeltaTimeInTicks );
        int nudgeSelectedNotes( const MTSelected& Selection, double DeltaTimeInTicks, bool Snap, double TotalTicks );
        int transposeSelectedNotes( const MTSelected& Selection, int DeltaNoteNumber );
        int editSelectedNotesStart( const MTSelected& Selection, int DeltaNoteStart, bool Snap );
        int editSelectedNotesEnd( const MTSelected& Selection, int DeltaNoteEnd );

        //int editNote( juce::ValueTree nTree, double DeltaTimeInBeats, int DeltaNoteNumber, double DeltaLength );
        //int editNoteLength( juce::ValueTree NoteOnEdit, const int deltaLength, bool isLeftEdge );
        int editSelectedNotesLength( const MTSelected& Selection, const int deltaLength, bool isLeftEdge );
        int editSelectedNotesVelocity( const MTSelected& Selection, const int Velocity );
        int editNoteVelocity( const MTSelected& Selection, int NoteID, const int Velocity );
        int editNoteVelocity( const MTSelected& Selection, const std::vector<int>& NoteIDs, const std::vector<int>& Velocities );
        int editNoteVelocity( const MTSelected& Selection, const NotesInRange& Range, int Velocity );

        int editSelectedNotesProbability( const MTSelected& Selection, const int Probability );
        int editNoteProbability( const MTSelected& Selection, int NoteID, const int Probability );
        int editNoteProbability( const MTSelected& Selection, const std::vector<int>& NoteIDs, const std::vector<int>& Probabilities );
        int editNoteProbability( const MTSelected& Selection, const NotesInRange& Range, int Probability );

        int editSelectedNotesRatchet( const MTSelected& Selection, const int Ratchet );
        int editNoteRatchet( const MTSelected& Selection, int NoteID, const int Ratchet );
        int editNoteRatchet( const MTSelected& Selection, const std::vector<int>& NoteIDs, const std::vector<int>& Ratchets );
        int editNoteRatchet( const MTSelected& Selection, const NotesInRange& Range, int Ratchet );

        int deleteSelectedNotes( const MTSelected& Selection );
        int editMuteSelectedNotes( const MTSelected& Selection, const bool MuteStatus );
        int editSelectedNotes( const MTSelected& Selection, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength );

        int pasteNote( const int NoteID, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength );
        int pasteSelectedNotes( const MTSelected& Selection, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength );
        int pasteNotes( const MTSelected& Selection, juce::Array<juce::ValueTree>& selectedNotes, double DeltaTimeInTicks );
        int pasteSelectedNotesDefault();

        int selectNote( const MTSelected& Selection, bool Select );
        int selectNote( const MTSelected& Selection, const int NoteId, bool Select );
        int selectNote( const MTSelected& Selection, const std::vector<int>& NoteIds, bool Select );
        int selectNote( const MTSelected& Selection, const int Note, const int Start, const int Length, bool Select );
        int selectNote( const MTSelected& Selection, const int Note, const int Start, bool Select );

        bool isValidSelectedNote( const int Note, const int Start, const int Length );
        bool isValidNote( const int Note, const int Start, const int Length );
        bool isValidNote( const int Note, const int Start );
        bool isValidNoteInRange( const NotesInRange& Range );

        int halfSpeedNotes( const MTSelected& Selection );
        int doubleSpeedNotes( const MTSelected& Selection );
        int moveAllNotes( const MTSelected& Selection, double DeltaInBeats );
        int deleteAllNotes( const MTSelected& Selection );
        int doubleLengthWithEvents( const MTSelected& Selection );

        int splitNotes( const MTSelected& Selection, const track::splitInfo& Info );
        int splitSelectedNotes( const MTSelected& Selection, const track::splitInfo& Info );
        int humanizeNotes( const MTSelected& Selection, const track::humanizeInfo& Info );
        // TIMING CORRECT (note quantize) of the selected track as ONE undo step. Returns notes changed, -1 if no such
        // track or unsupported Events::Range.
        int timingCorrect( const MTSelected& Selection, const track::timingCorrectInfo& Info );
        // Global TC flag (MPC "Timing Correct on"): when on, notes committed by RECALL are corrected with the stored
        // params (the last ones passed to setTimingCorrectParams / timingCorrect). Session-only, not persisted.
        void                        setTimingCorrectEnabled( bool On ) { tcEnabled.store( On ); }
        bool                        isTimingCorrectEnabled() const { return tcEnabled.load(); }
        bool                        toggleTimingCorrectEnabled() { const bool n = !tcEnabled.load(); tcEnabled.store( n ); return n; } // UI thread
        void                        setTimingCorrectParams( const track::timingCorrectInfo& Info ) { tcParams = Info; }
        const track::timingCorrectInfo& getTimingCorrectParams() const { return tcParams; }
        // AUTOMATION: per track a LEVEL lane and a PAN lane. The track's own pan has no consumer, so the PAN lane is keyed
        // to the PROGRAM the track plays (it drives/records the program's pan, the property the mixer knob writes).
        // Mode (0 Off, 1 Read, 2 Write; global, session-only, not persisted): Read plays the lanes back (level on the tick
        // thread, pan handed to the message thread), Write records level and program-pan moves made while the track plays,
        // Off ignores lanes. Lanes persist with the project. Clearing removes both lanes.
        void setAutomationMode( int Mode );
        int  getAutomationMode();
        // Clears the lane of the track in Selection as one undo step. Returns 0, -1 if no such track.
        int clearAutomation( const MTSelected& Selection );
        // Clears the lanes of every track of every sequence (one undo step).
        void clearAllAutomation();
        // The automation lane breakpoints of the track in Selection as events, sorted by tick (Level lane, then Pan lane
        // at equal ticks). setAutomationEvent: OldTick < 0 inserts, else moves the point at OldTick; 0 ok, -1 refused.
        // The caller opens the undo transaction (ScopedUndoTransaction); delete likewise.
        std::vector<MTAutomationEvent> getAutomationEvents( const MTSelected& Selection );
        int                            setAutomationEvent( const MTSelected& Selection, int Param, int OldTick, int NewTick, int Value );
        int                            deleteAutomationEvent( const MTSelected& Selection, int Param, int Tick );
        int randomNotes( const MTSelected& Selection, const track::randomEventsInfo& Info );
        int pitchQuantizeNotes( const MTSelected& Selection, const track::pitchQuantizeInfo& Info );
        int explode( const MTSelected& Selection );
        int copyNotesTo( const MTSelected& Selection, const track::copyTrackNotes Info );

        MTSampleEditorInterface* createSampleEditor();
        juce::UndoManager&       getUndoManager() noexcept;
        juce::ValueTree&         getValueTree() noexcept;
        juce::ValueTree          getTempoState() noexcept;
        juce::ValueTree&         getRuntimeState() noexcept;

        // Live Playback...
        int addLiveMidiMsg( const MTSelected& Selection, const juce::MidiMessage& Msg );
        int addLiveMidiNote( const MTSelected& Selection, int Note, int Velocity );
        int stopLiveMidiNote( const MTSelected& Selection, int Note );
        // Notes sounding on a track right now (sequence, arpeggiator and live input), bit n of Lo (0..63) / Hi (64..127); false = no such track.
        bool getActiveNotes( const MTSelected& Selection, uint64_t& Lo, uint64_t& Hi );
        // RECALL (retrospective capture): commits the live notes played on the selected track within the last WindowMs
        // (max 30 s) into its note list as one undo step. Playing: positions follow the track playhead at play time;
        // stopped: aligned to the sequence start. Returns notes committed, 0 if none captured, -1 if no such track.
        int recallLiveMidi( const MTSelected& Selection, int WindowMs = 30000 );

        // Arpeggiator (per track; the app drives the selected track: pass getSelected()). Works on live input and is
        // clocked by the transport, so it arpeggiates while the sequence plays; stopped transport = notes pass through.
        // Mode: 0 Up, 1 Down, 2 UpDown, 3 Random. Rate: 0 1/4, 1 1/8, 2 1/16 (default), 3 1/32, 4-7 the triplets
        // 1/4T 1/8T 1/16T 1/32T. Octaves 1-4. Gate 5-100 %. Out-of-range values are clamped. Set* return 0, or -1 if
        // there is no such track; is*/get* return false/-1 then.
        int  setArpEnabled( const MTSelected& Selection, bool On );
        bool isArpEnabled( const MTSelected& Selection );
        int  setArpMode( const MTSelected& Selection, int Mode );
        int  setArpRate( const MTSelected& Selection, int Rate );
        int  setArpOctaves( const MTSelected& Selection, int Octaves );
        int  setArpLatch( const MTSelected& Selection, bool On );
        int  setArpGate( const MTSelected& Selection, int Percent );
        bool isArpLatch( const MTSelected& Selection );
        int  getArpMode( const MTSelected& Selection );
        int  getArpRate( const MTSelected& Selection );
        int  getArpOctaves( const MTSelected& Selection );
        int addLiveMidiNote( const int Program, const int Note, const int Velocity );
        int stopLiveMidiNote( const int Program, const int Note );

        // Asset Manager...
        int getNumFolders();
        int getNumFiles();

        const juce::File&              getInternalDir();
        const juce::File&              getSoundsDir();
        const juce::File&              getDemosDir();
        const juce::File&              getTempDir();
        const juce::File&              getProjectDir();
        const juce::File&              getCurrentDir();
        const juce::Array<juce::File>& getDirectories();
        const juce::Array<juce::File>& getFiles();
        void                           dirSelect( int Index );
        void                           dirEnter( int Index );
        void                           dirGoUp();

        void setCurrentDir( juce::File& currDir );

        const std::vector<juce::String>& getAssets( AssetTypes Type = AssetTypes::Samples );
        std::vector<juce::String>        getAssetNames( AssetTypes Type = AssetTypes::Samples );
        const juce::String&              getAssetName( AssetTypes Type, int Index );
        const juce::String&              getAsset( AssetTypes Type, int Index );
        ASoundSlice*                     getAssetSlice( const juce::String& FileName, int Slice = 0 );

        int  addAsset( const juce::String& File, AssetTypes Type = AssetTypes::Samples );
        void removeAsset( const juce::String& File, AssetTypes Type = AssetTypes::Samples );
        void addAssetSlice( const juce::String& FileName, ASoundSlice* Slice, int Index = -1 );

        juce::ValueTree getAssetVT();

        //Key Group Colour
        void         setKeyGroupPadColour( MTPadColors padData );
        MTPadColors& getKeyGroupPadColour() { return keyGroupPadColour; }
        void         clearAllPadColour( void );

        // Preview Utilities...
        void        createPreview( const juce::File& F, float Gain ); // Gain Range: 0.0f to 1.0f.
        void        stopPreview();
        void        loopPreview( bool Loop );
        void        setPreviewMute( bool Mute );
        void        setPreviewGain( float Gain );
        bool        isPreviewRunning();
        bool        isPreviewLooping();
        bool        isPreviewMuted();
        MTMeterData readPreviewMeter();

        // Audio Recorder utilities...
        void enableRecorder( bool Enable );

        void setupRecorder( double timeInSeconds, float Gain, float Pan = 0.f, bool Looping = false, bool Overdub = false );
        void setMonitorOn( bool Monitor );
        void setRecorderMonitorGain( float Gain ); // Sampler monitor level, 0..1
        void setRecorderLoop( bool Looping );
        void setRecorderOverdub( bool Overdub );
        void setRecorderPan( float Pan );
        void setRecorderGain( float Gain );
        void setRecorderGainDB( float GainDB );
        void setRecorderThreshold( float Gain );
        void setRecorderThresholdDB( float DB );
        void setRecorderDirection( bool ForwardDirection );
        void startRecorder( int Mode, bool ForwardDirection = true, bool FromBeginging = true );
        void stopRecorder();
        void clearRecording();
        void saveRecording( const std::string& FilePath );

        float getRecorderMode();
        float getRecorderGain();
        float getRecorderProgress();
        float getRecorderThreshold();
        float getRecorderThresholdDB();

        bool isRecorderEnabled();
        bool isRecorderMonitorOn();
        bool isRecorderLooping();
        bool isRecorderOverdubbing();
        bool isRecorderInForwardDirection();
        bool isRecorderRunning();
        bool isRecorderPreviewing();

        void        setPlay( bool play );
        MTMeterData getRecorderInputMeter();
        MTMeterData getRecorderOutputMeter();
        void        startRecorderPreview( double fromStartTimeInSeconds );
        void        stopRecorderPreview();
        void        setRecorderListener( MTRecorderListener& Listener );

        juce::AudioBuffer<float>& getRecorderAudioBuffer();
        void pushRecorderMarker( int Pad = -1 );
        std::vector<Recorder::Marker> getRecorderMarkers();
        void clearRecorderMarkers();
        double getRecorderTimeSeconds();
        std::vector<Recorder::WaveformPeak> getRecorderWaveformPeaks( int MaxPeaks = 1024 );
        int getRecorderPeakCount();

        // MIDI device and utilities...
        int                                                     getNumMIDIDevices( bool isInputOrOutput ) const noexcept;
        const juce::ReferenceCountedArray<MidiDeviceListEntry>& getMidiDevices( bool isInputOrOutput ) const noexcept;
        const std::vector<juce::String>&                        getMidiDeviceNames( bool isInputOrOutput ) noexcept;

        void sendMidiMessage( const juce::String& Device, const juce::MidiMessage& Msg ) noexcept;

        MTBusManager& getBusManager();

        // Input BUS...
        const std::vector<std::string>& getInputBusNames();

        // Output BUS...
        const std::vector<std::string>& getOutputBusNames();

        // Inserts...
        const MTInsertsMap&       getInsertsMap();
        std::vector<juce::String> getInsertCategory();
        std::vector<juce::String> getInsertManufacturers();
        std::vector<juce::String> getAvailableInsertsList();
        std::vector<juce::String> getAvailableInsertsListByManufacturer( const juce::String& Manufacturer );
        std::vector<juce::String> getAvailableInsertsListByCatogary( const juce::String& Type );

        // Songs
        std::vector<juce::String> getSongsList();
        void                      createSong( const juce::String& Name );
        void                      deleteSong( const juce::String& Name );
        void                      renameSong( const juce::String& OldName, const juce::String& NewName );
        MTSongInfo                getSongInfo( const juce::String& SongName );
        void                      addSongStep( const juce::String& SongName, MTSongStepInfo& Step, int AtIndex );
        int                       appendSongSteps( const juce::String& SongName, std::vector<MTSongStepInfo>& Steps, bool CreateSong = false ); // one undo step
        void                      editSongStep( const juce::String& SongName, MTSongStepInfo& Step, int AtIndex );
        void                      deleteSongStep( const juce::String& SongName, int AtIndex );
        void                      deleteAllSongSteps( const juce::String& SongName );
        bool                      isSongExists( const juce::String& SongName );
        void                      playSong( const juce::String& SongName, int SequenceIndexToPlay );
        void                      resumeSong();
        void                      stopSong();
        void                      skipSongStep( bool AtNextBar ); // song playing: next step now (Sudden) or at the next bar (Next)

        // Next Sequence
        void playSequence( int Sequence );
        void stopSequence();
        void setNextSequence( int NextSequence );
        void setNextBarFlag( bool NextBarFlag );

        // Inserts.
        MTIInsertsInterface* getInsertsInterface( const int Program );
        MTIInsertsInterface* getInsertsInterface( const juce::String& BusName );

        // Audio devices...
        juce::AudioDeviceManager& getAudioDeviceManager();

        // Manager...
        inline MTAudioManager* getManager() { return manager.get(); }
        MTAudioAssetManager&   getAssetManager();

    private:
        std::unique_ptr<MTAudioManager> manager;
        MTPadColors                     keyGroupPadColour;
        std::atomic<bool>               tcEnabled{ false };
        track::timingCorrectInfo        tcParams; // message thread only
    }; // Class MTAudioInterface

    /// @brief This function returns global audio interface.
    /// @return Global Audio Interface.
    MTAudioInterface& getAudioInterface();

} // namespace mt

void createSampleProject();
