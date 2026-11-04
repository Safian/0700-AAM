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
    CHECK_EQ( kListSize, 41 );
    CHECK( std::set<std::string>( names.begin(), names.end() ).size() == names.size() );
    CHECK( names[0] == "Step Seq" );
    CHECK( names[8] == "-" );
    CHECK( names[9] == "+" );
    CHECK( names[kFirstQLink] == "Q-Link 1" );
    CHECK( names[kFirstPad] == "Pad 1" );
    CHECK( names[kXAxis] == "XYFXX-Axis" );
    CHECK( names[kYAxis] == "XYFXY-Axis" );
    CHECK( names[kTouchStrip] == "Touch Strip" );
    std::set<int> ccs;
    for( const auto& b : buttons() ) ccs.insert( b.defaultCc );
    CHECK_EQ( ccs.size(), 18 );
    CHECK( !ccs.count( 20 ) && !ccs.count( 24 ) && !ccs.count( 25 ) ); // Q-Link 20..23, XY 24/25

    // ---- panels: the XY rows reach the XY panel, touch strip reaches strip panel
    CHECK( panelOfIndex( 0 ) == ListPanel::Button );
    CHECK( panelOfIndex( 17 ) == ListPanel::Button );
    CHECK( panelOfIndex( 18 ) == ListPanel::QLink );
    CHECK( panelOfIndex( 21 ) == ListPanel::QLink );
    CHECK( panelOfIndex( 22 ) == ListPanel::Pad );
    CHECK( panelOfIndex( 37 ) == ListPanel::Pad );
    CHECK( panelOfIndex( 38 ) == ListPanel::Xy );
    CHECK( panelOfIndex( 39 ) == ListPanel::Xy );
    CHECK( panelOfIndex( 40 ) == ListPanel::Strip );

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

    // ---- MC-23: navigating buttons
    CHECK( isNavigatingButton( "Step Seq" ) );
    CHECK( isNavigatingButton( "Browser" ) );
    CHECK( isNavigatingButton( "XYFX" ) );
    CHECK( !isNavigatingButton( "Tap" ) );
    CHECK( !isNavigatingButton( "Enter" ) );
    CHECK( !isNavigatingButton( "-" ) );

    // ---- MC-22: ControllerMap
    ControllerMap ctrlMap;
    CHECK( ctrlMap.getItems().size() >= 38 );
    const auto* btn0 = ctrlMap.findItem( 0 );
    CHECK( btn0 != nullptr && btn0->name == "Step Seq" && btn0->momentary );
    const auto* ql0 = ctrlMap.findItem( kFirstQLink );
    CHECK( ql0 != nullptr && ql0->name == "Q-Link 1" && ql0->isAbsolute );

    // ---- MC-22: MidiLearnEngine
    MidiLearnEngine engine;
    CHECK_EQ( engine.numBindings(), 0 );

    MidiLearnBinding b1;
    b1.type = "cc";
    b1.channel = 1;
    b1.number = 7;
    b1.target = "Track Volume";
    b1.flip = false;
    engine.addBinding( b1 );

    MidiLearnBinding b2;
    b2.type = "cc";
    b2.channel = 2;
    b2.number = 10;
    b2.target = "Track Pan";
    b2.flip = true;
    engine.addBinding( b2 );

    CHECK_EQ( engine.numBindings(), 2 );

    auto res1 = engine.processIncoming( "cc", 1, 7, 127 );
    CHECK( res1.matched && res1.target == "Track Volume" && std::fabs( res1.value - 1.0f ) < 1e-4f );

    auto res2 = engine.processIncoming( "cc", 2, 10, 127 );
    // flipped: 127 -> 0
    CHECK( res2.matched && res2.target == "Track Pan" && std::fabs( res2.value - 0.0f ) < 1e-4f );

    auto res3 = engine.processIncoming( "cc", 3, 7, 127 ); // unmapped channel
    CHECK( !res3.matched );

    // ---- XY engine shared by MIDI Control > XY and XY FX "MIDI CC" (XY-14)
    {
        XyCcEngine  xy;
        XyAxisConfig cx, cy;
        cx.channel = 2; cx.cc = 24; cx.low = 10; cx.high = 110;
        cy.channel = 3; cy.cc = 25; cy.low = 0;  cy.high = 127; cy.muted = false;
        XyCc out[2];
        CHECK_EQ( xy.process( 0.5f, 1.0f, cx, cy, out ), 2 );
        CHECK_EQ( out[0].channel, 2 );
        CHECK_EQ( out[0].cc, 24 );
        CHECK_EQ( out[0].value, 60 );
        CHECK_EQ( out[1].channel, 3 );
        CHECK_EQ( out[1].cc, 25 );
        CHECK_EQ( out[1].value, 127 );
        CHECK_EQ( xy.process( 0.5f, 1.0f, cx, cy, out ), 0 ); // nothing changed: nothing sent
        CHECK_EQ( xy.process( 0.5f, 0.0f, cx, cy, out ), 1 ); // only Y moved
        CHECK_EQ( out[0].cc, 25 );
        CHECK_EQ( out[0].value, 0 );
        cx.muted = true;
        CHECK_EQ( xy.process( 1.0f, 0.0f, cx, cy, out ), 0 ); // X muted, Y unchanged
        xy.reset();
        CHECK_EQ( xy.process( 1.0f, 0.0f, cx, cy, out ), 1 ); // reset: the next touch sends its first position again
        cy.muted = true;
        xy.reset();
        CHECK_EQ( xy.process( 1.0f, 1.0f, cx, cy, out ), 0 ); // both muted
        cx.muted = false; cx.channel = 99; cx.cc = -5;
        xy.reset();
        CHECK_EQ( xy.process( 0.0f, 0.0f, cx, cy, out ), 1 ); // out-of-range configuration is clamped, never a bad message
        CHECK_EQ( out[0].channel, 16 );
        CHECK_EQ( out[0].cc, 0 );
    }

    if( g_fail == 0 )
        std::printf( "MidiControlLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
