// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free automation lane logic. Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../modules/engine/audio_engine/engine/mt_automation.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                           \
    do {                                                                                                           \
        long long va = (long long)( a ), vb = (long long)( b );                                                    \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )

using namespace mt::automation;

static bool hasTick( const Lane& l, int t )
{
    for( auto& p : l )
        if( p.tick == t ) return true;
    return false;
}

int main()
{
    Lane l;
    CHECK_EQ( valueAt( l, 10, 99 ), 99 ); // empty -> fallback

    // setPoint keeps order, replaces same tick
    CHECK_EQ( setPoint( l, 100, 50 ), true );
    CHECK_EQ( setPoint( l, 0, 10 ), true );
    CHECK_EQ( setPoint( l, 50, 30 ), true );
    CHECK_EQ( setPoint( l, 50, 31 ), true );
    CHECK_EQ( l.size(), 3 );
    CHECK_EQ( l[0].tick, 0 );
    CHECK_EQ( l[1].tick, 50 );
    CHECK_EQ( l[1].value, 31 );
    CHECK_EQ( l[2].tick, 100 );

    // interpolation, holds, rounding
    CHECK_EQ( valueAt( l, -5, 0 ), 10 );
    CHECK_EQ( valueAt( l, 0, 0 ), 10 );
    CHECK_EQ( valueAt( l, 25, 0 ), 21 ); // 10 + 21*0.5 = 20.5 -> 21
    CHECK_EQ( valueAt( l, 50, 0 ), 31 );
    CHECK_EQ( valueAt( l, 75, 0 ), 41 ); // 31 + 19*0.5 = 40.5 -> 41
    CHECK_EQ( valueAt( l, 100, 0 ), 50 );
    CHECK_EQ( valueAt( l, 5000, 0 ), 50 );
    Lane d;
    setPoint( d, 0, 100 );
    setPoint( d, 10, 0 );
    CHECK_EQ( valueAt( d, 5, 0 ), 50 );
    CHECK_EQ( valueAt( d, 3, 0 ), 70 );
    CHECK_EQ( valueAt( d, 1, 0 ), 90 );

    // eraseRange inclusive
    Lane e;
    for( int i = 0; i < 10; ++i ) setPoint( e, i * 10, i );
    CHECK_EQ( eraseRange( e, 20, 50 ), 4 );
    CHECK_EQ( e.size(), 6 );
    CHECK_EQ( eraseRange( e, 50, 20 ), 0 );
    CHECK_EQ( eraseRange( e, 1000, 2000 ), 0 );

    // full lane
    Lane f;
    for( int i = 0; i < kMaxPoints; ++i ) setPoint( f, i, i & 127 );
    CHECK_EQ( setPoint( f, kMaxPoints + 5, 1 ), false );
    CHECK_EQ( setPoint( f, 7, 1 ), true ); // replacing an existing tick still works

    // recordMove: thinning + overwrite + pass reset
    Lane     r;
    RecState s;
    for( int t = 0; t <= 400; t += 40 ) setPoint( r, t, 100 ); // old take: 0,40,...,400
    CHECK_EQ( (int)recordMove( r, s, 100, 20 ), (int)RecResult::Recorded );  // first move of the pass: only its own tick
    CHECK_EQ( hasTick( r, 80 ), true );
    CHECK_EQ( hasTick( r, 120 ), true );
    CHECK_EQ( (int)recordMove( r, s, 110, 20 ), (int)RecResult::Unchanged ); // same value dropped
    CHECK_EQ( (int)recordMove( r, s, 150, 30 ), (int)RecResult::Recorded );  // overwrites old 120 (between 100 and 150)
    CHECK_EQ( hasTick( r, 120 ), false );
    CHECK_EQ( hasTick( r, 160 ), true );
    // dense: 152 is < kMinGapTicks after 150 -> replaces the point at 150, no growth
    size_t before = r.size();
    CHECK_EQ( (int)recordMove( r, s, 152, 35 ), (int)RecResult::Recorded );
    CHECK_EQ( r.size(), before );
    CHECK_EQ( valueAt( r, 152, 0 ), 35 );
    CHECK_EQ( hasTick( r, 150 ), false );
    // far away (beyond the touch window): fresh touch, only its own tick, nothing between is erased
    before = r.size();
    CHECK_EQ( (int)recordMove( r, s, 350, 5 ), (int)RecResult::Recorded );
    CHECK_EQ( r.size(), before + 1 );
    CHECK_EQ( hasTick( r, 320 ), true );
    // loop wrap: tick goes backwards -> new pass, even the same value is recorded
    CHECK_EQ( (int)recordMove( r, s, 10, 5 ), (int)RecResult::Recorded );
    CHECK_EQ( s.lastTick, 10 );

    // serialisation round trip + tolerant parse
    Lane a;
    setPoint( a, 0, 1 );
    setPoint( a, 96, 127 );
    setPoint( a, 4000, 0 );
    CHECK_EQ( toString( a ) == "0:1,96:127,4000:0", true );
    CHECK_EQ( fromString( toString( a ) ) == a, true );
    Lane g = fromString( "x,10:5,,3:4,10:6,-4:1,20" );
    CHECK_EQ( g.size(), 2 );
    CHECK_EQ( g[0].tick, 3 );
    CHECK_EQ( g[1].tick, 10 );
    CHECK_EQ( g[1].value, 6 );
    CHECK_EQ( fromString( "" ).size(), 0 );
    CHECK_EQ( clampMode( 7 ), Write );
    CHECK_EQ( clampMode( -1 ), Off );

    // --- PAN lane ---
    CHECK_EQ( (int)Level, 0 ); // persisted ids: never reorder
    CHECK_EQ( (int)Pan, 1 );
    CHECK_EQ( (int)TouchX, 2 ); // XY FX gesture + insert lanes: persisted ids, never reorder
    CHECK_EQ( (int)TouchY, 3 );
    CHECK_EQ( (int)InsertParam0, 4 );
    CHECK_EQ( (int)InsertParam3, 7 );
    CHECK_EQ( (int)TouchGate, 8 );
    CHECK_EQ( (int)Count, 9 );
    CHECK_EQ( kNumFxLanes, 7 );
    CHECK_EQ( fxLaneIndex( TouchX ), 0 );
    CHECK_EQ( fxLaneIndex( TouchGate ), 6 );
    CHECK_EQ( fxLaneIndex( Pan ), -1 );
    CHECK_EQ( fxLaneIndex( Count ), -1 );
    CHECK_EQ( isInsertLane( InsertParam2 ), true );
    CHECK_EQ( isInsertLane( TouchGate ), false );
    CHECK_EQ( insertLaneSlot( InsertParam3 ), 3 );
    CHECK_EQ( insertLaneParam( 1 ), (int)InsertParam1 );
    CHECK_EQ( insertLaneParam( 4 ), -1 );
    CHECK_EQ( gateOpen( 1000 ), true );
    CHECK_EQ( gateOpen( 0 ), false );
    CHECK_EQ( normToValue( 0.5 ), 500 );
    CHECK_EQ( normToValue( -3.0 ), 0 );
    CHECK_EQ( normToValue( 9.0 ), 1000 );
    CHECK_EQ( normToValue( 0.0 / 0.0 ), 0 );
    for( int v = 0; v <= 1000; ++v )
        CHECK_EQ( normToValue( valueToNorm( v ) ), v );
    CHECK_EQ( panToValue( 0.0 ), 0 );
    CHECK_EQ( panToValue( -1.0 ), -1000 );
    CHECK_EQ( panToValue( 1.0 ), 1000 );
    CHECK_EQ( panToValue( 0.25 ), 250 );
    CHECK_EQ( panToValue( -0.0004 ), 0 );
    CHECK_EQ( panToValue( -0.0006 ), -1 );
    CHECK_EQ( panToValue( 7.0 ), 1000 );   // clamped
    CHECK_EQ( panToValue( -7.0 ), -1000 );
    CHECK_EQ( panToValue( 0.0 / 0.0 ), 0 ); // NaN -> centre
    CHECK_EQ( valueToPan( 500 ) == 0.5, true );
    CHECK_EQ( valueToPan( -5000 ) == -1.0, true );
    for( int v = -1000; v <= 1000; ++v ) // round trip is exact
        CHECK_EQ( panToValue( valueToPan( v ) ), v );

    // backward compatible persistence: a node without autoParam is a Level lane
    CHECK_EQ( paramFromStored( false, 0 ), (int)Level );
    CHECK_EQ( paramFromStored( false, 1 ), (int)Level ); // value ignored when the property is absent
    CHECK_EQ( paramFromStored( true, 0 ), (int)Level );
    CHECK_EQ( paramFromStored( true, 1 ), (int)Pan );
    CHECK_EQ( paramFromStored( true, 2 ), (int)TouchX );
    CHECK_EQ( paramFromStored( true, 8 ), (int)TouchGate );
    CHECK_EQ( paramFromStored( true, 9 ), -1 );  // unknown (future) param: not ours
    CHECK_EQ( paramFromStored( true, -1 ), -1 );

    // negative pan values survive the text form; interpolation through the centre
    Lane pl;
    setPoint( pl, 0, -1000 );
    setPoint( pl, 96, 1000 );
    setPoint( pl, 192, 0 );
    CHECK_EQ( toString( pl ) == "0:-1000,96:1000,192:0", true );
    CHECK_EQ( fromString( toString( pl ) ) == pl, true );
    CHECK_EQ( valueAt( pl, 48, 0 ), 0 );
    CHECK_EQ( valueAt( pl, 144, 0 ), 500 );
    CHECK_EQ( valueAt( pl, 500, 0 ), 0 );
    // an old project (Level-only text) parses unchanged
    CHECK_EQ( fromString( "0:100,96:127" ).size(), 2 );

    // recording a pan sweep thins like level
    Lane        rp;
    RecState    rs;
    CHECK_EQ( (int)recordMove( rp, rs, 10, panToValue( -0.5 ) ), (int)RecResult::Recorded );
    CHECK_EQ( (int)recordMove( rp, rs, 10, panToValue( -0.5 ) ), (int)RecResult::Unchanged );
    CHECK_EQ( (int)recordMove( rp, rs, 12, panToValue( -0.4 ) ), (int)RecResult::Recorded ); // < kMinGapTicks: replaces tick 10
    CHECK_EQ( rp.size(), 1 );
    CHECK_EQ( rp[0].tick, 12 );
    CHECK_EQ( rp[0].value, -400 );

    // deterministic owner: lowest order wins, ties -> first listed, none -> -1
    {
        const int o1[] = { 2003, 1002, 1005 };
        CHECK_EQ( pickPanOwner( o1, 3 ), 1 );
        const int o2[] = { 5, 5 };
        CHECK_EQ( pickPanOwner( o2, 2 ), 0 );
        CHECK_EQ( pickPanOwner( o2, 0 ), -1 );
    }
    CHECK_EQ( kNoValue < -kPanScale, true ); // sentinel can never be a lane value

    if( g_fail )
    {
        std::printf( "%d failure(s)\n", g_fail );
        return 1;
    }
    std::printf( "automation: all tests passed\n" );
    return 0;
}
