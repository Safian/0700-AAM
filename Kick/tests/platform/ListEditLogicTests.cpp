// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free List Edit logic (kick_ListEditLogic.h).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined ListEditLogicTests.cpp
#include "../../modules/kick_ui/modes/kick_ListEditLogic.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                                   \
    do {                                                                                                                   \
        long long va = (long long)( a ), vb = (long long)( b );                                                            \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )
#define CHECK( a ) CHECK_EQ( ( a ) ? 1 : 0, 1 )

using namespace kick_ui::list_edit_logic;

int main()
{
    // Time formatting: bar/beat 1-based, tick 0-based; the grid position is 01:01:00.
    CHECK( formatTime( { 1, 1, 0 } ) == "01:01:00" );
    CHECK( formatTime( { 12, 4, 95 } ) == "12:04:95" );

    // Insert BAR 1 / BEAT 1 / TICK 0 lands on the grid (LE-05: it used to land on tick 1).
    CHECK_EQ( bbtToTicks( 0, 96, { 1, 1, 0 } ), 0 );
    CHECK_EQ( bbtToTicks( 0, 96, { 1, 1, 1 } ), 1 );
    CHECK_EQ( bbtToTicks( 384, 96, { 2, 3, 10 } ), 384 + 192 + 10 );
    CHECK_EQ( bbtToTicks( 0, 96, { 1, 0, -5 } ), 0 ); // out-of-range inputs are clamped
    CHECK_EQ( maxTick( 96 ), 95 );
    CHECK_EQ( maxTick( 0 ), 0 );

    // list time -> dialog -> list round trip
    for( int t : { 0, 1, 95, 96, 200, 383 } )
    {
        auto b = ticksToBbt( t, 96 );
        CHECK_EQ( bbtToTicks( 0, 96, b ), t );
    }
    CHECK_EQ( ticksToBbt( -1, 96 ).beat, 1 );

    // Names
    CHECK( noteName( 0 ) == "C-2" );
    CHECK( noteName( 60 ) == "C3" );
    CHECK( noteName( 127 ) == "G8" );
    CHECK( noteName( 61 ) == "C#3" );
    CHECK( noteName( 128 ) == "-" );
    CHECK( padName( 0 ) == "A01" );
    CHECK( padName( 127 ) == "H16" );
    CHECK( padName( 17 ) == "B02" );
    CHECK( padName( -1 ) == "-" );

    // Delete rules (LE-04): nothing selected deletes nothing.
    CHECK( !canDeleteRow( 5, -1 ) );
    CHECK( !canDeleteRow( 0, 0 ) );
    CHECK( !canDeleteRow( 5, 5 ) );
    CHECK( canDeleteRow( 5, 0 ) );
    auto rows = rowsToDelete( { 1, 3, 3, 9, -1, 0 }, 5 );
    CHECK_EQ( rows.size(), 3 ); CHECK_EQ( rows[0], 3 ); CHECK_EQ( rows[1], 1 ); CHECK_EQ( rows[2], 0 );
    CHECK_EQ( selectionAfterDelete( 5, 1, 1 ), 1 );
    CHECK_EQ( selectionAfterDelete( 5, 4, 1 ), 3 );
    CHECK_EQ( selectionAfterDelete( 2, 0, 2 ), -1 );
    CHECK_EQ( selectionAfterDelete( 5, 1, 3 ), 1 );

    // Dial stepping (LE-03)
    CHECK_EQ( stepRow( -1, 1, 10 ), 0 );
    CHECK_EQ( stepRow( -1, -1, 10 ), 9 );
    CHECK_EQ( stepRow( 0, 1, 10 ), 1 );
    CHECK_EQ( stepRow( 9, 3, 10 ), 9 );
    CHECK_EQ( stepRow( 0, -3, 10 ), 0 );
    CHECK_EQ( stepRow( 2, 1, 0 ), -1 );

    // Cell ranges: no dead notes, ratchet 0 is only displayed clamped
    CHECK_EQ( clampCell( Cell::Velocity, 0 ), 1 );
    CHECK_EQ( clampCell( Cell::Length, 0 ), 1 );
    CHECK_EQ( clampCell( Cell::Length, 20000 ), 9999 );
    CHECK_EQ( clampCell( Cell::Probability, -3 ), 0 );
    CHECK_EQ( clampCell( Cell::Ratchet, 0 ), 1 );
    CHECK_EQ( clampCell( Cell::Pad, 200 ), 127 );
    CHECK_EQ( displayValue( Cell::Ratchet, 0 ), 1 );
    Cell c;
    CHECK( qlinkCell( 1, c ) && c == Cell::Pad );
    CHECK( qlinkCell( 2, c ) && c == Cell::Length );
    CHECK( qlinkCell( 3, c ) && c == Cell::Velocity );
    CHECK( qlinkCell( 4, c ) && c == Cell::Probability );
    CHECK( !qlinkCell( 5, c ) );
    CHECK_EQ( velocityDelta( 120, 20 ), 127 );
    CHECK_EQ( velocityDelta( 5, -20 ), 1 );

    // Auto scroll pauses after a touch
    CHECK( autoScrollAllowed( 100, 0 ) );
    CHECK( !autoScrollAllowed( 1000, 500 ) );
    CHECK( autoScrollAllowed( 5000, 500 ) );

    if( g_fail == 0 )
        std::printf( "ListEditLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
