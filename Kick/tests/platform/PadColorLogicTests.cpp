// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the Color Edit pad colour mode mapping (kick_PadColorLogic.h): round trip of all nine modes.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined PadColorLogicTests.cpp
#include "../../modules/kick_ui/modes/kick_PadColorLogic.h"
#include <cstdio>
#include <initializer_list>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                                   \
    do {                                                                                                                   \
        long long va = (long long)( a ), vb = (long long)( b );                                                            \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )
#define CHECK( a ) CHECK_EQ( ( a ) ? 1 : 0, 1 )

using namespace kick_ui::pad_color_logic;

int main()
{
    // Every one of the nine modes: decode then encode gives the same mode back.
    for( int m = Off; m <= OffDimFull; ++m )
    {
        CHECK( isValidMode( m ) );
        const int  padOff   = decodePadOff( m );
        const int  padOn    = decodePadOn( m );
        const bool emptyDim = decodeEmptyDim( m );
        CHECK_EQ( encode( padOff, padOn, emptyDim ), m );
    }

    // Every UI combination encodes to a valid mode (never INVALID) and decodes back to the same choices.
    for( int padOff : { kPadOffDim, kPadOffOff } )
        for( int padOn : { kOnVelocity, kOnFull, kOnOff } )
            for( bool empty : { false, true } )
            {
                const int m = encode( padOff, padOn, empty );
                CHECK( isValidMode( m ) );
                CHECK( m != Invalid );
                CHECK_EQ( decodePadOff( m ), padOff );
                CHECK_EQ( decodePadOn( m ), padOn );
                // Empty Pad only exists with Pad Off = Dim; with Off the empty pads are off.
                CHECK_EQ( decodeEmptyDim( m ) ? 1 : 0, ( padOff == kPadOffDim && empty ) ? 1 : 0 );
            }

    // Spot checks against the engine numbering.
    CHECK_EQ( encode( kPadOffOff, kOnOff, false ), Off );
    CHECK_EQ( encode( kPadOffOff, kOnVelocity, true ), OffVelocity );
    CHECK_EQ( encode( kPadOffDim, kOnFull, true ), DimFull );
    CHECK_EQ( encode( kPadOffDim, kOnFull, false ), OffDimFull );

    // Out-of-range selections fall back to a valid mode.
    CHECK( isValidMode( encode( 7, 9, true ) ) );
    CHECK( isValidMode( encode( -3, -3, false ) ) );
    CHECK( !isValidMode( Invalid ) );
    CHECK( !isValidMode( 99 ) );
    // Anything unknown decodes to a defined value.
    CHECK_EQ( decodePadOff( Invalid ), kPadOffOff );
    CHECK_EQ( decodePadOn( Invalid ), kOnOff );
    CHECK( !decodeEmptyDim( Invalid ) );
    CHECK( !decodeEmptyDim( 9 ) ); // DISABLE_PAD_COLOUR_MODE

    // Data Dial palette stepping stops at both ends.
    CHECK_EQ( stepColour( 0, -1 ), 0 );
    CHECK_EQ( stepColour( 17, 1 ), 17 );
    CHECK_EQ( stepColour( 5, 3 ), 8 );
    CHECK_EQ( stepColour( 5, -9 ), 0 );
    CHECK_EQ( stepColour( 5, 100 ), 17 );

    if( g_fail == 0 )
        std::printf( "PadColorLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
