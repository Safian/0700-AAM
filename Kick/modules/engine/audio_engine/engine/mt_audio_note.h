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
#include <set>
#include "mt_timing_correct.h"
#include "mt_humanize.h"

namespace mt
{
    using allNotesChanedFn = std::function<void()>;

    // Playback map vs. ValueTree: `notes` (start -> id -> MTNote) is derived from the NOTELIST ValueTree and is read by the tick
    // thread (MTTrack::fillMidiMessages -> getNotesAtPos) under MTTrack::lock. The list's own edits keep both in sync by hand
    // (guarded by EditGuard); changes that arrive from anywhere else (undo/redo, copies between sequences) are picked up by the
    // ValueTree::Listener below. LOCK ORDER: MTTrack::lock (registered with setReaderLock) -> MTNoteList::lock. The listener takes
    // them in that order and only for foreign changes; own edits return before locking.
    class MTNoteList : public juce::AsyncUpdater
    {
    public:
        explicit MTNoteList( const juce::ValueTree& VT, juce::UndoManager* UM, MTSequence& Seq );
        virtual ~MTNoteList();

        // The lock the tick thread holds while it reads getNotesAtPos() (MTTrack::lock). Set once, right after construction.
        void setReaderLock( juce::CriticalSection* L ) { readerLock = L; }

        allNotesChanedFn onAllNotesChanedFn;

        inline int getNumNotes() const { return (int)vTree.getNumChildren(); };
        inline int getNumSelectedNotes() const { return (int)selected.size(); };

        MTNote                           getNoteData( int NoteId );
        MTNote                           getNoteData( int Note, int Start, int Length );
        std::pair<juce::ValueTree, bool> getNoteVT( int Note, int Start, int Length );

        int getNoteID( int Note, int Start, int Length );
        int getSelectedNoteID( int Note, int Start, int Length );

        bool isValidSelectedNote( int Note, int Start, int Length );
        bool isValidNote( int Note, int Start, int Length );
        bool isValidNote( int Note, int Start );
        bool isValidNoteInRange( const NotesInRange& Range );

        int getNotesInRange( std::vector<MTNote>& Notes, const NotesInRange& Area );
        int setNoteVelocityInRange( const NotesInRange& Range, int Velocity );
        int setNoteProbabilityInRange( const NotesInRange& Range, int Probability );
        int setNoteRatchetInRange( const NotesInRange& Range, int Ratchet );

        int addNote( const MTNote& Note, bool SortNoteList );
        int addNote( int Note, int Velocity, int Probabality, int Ratchet, int Start, int Length, bool SortNoteList );

        int  modifyNote( const int NoteID, int Velocity, int Start, int Length, bool SortNoteList );
        void modifyBars( std::vector<MTNote>& noteList, int fromTimeToEdit, int toTimeToEdit, int numOfBars, int timToEdit, int newStartValue, int newEndValue, bool isMerge = true );
        int  addOrModifyNote( int Note, int Start, int Length, int Velocity = 127, int Probability = 100, int Ratchet = 1 );

        int  eraseNote( int NoteID );
        int  eraseNote( int Note, int Start, int Length );
        int  eraseInvalidNotes();
        bool isInvalidNoteExists();

        int  selectNote( const int NoteId, bool Select );
        int  selectNote( const std::vector<int>& NoteIds, bool Select );
        int  selectNote( const int Note, const int Start, bool Select );
        int  selectNote( const int Note, const int Start, const int Length, bool Select );
        bool isSelectedNoteInRange( const NotesInRange& Range );

        int        selectNotesInRange( const NotesInRange& Range, bool Select );
        inline int selectNotesInRange( const NotesInRange& Range ) { return selectNotesInRange( Range, true ); };
        inline int deSelectNotesInRange( const NotesInRange& Range ) { return selectNotesInRange( Range, false ); };

        int        selectAllNotes( bool Select );
        inline int selectAllNotes() { return selectAllNotes( true ); }
        inline int deSelectAllNotes() { return selectAllNotes( false ); }
        int        selectAllInNote( const int NoteNumber, bool Select );

        int deleteAllNotes();
        int deleteSelectedNotes();
        int deleteNotesInRange( const NotesInRange& Range );
        int deleteNotesOutsideRange( const NotesInRange& Range );
        int deleteNotesInNoteRange( int start, int End );
        int deleteNotesOutsideNoteRange( int start, int End );
        int deleteNoteInTimeRange( int StartTime, int EndTime );
        int deleteNotesOutsideTimeRange( int StartTime, int EndTime );
        int editMuteSelectedNotes( const bool MuteStatus );
        int editSelectedNotes( double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength );

