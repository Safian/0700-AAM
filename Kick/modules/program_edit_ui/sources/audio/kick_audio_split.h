// SPDX-License-Identifier: AGPL-3.0-or-later
// Audio editor SPLIT at the playhead: the geometry of the two halves (JUCE-free, header only; host test:
// Kick/tests/platform/AudioSplitTests.cpp). Same maths as the Cut tool: the sample is cut at a tick, the first half keeps the
// start time and gets the shorter length and loop end, the second half starts at the cut and keeps the old loop end.
#pragma once

#include <cmath>

namespace kick_ui::audiosplit
{
    struct Result
    {
        bool   ok{ false };
        int    prevLength{ 0 };    // ticks of the first half
        int    newStartTime{ 0 };  // tick where the second half starts
        int    newLength{ 0 };     // ticks of the second half
        double prevLoopEnd{ 0.0 }; // end (source frames) of the first half = start of the second
        double newLoopEnd{ 0.0 };  // end (source frames) of the second half
    };

    // loopEnd == 0 means "the whole source" (sourceFrames). The cut must leave at least minTicks on both sides.
    inline Result compute( int startTime, int length, int loopStart, int loopEnd, int sourceFrames, double samplesPerTick, int cutTick, int minTicks = 1 )
    {
        Result r;
        if( length <= 0 || samplesPerTick <= 0.0 ) return r;
        const int prev = cutTick - startTime;
        if( prev < minTicks || length - prev < minTicks ) return r;
        r.ok           = true;
        r.prevLength   = prev;
        r.newStartTime = cutTick;
        r.newLength    = length - prev;
        r.newLoopEnd   = loopEnd == 0 ? (double)sourceFrames : (double)loopEnd;
        r.prevLoopEnd  = loopStart + prev * samplesPerTick;
        if( r.prevLoopEnd >= r.newLoopEnd ) r.prevLoopEnd = r.newLoopEnd; // a stretched length cannot run past the source
        return r;
    }
}
