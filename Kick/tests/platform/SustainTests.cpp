// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the sustain pedal state machine. Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../modules/engine/audio_program/source/mt_sustain.h"
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

int main()
{
    using mt::SustainState;
    std::vector<int> rel;
    auto R = [&]( int n ) { rel.push_back( n ); };

    { // pedal up: note-off releases immediately
        SustainState s;
        CHECK( s.onNoteOff( 60 ) );
    }
    { // pedal down defers, pedal up releases once
        SustainState s;
        s.onCC64( 127, R );
        CHECK( s.isDown() );
        CHECK( !s.onNoteOff( 60 ) );
        CHECK( !s.onNoteOff( 62 ) );
        CHECK( s.isHeld( 60 ) );
        s.onCC64( 0, R );
        CHECK( rel.size() == 2 && rel[0] == 60 && rel[1] == 62 );
        CHECK( !s.isDown() && !s.isHeld( 60 ) );
        rel.clear();
        s.onCC64( 0, R ); // repeated up: nothing
        CHECK( rel.empty() );
    }
    { // re-strike clears the pending release
        SustainState s;
        s.onCC64( 64, R );
        s.onNoteOff( 60 );
        s.onNoteOn( 60 );
        s.onCC64( 63, R );
        CHECK( rel.empty() );
    }
    { // all-off resets the pedal too: later note-offs are not deferred
        SustainState s;
        s.onCC64( 127, R );
        s.onNoteOff( 60 );
        s.onAllOff();
        CHECK( !s.isDown() && !s.isHeld( 60 ) );
        CHECK( s.onNoteOff( 61 ) );
        s.onCC64( 0, R );
        CHECK( rel.empty() );
    }
    { // out-of-range notes are safe
        SustainState s;
        s.onCC64( 127, R );
        CHECK( s.onNoteOff( -1 ) );
        CHECK( s.onNoteOff( 200 ) );
        s.onNoteOn( 200 );
    }
    { // resetPerformance (MTTrack::stop, also muted): pedal-up value 0 releases every held note once
        SustainState s;
        s.onCC64( 127, R );
        s.onNoteOff( 60 );
        s.onNoteOff( 64 );
        rel.clear();
        s.onCC64( 0, R );
        CHECK( rel.size() == 2 && !s.isDown() && !s.isHeld( 60 ) );
        rel.clear();
        s.onCC64( 0, R ); // idempotent
        CHECK( rel.empty() );
    }
    { // sequencer (non-live) note-off with the pedal down releases at once; SustainState is not written by it
        SustainState s;
        s.onCC64( 127, R );
        CHECK( mt::releaseNowOnNoteOff( s, false, 60 ) );
        CHECK( !s.isHeld( 60 ) );
        rel.clear();
        s.onCC64( 0, R );
        CHECK( rel.empty() );
    }
    { // live note-off with the pedal down is held back, and recorded as held
        SustainState s;
        s.onCC64( 127, R );
        CHECK( !mt::releaseNowOnNoteOff( s, true, 60 ) );
        CHECK( s.isHeld( 60 ) );
    }
    if( g_fail ) return 1;
    std::printf( "SustainTests OK\n" );
    return 0;
}
