// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the Humanize arithmetic.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t HumanizeTests.cpp
#include "../../modules/engine/audio_engine/engine/mt_humanize.h"
#include <cstdio>
#include <cstdlib>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace mt::humanize;

int main()
{
    // The old bug: a note at tick 96 with 30 pulses collapsed to ~5. Now it stays within +-30 of 96.
    for( int i = 0; i < 1000; ++i )
    {
        const double u = i / 1000.0;
        const int    s = humanizedStart( 96, 30, 0, -1, u );
        CHECK( s >= 66 && s <= 126 );
    }
    CHECK( timeShift( 30, 0, 0.0 ) == -30 );
    CHECK( timeShift( 30, 0, 0.5 ) == 0 );
    CHECK( timeShift( 30, 0, 0.999999 ) == 30 );
    CHECK( timeShift( 0, 0, 0.7 ) == 0 );           // no amount, no move
    CHECK( timeShift( -5, 0, 0.7 ) == 0 );          // negative amount is treated as 0

    // Eagerness: +50 = only later, -50 = only earlier, symmetric order of magnitude.
    for( int i = 0; i < 1000; ++i )
    {
        const double u = i / 1000.0;
        CHECK( timeShift( 30, 50, u ) >= 0 && timeShift( 30, 50, u ) <= 30 );
        CHECK( timeShift( 30, -50, u ) <= 0 && timeShift( 30, -50, u ) >= -30 );
        const int mid = timeShift( 30, 25, u ); // half biased: -15..30
        CHECK( mid >= -15 && mid <= 30 );
    }
    CHECK( timeShift( 30, 999, 0.0 ) == 0 );        // eagerness clamped to +50
    CHECK( timeShift( 30, -999, 0.999999 ) == 0 );  // eagerness clamped to -50

    // Never leaves the track.
    CHECK( humanizedStart( 5, 30, 0, 1000, 0.0 ) == 0 );          // would be -25
    CHECK( humanizedStart( 990, 30, 0, 1000, 0.999999 ) == 1000 ); // would be 1020
    CHECK( humanizedStart( 0, 30, 0, -1, 0.0 ) == 0 );

    // Length / velocity: shrink only, never below 1, never above max.
    CHECK( shrunkValue( 96, 100, 1.0 - 1e-9, kMaxVelocity ) >= 1 );
    CHECK( shrunkValue( 96, 100, 0.999999, 100000 ) == 1 );
    CHECK( shrunkValue( 100, 20, 0.0, kMaxVelocity ) == 100 );    // u = 0: unchanged
    CHECK( shrunkValue( 100, 20, 0.5, kMaxVelocity ) == 90 );     // 100 * (1 - 0.1)
    CHECK( shrunkValue( 1, 100, 0.9, kMaxVelocity ) == 1 );       // a velocity of 1 stays 1, not 0 (note-off)
    CHECK( shrunkValue( 127, 0, 0.9, kMaxVelocity ) == 127 );     // strength 0: unchanged
    CHECK( shrunkValue( 200, 0, 0.0, kMaxVelocity ) == 127 );     // clamped to the MIDI range
    CHECK( shrunkValue( 50, 500, 0.5, kMaxVelocity ) >= 1 );      // strength clamped to 100

    if( g_fail == 0 ) std::printf( "HumanizeTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
