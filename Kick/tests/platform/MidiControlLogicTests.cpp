// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free MIDI Control logic (kick_MidiControlLogic.h).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined MidiControlLogicTests.cpp
#include "../../modules/kick_ui/modes/midicontrol/kick_MidiControlLogic.h"
#include <cstdio>
#include <set>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                                   \
    do {                                                                                                                   \
        long long va = (long long)( a ), vb = (long long)( b );                                                            \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )
#define CHECK( a ) CHECK_EQ( ( a ) ? 1 : 0, 1 )

using namespace kick_ui::midictl;

int main()
{
    // ---- control table: list size, unique names, unique default CC (TC and PAD Mute were both 14)
    auto names = controlListNames();
    CHECK_EQ( names.size(), kListSize );
    CHECK_EQ( kListSize, 40 );
    CHECK( std::set<std::string>( names.begin(), names.end() ).size() == names.size() );
    CHECK( names[0] == "Step Seq" );
    CHECK( names[8] == "-" );
    CHECK( names[9] == "+" );
    CHECK( names[kFirstQLink] == "Q-Link 1" );
    CHECK( names[kFirstPad] == "Pad 1" );
    CHECK( names[kXAxis] == "XYFXX-Axis" );
    CHECK( names[kYAxis] == "XYFXY-Axis" );
    std::set<int> ccs;
    for( const auto& b : buttons() ) ccs.insert( b.defaultCc );
    CHECK_EQ( ccs.size(), 18 );
    CHECK( !ccs.count( 20 ) && !ccs.count( 24 ) && !ccs.count( 25 ) ); // Q-Link 20..23, XY 24/25

    // ---- panels: the XY rows reach the XY panel
    CHECK( panelOfIndex( 0 ) == ListPanel::Button );
    CHECK( panelOfIndex( 17 ) == ListPanel::Button );
    CHECK( panelOfIndex( 18 ) == ListPanel::QLink );
    CHECK( panelOfIndex( 21 ) == ListPanel::QLink );
    CHECK( panelOfIndex( 22 ) == ListPanel::Pad );
    CHECK( panelOfIndex( 37 ) == ListPanel::Pad );
    CHECK( panelOfIndex( 38 ) == ListPanel::Xy );
    CHECK( panelOfIndex( 39 ) == ListPanel::Xy );

    // ---- Q-Link CC
    CHECK_EQ( relativeCc( 1 ), 1 );
    CHECK_EQ( relativeCc( -1 ), 127 );
    CHECK_EQ( relativeCc( 0 ), 0 );
    CHECK_EQ( relativeCc( 63 ), 63 );
    CHECK_EQ( relativeCc( 200 ), 63 );
    CHECK_EQ( relativeCc( -200 ), 65 );
    CHECK_EQ( absoluteCc( 10, 5, 0, 127 ), 15 );
    CHECK_EQ( absoluteCc( 126, 5, 0, 127 ), 127 );
    CHECK_EQ( absoluteCc( 2, -5, 0, 127 ), 0 );
    CHECK_EQ( absoluteCc( 50, 100, 20, 60 ), 60 );
    CHECK_EQ( absoluteCc( 50, -100, 20, 60 ), 20 );
    CHECK_EQ( absoluteCc( 5, 1, 60, 20 ), 20 ); // swapped range is repaired

    // ---- buttons
    bool tog = false;
    auto o   = buttonCc( false, true, tog );
    CHECK( o.send && o.value == 127 && tog );
    o = buttonCc( false, false, tog ); // release of a toggle: nothing
    CHECK( !o.send && tog );
    o = buttonCc( false, true, tog );
    CHECK( o.send && o.value == 0 && !tog );
    bool unused = false;
    o = buttonCc( true, true, unused );
    CHECK( o.send && o.value == 127 );
    o = buttonCc( true, false, unused );
    CHECK( o.send && o.value == 0 );

    // ---- pads
    CHECK( padVelocity( false, 0.3f ) == 1.0f );
    CHECK( padVelocity( true, 0.3f ) == 0.3f );
    CHECK( padVelocity( true, 2.0f ) == 1.0f );

    // ---- XY
    CHECK_EQ( scaleAxis( 0.0f, 0, 127 ), 0 );
    CHECK_EQ( scaleAxis( 1.0f, 0, 127 ), 127 );
    CHECK_EQ( scaleAxis( 0.5f, 20, 100 ), 60 );
    CHECK_EQ( scaleAxis( -3.0f, 0, 127 ), 0 );
    AxisFilter f;
    CHECK( f.changed( 10 ) );
    CHECK( !f.changed( 10 ) );
    CHECK( f.changed( 11 ) );
    f.reset();
    CHECK( f.changed( 11 ) );

    // ---- Learn
    LearnRow row{ "cc", 1, 21, false };
    CHECK( learnMatches( row, "cc", 1, 21 ) );
    CHECK( !learnMatches( row, "cc", 1, 22 ) );   // another CC number
    CHECK( !learnMatches( row, "cc", 2, 21 ) );   // another channel
    CHECK( !learnMatches( row, "Note", 1, 21 ) ); // a note is not a CC
    CHECK( !learnMatches( LearnRow{}, "cc", 1, 21 ) );
    CHECK_EQ( flipped( 10, false ), 10 );
    CHECK_EQ( flipped( 10, true ), 117 );
    CHECK_EQ( flipped( 200, true ), 0 );
    CHECK( switchFromCc( 127 ) && switchFromCc( 64 ) && !switchFromCc( 63 ) && !switchFromCc( 0 ) );
    CHECK( ccToPan( 0 ) == -1.0 );
    CHECK( ccToPan( 127 ) == 1.0 );
    CHECK( std::fabs( ccToPan( 64 ) ) < 0.01 );
    CHECK( ccToGain( 127 ) == 1.0 && ccToGain( 0 ) == 0.0 );

    // ---- monitor
    CHECK_EQ( velocityPercent( 127 ), 100 );
    CHECK_EQ( velocityPercent( 64 ), 50 );
    CHECK_EQ( velocityPercent( 0 ), 0 );
    std::vector<int> v;
    for( int i = 0; i < 10; ++i ) v.push_back( i );
    trimTo( v, 4 );
    CHECK_EQ( v.size(), 4 );
    CHECK_EQ( v.front(), 6 );
    trimTo( v, 10 );
    CHECK_EQ( v.size(), 4 );

    if( g_fail == 0 )
        std::printf( "MidiControlLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
