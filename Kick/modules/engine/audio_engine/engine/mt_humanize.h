// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pure arithmetic of Humanize (no JUCE, no engine types): Kick/tests/platform/HumanizeTests.cpp.
// The random source is a value in [0, 1) passed in by the caller so the tests are deterministic.
//
// Time: the start moves by at most `pulses` ticks (the dialog's AMOUNT; 96 ticks = a quarter note, so 24 = a 16th note) around
// the ORIGINAL start. Eagerness -50..50 biases the move: -50 only earlier, +50 only later, 0 symmetric. The start is clamped to
// [0, maxStart] (a note never leaves the track).
// Length and velocity: a note shrinks by up to `strengthPercent` of its value, never below 1 (a length of 0 or a velocity of 0
// would silence the note: velocity 0 is a note-off).

#pragma once

namespace mt
{
    namespace humanize
    {
        constexpr int kMaxVelocity = 127;

        constexpr int clampInt( int v, int lo, int hi ) noexcept { return v < lo ? lo : ( v > hi ? hi : v ); }

        // Signed move in ticks for u in [0, 1).
        constexpr int timeShift( int pulses, int eagerness, double u ) noexcept
        {
            const int    p = pulses < 0 ? 0 : pulses;
            const double e = clampInt( eagerness, -50, 50 ) / 50.0;
            const double lo = -p * ( e > 0.0 ? 1.0 - e : 1.0 );
            const double hi = p * ( e < 0.0 ? 1.0 + e : 1.0 );
            const double s  = lo + u * ( hi - lo );
            return (int)( s < 0.0 ? s - 0.5 : s + 0.5 ); // round to the nearest tick
        }

        // maxStart < 0: no upper limit.
        constexpr int humanizedStart( int start, int pulses, int eagerness, int maxStart, double u ) noexcept
        {
            const int v = start + timeShift( pulses, eagerness, u );
            return maxStart >= 0 ? clampInt( v, 0, maxStart ) : ( v < 0 ? 0 : v );
        }

        // value shrinks by up to strengthPercent (0..100) of itself, result in [1, max].
        constexpr int shrunkValue( int value, int strengthPercent, double u, int max ) noexcept
        {
            const int    s = clampInt( strengthPercent, 0, 100 );
            const double f = 1.0 - u * s / 100.0;
            return clampInt( (int)( value * f + 0.5 ), 1, max < 1 ? 1 : max );
        }
    }
}
