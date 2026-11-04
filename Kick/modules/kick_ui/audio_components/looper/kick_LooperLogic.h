// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) logic of the Looper screen (host test: Kick/tests/platform/LooperLogicTests.cpp).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>

namespace kick_ui::looper_logic
{
    // Loop length as a whole number of frames: the engine stores the length as float seconds, the tempo as double, and the
    // two never compared equal at e.g. 100 BPM (9.6 s), so every visit of the screen cleared the recorded loop.
    inline std::int64_t framesOf( double Seconds, double Rate ) noexcept
    {
        if( !( Seconds > 0.0 ) || !( Rate > 0.0 ) )
            return 0;
        return static_cast<std::int64_t>( std::llround( Seconds * Rate ) );
    }

    inline bool sameLoopLength( double SecondsA, double SecondsB, double Rate ) noexcept
    {
        return std::llabs( framesOf( SecondsA, Rate ) - framesOf( SecondsB, Rate ) ) <= 1; // float rounding of the stored length
    }

    // Seconds of a loop of Bars bars. No tempo = no answer (0): the caller leaves the loop alone instead of resizing it to 1 s.
    inline double loopSeconds( bool HasTempo, double SecondsUptoBars ) noexcept
    {
        return ( HasTempo && SecondsUptoBars > 0.0 ) ? SecondsUptoBars : 0.0;
    }

    // Export / Clear are available while the engine holds a recording, whatever started it (screen button or hardware).
    inline bool canExportOrClear( bool LooperEmpty, bool Recording ) noexcept
    {
        return !LooperEmpty && !Recording;
    }

    // Changing the bar count clears the loop: ask first when there is something to lose.
    inline bool needsClearConfirm( bool LooperEmpty, int OldBars, int NewBars ) noexcept
    {
        return !LooperEmpty && OldBars != NewBars;
    }

    // Number of beats for given bar count and time signature numerator (LO-09)
    inline int calculateTotalBeats( int bars, int numerator ) noexcept
    {
        const int b = std::max( 1, bars );
        const int num = std::max( 1, numerator );
        return b * num;
    }

    // Beat width calculation for the scale area (LO-09)
    inline float calculateBeatWidth( float totalWidth, int totalBeats ) noexcept
    {
        if( totalWidth <= 0.0f || totalBeats <= 0 )
            return 0.0f;
        return totalWidth / static_cast<float>( totalBeats );
    }

    // PPQ resolution calculation ensuring no division by zero on ARM (LO-09, LO-16)
    inline int safePpqResolution( int denominator ) noexcept
    {
        const int d = std::max( 1, denominator );
        return 960 / std::max( 1, d / 2 );
    }
}
