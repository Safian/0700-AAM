// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the pan -> MIDI CC10 mapping. Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../modules/engine/audio_program/source/mt_midi_pan.h"
#include <cmath>
#include <cstdio>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                           \
    do {                                                                                                           \
        long long va = (long long)( a ), vb = (long long)( b );                                                    \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )

using mt::midipan::panToCC10;

int main()
{
    CHECK_EQ( panToCC10( 0.0 ), 64 );
    CHECK_EQ( panToCC10( -1.0 ), 0 );
    CHECK_EQ( panToCC10( 1.0 ), 127 );
    CHECK_EQ( panToCC10( -5.0 ), 0 );
    CHECK_EQ( panToCC10( 5.0 ), 127 );
    CHECK_EQ( panToCC10( std::nan( "" ) ), 64 );
    CHECK_EQ( panToCC10( -0.5 ), 32 );

    int prev = -1;
    for( int i = -150; i <= 150; ++i ) // beyond the range on purpose, step .01
    {
        const int v = panToCC10( i * 0.01 );
        if( v < 0 || v > 127 ) { std::printf( "FAIL range at %d: %d\n", i, v ); ++g_fail; }
        if( v < prev ) { std::printf( "FAIL monotonic at %d: %d < %d\n", i, v, prev ); ++g_fail; }
        prev = v;
    }
    if( g_fail == 0 ) std::printf( "midi pan tests OK\n" );
    return g_fail ? 1 : 0;
}
