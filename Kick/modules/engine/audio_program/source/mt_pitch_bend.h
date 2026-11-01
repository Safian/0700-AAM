// SPDX-License-Identifier: AGPL-3.0-or-later
// JUCE-free pitch bend maths: 14-bit MIDI pitch wheel value -> playback speed ratio.
#pragma once

#include <cmath>

namespace mt::pitchbend
{
    constexpr int   CENTRE          = 8192;
    constexpr int   MAX_VALUE       = 16383;
    constexpr float RANGE_SEMITONES = 2.f; // fixed +-2 semitones

    // Out of range input is clamped to 0..16383. Centre (8192) gives exactly 1.0.
    inline float ratioFromWheel( int Value )
    {
        if( Value < 0 ) Value = 0;
        if( Value > MAX_VALUE ) Value = MAX_VALUE;
        if( Value == CENTRE ) return 1.f;
        const float semis = ( static_cast<float>( Value - CENTRE ) / static_cast<float>( CENTRE ) ) * RANGE_SEMITONES;
        return std::pow( 2.f, semis / 12.f );
    }
} // namespace mt::pitchbend
