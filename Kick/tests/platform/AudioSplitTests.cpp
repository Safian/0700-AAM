// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the Audio editor SPLIT geometry.

#include "../../modules/program_edit_ui/sources/audio/kick_audio_split.h"
#include <cstdio>
#include <cstdlib>

using namespace kick_ui::audiosplit;

static void check( bool ok, const char* why )
{
    if( !ok ) { std::fprintf( stderr, "FAIL: %s\n", why ); std::exit( 1 ); }
}

int main()
{
    // 1 bar = 1920 ticks, 48000 frames per second at 120 bpm: 2 s = 96000 frames over 1920 ticks = 50 frames per tick.
    auto r = compute( 0, 1920, 0, 0, 96000, 50.0, 480 );
    check( r.ok && r.prevLength == 480 && r.newStartTime == 480 && r.newLength == 1440, "cut at 480 ticks" );
    check( r.prevLoopEnd == 24000.0 && r.newLoopEnd == 96000.0, "first half ends at 24000 frames, second half keeps the whole-source end" );
    r = compute( 960, 1000, 100, 50100, 96000, 50.0, 1460 );
    check( r.ok && r.prevLength == 500 && r.newLength == 500 && r.prevLoopEnd == 25100.0 && r.newLoopEnd == 50100.0, "offset sample with its own loop points" );
    check( ! compute( 0, 1920, 0, 0, 96000, 50.0, 0 ).ok, "cut at the very start: nothing to split" );
    check( ! compute( 0, 1920, 0, 0, 96000, 50.0, 1920 ).ok, "cut at the very end: nothing to split" );
    check( ! compute( 100, 1920, 0, 0, 96000, 50.0, 50 ).ok, "cut before the sample" );
    check( ! compute( 0, 1920, 0, 0, 96000, 50.0, 5000 ).ok, "cut after the sample" );
    check( ! compute( 0, 0, 0, 0, 96000, 50.0, 10 ).ok && ! compute( 0, 1920, 0, 0, 96000, 0.0, 10 ).ok, "degenerate input" );
    r = compute( 0, 1920, 0, 10000, 96000, 50.0, 1900 );
    check( r.ok && r.prevLoopEnd == 10000.0, "a stretched sample cannot split past its source end" );
    check( ! compute( 0, 1920, 0, 0, 96000, 50.0, 3, 24 ).ok && compute( 0, 1920, 0, 0, 96000, 50.0, 24, 24 ).ok, "min piece length" );
    std::puts( "AudioSplitTests: OK" );
    return 0;
}
