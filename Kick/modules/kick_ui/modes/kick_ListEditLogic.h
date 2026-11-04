/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * List Edit pure logic: time <-> BAR:BEAT:TICK, the insert position, note names, selection and delete
 * rules, the Q-Link target. No JUCE, no engine: header only, covered by
 * Kick/tests/platform/ListEditLogicTests.cpp.
 */
#pragma once

#include <algorithm>
#include <cstdio>
#include <string>
#include <vector>

namespace kick_ui
{
    namespace list_edit_logic
    {
        // ---- time ------------------------------------------------------------------------------------------
        // BAR and BEAT are shown 1-based, TICK 0-based (the grid position is 01:01:00). The same convention is
        // used by the Insert / Edit time dialogs, so what the list shows is what can be typed.
        struct Bbt
        {
            int bar{ 1 };  // 1-based
            int beat{ 1 }; // 1-based
            int tick{ 0 }; // 0-based
        };

        inline std::string formatTime( const Bbt& T )
        {
            char buf[48];
            std::snprintf( buf, sizeof( buf ), "%02d:%02d:%02d", T.bar, T.beat, T.tick );
            return buf;
        }

        // Position in ticks of a BAR:BEAT:TICK typed in the dialog. 'BarStartTick' is the start of the bar.
        inline int bbtToTicks( int BarStartTick, int TicksPerBeat, const Bbt& T )
        {
            return BarStartTick + ( std::max( 1, T.beat ) - 1 ) * TicksPerBeat + std::max( 0, T.tick );
        }

        // Largest TICK value a beat holds.
        inline int maxTick( int TicksPerBeat ) { return std::max( 0, TicksPerBeat - 1 ); }

        // Inverse for a bar with a uniform beat length.
        inline Bbt ticksToBbt( int TickInBar, int TicksPerBeat )
        {
            Bbt t;
            if( TicksPerBeat <= 0 || TickInBar < 0 )
                return t;
            t.beat = TickInBar / TicksPerBeat + 1;
            t.tick = TickInBar % TicksPerBeat;
            return t;
        }

        // ---- note names ------------------------------------------------------------------------------------
        // Drum / clip programs show pads (A01..H16); keygroup / MIDI programs show note names (C-2 = 0 ... G8 = 127).
        inline std::string noteName( int Note )
        {
            static const char* names[] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
            if( Note < 0 || Note > 127 )
                return "-";
            const int octave = Note / 12 - 2;
            return std::string( names[Note % 12] ) + std::to_string( octave );
        }

        inline std::string padName( int Note )
        {
            if( Note < 0 || Note > 127 )
                return "-";
            char buf[8];
            std::snprintf( buf, sizeof( buf ), "%c%02d", 'A' + Note / 16, Note % 16 + 1 );
            return buf;
        }

        // ---- selection / delete ----------------------------------------------------------------------------
        // Delete only acts on a real selection (a table with nothing selected reported row 0).
        inline bool canDeleteRow( int NumRows, int SelectedRow ) { return SelectedRow >= 0 && SelectedRow < NumRows; }

        // Rows to delete, highest index first so earlier indices stay valid; invalid and duplicate rows dropped.
        inline std::vector<int> rowsToDelete( std::vector<int> Rows, int NumRows )
        {
            std::sort( Rows.begin(), Rows.end(), []( int a, int b ) { return a > b; } );
            Rows.erase( std::unique( Rows.begin(), Rows.end() ), Rows.end() );
            Rows.erase( std::remove_if( Rows.begin(), Rows.end(), [NumRows]( int r ) { return r < 0 || r >= NumRows; } ), Rows.end() );
            return Rows;
        }

        // Row selected after a delete: the one that moved up into the deleted slot, else the last; -1 when empty.
        inline int selectionAfterDelete( int NumRowsBefore, int LowestDeletedRow, int NumDeleted )
        {
            const int left = NumRowsBefore - NumDeleted;
            if( left <= 0 )
                return -1;
            return std::min( LowestDeletedRow, left - 1 );
        }

