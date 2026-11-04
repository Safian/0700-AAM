// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the pad colour defaults per program type (mt_pad_colour_defaults.h).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined PadColourDefaultsTests.cpp
#include "../../modules/engine/audio_program/source/mt_pad_colour_defaults.h"
#include <cstdio>
#include <cstring>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                                   \
    do {                                                                                                                   \
        long long va = (long long)( a ), vb = (long long)( b );                                                            \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )
#define CHECK( a ) CHECK_EQ( ( a ) ? 1 : 0, 1 )

using namespace mt::padcolour;

int main()
{
    // groups: Drum, Clip, everything else
    CHECK( groupOfType( 0 ) == Group::Drum );
    CHECK( groupOfType( 4 ) == Group::Clip );
    for( int t : { 1, 2, 3, 5, 6, 7 } ) CHECK( groupOfType( t ) == Group::Other );
    CHECK( std::strcmp( groupKey( Group::Drum ), "drum" ) == 0 && std::strcmp( groupKey( Group::Clip ), "clip" ) == 0 && std::strcmp( groupKey( Group::Other ), "other" ) == 0 );

    // round trip
    Defaults d;
    for( int i = 0; i < kNumPads; ++i ) d.colours[(size_t)i] = ( i * 7 ) % 18;
    d.mode  = 5;
    d.empty = 1;
    const auto text = encode( d );
    Defaults   back;
    CHECK( decode( text, back ) );
    CHECK_EQ( back.mode, 5 ); CHECK_EQ( back.empty, 1 );
    for( int i = 0; i < kNumPads; ++i ) CHECK_EQ( back.colours[(size_t)i], d.colours[(size_t)i] );

    // anything else is refused and leaves the output alone
    Defaults keep;
    keep.mode = 3;
    CHECK( !decode( "", keep ) );
    CHECK( !decode( "4;0;1,2,3", keep ) );                              // too few colours
    CHECK( !decode( text + ",1", keep ) );                              // too many
    CHECK( !decode( "9;0;" + text.substr( 4 ), keep ) );                // mode out of range
    CHECK( !decode( "4;3;" + text.substr( 4 ), keep ) );                // empty mode out of range
    { auto bad = text; bad[bad.size() - 1] = 'x'; CHECK( !decode( bad, keep ) ); }
    { auto bad = text; bad.replace( 4, 1, "99" ); CHECK( !decode( bad, keep ) ); }   // colour above the palette
    { auto bad = text; bad.replace( 4, 1, "-5" ); CHECK( !decode( bad, keep ) ); }     // negative colour
    CHECK_EQ( keep.mode, 3 );

    // user default wins over the factory table; no user default = factory
    std::array<int, kNumPads> factory{};
    for( int i = 0; i < kNumPads; ++i ) factory[(size_t)i] = i % 4;
    CHECK_EQ( colourFor( nullptr, factory, 5 ), 1 );
    CHECK_EQ( colourFor( &d, factory, 5 ), d.colours[5] );
    CHECK_EQ( colourFor( &d, factory, -1 ), 0 );
    CHECK_EQ( colourFor( &d, factory, 128 ), 0 );

    if( g_fail == 0 )
        std::printf( "PadColourDefaultsTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
