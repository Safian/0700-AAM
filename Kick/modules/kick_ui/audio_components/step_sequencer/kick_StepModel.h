/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Step sequencer pure logic (page / step / pad mapping, value ranges, nudge wrap, the note that
 * belongs to a step, time-division list index). No JUCE, no engine: header only, covered by the
 * host test Kick/tests/platform/StepModelTests.cpp.
 */
#pragma once

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace kick_ui
{
    namespace step_model
    {
        constexpr int kVelMin   = 1; // velocity 0 is a silent note-off: never stored for a step
        constexpr int kVelMax   = 127;
        constexpr int kProbMin  = 0;
        constexpr int kProbMax  = 100;
        constexpr int kRatMin   = 1;
        constexpr int kRatMax   = 8;
        constexpr int kPadCount = 16; // hardware pads

        enum class Param
        {
            Velocity,
            Probability,
            Ratchet
        };

        inline int clampTo( int V, int Lo, int Hi ) { return std::min( Hi, std::max( Lo, V ) ); }

        inline int clampParam( Param P, int V )
        {
            switch( P )
            {
                case Param::Velocity: return clampTo( V, kVelMin, kVelMax );
                case Param::Probability: return clampTo( V, kProbMin, kProbMax );
                case Param::Ratchet: return clampTo( V, kRatMin, kRatMax );
            }
            return V;
        }

        inline int paramMax( Param P )
        {
            return P == Param::Velocity ? kVelMax : ( P == Param::Probability ? kProbMax : kRatMax );
        }

        // Number of pages for the PAGE selector and for the bank buttons: always rounded UP
        // (16T = 24 steps / 16 per page = 2 pages), at least one.
        inline int pageCount( int NumSteps, int StepsPerPage )
        {
            if( NumSteps <= 0 || StepsPerPage <= 0 )
                return 1;
            return ( NumSteps + StepsPerPage - 1 ) / StepsPerPage;
        }

        // Hardware bank button A..H (0..7) selects page 'Button'; false when that page does not exist.
        inline bool bankButtonSelectsPage( int Button, int NumSteps, int StepsPerPage )
        {
            return Button >= 0 && Button < pageCount( NumSteps, StepsPerPage );
        }

        // Absolute step index of a hardware pad (0..15) or Q-Link (0..3) on a page.
        inline int absoluteStep( int Page, int StepsPerPage, int Local )
        {
            return Page * std::max( 1, StepsPerPage ) + Local;
        }

        // A pad/Q-Link that points past the end of the bar (last, partial page) maps to no step.
        inline bool stepExists( int AbsStep, int NumSteps ) { return AbsStep >= 0 && AbsStep < NumSteps; }

        // The note that BELONGS to a step starts inside it. A longer note that began earlier only
        // overlaps the step and is not that step's note.
        inline bool noteStartsInStep( int NoteStart, int StepStart, int StepLength )
        {
            return NoteStart >= StepStart && NoteStart < StepStart + StepLength;
        }

        inline int stepStartTick( int BarStartTick, int Step, int TicksPerStep ) { return BarStartTick + Step * TicksPerStep; }

        // Position of a tick inside a sequence of TotalTicks: wraps both ways.
        inline int wrapTick( int Tick, int TotalTicks )
        {
            if( TotalTicks <= 0 )
                return Tick;
            auto t = Tick % TotalTicks;
            return t < 0 ? t + TotalTicks : t;
        }

        // Nudge: N steps (N >= 1) forwards or backwards, wrapping inside the sequence.
        inline int nudgedStart( int Start, bool Forward, int Steps, int TicksPerStep, int TotalTicks )
        {
            const auto delta = ( Forward ? 1 : -1 ) * std::max( 1, Steps ) * TicksPerStep;
            return wrapTick( Start + delta, TotalTicks );
        }

        // A dial/Q-Link turn delivers a signed count; its magnitude is the number of steps.
        inline int turnSteps( int Delta ) { return std::max( 1, std::abs( Delta ) ); }

        // Offset slider / preset results: keep the value inside the param range. Velocity never reaches 0.
        inline int applyOffset( Param P, int Value, int Offset ) { return clampParam( P, Value + Offset ); }

        // Inverse preset: mirror inside [min..max] of the param, never below its minimum.
        inline int inverseValue( Param P, int Value )
        {
            const auto lo = P == Param::Velocity ? kVelMin : ( P == Param::Probability ? kProbMin : kRatMin );
            const auto hi = paramMax( P );
            return clampTo( hi + lo - Value, lo, hi );
        }

        // Preset value "off": a velocity or ratchet below its minimum (Pulse low half, Flip) switches the
        // step off (its note is deleted) instead of writing a silent note. Probability 0 is a valid
        // value ("never plays") and is written as it is.
        inline bool presetValueMeansOff( Param P, int Value )
        {
            return P == Param::Velocity ? Value < kVelMin : ( P == Param::Probability ? false : Value < kRatMin );
        }

        // Steps in a bar from the real bar length: correct for any time signature (3/4 at 1/16 = 12 steps).
        inline int stepsPerBar( int TicksPerBar, int TicksPerStep, int Fallback )
        {
            if( TicksPerBar <= 0 || TicksPerStep <= 0 )
                return Fallback;
            return std::max( 1, TicksPerBar / TicksPerStep );
        }

        // Value of a step with several notes: average, kept inside the param range.
        inline int averageValue( Param P, const std::vector<int>& Values )
        {
            if( Values.empty() )
                return P == Param::Velocity ? kVelMax : ( P == Param::Probability ? kProbMax : kRatMin );
            long long sum = 0;
            for( auto v : Values )
                sum += v;
            return clampParam( P, (int)( sum / (long long)Values.size() ) );
        }

        // Index of the quantization (TimeBase enum value as stored by the engine: 1, 2, 3, 4, 6, 8, 12,
        // 16, 24, 32, 48, 64, 96) in the TIME DIV list { 1, 4, 4T, 8, 8T, 16, 16T, 32, 32T, 64, 64T }.
        // 78 is the legacy wrong value for 32T. 1/2 and 1/2T are not in the list: they show the next
        // coarser entry "1", and the quantization itself is never rewritten by merely showing it.
        inline int timeDivIndex( int Quantization )
        {
            switch( Quantization )
            {
                case 1:
                case 2:
                case 3: return 0;
                case 4: return 1;
                case 6: return 2;
                case 8: return 3;
                case 12: return 4;
                case 16: return 5;
                case 24: return 6;
                case 32: return 7;
                case 48:
                case 78: return 8;
                case 64: return 9;
                case 96: return 10;
                default: return 5;
            }
        }

        // True when the list can show this quantization exactly.
        inline bool timeDivListHasQuantization( int Quantization ) { return Quantization != 2 && Quantization != 3; }

        // Base values the engine uses for the TIME DIV string entries.
        inline int timeBaseValue( int ListIndex )
        {
            static const int v[] = { 1, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96 };
            return ( ListIndex >= 0 && ListIndex < 11 ) ? v[ListIndex] : 16;
        }

        // Page that contains a step (for page following).
        inline int pageOfStep( int AbsStep, int StepsPerPage ) { return AbsStep / std::max( 1, StepsPerPage ); }

        // Playhead tick -> step in the bar (-1 when outside the bar).
        inline int playStep( int PlayTick, int BarStartTick, int TicksPerStep, int NumSteps )
        {
            if( TicksPerStep <= 0 || PlayTick < BarStartTick )
                return -1;
            const auto s = ( PlayTick - BarStartTick ) / TicksPerStep;
            return s < NumSteps ? s : -1;
        }
    }
}
