// SPDX-License-Identifier: AGPL-3.0-or-later
// JUCE-free pitch bend maths: 14-bit MIDI pitch wheel value -> playback speed ratio.
#pragma once

#include <atomic>
#include <cmath>

namespace mt::pitchbend
{
    constexpr int   CENTRE          = 8192;
    constexpr int   MAX_VALUE       = 16383;
    constexpr float RANGE_SEMITONES = 2.f; // default: +-2 semitones
    constexpr float MIN_RANGE       = 0.5f;
    constexpr float MAX_RANGE       = 24.f;

    // The bend range (+- semitones) of the sample programs (drum, keygroup); one value for all programs, set from the UI
    // (Piano screen) and read on the tick thread. MIDI programs pass the wheel value on unchanged: the external synth decides.
    inline std::atomic<float>& rangeStore()
    {
        static std::atomic<float> r{ RANGE_SEMITONES };
        return r;
    }
    inline float rangeSemitones() { return rangeStore().load( std::memory_order_relaxed ); }
    inline void  setRangeSemitones( float Semis )
    {
        if( !( Semis >= MIN_RANGE ) ) Semis = MIN_RANGE; // also catches NaN
        if( Semis > MAX_RANGE ) Semis = MAX_RANGE;
        rangeStore().store( Semis, std::memory_order_relaxed );
    }

    // Out of range input is clamped to 0..16383. Centre (8192) gives exactly 1.0.
    inline float ratioFromWheel( int Value, float Range )
    {
        if( Value < 0 ) Value = 0;
        if( Value > MAX_VALUE ) Value = MAX_VALUE;
        if( Value == CENTRE ) return 1.f;
        const float semis = ( static_cast<float>( Value - CENTRE ) / static_cast<float>( CENTRE ) ) * Range;
        return std::pow( 2.f, semis / 12.f );
    }
    // With the configured range (default +-2 semitones).
    inline float ratioFromWheel( int Value ) { return ratioFromWheel( Value, rangeSemitones() ); }
} // namespace mt::pitchbend
