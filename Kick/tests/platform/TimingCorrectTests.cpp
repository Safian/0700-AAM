// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free timing-correct logic. Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../modules/engine/audio_engine/engine/mt_timing_correct.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                           \
    do {                                                                                                           \
        long long va = (long long)( a ), vb = (long long)( b );                                                    \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )

using namespace mt::tc;

int main()
{
    Params p; // 1/16 = 24 ticks, straight, full strength
    CHECK_EQ( snapTick( 0, p ), 0 );
    CHECK_EQ( snapTick( 11, p ), 0 );
    CHECK_EQ( snapTick( 12, p ), 0 );  // tie -> earlier
    CHECK_EQ( snapTick( 13, p ), 24 );
    CHECK_EQ( snapTick( 100, p ), 96 );
    p.gridTicks = 0;
    CHECK_EQ( snapTick( 13, p ), 13 ); // no grid -> untouched

    // Strength 50 / 0
    p = Params{};
    p.strengthPct = 50;
    CHECK_EQ( snapTick( 20, p ), 22 ); // 20 -> 24, halfway = 22
    p.strengthPct = 0;
    CHECK_EQ( snapTick( 20, p ), 20 );

    // Window: 50% of half a step = 6 ticks
    p = Params{};
    p.windowPct = 50;
    CHECK_EQ( snapTick( 5, p ), 0 );   // 5 <= 6 -> moved
    CHECK_EQ( snapTick( 8, p ), 8 );   // 8 > 6 -> untouched
    p.windowPct = 0;
    CHECK_EQ( snapTick( 1, p ), 1 );
    CHECK_EQ( snapTick( 0, p ), 0 );

    // Swing 66% at 1/8 (48 ticks): odd points delayed by 15
    p = Params{};
    p.gridTicks = 48;
    p.swingPct  = 66;
    CHECK_EQ( gridPoint( 1, 48, 66 ), 48 + 15 ); // (66-50)/50*48 = 15.36 -> 15
    p.swingPct  = 75;
    CHECK_EQ( gridPoint( 1, 48, 75 ), 48 + 24 );
    CHECK_EQ( gridPoint( 2, 48, 75 ), 96 );
    CHECK_EQ( snapTick( 70, p ), 72 );   // nearest of 48 / 72 / 96
    CHECK_EQ( snapTick( 90, p ), 96 );
    p.swingPct = 50;
    CHECK_EQ( snapTick( 70, p ), 48 );   // |70-48|=22 < |70-96|=26

    // Shift
    p = Params{};
    p.shiftTicks = 3;
    CHECK_EQ( snapTick( 25, p ), 27 );
    p.shiftTicks = -30;
    CHECK_EQ( snapTick( 10, p ), 0 );    // clamped at 0

    // Start mode keeps length; wrap at track end
    p = Params{};
    std::vector<Note> v{ { 36, 13, 10 }, { 38, 95, 5 } };
    CHECK_EQ( correct( v, p, 96 ), 2 );
    CHECK_EQ( v[0].start, 24 );
    CHECK_EQ( v[0].length, 10 );
    CHECK_EQ( v[1].start, 0 ); // 95 -> 96 -> wraps to 0
    // Not-target notes are left alone
    v = { { 36, 13, 10, false } };
    CHECK_EQ( correct( v, p, 96 ), 0 );
    CHECK_EQ( v[0].start, 13 );

    // End mode: start fixed, end snapped, min length 1
    p.type = Type::End;
    v      = { { 36, 4, 18 }, { 36, 0, 2 } };
    correct( v, p, 0 );
    CHECK_EQ( v[0].start, 4 );
    CHECK_EQ( v[0].length, 20 ); // end 22 -> 24
    CHECK_EQ( v[1].length, 1 );  // end 2 -> 0 -> length clamped to 1

    // Length mode: multiples of the grid, min one step
    p.type = Type::Length;
    v      = { { 36, 0, 30 }, { 36, 100, 3 }, { 36, 200, 40 } };
    correct( v, p, 0 );
    CHECK_EQ( v[0].length, 24 );
    CHECK_EQ( v[1].length, 24 );
    CHECK_EQ( v[2].length, 48 );

    // Legato: to the next note of the same pitch; last one untouched; other pitch ignored
    p.type = Type::Legato;
    v      = { { 36, 0, 5 }, { 40, 10, 5 }, { 36, 48, 5 }, { 36, 100, 7 } };
    CHECK_EQ( correct( v, p, 0 ), 2 );
    CHECK_EQ( v[0].length, 48 );
    CHECK_EQ( v[1].length, 5 );
    CHECK_EQ( v[2].length, 52 );
    CHECK_EQ( v[3].length, 7 );

    // Idempotent on the grid
    p = Params{};
    v = { { 36, 24, 24 }, { 36, 48, 24 } };
    CHECK_EQ( correct( v, p, 96 ), 0 );

    if( g_fail ) { std::printf( "%d failure(s)\n", g_fail ); return 1; }
    std::printf( "timing-correct tests passed\n" );
    return 0;
}
