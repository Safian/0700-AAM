// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the CV step -> DAC scaling. Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../modules/hardware/source/kick_cv_scale.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK_EQ( a, b )                                                                                           \
    do {                                                                                                           \
        long long va = (long long)( a ), vb = (long long)( b );                                                    \
        if( va != vb ) { std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } \
    } while( 0 )

int main()
{
    CHECK_EQ( mt::stepToDac( 0 ), 0 );
    CHECK_EQ( mt::stepToDac( 127 ), 4095 );
    CHECK_EQ( mt::stepToDac( 64 ), 2063 );
    CHECK_EQ( mt::stepToDac( 1 ), 32 );
    CHECK_EQ( mt::stepToDac( 200 ), 4095 ); // clamped
    CHECK_EQ( mt::stepToDac( 65535 ), 4095 );
    if( g_fail ) { std::printf( "%d failure(s)\n", g_fail ); return 1; }
    std::printf( "CvScaleTests OK\n" );
    return 0;
}
