#include "../../modules/engine/audio_engine/engine/mt_time_signature.h"
#include <cstdio>
using namespace mt::timesig;
static int fails = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL line %d: %s\n", __LINE__, #c ); ++fails; } } while( 0 )
int main()
{
    CHECK( isValid( 4, 4 ) );
    CHECK( isValid( 7, 8 ) );
    CHECK( isValid( 16, 32 ) );
    CHECK( !isValid( 0, 4 ) );
    CHECK( !isValid( 17, 4 ) );
    CHECK( !isValid( 4, 6 ) );
    CHECK( !isValid( 4, 2 ) );

    const int ppqn = 960;
    CHECK( ticksPerBar( 4, 4, ppqn ) == 3840 );
    CHECK( ticksPerBar( 3, 4, ppqn ) == 2880 );
    CHECK( ticksPerBar( 6, 8, ppqn ) == 2880 );
    CHECK( ticksPerBar( 7, 8, ppqn ) == 3360 );
    CHECK( ticksPerBar( 5, 4, ppqn ) == 4800 );

    // 4 bars of 4/4 = 15360 ticks -> 3/4 (2880): 5.33 -> 6 bars (never cut)
    CHECK( remapBars( 4, 3840, 2880, 1, 100 ) == 6 );
    // 4/4 -> 4/4 is the identity
    CHECK( remapBars( 4, 3840, 3840, 1, 100 ) == 4 );
    // 4/4 -> 2/4 (1920): twice the bars
    CHECK( remapBars( 4, 3840, 1920, 1, 100 ) == 8 );
    // 4/4 -> 7/8 (3360): 4*3840/3360 = 4.57 -> 5
    CHECK( remapBars( 4, 3840, 3360, 1, 100 ) == 5 );
    // clamps
    CHECK( remapBars( 80, 3840, 960, 1, 100 ) == 100 );
    CHECK( remapBars( 1, 960, 3840, 1, 100 ) == 1 );
    CHECK( remapBars( 0, 3840, 3840, 1, 100 ) == 1 );

    // loop: whole-sequence loop follows the length
    {
        Loop l = remapLoop( 0, 4, 4, 6, 3840, 2880 );
        CHECK( l.start == 0 && l.end == 6 );
    }
    // loop bars 1..2 (0-based start 1, end 3) in 4/4 -> 3/4: ticks 3840..11520 -> start 1 (floor 1.33), end ceil(4.0)=4
    {
        Loop l = remapLoop( 1, 3, 4, 6, 3840, 2880 );
        CHECK( l.start == 1 && l.end == 4 );
    }
    // a loop never ends before it starts
    {
        Loop l = remapLoop( 3, 3, 4, 2, 3840, 7680 );
        CHECK( l.start <= l.end );
        CHECK( l.end <= 2 );
    }
    // end past the new length is clamped
    {
        Loop l = remapLoop( 0, 3, 8, 2, 3840, 7680 );
        CHECK( l.end <= 2 );
    }
    // zero-size new grid never divides by zero
    CHECK( remapBars( 4, 3840, 0, 1, 100 ) == 1 );
    std::puts( fails ? "FAILED" : "OK" );
    return fails ? 1 : 0;
}