        // Dial / +/-: row stepping with no selection starts at the first row (down) or the last (up).
        inline int stepRow( int Selected, int Delta, int NumRows )
        {
            if( NumRows <= 0 )
                return -1;
            if( Selected < 0 )
                return Delta >= 0 ? 0 : NumRows - 1;
            return std::min( NumRows - 1, std::max( 0, Selected + Delta ) );
        }

        // ---- cell values -----------------------------------------------------------------------------------
        enum class Cell
        {
            Pad,
            Length,
            Velocity,
            Probability,
            Ratchet
        };

        inline int clampCell( Cell C, int V )
        {
            switch( C )
            {
                case Cell::Pad: return std::min( 127, std::max( 0, V ) );
                case Cell::Length: return std::min( 9999, std::max( 1, V ) ); // a velocity of 0 / length of 0 would be a dead note
                case Cell::Velocity: return std::min( 127, std::max( 1, V ) );
                case Cell::Probability: return std::min( 100, std::max( 0, V ) );
                case Cell::Ratchet: return std::min( 8, std::max( 1, V ) );
            }
            return V;
        }

        // Q-Link 1..4 edit the selected row: Pad, Length, Velocity, Probability.
        inline bool qlinkCell( int Knob, Cell& Out )
        {
            switch( Knob )
            {
                case 1: Out = Cell::Pad; return true;
                case 2: Out = Cell::Length; return true;
                case 3: Out = Cell::Velocity; return true;
                case 4: Out = Cell::Probability; return true;
                default: return false;
            }
        }

        // Stored values can lie outside what the cell controls show (ratchet 0): show them clamped, never write back.
        inline int displayValue( Cell C, int Stored ) { return clampCell( C, Stored ); }

        // Auto scroll must not fight the finger: follow resumes this long after the last touch.
        constexpr int kAutoScrollPauseMs = 4000;
        inline bool autoScrollAllowed( unsigned NowMs, unsigned LastTouchMs ) { return LastTouchMs == 0 || NowMs - LastTouchMs >= (unsigned)kAutoScrollPauseMs; }

        // Group velocity edit.
        inline int velocityDelta( int Velocity, int Delta ) { return clampCell( Cell::Velocity, Velocity + Delta ); }

        // ---- event rows (notes and automation breakpoints in one list) -------------------------------------
        // View menu of the manual: All / Notes / Track Automation (the other event types have no model here).
        enum class EventFilter
        {
            All,
            Notes,
            Automation
        };

        struct EventRef
        {
            bool automation{ false };
            int  index{ 0 };      // index in the note list / in the automation event list
            int  tick{ 0 };
            bool sameTime{ false }; // same position as the row above: its time is shown greyed
        };

        // Rows of the list: both sources are sorted by tick; at equal ticks notes come first. Equal times grey out.
        inline std::vector<EventRef> mergeEvents( const std::vector<int>& NoteTicks, const std::vector<int>& AutoTicks, EventFilter F )
        {
            std::vector<EventRef> rows;
            const bool            wantNotes = F != EventFilter::Automation;
            const bool            wantAuto  = F != EventFilter::Notes;
            size_t                n = 0, a = 0;
            const size_t          nn = wantNotes ? NoteTicks.size() : 0, na = wantAuto ? AutoTicks.size() : 0;
            while( n < nn || a < na )
            {
                EventRef r;
                if( a >= na || ( n < nn && NoteTicks[n] <= AutoTicks[a] ) )
                {
                    r.automation = false;
                    r.index      = (int)n;
                    r.tick       = NoteTicks[n++];
                }
                else
                {
                    r.automation = true;
                    r.index      = (int)a;
                    r.tick       = AutoTicks[a++];
                }
                r.sameTime = !rows.empty() && rows.back().tick == r.tick;
                rows.push_back( r );
            }
            return rows;
        }

