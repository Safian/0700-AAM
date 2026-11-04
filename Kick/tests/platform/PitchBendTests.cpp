// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the JUCE-free pitch bend maths.
#include "../../modules/engine/audio_program/source/mt_pitch_bend.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>

static void check( bool ok, const char* why )
{
    if( !ok ) { std::fprintf( stderr, "FAIL: %s\n", why ); std::exit( 1 ); }
}

static bool near( float a, float b ) { return std::fabs( a - b ) < 1e-4f; }

int main()
{
    using namespace mt::pitchbend;
    check( ratioFromWheel( 8192 ) == 1.f, "centre is exactly 1" );
    check( near( ratioFromWheel( 16383 ), 1.1224f ), "max up ~ +2 semitones" );
    check( near( ratioFromWheel( 0 ), std::pow( 2.f, -2.f / 12.f ) ), "min is exactly -2 semitones" );
    check( near( ratioFromWheel( 0 ), 0.8909f ), "min ~ 0.8909" );
    check( ratioFromWheel( -5 ) == ratioFromWheel( 0 ), "clamp low" );
    check( ratioFromWheel( 99999 ) == ratioFromWheel( 16383 ), "clamp high" );
    check( ratioFromWheel( 4096 ) < 1.f && ratioFromWheel( 12288 ) > 1.f, "monotonic sides" );
    check( near( ratioFromWheel( 12288 ), std::pow( 2.f, 1.f / 12.f ) ), "half up = +1 semitone" );
    // Configurable range (touch piano bend range)
    check( rangeSemitones() == 2.f, "default range is +-2" );
    check( std::fabs( ratioFromWheel( 16383, 12.f ) - 2.f ) < 1e-3f, "+-12 range: full up = one octave (to within one wheel step)" );
    check( near( ratioFromWheel( 0, 12.f ), 0.5f ), "+-12 range: full down = one octave" );
    setRangeSemitones( 7.f );
    check( rangeSemitones() == 7.f && std::fabs( ratioFromWheel( 16383 ) - std::pow( 2.f, 7.f / 12.f ) ) < 1e-3f, "configured range is used by ratioFromWheel" );
    check( ratioFromWheel( 8192 ) == 1.f, "centre stays 1 at any range" );
    setRangeSemitones( 99.f );
    check( rangeSemitones() == MAX_RANGE, "range is clamped high" );
    setRangeSemitones( -1.f );
    check( rangeSemitones() == MIN_RANGE, "range is clamped low" );
    setRangeSemitones( 2.f );
    check( near( ratioFromWheel( 16383 ), 1.1224f ), "range back to +-2" );
    std::puts( "OK" );
    return 0;
}
