// SPDX-License-Identifier: AGPL-3.0-or-later
// Parameter automation lanes: pure, JUCE-free logic (breakpoint list, interpolation, record/overwrite, thinning,
// text serialisation). No allocation outside the writer-side functions (insert/record/parse); the reader side
// (valueAt, Cursor) never allocates, locks or throws, so it is safe on the tick thread.
#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace mt::automation
{
    // Sequence/global automation mode (MPC: AUTOMATION button = Read/Write, Shift+AUTOMATION = Off).
    enum Mode : int
    {
        Off   = 0, // lanes are ignored, nothing is recorded
        Read  = 1, // lanes are played back
        Write = 2  // parameter moves made while the transport plays are recorded
    };

    // Automatable parameters. LEVEL is the track's level. PAN is the pan of the PROGRAM the track plays (the track's
    // own pan has no consumer in the engine; the program's pan is what the mixer knobs write and what the drum/clip/
    // keygroup voices and the MIDI program's CC10 path consume). The numeric ids are persisted (autoParam): never reorder.
    enum Param : int
    {
        Level = 0, // MTTrack level 0..127 (velocity scaler)
        Pan   = 1, // program pan, fixed point: -kPanScale..kPanScale (= PAN::MIN..PAN::MAX = -1..1)
        Count
    };

    constexpr int kPanScale = 1000; // lane value = round( pan * kPanScale ); 1 unit = 0.001, finer than PAN::STEP (0.01)
    constexpr int kNoValue  = INT32_MIN; // "nothing" for atomics that carry a lane value

    inline int32_t panToValue( double Pan )
    {
        if( !( Pan == Pan ) )
            return 0;
        const double v = Pan * kPanScale;
        const double c = v < -kPanScale ? -kPanScale : ( v > kPanScale ? kPanScale : v );
        return (int32_t)( c < 0 ? c - 0.5 : c + 0.5 );
    }
    inline double  valueToPan( int32_t Value )
    {
        const int c = Value < -kPanScale ? -kPanScale : ( Value > kPanScale ? kPanScale : Value );
        return (double)c / kPanScale;
    }

    // Persistence: a lane node stores its Param as `autoParam`. A node WITHOUT autoParam (projects saved before the
    // PAN lane existed) is a Level lane; unknown values are not a lane we understand (nullopt-like: returns -1).
    inline int paramFromStored( bool HasParam, int Stored )
    {
        if( !HasParam )
            return Level;
        return ( Stored >= 0 && Stored < Count ) ? Stored : -1;
    }

    // Deterministic owner among several tracks that want to drive / record the same program's pan: the lowest
    // order key (sequence index * 1000 + track index, supplied by the caller) wins; ties -> first listed. -1 if none.
    inline int pickPanOwner( const int* Orders, int N )
    {
        int best = -1;
        for( int i = 0; i < N; ++i )
            if( best < 0 || Orders[i] < Orders[best] )
                best = i;
        return best;
    }

    struct Point
    {
        int32_t tick{ 0 };
        int32_t value{ 0 };
        bool    operator==( const Point& o ) const { return tick == o.tick && value == o.value; }
    };

    using Lane = std::vector<Point>; // always sorted by tick, ticks unique

    constexpr int kMaxPoints   = 4096; // per lane
    constexpr int kMinGapTicks = 4;    // recorded points closer than this (within one pass) replace the previous one
    // A new move continues the previous one (and overwrites the old lane between them) only when it follows within
    // this many ticks; otherwise it is a fresh touch and overwrites just its own tick. 96 ticks = 1 beat at PPQN 96.
    constexpr int kTouchWindowTicks = 96;

    inline int clampMode( int M ) { return M < Off ? Off : ( M > Write ? Write : M ); }

    // Insert or replace the point at Tick keeping the lane sorted. Returns false if the lane is full (and the tick is new).
    inline bool setPoint( Lane& L, int32_t Tick, int32_t Value )
    {
        auto it = std::lower_bound( L.begin(), L.end(), Tick, []( const Point& p, int32_t t ) { return p.tick < t; } );
        if( it != L.end() && it->tick == Tick )
        {
            it->value = Value;
            return true;
        }
        if( (int)L.size() >= kMaxPoints )
            return false;
        L.insert( it, Point{ Tick, Value } );
        return true;
    }

    // Remove all points with From <= tick <= To. Returns the number removed.
    inline int eraseRange( Lane& L, int32_t From, int32_t To )
    {
        if( To < From )
            return 0;
        auto lo = std::lower_bound( L.begin(), L.end(), From, []( const Point& p, int32_t t ) { return p.tick < t; } );
        auto hi = std::upper_bound( lo, L.end(), To, []( int32_t t, const Point& p ) { return t < p.tick; } );
        const int n = (int)( hi - lo );
        L.erase( lo, hi );
        return n;
    }

    // Interpolated value at Tick (linear, rounded to nearest; before the first / after the last point the lane holds
    // that point's value). Empty lane: Fallback. Reader side: no allocation.
    inline int32_t valueAt( const Lane& L, int32_t Tick, int32_t Fallback )
    {
        if( L.empty() )
            return Fallback;
        if( Tick <= L.front().tick )
            return L.front().value;
        if( Tick >= L.back().tick )
            return L.back().value;
        auto hi = std::upper_bound( L.begin(), L.end(), Tick, []( int32_t t, const Point& p ) { return t < p.tick; } );
        auto lo = hi - 1;
        const int64_t dt = (int64_t)hi->tick - lo->tick; // > 0
        const int64_t dv = (int64_t)hi->value - lo->value;
        const int64_t n  = dv * ( (int64_t)Tick - lo->tick );
        // round half away from zero
        const int64_t q = ( n >= 0 ? ( n + dt / 2 ) : ( n - dt / 2 ) ) / dt;
        return (int32_t)( lo->value + q );
    }

    // Per-lane record state for one write pass (kept by the recorder, not stored in the lane).
    struct RecState
    {
        int32_t lastTick{ -1 };  // tick of the last recorded move (-1 = none yet / pass reset)
        int32_t lastValue{ -1 }; // its value
        void    reset() { lastTick = -1; lastValue = -1; }
    };

    enum class RecResult
    {
        Unchanged, // value equal to the previous move: nothing recorded
        Recorded,  // point inserted/replaced
        Full       // lane is full
    };

    // Record a parameter move (Value at Tick) in Write mode.
    //  - unchanged value -> dropped (thinning)
    //  - tick went backwards (loop wrapped / seek) -> a new pass starts: the touch state is reset
    //  - within the touch window the old lane content between the previous move and this one is overwritten
    //  - closer than kMinGapTicks to the previous move -> the previous point is replaced, not a new one added
    inline RecResult recordMove( Lane& L, RecState& S, int32_t Tick, int32_t Value )
    {
        if( S.lastTick >= 0 && Tick < S.lastTick )
            S.reset();
        if( S.lastTick >= 0 && Value == S.lastValue )
            return RecResult::Unchanged;

        const bool continuing = S.lastTick >= 0 && ( Tick - S.lastTick ) <= kTouchWindowTicks;
        if( continuing && ( Tick - S.lastTick ) < kMinGapTicks && Tick != S.lastTick )
        {
            // too dense: move the previous point to the new tick instead of adding another
            eraseRange( L, S.lastTick, Tick );
            if( !setPoint( L, Tick, Value ) )
                return RecResult::Full;
            S.lastTick = Tick;
            S.lastValue = Value;
            return RecResult::Recorded;
        }
        if( continuing )
            eraseRange( L, S.lastTick + 1, Tick ); // overwrite the old take between the two moves
        if( !setPoint( L, Tick, Value ) )
            return RecResult::Full;
        S.lastTick  = Tick;
        S.lastValue = Value;
        return RecResult::Recorded;
    }

    // Text form "tick:value,tick:value" (for the ValueTree property). Values are small ints; no locale dependence.
    inline std::string toString( const Lane& L )
    {
        std::string s;
        char        buf[32];
        for( size_t i = 0; i < L.size(); ++i )
        {
            std::snprintf( buf, sizeof buf, "%s%d:%d", i ? "," : "", (int)L[i].tick, (int)L[i].value );
            s += buf;
        }
        return s;
    }

    // Tolerant parser: skips malformed items, enforces sorted/unique ticks and kMaxPoints, ignores negative ticks.
    inline Lane fromString( const std::string& S )
    {
        Lane        out;
        const char* p = S.c_str();
        while( *p )
        {
            char* e = nullptr;
            long  t = std::strtol( p, &e, 10 );
            if( e != p && *e == ':' )
            {
                p = e + 1;
                long v = std::strtol( p, &e, 10 );
                if( e != p && t >= 0 && t <= INT32_MAX && v >= INT32_MIN && v <= INT32_MAX )
                    setPoint( out, (int32_t)t, (int32_t)v );
                p = e;
            }
            else
                p = ( e != p ) ? e : p; // malformed item: resynchronise at the next comma
            while( *p && *p != ',' )
                ++p;
            if( *p == ',' )
                ++p;
        }
        return out;
    }
} // namespace mt::automation
