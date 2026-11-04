// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free Looper logic (kick_LooperLogic.h).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined LooperLogicTests.cpp
#include "../../modules/kick_ui/audio_components/looper/kick_LooperLogic.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK( a )                                                                      \
    do {                                                                                \
        if( !( a ) ) { std::printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #a ); ++g_fail; } \
    } while( 0 )

using namespace kick_ui::looper_logic;

int main()
{
    // 100 BPM, 4 bars = 9.6 s: float storage of the engine vs the double of the tempo manager (the L3 bug)
    const double secs = 9.6;
    const float  stored = static_cast<float>( secs );
    CHECK( static_cast<double>( stored ) != secs ); // the old exact comparison fails
    CHECK( sameLoopLength( secs, static_cast<double>( stored ), 48000.0 ) );
    CHECK( sameLoopLength( secs, static_cast<double>( stored ), 44100.0 ) );
    CHECK( !sameLoopLength( 9.6, 9.7, 48000.0 ) );
    CHECK( !sameLoopLength( 2.0, 4.0, 48000.0 ) );
    CHECK( framesOf( 2.0, 48000.0 ) == 96000 );
    CHECK( framesOf( 0.0, 48000.0 ) == 0 );
    CHECK( framesOf( -1.0, 48000.0 ) == 0 );
    CHECK( framesOf( 1.0, 0.0 ) == 0 );

    CHECK( loopSeconds( true, 4.8 ) == 4.8 );
    CHECK( loopSeconds( false, 4.8 ) == 0.0 );
    CHECK( loopSeconds( true, 0.0 ) == 0.0 );

    // hardware-recorded loops enable Export / Clear too (L7)
    CHECK( canExportOrClear( false, false ) );
    CHECK( !canExportOrClear( true, false ) );
    CHECK( !canExportOrClear( false, true ) );

    CHECK( needsClearConfirm( false, 4, 8 ) );
    CHECK( !needsClearConfirm( true, 4, 8 ) );
    CHECK( !needsClearConfirm( false, 4, 4 ) );

    // ---- LO-09: Sequence time signature and beats calculation
    CHECK( calculateTotalBeats( 4, 4 ) == 16 );
    CHECK( calculateTotalBeats( 2, 3 ) == 6 );
    CHECK( calculateTotalBeats( 4, 6 ) == 24 );
    CHECK( calculateTotalBeats( 0, 4 ) == 4 ); // clamped to >= 1

    CHECK( std::fabs( calculateBeatWidth( 800.0f, 16 ) - 50.0f ) < 1e-4f );
    CHECK( calculateBeatWidth( 0.0f, 16 ) == 0.0f );
    CHECK( calculateBeatWidth( 800.0f, 0 ) == 0.0f );

    // Safe PPQ resolution (no ARM zero-division crash)
    CHECK( safePpqResolution( 4 ) == 480 );
    CHECK( safePpqResolution( 2 ) == 960 );
    CHECK( safePpqResolution( 0 ) == 960 );
    CHECK( safePpqResolution( -1 ) == 960 );

    if( g_fail == 0 )
        std::printf( "LooperLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