        int pasteNote( const int NoteID, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength );
        int pasteSelectedNotes( double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength );
        int pasteNotes( juce::Array<juce::ValueTree>& SelectedNotes, double DeltaTimeInTicks );
        int pasteSelectedNotesDefault();

        int editNote( const int NoteID, int NoteStart );
        int editNote( const int NoteId, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength, bool SortNoteList = true );
        int editNoteLength( const int NoteId, double DeltaLength, bool SortNoteList = true, bool isLeftEdge = false );
        int editNote( const std::vector<int>& NoteIds, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength );
        int editNote( const NotesInRange& Range, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength );
        int nudgeNotes( int noteNumber, double DeltaTimeInTicks );
        int nudgeSelectedNotes( double DeltaTimeInTicks, bool Snap, double TotalTicks );
        int transposeSelectedNotes( int DeltaNoteNumber );
        int editSelectedNotesStart( int DeltaNoteStart, bool Snap );
        int editSelectedNotesEnd( int DeltaNoteStart );

        int editSelectedNote( const int deltaLength, bool isLeftEdge );
        int editSelectedNotesLength( const int deltaLength, bool isLeftEdge );
        int editSelectedNotesVelocity( const int Velocity );
        int editSelectedNotesProbability( const int Probability );
        int editSelectedNotesRatchet( const int Ratchet );

        int editNoteVelocity( const int NoteID, const int Velocity );
        int editNoteVelocity( const std::vector<int>& NoteIDs, const std::vector<int>& Velocities );
        int editNoteVelocity( const NotesInRange& Range, int Velocity );

        int editNoteProbability( const int NoteID, const int Probability );
        int editNoteProbability( const std::vector<int>& NoteIDs, const std::vector<int>& Probabilities );
        int editNoteProbability( const NotesInRange& Range, int Probability );

        int editNoteRatchet( const int NoteID, const int Ratchet );
        int editNoteRatchet( const std::vector<int>& NoteIDs, const std::vector<int>& Ratchets );
        int editNoteRatchet( const NotesInRange& Range, int Ratchet );
        int editNoteMute( const int NoteID, const bool MuteStatus );

        int scaleSelectedNotes( float factor );
        int scaleAllNotes( float factor );
        int splitNotes( const track::splitInfo& Info );
        int splitSelectedNotes( const track::splitInfo& Info );
        int humanizeNotes( const track::humanizeInfo& Info );
        // Timing correct: all notes or the selected ones (Info.events); onlyIds != nullptr restricts to notes with these ids.
        // trackLenTicks > 0: Start mode wraps at the track length; <= 0: at the sequence length. MTTrack callers go through
        // MTTrack::timingCorrectLocked (takes the tick-thread lock first).
        int timingCorrect( const track::timingCorrectInfo& Info, const std::set<int>* onlyIds = nullptr, int trackLenTicks = 0 );
        int humanizeNotes( const track::humanizeInfo& Info, juce::ValueTree& vTree, juce::Random& r, bool& isSort );
        int randomNotes( const track::randomEventsInfo& Info );
        int pitchQuantizeNotes( const track::pitchQuantizeInfo& Info );
        int pitchQuantizeNotes( const track::pitchQuantizeInfo& Info, juce::ValueTree& Notes );

        void setMidiList( juce::MidiMessageSequence* MidiList );

        void modifyLength( int TimeToEdit, bool Insert, int AtPosition );
        int  explode();
        int  copyNotesTo( const track::copyTrackNotes& Info );

        inline juce::ValueTree&             getNoteListVT() { return vTree; };
        inline juce::HashMap<int, MTNote*>* getNotesAtPos( int start )
        {
            if( notes.contains( start ) )
                return notes[start];
            return nullptr;
        }
        inline juce::Array<juce::ValueTree>& getSelectedNoteList() { return selected; };

    private:
        void forAllNotes( std::function<void( juce::ValueTree& Note )> );
        void forSelectedNotes( std::function<void( juce::ValueTree& Note )> );
        void forNotesInRange( const NotesInRange& Range, std::function<void( juce::ValueTree& Note )> );
        void forNotesOutsideRange( const NotesInRange& Range, std::function<void( juce::ValueTree& Note )> );

        juce::ValueTree find( const int NoteID );
        juce::ValueTree find( juce::ValueTree& Note );
        juce::ValueTree find( int Note, int Start, int Length );
        juce::ValueTree find( int Note, int Start );