        // The row the playhead has reached: the last row starting at or before it (-1 when the playhead is before the first).
        inline int rowAtPlayhead( const std::vector<EventRef>& Rows, int PlayTick )
        {
            int found = -1;
            for( size_t i = 0; i < Rows.size(); ++i )
            {
                if( Rows[i].tick > PlayTick )
                    break;
                found = (int)i;
            }
            return found;
        }

        // "Go to playhead": the row to bring into view = the row at the playhead, else the first row.
        inline int goToPlayheadRow( const std::vector<EventRef>& Rows, int PlayTick )
        {
            if( Rows.empty() )
                return -1;
            const int r = rowAtPlayhead( Rows, PlayTick );
            return r < 0 ? 0 : r;
        }

        // ---- automation values -----------------------------------------------------------------------------
        constexpr int kAutoLevel    = 0;
        constexpr int kAutoPan      = 1;
        constexpr int kAutoPanScale = 1000; // lane value = pan * 1000

        inline const char* autoName( int Param ) { return Param == kAutoPan ? "Pan" : "Level"; }

        // Shown / edited value: Level 0..127, Pan -100..100 (percent, left negative).
        inline int autoDisplay( int Param, int LaneValue ) { return Param == kAutoPan ? ( LaneValue < 0 ? -( ( -LaneValue + 5 ) / 10 ) : ( LaneValue + 5 ) / 10 ) : std::min( 127, std::max( 0, LaneValue ) ); }
        inline int autoFromDisplay( int Param, int Shown )
        {
            if( Param == kAutoPan )
                return std::min( 100, std::max( -100, Shown ) ) * 10;
            return std::min( 127, std::max( 0, Shown ) );
        }
        inline int autoMinShown( int Param ) { return Param == kAutoPan ? -100 : 0; }
        inline int autoMaxShown( int Param ) { return Param == kAutoPan ? 100 : 127; }

        // ---- group edits -----------------------------------------------------------------------------------
        // A note moved by Delta ticks stays inside the sequence: [0, SeqTicks - 1].
        inline int nudgeStart( int Start, int Delta, int SeqTicks )
        {
            const int hi = std::max( 0, SeqTicks - 1 );
            return std::min( hi, std::max( 0, Start + Delta ) );
        }

        // Selection set kept sorted and unique: touch / Shift toggles a row.
        inline std::vector<int> toggleRow( std::vector<int> Sel, int Row )
        {
            auto it = std::find( Sel.begin(), Sel.end(), Row );
            if( it != Sel.end() )
                Sel.erase( it );
            else
            {
                Sel.push_back( Row );
                std::sort( Sel.begin(), Sel.end() );
            }
            return Sel;
        }

        // The selection after rows [LowestDeleted...] were deleted: the row that moved up into the first deleted slot.
        inline int selectionAfterGroupDelete( int NumRowsBefore, const std::vector<int>& Deleted )
        {
            if( Deleted.empty() )
                return -1;
            const int lowest = *std::min_element( Deleted.begin(), Deleted.end() );
            return selectionAfterDelete( NumRowsBefore, lowest, (int)Deleted.size() );
        }

        // Step record (REC armed, stopped, pad pressed): the note starts at the playhead and is one TC division long
        // (0 = BAR in the TC list: then the fallback, one grid step of the screen); velocity 1..127.
        inline int stepRecordLength( int TcGridTicks, int FallbackTicks ) { return std::max( 1, TcGridTicks > 0 ? TcGridTicks : FallbackTicks ); }
        inline int stepRecordVelocity( int PadVelocity ) { return std::min( 127, std::max( 1, PadVelocity ) ); }
        // Auto-advance: the playhead moves by the note length, never past the end of the sequence.
        inline int stepRecordNextTick( int Start, int Length, int SeqTicks ) { return std::min( std::max( 0, SeqTicks ), Start + Length ); }
    }
}
