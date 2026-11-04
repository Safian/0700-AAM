// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free step sequencer logic (kick_StepModel.h).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined StepModelTests.cpp
#include "../../modules/kick_ui/audio_components/step_sequencer/kick_StepModel.h"
#include <algorithm>
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

    // ---- SS-17: non-4/4 time signatures (PPQN 96: quarter 96 ticks, 1/16 = 24; bar = num * 384 / den)
    {
        struct TS { int num, den; };
        const TS sigs[] = { { 2, 4 }, { 3, 4 }, { 5, 4 }, { 6, 8 }, { 7, 8 }, { 12, 8 }, { 7, 4 }, { 9, 8 } };
        const int stepTicks[] = { 96, 48, 24, 12, 6 }; // 1/4, 1/8, 1/16, 1/32, 1/64
        for( auto& t : sigs )
        {
            const int bar = t.num * 384 / t.den;
            for( int st : stepTicks )
            {
                const int steps = stepsPerBar( bar, st, 16 );
                CHECK_EQ( steps, bar / st );                      // the bar length decides, never the enum value
                CHECK( steps >= 1 );
                CHECK_EQ( steps * st + uncoveredTicks( bar, st, 16 ), bar );
                // every step of the bar maps to a page and back; the last step exists, the next one does not
                const int perPage = std::min( 16, steps );
                CHECK( stepExists( steps - 1, steps ) );
                CHECK( !stepExists( steps, steps ) );
                CHECK_EQ( pageCount( steps, perPage ), ( steps + perPage - 1 ) / perPage );
                CHECK_EQ( absoluteStep( pageCount( steps, perPage ) - 1, perPage, 0 ), ( pageCount( steps, perPage ) - 1 ) * perPage );
                // a note that starts inside the last step belongs to it, one at the next bar's start does not
                CHECK( noteStartsInStep( ( steps - 1 ) * st + st - 1, ( steps - 1 ) * st, st ) );
                CHECK( !noteStartsInStep( steps * st, ( steps - 1 ) * st, st ) );
                // the playhead of the bar after the first one (bars have the SAME length here: an even-bar project)
                CHECK_EQ( playStep( bar + 5, bar, st, steps ), 0 );
                CHECK_EQ( playStep( bar + steps * st - 1, bar, st, steps ), steps - 1 );
                CHECK_EQ( playStep( 2 * bar - 1, bar, st, steps ), uncoveredTicks( bar, st, 16 ) > 0 ? -1 : steps - 1 ); // the unreachable tail shows no step
                CHECK_EQ( playStep( 2 * bar, bar, st, steps ), -1 );
            }
        }
        // 3/4 at 1/16: 12 steps, a single page; 5/4 at 1/16: 20 steps, 2 pages (the second one has 4)
        CHECK_EQ( stepsPerBar( 288, 24, 16 ), 12 );
        CHECK_EQ( pageCount( 12, 12 ), 1 );
        CHECK_EQ( stepsPerBar( 480, 24, 16 ), 20 );
        CHECK_EQ( pageCount( 20, 16 ), 2 );
        CHECK( stepExists( 19, 20 ) && !stepExists( 20, 20 ) );
        // 7/8 at 1/4 notes leaves the last eighth unreachable (3 steps = 288 of 336 ticks); 1/8 covers the bar
        CHECK_EQ( stepsPerBar( 336, 96, 16 ), 3 );
        CHECK_EQ( uncoveredTicks( 336, 96, 16 ), 48 );
        CHECK_EQ( uncoveredTicks( 336, 48, 16 ), 0 );
        // bars of different length in one sequence (a 4/4 bar then a 3/4 bar): each bar is measured on its own
        CHECK_EQ( stepsPerBar( 384, 24, 16 ), 16 );
        CHECK_EQ( stepsPerBar( 288, 24, 16 ), 12 );
        CHECK_EQ( playStep( 384 + 287, 384, 24, 12 ), 11 );
        CHECK_EQ( playStep( 384 + 288, 384, 24, 12 ), -1 );
        // triplets: 1/16T = 16 ticks -> 24 steps in 4/4, 18 in 3/4
        CHECK_EQ( stepsPerBar( 384, 16, 16 ), 24 );
        CHECK_EQ( stepsPerBar( 288, 16, 16 ), 18 );
    }

    // ---- step entry velocity (settings window): Auto keeps each view's default
    CHECK_EQ( entryVelocity( 0, false, kClassicDefaultVelocity, 55, false ), 127 );
    CHECK_EQ( entryVelocity( 0, false, kGridDefaultVelocity, 55, false ), 100 );
    CHECK_EQ( entryVelocity( 90, false, kClassicDefaultVelocity, 55, false ), 90 );   // the setting applies to both views
    CHECK_EQ( entryVelocity( 90, false, kGridDefaultVelocity, 55, false ), 90 );
    CHECK_EQ( entryVelocity( 90, true, kClassicDefaultVelocity, 55, false ), 55 );    // pad velocity switch
    CHECK_EQ( entryVelocity( 90, false, kClassicDefaultVelocity, 55, true ), 55 );    // user level
    CHECK_EQ( entryVelocity( 0, true, kClassicDefaultVelocity, 0, false ), 1 );       // never 0
    CHECK_EQ( entryVelocity( 500, false, kClassicDefaultVelocity, 0, false ), 127 );
    // Q-Link / dial below the lowest velocity deletes the step
    CHECK( velocityTurnDeletes( 1, -1 ) );
    CHECK( velocityTurnDeletes( 3, -5 ) );
    CHECK( !velocityTurnDeletes( 3, -2 ) );
    CHECK( !velocityTurnDeletes( 1, 1 ) );
    CHECK( !velocityTurnDeletes( 127, 5 ) );

    if( g_fail == 0 )
        std::printf( "StepModelTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
