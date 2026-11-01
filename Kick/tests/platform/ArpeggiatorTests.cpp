// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free arpeggiator logic (mt_arpeggiator.h).
#include "../../modules/engine/audio_engine/engine/mt_arpeggiator.h"
#include <cstdio>
#include <cstdlib>
#include <vector>

using namespace mt::arp;

static void check( bool ok, const char* why )
{
    if( !ok ) { std::fprintf( stderr, "FAIL: %s\n", why ); std::exit( 1 ); }
}

struct Ev { int64_t t; int note; bool on; };

// Runs ticks [from, to] and collects events.
static std::vector<Ev> run( Arpeggiator& a, int64_t from, int64_t to )
{
    std::vector<Ev> v;
    for( int64_t t = from; t <= to; ++t )
    {
        Result r;
        a.tick( t, r );
        for( int i = 0; i < r.n; ++i ) v.push_back( { t, r.ev[(size_t)i].note, r.ev[(size_t)i].on } );
    }
    return v;
}
static std::vector<int> ons( const std::vector<Ev>& v )
{
    std::vector<int> o;
    for( auto& e : v ) if( e.on ) o.push_back( e.note );
    return o;
}
static Arpeggiator make( int mode, int rate = R1_16, int oct = 1 )
{
    Arpeggiator a( 1234 );
    Result r;
    a.setEnabled( true, r );
    a.setMode( mode ); a.setRate( rate ); a.setOctaves( oct );
    return a;
}

