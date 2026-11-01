#include "../../modules/engine/audio_engine/engine/mt_mute_filter.h"
#include <cstdio>
using namespace mt::mutefilter;
static int fails = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL line %d: %s\n", __LINE__, #c ); ++fails; } } while( 0 )
int main()
{
    CHECK( passesWhileMuted( 0x80, 60, 0 ) );
    CHECK( passesWhileMuted( 0x8F, 0, 0 ) );
    CHECK( passesWhileMuted( 0xE3, 0, 0 ) );
    CHECK( passesWhileMuted( 0xB0, 64, 0 ) );
    CHECK( passesWhileMuted( 0xB5, 123, 0 ) );
    CHECK( passesWhileMuted( 0x90, 60, 0 ) );   // velocity-0 note-on = note-off
    CHECK( !passesWhileMuted( 0x90, 60, 100 ) );
    CHECK( !passesWhileMuted( 0xB0, 1, 0 ) );
    CHECK( !passesWhileMuted( 0xB0, 10, 0 ) );
    CHECK( !passesWhileMuted( 0xD0, 5, 0 ) );
    CHECK( !passesWhileMuted( 0xC0, 5, 0 ) );
    std::puts( fails ? "FAILED" : "OK" );
    return fails ? 1 : 0;
}
