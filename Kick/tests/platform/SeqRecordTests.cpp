// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for SEQUENCE RECORDING logic: arm/record state machine, REC vs OVERDUB erase rule, capture buffer
// (event tick -> note start/length with loop wrap), and a concurrent push/drain run (TSAN friendly).
#include "../../modules/engine/audio_engine/engine/mt_seq_record.h"
#include <cstdio>
#include <cstdlib>
#include <thread>

using namespace mt::seqrec;

static void check( bool ok, const char* why )
{
    if( !ok ) { std::fprintf( stderr, "FAIL: %s\n", why ); std::exit( 1 ); }
}
static Event E( int tick, int note, int vel, bool on, int ch = 0 )
{
    Event e;
    e.tick = tick; e.note = (uint8_t)note; e.velocity = (uint8_t)vel; e.channel = (uint8_t)ch; e.on = on;
    return e;
}

int main()
{
    { // state machine
        Control c;
        bool    clr = true;
        c = onPlayStarted( c, clr );
        check( c.phase == Phase::Off && !clr, "PLAY without arm: nothing starts" );
        c = onRecPressed( c );
        check( c.phase == Phase::Armed && !c.overdub, "REC arms" );
        c = onRecPressed( c );
        check( c.phase == Phase::Off, "REC again disarms" );
        c = onRecPressed( c );
        c = onPlayStarted( c, clr );
        check( c.phase == Phase::Recording && clr, "PLAY starts the take; plain REC erases" );
        c = onPlayStarted( c, clr );
        check( c.phase == Phase::Recording && !clr, "PLAY again does not restart / erase" );
        c = onRecPressed( c );
        check( c.phase == Phase::Off && !c.overdub, "REC during the take = punch out" );
        c = onRecPressed( c );
        c = onOverdubPressed( c );
        check( c.phase == Phase::Armed && c.overdub, "OVERDUB while armed switches the mode" );
        c = onPlayStarted( c, clr );
        check( c.phase == Phase::Recording && !clr, "OVERDUB take keeps the existing events" );
        c = onTransportStopped( c );
        check( c.phase == Phase::Off && !c.overdub, "STOP ends the take and resets the one-shot overdub" );
        c = onOverdubPressed( c );
        check( c.phase == Phase::Armed && c.overdub, "OVERDUB alone arms (dummy)" );
        c = onOverdubPressed( c );
        check( c.phase == Phase::Armed && !c.overdub, "OVERDUB again: back to a plain REC take" );
        c = onTransportStopped( c );
        check( c.phase == Phase::Armed, "STOP while only armed keeps the arm" );
    }
    { // span with loop wrap
        check( spanTicks( 100, 160, 960 ) == 60, "plain length" );
        check( spanTicks( 900, 20, 960 ) == 80, "released after the wrap" );
        check( spanTicks( 100, 100, 960 ) == 1, "same tick = 1" );
        check( spanTicks( 100, 100, 0 ) == 1, "no loop length: >= 1" );
        check( spanTicks( 10, 5, 0 ) == 1, "no loop length, negative span: 1" );
        check( spanTicks( 0, 5000, 960 ) == 960, "never longer than a loop" );
    }
    { // pairing, orphans, sorting
        Capture c;
        c.push( E( 480, 60, 100, true ) );
        c.push( E( 100, 36, 90, true, 9 ) );
        c.push( E( 130, 36, 0, false, 9 ) );
        c.push( E( 500, 61, 0, false ) );   // off without on: dropped
        c.push( E( 600, 60, 100, false ) ); // closes 60
        c.push( E( 700, 62, 70, true ) );
        c.push( E( 710, 62, 0, true ) );    // note-on with velocity 0 = off
        std::vector<Note> n;
        check( c.drain( n, 960, 0, false ) == 3, "three notes" );
        check( n[0].note == 36 && n[0].startTick == 100 && n[0].lengthTicks == 30 && n[0].channel == 9 && n[0].velocity == 90, "36 first (sorted)" );
        check( n[1].note == 60 && n[1].startTick == 480 && n[1].lengthTicks == 120 && n[1].velocity == 100, "60" );
        check( n[2].note == 62 && n[2].startTick == 700 && n[2].lengthTicks == 10, "62 closed by velocity-0 on" );
        check( c.size() == 0, "all consumed" );
    }
    { // held across drains, re-strike, fold
        Capture c;
        c.push( E( 900, 64, 80, true ) );
        std::vector<Note> n;
        check( c.drain( n, 960, 0, false ) == 0 && c.size() == 1, "open note stays" );
        c.push( E( 30, 64, 0, false ) );
        check( c.drain( n, 960, 0, false ) == 1 && n[0].startTick == 900 && n[0].lengthTicks == 90, "closed in the next drain, wrapped" );
        c.push( E( 940, 65, 80, true ) );
        n.clear();
        check( c.drain( n, 960, 950, true ) == 1 && n[0].lengthTicks == 10 && c.size() == 0, "closeOpen at the take end" );
        c.push( E( 10, 66, 80, true ) );
        c.push( E( 20, 66, 90, true ) );
        c.push( E( 50, 66, 0, false ) );
        n.clear();
        check( c.drain( n, 960, 0, false ) == 2, "re-strike closes the earlier note" );
        check( n[0].startTick == 10 && n[0].lengthTicks == 10 && n[0].velocity == 80, "first strike" );
        check( n[1].startTick == 20 && n[1].lengthTicks == 30 && n[1].velocity == 90, "second strike" );
        c.push( E( 1200, 70, 80, true ) );
        c.push( E( 1210, 70, 0, false ) );
        n.clear();
        c.drain( n, 960, 0, false );
        check( n.size() == 1 && n[0].startTick == 240, "tick folded into the track length" );
    }
    { // capacity
        Capture c;
        size_t  ok = 0;
        for( size_t i = 0; i < kCapacity + 10; ++i )
            ok += c.push( E( 1, 60, 100, true ) ) ? 1 : 0;
        check( ok == kCapacity, "full buffer drops the surplus" );
        c.clear();
        check( c.size() == 0, "clear" );
    }
    { // producer (input thread) vs drainer (message thread): no pair lost or duplicated
        Capture           c;
        std::atomic<bool> go{ true };
        const int         kNotes = 20000;
        std::thread       prod( [&] {
            for( int i = 0; i < kNotes; ++i )
            {
                Event on  = E( i % 900, 40 + ( i % 40 ), 100, true );
                Event off = E( ( i % 900 ) + 5, 40 + ( i % 40 ), 0, false );
                while( !c.push( on ) ) std::this_thread::yield();
                while( !c.push( off ) ) std::this_thread::yield();
            }
            go = false;
        } );
        int               total = 0;
        std::vector<Note> n;
        while( go.load() )
        {
            n.clear();
            total += c.drain( n, 960, 0, false );
        }
        prod.join();
        n.clear();
        total += c.drain( n, 960, 0, true );
        check( total == kNotes, "every pair came out exactly once" );
    }
    std::puts( "OK" );
    return 0;
}
