/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Pure logic of the Song screen: where the Insert button puts a step, what Delete deletes, song
 * name validation. No JUCE, no engine: header only, covered by Kick/tests/platform/SequenceLogicTests.cpp.
 */
#pragma once

#include <cctype>
#include <string>
#include <vector>

namespace kick_ui
{
    namespace song_logic
    {
        // Insert: the step list has 'NumSteps' steps, 'Selected' is the selected row (-1 when none),
        // 'PadSeq' the sequence of the selected pad, 'SelectedSeq' the sequence of the selected step.
        struct InsertPlan
        {
            enum class Kind
            {
                Add,         // a new step with the pad's sequence at 'index'
                RepeatPlus1  // the selected step already has this sequence: repeat + 1
            };
            Kind kind{ Kind::Add };
            int  index{ 0 };      // position of the new step, or of the edited step
            int  selectAfter{ 0 }; // row selected afterwards
        };

        inline InsertPlan planInsert( int NumSteps, int Selected, int PadSeq, int SelectedSeq )
        {
            InsertPlan p;
            const bool hasSel = Selected >= 0 && Selected < NumSteps;
            if( hasSel && PadSeq == SelectedSeq )
            {
                p.kind        = InsertPlan::Kind::RepeatPlus1;
                p.index       = Selected;
                p.selectAfter = Selected;
            }
            else
            {
                // After the selected step; with nothing selected at the END of the song.
                p.kind        = InsertPlan::Kind::Add;
                p.index       = hasSel ? Selected + 1 : NumSteps;
                p.selectAfter = p.index;
            }
            return p;
        }

        // Delete: only a real selection deletes; the row after the deleted one (or the last) stays selected.
        inline bool canDelete( int NumSteps, int Selected ) { return Selected >= 0 && Selected < NumSteps; }

        inline int selectionAfterDelete( int NumSteps, int Deleted )
        {
            const int left = NumSteps - 1;
            if( left <= 0 )
                return -1;
            return Deleted < left ? Deleted : left - 1;
        }

        // Song name: trimmed, not empty, not the "create new song" entry, not a duplicate (case-insensitive).
        enum class NameCheck
        {
            Ok,
            Empty,
            Reserved,
            Duplicate
        };

        inline std::string trim( const std::string& S )
        {
            size_t a = 0, b = S.size();
            while( a < b && std::isspace( (unsigned char)S[a] ) ) ++a;
            while( b > a && std::isspace( (unsigned char)S[b - 1] ) ) --b;
            return S.substr( a, b - a );
        }

        inline std::string lower( std::string S )
        {
            for( auto& c : S )
                c = (char)std::tolower( (unsigned char)c );
            return S;
        }

        inline NameCheck checkName( const std::string& Name, const std::vector<std::string>& Existing, const std::string& IgnoreExisting = {} )
        {
            const auto n = trim( Name );
            if( n.empty() )
                return NameCheck::Empty;
            if( lower( n ) == "create new song" )
                return NameCheck::Reserved;
            for( auto& e : Existing )
                if( lower( e ) == lower( n ) && lower( e ) != lower( IgnoreExisting ) )
                    return NameCheck::Duplicate;
            return NameCheck::Ok;
        }

        // ---- manual limits (Song Mode: up to 32 songs, up to 999 steps, each step repeats up to 999 times) ------------
        constexpr int kMaxSongs  = 32;
        constexpr int kMaxSteps  = 999;
        constexpr int kMaxRepeat = 999;
        inline bool canCreateSong( int NumSongs ) { return NumSongs < kMaxSongs; }
        inline bool canAddStep( int NumSteps ) { return NumSteps < kMaxSteps; }

        // Data Dial / +/- with a step selected: its sequence changes ("tap the sequence name, then dial / +/-").
        // A step whose sequence is missing (-1) starts at the first (up) or last (down) sequence.
        inline int stepSequenceAfterDial( int Current, int Delta, int NumSequences )
        {
            if( NumSequences <= 0 )
                return -1;
            if( Current < 0 || Current >= NumSequences )
                return Delta >= 0 ? 0 : NumSequences - 1;
            const int v = Current + Delta;
            return v < 0 ? 0 : ( v >= NumSequences ? NumSequences - 1 : v );
        }

        // Dial with no step selected: it walks the steps (the first step down, the last one up).
        inline int stepRow( int Selected, int Delta, int NumSteps )
        {
            if( NumSteps <= 0 )
                return -1;
            if( Selected < 0 || Selected >= NumSteps )
                return Delta >= 0 ? 0 : NumSteps - 1;
            const int v = Selected + Delta;
            return v < 0 ? 0 : ( v >= NumSteps ? NumSteps - 1 : v );
        }

        // Record-armed song (manual: "press or tap the pad ... it appears as the next step ... press a pad several
        // times to add to the number of times it plays"): the step goes to the END of the song; the same sequence as
        // the last step only raises that step's repeat. Nothing plays while a song is recorded this way.
        inline InsertPlan planRecord( int NumSteps, int LastStepSeq, int PadSeq )
        {
            InsertPlan p;
            if( NumSteps > 0 && LastStepSeq == PadSeq )
            {
                p.kind        = InsertPlan::Kind::RepeatPlus1;
                p.index       = NumSteps - 1;
                p.selectAfter = NumSteps - 1;
            }
            else
            {
                p.kind        = InsertPlan::Kind::Add;
                p.index       = NumSteps;
                p.selectAfter = NumSteps;
            }
            return p;
        }

        // Repeat counts are 1..999.
        inline int clampRepeat( int R ) { return R < 1 ? 1 : ( R > kMaxRepeat ? kMaxRepeat : R ); }

        // Next / Sudden need a following step.
        inline bool canSkipStep( int CurrentStep, int NumSteps ) { return CurrentStep >= 0 && CurrentStep + 1 < NumSteps; }

        // A step whose sequence no longer exists keeps its slot and is shown as missing.
        inline bool isMissingStep( int SeqNo ) { return SeqNo < 0; }

        // After a song ended by itself (the engine reports 'song stopped'): the screen leaves the playing lock.
        inline bool unlockAfterSongEnd( bool UiPlaying ) { return UiPlaying; }
    }
}