int main()
{
    check( stepTicks( R1_4, 96 ) == 96 && stepTicks( R1_8, 96 ) == 48 && stepTicks( R1_16, 96 ) == 24 && stepTicks( R1_32, 96 ) == 12, "straight rates" );
    check( stepTicks( R1_4T, 96 ) == 64 && stepTicks( R1_8T, 96 ) == 32 && stepTicks( R1_16T, 96 ) == 16 && stepTicks( R1_32T, 96 ) == 8, "triplet rates" );
    check( stepTicks( R1_32T, 4 ) == 1, "step never below 1 tick" );

    { // Up, held set sorted regardless of press order, grid-aligned at 1/16 = 24 ticks, gate 50% = 12 ticks
        auto a = make( Up );
        a.noteOn( 64, 100 ); a.noteOn( 60, 90 ); a.noteOn( 67, 80 );
        auto v = run( a, 1, 24 * 4 );
        check( ons( v ) == std::vector<int>( { 60, 64, 67, 60 } ), "up order" );
        check( v[0].t == 24 && v[0].on && v[1].t == 36 && !v[1].on && v[1].note == 60, "grid aligned, gate off at +12" );
    }
    { // Down
        auto a = make( Down );
        a.noteOn( 60, 100 ); a.noteOn( 64, 100 ); a.noteOn( 67, 100 );
        check( ons( run( a, 1, 24 * 4 ) ) == std::vector<int>( { 67, 64, 60, 67 } ), "down order" );
    }
    { // UpDown: endpoints not repeated
        auto a = make( UpDown );
        a.noteOn( 60, 100 ); a.noteOn( 64, 100 ); a.noteOn( 67, 100 );
        check( ons( run( a, 1, 24 * 6 ) ) == std::vector<int>( { 60, 64, 67, 64, 60, 64 } ), "updown order" );
    }
    { // single note in every mode
        for( int m = 0; m < NumModes; ++m )
        {
            auto a = make( m );
            a.noteOn( 62, 100 );
            check( ons( run( a, 1, 24 * 3 ) ) == std::vector<int>( { 62, 62, 62 } ), "single note" );
        }
    }
    { // octave expansion
        auto a = make( Up, R1_16, 3 );
        a.noteOn( 60, 100 ); a.noteOn( 64, 100 );
        check( a.seqSize() == 6, "3 octaves of 2 notes" );
        check( ons( run( a, 1, 24 * 7 ) ) == std::vector<int>( { 60, 64, 72, 76, 84, 88, 60 } ), "octave up order" );
        auto b = make( Up, R1_16, 4 );
        b.noteOn( 120, 100 );
        check( b.seqSize() == 1, "notes above 127 dropped" );
    }
    { // random: deterministic for equal seed, all notes from the set, different seeds differ
        auto a = make( Random ), b = make( Random );
        for( int n : { 60, 64, 67, 71 } ) { a.noteOn( n, 100 ); b.noteOn( n, 100 ); }
        auto oa = ons( run( a, 1, 24 * 40 ) ), ob = ons( run( b, 1, 24 * 40 ) );
        check( oa == ob && oa.size() == 40, "random deterministic" );
        bool inSet = true, varied = false;
        for( int n : oa ) { if( n != 60 && n != 64 && n != 67 && n != 71 ) inSet = false; if( n != oa[0] ) varied = true; }
        check( inSet && varied, "random uses the set" );
        Arpeggiator c( 99 ); Result r; c.setEnabled( true, r ); c.setMode( Random );
        for( int n : { 60, 64, 67, 71 } ) c.noteOn( n, 100 );
        check( ons( run( c, 1, 24 * 40 ) ) != oa, "different seed differs" );
    }
    { // release without latch removes the note; sounding note ends by gate
        auto a = make( Up );
        a.noteOn( 60, 100 ); a.noteOn( 64, 100 );
        run( a, 1, 24 );
        check( a.noteOff( 64 ) && a.heldCount() == 1, "release removes" );
        check( !a.noteOff( 64 ), "second release not ours" );
        auto v = run( a, 25, 24 * 3 );
        check( ons( v ) == std::vector<int>( { 60, 60 } ), "only 60 left" );
        a.noteOff( 60 );
        v = run( a, 24 * 3 + 1, 24 * 5 );
        check( ons( v ).empty(), "silent after all released" );
        bool offSeen = false;
        for( auto& e : run( a, 24 * 5 + 1, 24 * 6 ) ) offSeen |= !e.on;
        check( !offSeen, "no hanging note-off later" );
    }
    { // latch: keys released keep playing; a new chord after full release replaces; latch off drops
        auto a = make( Up );
        a.setLatch( true );
        a.noteOn( 60, 100 ); a.noteOn( 64, 100 );
        a.noteOff( 60 ); a.noteOff( 64 );
        check( a.heldCount() == 2, "latched" );
        check( ons( run( a, 1, 24 * 2 ) ) == std::vector<int>( { 60, 64 } ), "latched keeps playing" );
        a.noteOn( 67, 100 );
        check( a.heldCount() == 1, "new chord replaces latched" );
        a.noteOn( 69, 100 );
        check( a.heldCount() == 2, "second key of new chord adds" );
        a.setLatch( false );
        check( a.heldCount() == 2, "still down: kept" );
        a.noteOff( 67 ); a.noteOff( 69 );
        check( a.heldCount() == 0, "released without latch" );
        a.setLatch( true );
        a.noteOn( 50, 100 ); a.noteOff( 50 );
        a.setLatch( false );
        check( a.heldCount() == 0, "latch off drops latched notes" );
    }
    { // disable releases the sounding note; disabled arp is silent; re-enable starts clean
        auto a = make( Up );
        a.noteOn( 60, 100 );
        run( a, 1, 24 );
        Result r; a.setEnabled( false, r );
        check( r.n == 1 && !r.ev[0].on && r.ev[0].note == 60, "disable sends note-off" );
        check( run( a, 25, 100 ).empty(), "disabled silent" );
        Result r2; a.setEnabled( true, r2 );
        check( r2.n == 0 && a.heldCount() == 0, "re-enable clean" );
    }
    { // transport jump resyncs without a burst of steps; gate 100% never overlaps notes
        auto a = make( Up );
        a.noteOn( 60, 100 );
        a.setGatePercent( 100 );
        auto v = run( a, 1, 24 * 2 );
        check( v.size() == 3 && v[1].note == 60 && !v[1].on && v[1].t == 48 && v[2].on && v[2].t == 48, "gate 100: off before next on" );
        Result r; a.tick( 96000, r );
        check( r.n <= 2, "jump handled as one tick" );
        Result r2; a.tick( 5, r2 );
        check( r2.n <= 2, "backwards jump handled" );
    }
    { // clamping
        Arpeggiator a; a.setMode( 99 ); a.setRate( -5 ); a.setOctaves( 9 ); a.setGatePercent( 1000 );
        check( a.getMode() == NumModes - 1 && a.getRate() == 0 && a.getOctaves() == 4 && a.getGatePercent() == 100, "clamped" );
    }
    { // capacity
        auto a = make( Up, R1_16, 4 );
        for( int n = 0; n < 128; ++n ) a.noteOn( n, 100 );
        check( a.heldCount() == kMaxHeld && a.seqSize() <= kMaxSeq, "bounded" );
    }
    std::puts( "arpeggiator tests passed" );
    return 0;
}
