// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free XY mapping (kick_xy_fx_map.h). Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined XyFxMapTests.cpp
#include "../../modules/kick_ui_components/source/kick_xy_fx_map.h"
#include "../../../AudIO/AudIO/Dsp/TouchFx.h"

#include <cmath>
#include <cstdio>
#include <set>

static int g_fail = 0;
#define CHECK( a )                                                                      \
    do {                                                                                \
        if( !( a ) ) { std::printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #a ); ++g_fail; } \
    } while( 0 )

using namespace kick_ui::xyfx;

int main()
{
    // ---- preset table: 11 presets, unique names, mode / filter in the range of AudIO::TouchFx
    std::set<std::string> names;
    for( int i = 0; i < kNumPresets; ++i )
    {
        const auto& p = preset( i );
        names.insert( p.name );
        CHECK( p.mode >= 1 && p.mode <= 5 );
        CHECK( p.filter >= 0 && p.filter <= 2 );
        // a beat division axis is always X; Y never is
        CHECK( p.y != Axis::BeatDivision );
    }
    CHECK( names.size() == static_cast<size_t>( kNumPresets ) );
    CHECK( std::string( preset( 3 ).name ) == "LPF Manual" );
    CHECK( preset( 3 ).x == Axis::Cutoff && preset( 3 ).y == Axis::Resonance );
    CHECK( preset( 0 ).mode == static_cast<int>( AudIO::TouchFx::Mode::BeatRepeat ) );
    CHECK( preset( 6 ).mode == static_cast<int>( AudIO::TouchFx::Mode::FilterBeat ) );
    CHECK( preset( 9 ).mode == static_cast<int>( AudIO::TouchFx::Mode::PhaserManual ) );
    CHECK( preset( 10 ).mode == static_cast<int>( AudIO::TouchFx::Mode::PhaserBeat ) );
    CHECK( preset( 2 ).filter == static_cast<int>( AudIO::TouchFx::FilterType::HighPass ) );
    CHECK( std::string( preset( 99 ).name ) == "Phaser" ); // clamped
    CHECK( std::string( axisName( Axis::Depth ) ) == "Depth" );

    // ---- cells: hit test and drawing share the geometry (with spacing), no cell for a size of 0
    const float W = 814.f, H = 814.f, S = 2.f; // cell = 100 px
    CHECK( cellSize( W, S ) == 100.f );
    auto c = cellAt( 5.f, 5.f, W, H, S );
    CHECK( c.column == 0 && c.row == 0 );
    c = cellAt( 101.f, 5.f, W, H, S ); // in the gap after cell 0
    CHECK( c.column == 0 );
    c = cellAt( 102.f, 5.f, W, H, S ); // first pixel of cell 1
    CHECK( c.column == 1 );
    c = cellAt( W, H, W, H, S );
    CHECK( c.column == 7 && c.row == 7 );
    c = cellAt( -1.f, 5.f, W, H, S );
    CHECK( c.column == -1 );
    c = cellAt( 5.f, 5.f, 0.f, 0.f, S );
    CHECK( c.column == -1 ); // not laid out yet
    CHECK( normalised( 5.f, 0.f ) == 0.f );
    CHECK( std::isfinite( normalised( 5.f, 0.f ) ) );
    CHECK( normalised( 400.f, 800.f ) == 0.5f );
    CHECK( normalised( 900.f, 800.f ) == 1.f );
    CHECK( normalised( -5.f, 800.f ) == 0.f );
    CHECK( divisionOfColumn( 0 ) == 0 && divisionOfColumn( 7 ) == 7 && divisionOfColumn( 12 ) == 7 );

    // ---- value text
    CHECK( axisText( Axis::BeatDivision, 0 ) == "2 Bar" );
    CHECK( axisText( Axis::BeatDivision, 7 ) == "1/64" );
    CHECK( axisText( Axis::Cutoff, 0.f ) == "20 Hz" );
    CHECK( axisText( Axis::Cutoff, 1.f ) == "20.0 kHz" );
    CHECK( axisText( Axis::Resonance, 0.f ) == "Q 0.5" );
    CHECK( axisText( Axis::Resonance, 1.f ) == "Q 12.0" );
    CHECK( axisText( Axis::Depth, 0.5f ) == "50 %" );
    CHECK( axisLow( Axis::Cutoff ) == "20 Hz" && axisHigh( Axis::BeatDivision ) == "1/64" );

    // ---- attack / release
    CHECK( envelopeMs( 0.f ) == 1.f );
    CHECK( envelopeMs( 100.f ) == 2000.f );
    CHECK( envelopeMs( 50.f ) < 1000.f ); // squared: the short end has the resolution
    CHECK( envelopeMs( -5.f ) == 1.f && envelopeMs( 500.f ) == 2000.f );
    CHECK( envelopeText( 0.f ) == "1 ms" );
    CHECK( envelopeText( 100.f ) == "2.00 s" );

    // ---- gate
    Gate g;
    g.touchDown();
    CHECK( !g.open() ); // effect off: nothing opens
    g.setEnabled( true );
    CHECK( !g.open() );
    g.touchDown();
    CHECK( g.open() );
    g.touchUp();
    CHECK( !g.open() ); // no latch: the finger up closes it
    g.setLatch( true );
    g.touchDown();
    g.touchUp();
    CHECK( g.open() );  // latched: stays open
    g.touchDown();      // a new touch moves the position, still open
    CHECK( g.open() );
    g.touchUp();
    CHECK( g.open() );
    g.setLatch( false );
    CHECK( !g.open() ); // letting go of Latch closes it
    g.setLatch( true );
    g.touchDown();
    g.touchUp();
    g.setEnabled( false ); // switching the effect off closes everything
    CHECK( !g.open() );
    g.setEnabled( true );
    CHECK( !g.open() );
    g.touchDown();
    g.closeAll(); // leaving the screen
    CHECK( !g.open() );
    g.setLatch( false );
    g.touchDown();
    g.setLatch( false );
    CHECK( g.open() ); // Latch off while the finger is down keeps the gate open until the finger leaves
    g.touchUp();
    CHECK( !g.open() );

    if( g_fail == 0 )
        std::printf( "XyFxMapTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
