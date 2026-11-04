#include "../../modules/engine/interface/source/mt_track_monitor.h"
#include <cstdio>
using namespace mt::trackmonitor;
static int fails = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL line %d: %s\n", __LINE__, #c ); ++fails; } } while( 0 )
int main()
{
    // default and clamping: an unknown / corrupt saved value falls back to In (the old behaviour)
    CHECK( clamp( -1 ) == In );
    CHECK( clamp( 3 ) == In );
    CHECK( clamp( 99 ) == In );
    CHECK( clamp( Off ) == Off );
    CHECK( clamp( Auto ) == Auto );
    CHECK( Default == In );
    // button cycle
    CHECK( next( Off ) == In );
    CHECK( next( In ) == Auto );
    CHECK( next( Auto ) == Off );
    CHECK( next( 42 ) == Auto ); // corrupt value = In, so the next one is Auto

    // In: everything passes, playing or not
    CHECK( passes( In, false, true, 0x90, 60, 100 ) );
    CHECK( passes( In, false, false, 0xB0, 1, 50 ) );
    // Off: new notes and controllers blocked, releases pass
    CHECK( !passes( Off, true, false, 0x90, 60, 100 ) );
    CHECK( !passes( Off, true, true, 0xB0, 1, 50 ) );
    CHECK( !passes( Off, false, false, 0xC0, 5, 0 ) );
    CHECK( passes( Off, false, true, 0x80, 60, 0 ) );
    CHECK( passes( Off, false, true, 0x93, 60, 0 ) );     // velocity-0 note-on is a note-off
    CHECK( passes( Off, false, true, 0xB0, 123, 0 ) );    // all notes off
    CHECK( passes( Off, false, true, 0xB2, 64, 0 ) );     // pedal up
    CHECK( !passes( Off, false, true, 0xB2, 64, 127 ) );  // pedal down is new
    CHECK( passes( Off, false, true, 0xE0, 0, 64 ) );     // bend centre
    CHECK( !passes( Off, false, true, 0xE0, 0, 100 ) );   // bend moved
    // Auto: heard when armed or when the track is not playing back
    CHECK( passes( Auto, false, false, 0x90, 60, 100 ) );
    CHECK( passes( Auto, true, true, 0x90, 60, 100 ) );
    CHECK( !passes( Auto, false, true, 0x90, 60, 100 ) );
    CHECK( passes( Auto, false, true, 0x80, 60, 0 ) );    // but it still lets go
    std::puts( fails ? "FAILED" : "OK" );
    return fails ? 1 : 0;
}
