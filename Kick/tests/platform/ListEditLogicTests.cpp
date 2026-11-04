// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free List Edit logic (kick_ListEditLogic.h).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined ListEditLogicTests.cpp
#include "../../modules/kick_ui/modes/kick_ListEditLogic.h"
#include <cstdio>
#include <string>

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

    // ---- event rows: notes and automation breakpoints merged by time (LE-21), virtual 3000-event list (LE-07)
    {
        std::vector<int> notes{ 0, 96, 96, 384 }, autos{ 0, 96, 200 };
        auto all = mergeEvents( notes, autos, EventFilter::All );
        CHECK_EQ( all.size(), 7 );
        CHECK( !all[0].automation && all[0].tick == 0 && !all[0].sameTime ); // note first at equal ticks
        CHECK( all[1].automation && all[1].tick == 0 && all[1].sameTime );   // same position: time greyed
        CHECK( !all[2].automation && all[2].tick == 96 && !all[2].sameTime );
        CHECK( !all[3].automation && all[3].sameTime );
        CHECK( all[4].automation && all[4].tick == 96 && all[4].sameTime );
        CHECK( all[5].automation && all[5].tick == 200 );
        CHECK( !all[6].automation && all[6].tick == 384 && all[6].index == 3 );
        CHECK_EQ( mergeEvents( notes, autos, EventFilter::Notes ).size(), 4 );
        CHECK_EQ( mergeEvents( notes, autos, EventFilter::Automation ).size(), 3 );
        CHECK( mergeEvents( {}, {}, EventFilter::All ).empty() );
        // 3000 events: sorted, every note appears once
        std::vector<int> big, bigAuto;
        for( int i = 0; i < 3000; ++i ) big.push_back( i * 24 );
        for( int i = 0; i < 50; ++i ) bigAuto.push_back( i * 1440 + 7 );
        auto rows = mergeEvents( big, bigAuto, EventFilter::All );
        CHECK_EQ( rows.size(), 3050 );
        for( size_t i = 1; i < rows.size(); ++i ) CHECK( rows[i - 1].tick <= rows[i].tick );
        // playhead rows
        CHECK_EQ( rowAtPlayhead( all, -1 ), -1 );
        CHECK_EQ( rowAtPlayhead( all, 0 ), 1 );    // the last row starting at tick 0
        CHECK_EQ( rowAtPlayhead( all, 100 ), 4 );
        CHECK_EQ( rowAtPlayhead( all, 100000 ), 6 );
        CHECK_EQ( goToPlayheadRow( all, -5 ), 0 );
        CHECK_EQ( goToPlayheadRow( all, 250 ), 5 );
        CHECK_EQ( goToPlayheadRow( {}, 10 ), -1 );
    }
    // automation values: Level 0..127, Pan -100..100 percent
    CHECK( std::string( autoName( kAutoLevel ) ) == "Level" && std::string( autoName( kAutoPan ) ) == "Pan" );
    CHECK_EQ( autoDisplay( kAutoPan, -1000 ), -100 );
    CHECK_EQ( autoDisplay( kAutoPan, 500 ), 50 );
    CHECK_EQ( autoDisplay( kAutoPan, -504 ), -50 );
    CHECK_EQ( autoDisplay( kAutoLevel, 200 ), 127 );
    CHECK_EQ( autoFromDisplay( kAutoPan, 50 ), 500 );
    CHECK_EQ( autoFromDisplay( kAutoPan, -300 ), -1000 );
    CHECK_EQ( autoFromDisplay( kAutoLevel, -4 ), 0 );
    CHECK_EQ( autoMinShown( kAutoPan ), -100 ); CHECK_EQ( autoMaxShown( kAutoLevel ), 127 );
    for( int shown = -100; shown <= 100; ++shown ) CHECK_EQ( autoDisplay( kAutoPan, autoFromDisplay( kAutoPan, shown ) ), shown ); // round trip

    // group edits
    CHECK_EQ( nudgeStart( 10, -50, 1536 ), 0 );
    CHECK_EQ( nudgeStart( 1530, 50, 1536 ), 1535 );
    CHECK_EQ( nudgeStart( 100, 24, 1536 ), 124 );
    {
        auto sel = toggleRow( {}, 5 );
        sel = toggleRow( sel, 2 );
        CHECK_EQ( sel.size(), 2 ); CHECK_EQ( sel[0], 2 ); CHECK_EQ( sel[1], 5 );
        sel = toggleRow( sel, 2 );
        CHECK_EQ( sel.size(), 1 ); CHECK_EQ( sel[0], 5 );
        CHECK_EQ( selectionAfterGroupDelete( 10, { 4, 5, 6 } ), 4 );
        CHECK_EQ( selectionAfterGroupDelete( 10, { 7, 8, 9 } ), 6 );
        CHECK_EQ( selectionAfterGroupDelete( 3, { 0, 1, 2 } ), -1 );
        CHECK_EQ( selectionAfterGroupDelete( 3, {} ), -1 );
    }
    // step record: TC division, BAR (0) falls back, velocity, auto advance stays inside the sequence
    CHECK_EQ( stepRecordLength( 48, 96 ), 48 );
    CHECK_EQ( stepRecordLength( 0, 96 ), 96 );
    CHECK_EQ( stepRecordLength( 0, 0 ), 1 );
    CHECK_EQ( stepRecordVelocity( 0 ), 1 ); CHECK_EQ( stepRecordVelocity( 300 ), 127 );
    CHECK_EQ( stepRecordNextTick( 0, 48, 1536 ), 48 );
    CHECK_EQ( stepRecordNextTick( 1500, 96, 1536 ), 1536 );

    if( g_fail == 0 )
        std::printf( "ListEditLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
