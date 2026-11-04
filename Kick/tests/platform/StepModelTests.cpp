// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free step sequencer logic (kick_StepModel.h).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined StepModelTests.cpp
#include "../../modules/kick_ui/audio_components/step_sequencer/kick_StepModel.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                                   \
    do {                                                                                                                   \
        long long va = (long long)( a ), vb = (long long)( b );                                                            \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )
#define CHECK( a ) CHECK_EQ( ( a ) ? 1 : 0, 1 )

using namespace kick_ui::step_model;

int main()
{
    // Page count rounds UP (SS-06): 16T = 24 steps, 16 per page -> 2 pages; bank button B must step.
    CHECK_EQ( pageCount( 16, 16 ), 1 );
    CHECK_EQ( pageCount( 24, 16 ), 2 );
    CHECK_EQ( pageCount( 32, 16 ), 2 );
    CHECK_EQ( pageCount( 64, 16 ), 4 );
    CHECK_EQ( pageCount( 6, 6 ), 1 );
    CHECK_EQ( pageCount( 12, 12 ), 1 );
    CHECK_EQ( pageCount( 0, 16 ), 1 );
    CHECK_EQ( pageCount( 16, 0 ), 1 );
    CHECK( bankButtonSelectsPage( 0, 24, 16 ) );
    CHECK( bankButtonSelectsPage( 1, 24, 16 ) );
    CHECK( !bankButtonSelectsPage( 2, 24, 16 ) );
    CHECK( !bankButtonSelectsPage( 1, 16, 16 ) );
    CHECK( !bankButtonSelectsPage( -1, 16, 16 ) );

    // Page relative pad / Q-Link (SS-02): page 1 of 32 steps, pad 3 -> step 19.
    CHECK_EQ( absoluteStep( 0, 16, 3 ), 3 );
    CHECK_EQ( absoluteStep( 1, 16, 3 ), 19 );
    CHECK_EQ( absoluteStep( 1, 0, 3 ), 4 ); // degenerate page size behaves like 1
    CHECK( stepExists( 23, 24 ) );
    CHECK( !stepExists( 24, 24 ) ); // pad past the end of the last, partial page
    CHECK( !stepExists( -1, 24 ) );

    // A step's note starts inside it (SS-11): an earlier long note only overlaps.
    CHECK( noteStartsInStep( 96, 96, 24 ) );
    CHECK( noteStartsInStep( 119, 96, 24 ) );
    CHECK( !noteStartsInStep( 120, 96, 24 ) );
    CHECK( !noteStartsInStep( 90, 96, 24 ) );
    CHECK_EQ( stepStartTick( 1536, 3, 96 ), 1536 + 288 );

    // Nudge wraps both ways, N steps (SS-16).
    CHECK_EQ( nudgedStart( 0, false, 1, 96, 1536 ), 1536 - 96 );
    CHECK_EQ( nudgedStart( 1440, true, 1, 96, 1536 ), 0 );
    CHECK_EQ( nudgedStart( 0, true, 3, 96, 1536 ), 288 );
    CHECK_EQ( nudgedStart( 96, false, 3, 96, 1536 ), 1536 - 192 );
    CHECK_EQ( nudgedStart( 96, true, 0, 96, 1536 ), 192 ); // 0 steps counts as 1
    CHECK_EQ( nudgedStart( 100, true, 1, 96, 0 ), 196 );   // no total: no wrap
    CHECK_EQ( wrapTick( -1, 100 ), 99 );
    CHECK_EQ( wrapTick( 100, 100 ), 0 );
    CHECK_EQ( turnSteps( 0 ), 1 );
    CHECK_EQ( turnSteps( -4 ), 4 );
    CHECK_EQ( turnSteps( 3 ), 3 );

    // Value ranges (SS-04): velocity never 0.
    CHECK_EQ( clampParam( Param::Velocity, 0 ), 1 );
    CHECK_EQ( clampParam( Param::Velocity, -20 ), 1 );
    CHECK_EQ( clampParam( Param::Velocity, 300 ), 127 );
    CHECK_EQ( clampParam( Param::Probability, 0 ), 0 ); // 0% is a legal value
    CHECK_EQ( clampParam( Param::Probability, 101 ), 100 );
    CHECK_EQ( clampParam( Param::Ratchet, 0 ), 1 );
    CHECK_EQ( clampParam( Param::Ratchet, 9 ), 8 );
    CHECK_EQ( applyOffset( Param::Velocity, 10, -64 ), 1 );
    CHECK_EQ( applyOffset( Param::Velocity, 100, 64 ), 127 );
    CHECK_EQ( applyOffset( Param::Probability, 10, -64 ), 0 );
    CHECK_EQ( inverseValue( Param::Velocity, 127 ), 1 );
    CHECK_EQ( inverseValue( Param::Velocity, 1 ), 127 );
    CHECK_EQ( inverseValue( Param::Velocity, 64 ), 64 );
    CHECK_EQ( inverseValue( Param::Probability, 100 ), 0 );
    CHECK_EQ( inverseValue( Param::Ratchet, 1 ), 8 );
    CHECK_EQ( inverseValue( Param::Ratchet, 8 ), 1 );

    // Preset "off": velocity/ratchet below minimum delete the note; probability 0 is written.
    CHECK( presetValueMeansOff( Param::Velocity, 0 ) );
    CHECK( !presetValueMeansOff( Param::Velocity, 1 ) );
    CHECK( presetValueMeansOff( Param::Ratchet, 0 ) );
    CHECK( !presetValueMeansOff( Param::Probability, 0 ) );

    // Average of several notes in one step.
    CHECK_EQ( averageValue( Param::Velocity, { 100, 50 } ), 75 );
    CHECK_EQ( averageValue( Param::Velocity, { 0 } ), 1 );
    CHECK_EQ( averageValue( Param::Velocity, {} ), 127 );
    CHECK_EQ( averageValue( Param::Probability, {} ), 100 );
    CHECK_EQ( averageValue( Param::Ratchet, {} ), 1 );
    CHECK_EQ( averageValue( Param::Ratchet, { 8, 8, 8 } ), 8 );

    // Steps per bar from the real bar length (SS-17): 4/4 and 3/4 at 1/16 (PPQN 96 -> 24 ticks/step).
    CHECK_EQ( stepsPerBar( 384, 24, 16 ), 16 );
    CHECK_EQ( stepsPerBar( 288, 24, 16 ), 12 );
    CHECK_EQ( stepsPerBar( 336, 24, 16 ), 14 ); // 7/4
    CHECK_EQ( stepsPerBar( 0, 24, 16 ), 16 );
    CHECK_EQ( stepsPerBar( 384, 0, 16 ), 16 );
    CHECK_EQ( stepsPerBar( 10, 24, 16 ), 1 );

    // Time division list index (SS-05): every stored quantization maps; 32T used to fall to 1/16.
    CHECK_EQ( timeDivIndex( 1 ), 0 );
    CHECK_EQ( timeDivIndex( 2 ), 0 );
    CHECK_EQ( timeDivIndex( 3 ), 0 );
    CHECK_EQ( timeDivIndex( 4 ), 1 );
    CHECK_EQ( timeDivIndex( 6 ), 2 );
    CHECK_EQ( timeDivIndex( 8 ), 3 );
    CHECK_EQ( timeDivIndex( 12 ), 4 );
    CHECK_EQ( timeDivIndex( 16 ), 5 );
    CHECK_EQ( timeDivIndex( 24 ), 6 );
    CHECK_EQ( timeDivIndex( 32 ), 7 );
    CHECK_EQ( timeDivIndex( 48 ), 8 );
    CHECK_EQ( timeDivIndex( 78 ), 8 );
    CHECK_EQ( timeDivIndex( 64 ), 9 );
    CHECK_EQ( timeDivIndex( 96 ), 10 );
    CHECK_EQ( timeDivIndex( 999 ), 5 );
    CHECK( !timeDivListHasQuantization( 2 ) );
    CHECK( !timeDivListHasQuantization( 3 ) );
    CHECK( timeDivListHasQuantization( 48 ) );
    // The list index and the engine value round-trip for every entry the list has.
    const int listValues[] = { 1, 4, 6, 8, 12, 16, 24, 32, 48, 64, 96 };
    for( int i = 0; i < 11; ++i )
    {
        CHECK_EQ( timeBaseValue( i ), listValues[i] );
        CHECK_EQ( timeDivIndex( timeBaseValue( i ) ), i );
    }

    // Play position and page following (SS-21).
    CHECK_EQ( playStep( 1536, 1536, 96, 16 ), 0 );
    CHECK_EQ( playStep( 1536 + 100, 1536, 96, 16 ), 1 );
    CHECK_EQ( playStep( 1536 + 96 * 16, 1536, 96, 16 ), -1 ); // next bar
    CHECK_EQ( playStep( 10, 1536, 96, 16 ), -1 );             // earlier bar
    CHECK_EQ( playStep( 10, 0, 0, 16 ), -1 );
    CHECK_EQ( pageOfStep( 15, 16 ), 0 );
    CHECK_EQ( pageOfStep( 16, 16 ), 1 );
    CHECK_EQ( pageOfStep( 20, 0 ), 20 );

    if( g_fail == 0 )
        std::printf( "StepModelTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
