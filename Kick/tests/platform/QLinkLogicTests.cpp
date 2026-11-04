// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free Q-Link Edit logic and Q-Link routing (kick_KnobLogic.h).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined QLinkLogicTests.cpp
#include "../../modules/kick_ui/modes/knobedit/kick_KnobLogic.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                                   \
    do {                                                                                                                   \
        long long va = (long long)( a ), vb = (long long)( b );                                                            \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )
#define CHECK( a ) CHECK_EQ( ( a ) ? 1 : 0, 1 )
#define CHECK_NEAR( a, b ) CHECK( std::fabs( ( a ) - ( b ) ) < 1e-9 )

using namespace kick_ui::knob_logic;

int main()
{
    // ---- title table: lists and handler share it
    CHECK( parseProgramParam( "Pan" ) == Param::Pan );
    CHECK( parseProgramParam( "Velocity" ) == Param::Level );
    CHECK( parseProgramParam( "Polyphony" ) == Param::Poly );
    CHECK( parseProgramParam( "Program Polyphony" ) == Param::Poly );
    CHECK( parseProgramParam( " Global SemiTune " ) == Param::Semi );
    CHECK( parseProgramParam( "GlobalFineTune" ) == Param::Fine );
    CHECK( parseProgramParam( "OFF" ) == Param::None );
    CHECK( parseProgramParam( "sends01" ) == Param::None );
    CHECK( parsePadParam( "PadMute" ) == Param::PadMute );
    CHECK( parsePadParam( "Level" ) == Param::PadLevel );
    CHECK( parsePadParam( "Glide All" ) == Param::None );
    CHECK( isSwitch( Param::Solo ) && isSwitch( Param::PadMute ) && !isSwitch( Param::Pan ) );

    // ---- QL-H2: one tick = one increment of the parameter, not the whole scale
    CHECK_NEAR( stepValue( Param::Pan, 0.0, 1 ), 0.01 );
    CHECK_NEAR( stepValue( Param::Pan, 0.0, -1 ), -0.01 );
    CHECK_NEAR( stepValue( Param::Pan, 0.99, 5 ), 1.0 );
    CHECK_NEAR( stepValue( Param::PadPan, -1.0, -3 ), -1.0 );
    double pan = 0;
    for( int i = 0; i < 30; ++i ) pan = stepValue( Param::Pan, pan, 1 );
    CHECK_NEAR( pan, 0.3 ); // no float drift
    CHECK_EQ( (int)stepValue( Param::Level, 100, 1 ), 101 );
    CHECK_EQ( (int)stepValue( Param::Level, 127, 9 ), 127 );
    CHECK_EQ( (int)stepValue( Param::Level, 2, -9 ), 0 );
    CHECK_EQ( (int)stepValue( Param::Semi, 12, 1 ), 12 );
    CHECK_EQ( (int)stepValue( Param::Fine, -98, -5 ), -99 );
    CHECK_EQ( (int)stepValue( Param::Poly, 1, -1 ), 1 );
    CHECK_EQ( (int)stepValue( Param::Poly, 63, 4 ), 64 );
    CHECK_NEAR( stepInsert( 0.5, 1 ), 0.51 );
    CHECK_NEAR( stepInsert( 0.995, 1 ), 1.0 );
    CHECK_NEAR( stepInsert( 0.0, -4 ), 0.0 );

    // ---- QL-15: acceleration keeps single ticks exact, grows with fast turns, keeps the sign
    CHECK_EQ( acceleratedCount( 1 ), 1 );
    CHECK_EQ( acceleratedCount( -1 ), -1 );
    CHECK_EQ( acceleratedCount( 0 ), 0 );
    CHECK_EQ( acceleratedCount( 2 ), 2 );
    CHECK_EQ( acceleratedCount( 3 ), 6 );
    CHECK_EQ( acceleratedCount( -5 ), -15 );

    // ---- QL-H4: mute / solo follow the direction, no movement keeps the state
    CHECK( mt::mixlogic::toggleFromDelta( false, 1 ) );
    CHECK( !mt::mixlogic::toggleFromDelta( true, -1 ) );
    CHECK( mt::mixlogic::toggleFromDelta( true, 0 ) );
    CHECK( !mt::mixlogic::toggleFromDelta( false, 0 ) );

    // ---- QL-H3: bank offset on both read and write (the same function)
    CHECK_EQ( padNote( 0, 3 ), 3 );
    CHECK_EQ( padNote( 2, 3 ), 35 );
    CHECK_EQ( padNote( 7, 15 ), 127 );
    CHECK_EQ( padNote( -1, -1 ), 0 );
    CHECK_EQ( padNote( 99, 99 ), 127 );

    // ---- QL-05: only "CC<n>" is a controller property
    CHECK_EQ( ccNumberOfProperty( "CC0" ), 0 );
    CHECK_EQ( ccNumberOfProperty( "CC127" ), 127 );
    CHECK_EQ( ccNumberOfProperty( "CC128" ), -1 );
    CHECK_EQ( ccNumberOfProperty( "CC" ), -1 );
    CHECK_EQ( ccNumberOfProperty( "ProgCH" ), -1 );
    CHECK_EQ( ccNumberOfProperty( "Accent" ), -1 );
    CHECK_EQ( ccNumberOfProperty( "cc7" ), -1 );
    CHECK_EQ( ccNumberOfProperty( "CC7x" ), -1 );

    // ---- QL-01 / QL-17: routing decision
    RouteInput in;
    in.screenHandlesCommands = true;
    in.isNavigation          = true;
    in.hasFocusedControl     = true;

    // data dial on a touched control -> the control
    CHECK( routeHardwareMessage( in ) == Route::FocusedControl );
    // Q-Link after a touch -> the screen, never the control (the P0 bug)
    in.isQLink = true;
    CHECK( routeHardwareMessage( in ) == Route::Screen );
    // Q-Link in Custom mode -> Knob Edit, with or without focus
    in.qlinkMode = 0;
    CHECK( routeHardwareMessage( in ) == Route::KnobEditCustom );
    in.hasFocusedControl = false;
    CHECK( routeHardwareMessage( in ) == Route::KnobEditCustom );
    // data dial in Custom mode is not a Q-Link
    in.isQLink = false;
    CHECK( routeHardwareMessage( in ) == Route::Screen );
    // a pad button never goes to the control
    in.hasFocusedControl = true;
    in.isPadButton       = true;
    CHECK( routeHardwareMessage( in ) == Route::Screen );
    // a screen without handler gets nothing
    in.screenHandlesCommands = false;
    CHECK( routeHardwareMessage( in ) == Route::None );

    // ---- QL-10: Screen Mode Q-Link roles
    const auto xyRoles = screenQLinkRoles( "HomeXYView" );
    CHECK( xyRoles[0].roleName == "Q-LINK 1" );
    CHECK( xyRoles[0].paramName == "Cutoff / Preset" );
    CHECK( xyRoles[3].paramName == "Wet/Dry" );

    const auto looperRoles = screenQLinkRoles( "MenuLooper" );
    CHECK( looperRoles[0].paramName == "Threshold" );
    CHECK( looperRoles[1].paramName == "Monitor" );
    CHECK( looperRoles[2].paramName == "Gain" );
    CHECK( looperRoles[3].paramName == "Pan" );

    const auto gridRoles = screenQLinkRoles( "Grid" );
    CHECK( gridRoles[0].paramName == "Scroll X" );
    CHECK( gridRoles[2].paramName == "Zoom X" );

    const auto defaultRoles = screenQLinkRoles( "Main" );
    CHECK( defaultRoles[0].paramName == "Track Transpose" );
    CHECK( defaultRoles[1].paramName == "Track Level" );

    if( g_fail == 0 )
        std::printf( "QLinkLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
