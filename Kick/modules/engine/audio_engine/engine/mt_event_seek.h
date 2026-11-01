// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pure selection logic of "LOCATE + < / >" (jump to the previous / next event), no JUCE, no engine types, so it
// can be unit tested on a host: Kick/tests/platform/EventSeekTests.cpp.
//
// An "event" is the start tick of a note of the SELECTED track (Akai MPC convention: the locate jump follows the
// selected track). Several notes on the same tick are one event. Only the start matters (not length/ratchet).
//
// Policy: no wrap-around (like the bar seek). The caller decides what "no event" means; the bridge uses:
//   previous with no earlier event -> tick 0 (start of the sequence), unless already there
//   next with no later event       -> stay where it is
#pragma once

#include <vector>

namespace mt
{
    namespace event_seek
    {
        // Largest start strictly before `current`, or -1. `starts` may be unsorted and contain duplicates/negatives
        // (negatives are ignored).
        inline int previousEvent( const std::vector<int>& starts, int current ) noexcept
        {
            int best = -1;
            for( int s : starts )
                if( s >= 0 && s < current && s > best )
                    best = s;
            return best;
        }

        // Smallest start strictly after `current`, or -1.
        inline int nextEvent( const std::vector<int>& starts, int current ) noexcept
        {
            int best = -1;
            for( int s : starts )
                if( s >= 0 && s > current && ( best < 0 || s < best ) )
                    best = s;
            return best;
        }

        // direction < 0: previous, direction > 0: next, 0: -1.
        inline int findEvent( const std::vector<int>& starts, int current, int direction ) noexcept
        {
            return direction < 0 ? previousEvent( starts, current ) : ( direction > 0 ? nextEvent( starts, current ) : -1 );
        }

        // Track-aware variant. `fromSeqTick` is a SEQUENCE tick, `starts` are track-local note starts and the track repeats
        // every `trackLenTicks` (a track shorter than the sequence loops inside it). Searches the repetition that contains
        // `fromSeqTick` and returns a SEQUENCE tick (base + local start). Past the last event of this repetition the next
        // returns the first event of the following repetition; before the first event the previous returns the last event
        // of the preceding repetition (only if there is one, i.e. base > 0). -1: no event / direction 0. The caller drops
        // results beyond the sequence end. trackLenTicks <= 0 falls back to the plain findEvent (no repetition).
        inline long long findEventInTrack( const std::vector<int>& starts, long long fromSeqTick, int direction, int trackLenTicks ) noexcept
        {
            if( trackLenTicks <= 0 )
                return findEvent( starts, (int)fromSeqTick, direction );
            if( 0 == direction || fromSeqTick < 0 )
                return -1;
            std::vector<int> local;
            local.reserve( starts.size() );
            for( int s : starts )
                if( s >= 0 && s < trackLenTicks )
                    local.push_back( s );
            if( local.empty() )
                return -1;
            const long long base = fromSeqTick - fromSeqTick % trackLenTicks;
            const int       cur  = (int)( fromSeqTick - base );
            if( direction > 0 )
            {
                const int n = nextEvent( local, cur );
                if( n >= 0 )
                    return base + n;
                return base + trackLenTicks + nextEvent( local, -1 ); // first event of the next repetition
            }
            const int p = previousEvent( local, cur );
            if( p >= 0 )
                return base + p;
            if( base <= 0 )
                return -1;
            return base - trackLenTicks + previousEvent( local, trackLenTicks ); // last event of the previous repetition
        }

        // Clamp a tick into [0, lengthInTicks - 1] (a length below 1 collapses to 0).
        constexpr int clampTick( long long tick, long long lengthInTicks ) noexcept
        {
            const long long last = lengthInTicks < 1 ? 0 : lengthInTicks - 1;
            return static_cast<int>( tick < 0 ? 0 : ( tick > last ? last : tick ) );
        }
    } // namespace event_seek
} // namespace mt
