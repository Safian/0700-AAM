// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the sustain pedal state machine. Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../modules/engine/audio_program/source/mt_sustain.h"
#include <cstdio>
#include <vector>
#include <thread>
#include <atomic>

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
    { // AA1a (a): live C3 held by the pedal, sequencer C3 on/off must not release it
        SustainState s;
        rel.clear();
        s.onCC64( 127, R );
        s.onNoteOn( 60 );
        CHECK( !mt::releaseNowOnNoteOff( s, true, 60 ) ); // live off, held
        s.onSeqNoteOn( 60 );
        CHECK( !mt::releaseNowOnNoteOff( s, false, 60 ) ); // sequencer off: no release while pedal holds it
        CHECK( s.isHeld( 60 ) && s.seqCount( 60 ) == 0 );
        s.onCC64( 0, R );
        CHECK( rel.size() == 1 && rel[0] == 60 );
    }
    { // AA1a (b): held C3 + long sequencer C3, pedal up must not cut the sequencer note
        SustainState s;
        rel.clear();
        s.onCC64( 127, R );
        CHECK( !mt::releaseNowOnNoteOff( s, true, 60 ) );
        s.onSeqNoteOn( 60 );
        s.onCC64( 0, R );
        CHECK( rel.empty() && !s.isHeld( 60 ) );
        CHECK( mt::releaseNowOnNoteOff( s, false, 60 ) ); // its own note-off releases it
        CHECK( s.seqCount( 60 ) == 0 );
    }
    { // sequencer-only note with the pedal down still releases at once; counter never underflows; clearSeq/allOff reset
        SustainState s;
        s.onCC64( 127, R );
        s.onSeqNoteOn( 62 );
        CHECK( mt::releaseNowOnNoteOff( s, false, 62 ) );
        CHECK( mt::releaseNowOnNoteOff( s, false, 62 ) && s.seqCount( 62 ) == 0 );
        s.onSeqNoteOn( 64 ); s.onSeqNoteOn( 64 );
        CHECK( s.seqCount( 64 ) == 2 );
        s.clearSeq();
        CHECK( s.seqCount( 64 ) == 0 );
        s.onSeqNoteOn( 65 ); s.onAllOff();
        CHECK( s.seqCount( 65 ) == 0 );
    }
    { // two-thread stress: pedal toggles vs sequencer note on/off on the same pitch (+ a live note held by the pedal).
      // Invariant at the end: pedal up and no sequencer note active -> nothing is held (no stuck voice).
        SustainState s;
        std::atomic<int> relCount{ 0 };
        auto Rel = [&]( int ) { ++relCount; };
        std::thread a( [&] {
            for( int i = 0; i < 200000; ++i )
            {
                s.onCC64( 127, Rel );
                s.onNoteOff( 60 ); // live note-off while pedal down: held
                s.onCC64( 0, Rel );
            }
        } );
        std::thread b( [&] {
            for( int i = 0; i < 200000; ++i )
            {
                s.onSeqNoteOn( 60 );
                (void)mt::releaseNowOnNoteOff( s, false, 60 );
            }
        } );
        a.join(); b.join();
        s.onCC64( 0, Rel );
        CHECK( !s.isDown() && !s.isHeld( 60 ) && s.seqCount( 60 ) == 0 );
    }
    if( g_fail ) return 1;
    std::printf( "SustainTests OK\n" );
    return 0;
}