        int addNote( juce::ValueTree NoteVT, bool SortNoteList = true );
        int eraseNote( juce::ValueTree NoteVT );

        int editNote( juce::ValueTree& VT, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength, bool SortNoteList = true );
        int pasteNote( juce::ValueTree& VT, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength, bool SortNoteList = true );
        int pasteNote( juce::ValueTree& VT, double DeltaTimeInTicks, bool SortNoteList = true );
        int pasteSelectNote( juce::ValueTree& VT, double DeltaTimeInTicks, int DeltaNoteNumber, double DeltaLength, bool SortNoteList = true );
        int nudgeNote( juce::ValueTree& VT, double DeltaTimeInTicks, bool SortNoteList = true );
        int editNoteLength( juce::ValueTree noteOnEdit, const int deltaLength, bool isLeftEdge );
        int editNoteVelocity( juce::ValueTree& VT, const int Velocity );
        int scaleNote( juce::ValueTree& VT, float factor, bool SortNoteList = true );
        int editNoteProbability( juce::ValueTree& VT, const int Probability );
        int editNoteRatchet( juce::ValueTree& VT, const int Ratchet );
        int editNoteMute( juce::ValueTree& VT, const bool MuteStatus );
        int transposeNote( juce::ValueTree& VT, int DeltaNoteNumber, bool SortNoteList = true );
        int editNoteStart( juce::ValueTree& VT, int DeltaNoteStart );
        int editNoteEnd( juce::ValueTree& VT, int DeltaNoteEnd );

        bool isSelectedNoteStartZero( double Length, bool Snap );
        bool isSelectedNotesWithinRange( int DeltaNoteNum );
        bool isValidNudge( double Length, double TotalTicks, bool Snap );

    private:
        // ValueTree listener on NOTELIST (a member, not a base: MTTrack is already a ValueTree::Listener): keeps `notes` in step
        // with undo/redo and other foreign edits.
        struct MapListener : public juce::ValueTree::Listener
        {
            explicit MapListener( MTNoteList& O ) : owner( O ) {}
            void valueTreeChildAdded( juce::ValueTree& P, juce::ValueTree& C ) override { owner.onChildAdded( P, C ); }
            void valueTreeChildRemoved( juce::ValueTree& P, juce::ValueTree& C, int ) override { owner.onChildRemoved( P, C ); }
            void valueTreePropertyChanged( juce::ValueTree& T, const juce::Identifier& I ) override { owner.onPropertyChanged( T, I ); }
            void valueTreeRedirected( juce::ValueTree& T ) override { owner.onRedirected( T ); }
            MTNoteList& owner;
        };
        void onChildAdded( juce::ValueTree& Parent, juce::ValueTree& Child );
        void onChildRemoved( juce::ValueTree& Parent, juce::ValueTree& Child );
        void onPropertyChanged( juce::ValueTree& Tree, const juce::Identifier& Property );
        void onRedirected( juce::ValueTree& Tree );

        struct EditGuard
        {
            explicit EditGuard( juce::ThreadLocalValue<int>& D ) : depth( D ) { ++depth.get(); }
            ~EditGuard() { --depth.get(); }
            juce::ThreadLocalValue<int>& depth;
        };
        bool    isOwnEdit() { return editDepth.get() > 0; }
        MTNote* takeFromMap( int Id, int HintStart ); // removes + returns the map entry for Id (nullptr if none); caller deletes
        void    putIntoMap( int Start, int Id, MTNote* N );
        void    rebuildMap();                           // caller holds the locks

        void sort();
        int  selectNote( juce::ValueTree Note, bool Select );
        int  updateNote( MTNote& Note, int Start, int Length = 0 );

        // Funtions.
        void clearNotesToDelete();
        void triggerAllNoteChanged()
        {
            if( onAllNotesChanedFn ) onAllNotesChanedFn();
        }

    private:
        MTSequence&           seq;
        juce::UndoManager*    um{ nullptr };
        juce::CriticalSection lock;
        juce::CriticalSection* readerLock{ nullptr };
        juce::ThreadLocalValue<int> editDepth;
        MapListener                 mapListener{ *this };
        juce::ValueTree       parent;
        juce::ValueTree       vTree;

        juce::Array<juce::ValueTree> selected;
        juce::Array<juce::ValueTree> temp;

        juce::MidiMessageSequence*                       pMidiSeq{ nullptr };
        juce::HashMap<int, juce::HashMap<int, MTNote*>*> notes;
        int                                              uniqueNoteId{ 0 };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( MTNoteList );
    };
} // namespace MT
