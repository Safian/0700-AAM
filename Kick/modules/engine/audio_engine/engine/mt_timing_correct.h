// SPDX-License-Identifier: AGPL-3.0-or-later
// JUCE-free logic for TIMING CORRECT (note quantize): snap a tick to a grid with swing, shift, window and strength,
// and apply it to a list of notes in one of four modes (Start / End / Length / Legato). Ticks are the engine's
// (PPQN 96, so a quarter note = 96 ticks). No allocation outside the caller's vector.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <vector>

namespace mt::tc
{
    enum class Type { Start, End, Length, Legato };

    struct Params
    {
        int  gridTicks{ 24 };    // grid in ticks (> 0). 1/16 at PPQN 96.
        int  swingPct{ 50 };     // 50 = straight, 66 ~ triplet feel, 75 max. Every second grid point is delayed.
        int  shiftTicks{ 0 };    // added to the grid target (Start/End)
        int  windowPct{ 100 };   // only notes within windowPct% of half a grid step of a grid point are moved (100 = all)
        int  strengthPct{ 100 }; // 100 = fully on the grid, 0 = untouched
        Type type{ Type::Start };
    };

    struct Note
    {
        int  number{ 0 };
        int  start{ 0 };
        int  length{ 1 };
        bool target{ true }; // false: not corrected, but still considered (Legato looks at the next note of the pitch)
    };

    // Integer a*pct/100, rounded half away from zero.
    inline int scalePct( int a, int pct )
    {
        const int64_t v = (int64_t)a * pct;
        return (int)( v >= 0 ? ( v + 50 ) / 100 : -( ( -v + 50 ) / 100 ) );
    }

    // Move `value` towards `target` by strength, unless it is farther than the window allows.
    inline int pull( int value, int target, int gridTicks, int windowPct, int strengthPct )
    {
        windowPct   = std::clamp( windowPct, 0, 100 );
        strengthPct = std::clamp( strengthPct, 0, 100 );
        if( windowPct < 100 && (int64_t)std::abs( value - target ) * 200 > (int64_t)gridTicks * windowPct )
            return value;
        return value + scalePct( target - value, strengthPct );
    }

    // Position of grid point i: i*grid, every odd point delayed by (swing-50)/50 of a grid step.
    inline int gridPoint( int64_t i, int gridTicks, int swingPct )
    {
        const int off = ( i & 1 ) ? scalePct( gridTicks, ( std::clamp( swingPct, 50, 75 ) - 50 ) * 2 ) : 0;
        return (int)( i * gridTicks + off );
    }

    // Nearest (swung) grid point to tick, for tick >= 0. Ties go to the earlier point.
    inline int nearestGridPoint( int tick, int gridTicks, int swingPct )
    {
        const int64_t i0   = tick / gridTicks;
        int           best = gridPoint( i0 > 0 ? i0 - 1 : 0, gridTicks, swingPct );
        for( int64_t i = ( i0 > 0 ? i0 - 1 : 0 ); i <= i0 + 2; ++i )
        {
            const int p = gridPoint( i, gridTicks, swingPct );
            if( std::abs( p - tick ) < std::abs( best - tick ) )
                best = p;
        }
        return best;
    }

    // Quantise one tick (>= 0). gridTicks <= 0 returns the tick unchanged.
    inline int snapTick( int tick, const Params& p )
    {
        if( p.gridTicks <= 0 || tick < 0 )
            return tick;
        const int target = nearestGridPoint( tick, p.gridTicks, p.swingPct );
        const int moved  = pull( tick, target, p.gridTicks, p.windowPct, p.strengthPct );
        // The shift is applied to notes that are inside the window (i.e. that were corrected), also at strength 0.
        const bool inside = p.windowPct >= 100 || (int64_t)std::abs( tick - target ) * 200 <= (int64_t)p.gridTicks * p.windowPct;
        return std::max( 0, moved + ( inside ? scalePct( p.shiftTicks, std::clamp( p.strengthPct, 0, 100 ) ) : 0 ) );
    }

    // Quantise a length to a multiple of the grid (min one grid step) with window/strength; result >= 1.
    inline int snapLength( int length, const Params& p )
    {
        if( p.gridTicks <= 0 )
            return length;
        const int q = std::max( 1, ( ( length + p.gridTicks / 2 ) / p.gridTicks ) ) * p.gridTicks;
        return std::max( 1, pull( length, q, p.gridTicks, p.windowPct, p.strengthPct ) );
    }

    // Correct all notes with target == true, in place. trackLenTicks > 0: a start that lands on or past the end of the
    // track wraps to its beginning (loop semantics). Start: moves the start, keeps the length. End: keeps the start,
    // moves the end (changes the length, min 1). Length: keeps the start, snaps the length. Legato: length = distance to
    // the next note of the same pitch (the last one of a pitch is left alone); no grid used.
    // Returns the number of notes whose start or length changed.
    inline int correct( std::vector<Note>& notes, const Params& p, int trackLenTicks )
    {
        int changed = 0;
        if( p.type == Type::Legato )
        {
            // Next start of the same pitch strictly after this note's start (uses the notes' current starts).
            for( auto& n : notes )
            {
                if( !n.target )
                    continue;
                int next = -1;
                for( const auto& o : notes )
                    if( o.number == n.number && o.start > n.start && ( next < 0 || o.start < next ) )
                        next = o.start;
                if( next > n.start && next - n.start != n.length )
                {
                    n.length = next - n.start;
                    ++changed;
                }
            }
            return changed;
        }

        for( auto& n : notes )
        {
            if( !n.target )
                continue;
            const int s0 = n.start, l0 = n.length;
            switch( p.type )
            {
                case Type::Start:
                {
                    int s = snapTick( n.start, p );
                    if( trackLenTicks > 0 && s >= trackLenTicks )
                        s %= trackLenTicks;
                    n.start = s;
                    break;
                }
                case Type::End:
                    n.length = std::max( 1, snapTick( n.start + n.length, p ) - n.start );
                    break;
                case Type::Length:
                    n.length = snapLength( n.length, p );
                    break;
                case Type::Legato: break;
            }
            if( n.start != s0 || n.length != l0 )
                ++changed;
        }
        return changed;
    }
} // namespace mt::tc
