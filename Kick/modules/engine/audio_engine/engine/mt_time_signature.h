/*
 * Kick - Behringer KICK drum machine software.
 * Licensed under AGPL-3.0-or-later; see the repository LICENSE.
 */
#pragma once

// JUCE-free: re-mapping of a sequence when its time signature changes. Events live in ticks, so they never move;
// what changes is the bar grid. The sequence keeps its duration in ticks, rounded UP to whole new bars (nothing is
// cut off); loop points and track lengths follow by their tick position.
namespace mt::timesig
{
    constexpr bool isValid( int Numerator, int Denominator )
    {
        return Numerator > 0 && Numerator <= 16 && ( Denominator == 4 || Denominator == 8 || Denominator == 16 || Denominator == 32 );
    }

    constexpr long long ticksPerBar( int Numerator, int Denominator, int Ppqn ) // Ppqn = ticks per quarter note
    {
        return static_cast<long long>( Ppqn ) * 4 * Numerator / Denominator;
    }

    constexpr int ceilDiv( long long A, long long B ) { return B <= 0 ? 0 : static_cast<int>( ( A + B - 1 ) / B ); }
    constexpr int floorDiv( long long A, long long B ) { return B <= 0 ? 0 : static_cast<int>( A / B ); }
    constexpr int clampInt( int V, int Lo, int Hi ) { return V < Lo ? Lo : ( V > Hi ? Hi : V ); }

    // Bars (rounded up) that cover Bars * OldTicksPerBar ticks on the new grid, clamped to [MinBars, MaxBars].
    constexpr int remapBars( int Bars, long long OldTicksPerBar, long long NewTicksPerBar, int MinBars, int MaxBars )
    {
        return clampInt( ceilDiv( static_cast<long long>( Bars ) * OldTicksPerBar, NewTicksPerBar ), MinBars, MaxBars );
    }

    struct Loop
    {
        int start; // 0-based bar index of the loop start (as MTSequence::loopStart)
        int end;   // end bar, exclusive (as MTSequence::loopEnd)
    };

    // LoopEnd == OldLength follows the new length. The start rounds down, the end up, so the loop never shrinks.
    constexpr Loop remapLoop( int LoopStart, int LoopEnd, int OldLength, int NewLength, long long OldTicksPerBar, long long NewTicksPerBar )
    {
        const int start = clampInt( floorDiv( static_cast<long long>( LoopStart ) * OldTicksPerBar, NewTicksPerBar ), 0, NewLength > 0 ? NewLength - 1 : 0 );
        int       end   = ( LoopEnd >= OldLength ) ? NewLength : clampInt( ceilDiv( static_cast<long long>( LoopEnd ) * OldTicksPerBar, NewTicksPerBar ), 0, NewLength );
        if( end < start ) end = start;
        return { start, end };
    }
} // namespace mt::timesig
