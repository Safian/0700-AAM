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

        // A step whose sequence no longer exists keeps its slot and is shown as missing.
        inline bool isMissingStep( int SeqNo ) { return SeqNo < 0; }

        // After a song ended by itself (the engine reports 'song stopped'): the screen leaves the playing lock.
        inline bool unlockAfterSongEnd( bool UiPlaying ) { return UiPlaying; }
    }
}
