// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for RECALL (retrospective MIDI capture) buffer + pairing logic.
#include "../../modules/engine/audio_engine/engine/mt_live_capture.h"
#include <cstdio>
#include <cstdlib>

using namespace mt::live;

static void check( bool ok, const char* why )
{
    if( !ok ) { std::fprintf( stderr, "FAIL: %s\n", why ); std::exit( 1 ); }
}
static Event E( int64_t t, int tick, int note, int vel, bool on, int ch = 0 )
{
    Event e;
    e.timeMs = t; e.tick = tick; e.note = (uint8_t)note; e.velocity = (uint8_t)vel; e.channel = (uint8_t)ch; e.on = on;
    return e;
}

int main()
{
    const double tpm = 0.192; // 120 bpm, PPQN 96
    { // pairing, ticks mode, orphans dropped
        std::vector<Event> ev = { E( 1000, 96, 60, 100, true ), E( 1500, 96, 60, 0, false ), E( 1600, 10, 62, 90, true ), // 62 never released
                                  E( 1700, 0, 64, 0, false ),                                                             // off without on
                                  E( 2000, 200, 65, 80, true ), E( 2250, 0, 65, 0, false ) };
        auto n = pairEvents( ev, 3000, 30000, tpm, 0 );
        check( n.size() == 2, "orphans dropped" );
        check( n[0].note == 60 && n[0].startTick == 96 && n[0].lengthTicks == 96 && n[0].velocity == 100, "note 60" );
        check( n[1].note == 65 && n[1].startTick == 200 && n[1].lengthTicks == 48, "note 65" );
    }
    { // note-on with velocity 0 acts as note-off; length cap; loop wrap
        std::vector<Event> ev = { E( 0, 380, 60, 100, true ), E( 10000, 0, 60, 0, true ) };
        auto n = pairEvents( ev, 10000, 30000, tpm, 384 );
        check( n.size() == 1 && n[0].startTick == 380 && n[0].lengthTicks == 384, "vel0 off, length cap" );
        std::vector<Event> ev2 = { E( 0, 400, 60, 100, true ), E( 100, 0, 60, 0, false ) };
        auto n2 = pairEvents( ev2, 100, 30000, tpm, 384 );
        check( n2.size() == 1 && n2[0].startTick == 16, "start wraps into loop" );
    }
    { // stopped mode: aligned to earliest note, tick<0 ok
        std::vector<Event> ev = { E( 5000, -1, 60, 100, true ), E( 5100, -1, 60, 0, false ), E( 5500, -1, 62, 90, true, 1 ), E( 5600, -1, 62, 0, false, 1 ) };
        auto n = pairEvents( ev, 6000, 30000, tpm, 0 );
        check( n.size() == 2 && n[0].startTick == 0 && n[1].startTick == 96 && n[1].channel == 1, "stopped alignment" );
    }
    { // mixed buffer: stopped take + playing notes, nothing dropped, per-event alignment
        std::vector<Event> ev = { E( 1000, -1, 60, 100, true ), E( 1100, -1, 60, 0, false ), E( 1500, -1, 62, 90, true ), E( 1600, -1, 62, 0, false ),
                                  E( 2000, 100, 64, 80, true ), E( 2100, 120, 64, 0, false ) };
        auto n = pairEvents( ev, 3000, 30000, tpm, 384 );
        check( n.size() == 3, "mixed keeps all" );
        check( n[0].note == 60 && n[0].startTick == 0, "stopped first at 0" );
        check( n[2].note == 64 && n[2].startTick == 100, "playing uses tick" );
        check( n[1].note == 62 && n[1].startTick == 96, "stopped offset" );
    }
    { // stopped take longer than track: folded with %, nothing dropped
        std::vector<Event> ev = { E( 0, -1, 60, 100, true ), E( 50, -1, 60, 0, false ), E( 4000, -1, 62, 100, true ), E( 4050, -1, 62, 0, false ) };
        auto n = pairEvents( ev, 5000, 30000, tpm, 384 ); // 4000ms = 768 ticks = 2 loops -> start 0
        check( n.size() == 2 && n[0].startTick == 0 && n[1].startTick == 0, "long stopped take folded" );
        std::vector<Event> ev2 = { E( 0, -1, 60, 100, true ), E( 50, -1, 60, 0, false ), E( 2500, -1, 62, 100, true ), E( 2550, -1, 62, 0, false ) };
        auto n2 = pairEvents( ev2, 5000, 30000, tpm, 384 ); // 480 ticks -> 96
        check( n2.size() == 2 && n2[1].note == 62 && n2[1].startTick == 96, "fold remainder" );
    }
    { // stopped alignment ignores playing notes when finding the earliest
        std::vector<Event> ev = { E( 100, 10, 64, 80, true ), E( 150, 12, 64, 0, false ), E( 1000, -1, 60, 100, true ), E( 1100, -1, 60, 0, false ) };
        auto n = pairEvents( ev, 2000, 30000, tpm, 384 );
        check( n.size() == 2 && n[0].note == 60 && n[0].startTick == 0 && n[1].startTick == 10, "earliest among stopped only" );
    }
    { // age trimming: window and hard cap
        std::vector<Event> ev = { E( 0, 1, 60, 100, true ), E( 50, 1, 60, 0, false ), E( 9000, 5, 62, 100, true ), E( 9100, 5, 62, 0, false ) };
        check( pairEvents( ev, 10000, 2000, tpm, 0 ).size() == 1, "window trims old" );
        check( pairEvents( ev, 40000, 100000, tpm, 0 ).empty(), "hard age cap" );
    }
    { // re-strike while held drops the earlier on
        std::vector<Event> ev = { E( 0, 0, 60, 100, true ), E( 100, 10, 60, 70, true ), E( 200, 10, 60, 0, false ) };
        auto n = pairEvents( ev, 300, 30000, tpm, 0 );
        check( n.size() == 1 && n[0].startTick == 10 && n[0].velocity == 70, "re-strike" );
    }
    { // ring overwrites oldest, cap = kCapacity, order preserved
        Ring r;
        for( size_t i = 0; i < kCapacity + 5; ++i ) r.push( E( (int64_t)i, -1, 60, 1, true ) );
        auto s = r.snapshot();
        check( s.size() == kCapacity && s.front().timeMs == 5 && s.back().timeMs == (int64_t)kCapacity + 4, "ring wrap" );
        r.clear();
        check( r.snapshot().empty(), "ring clear" );
    }
    std::puts( "live capture tests passed" );
    return 0;
}
