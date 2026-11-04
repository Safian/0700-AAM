// SPDX-License-Identifier: AGPL-3.0-or-later
// Layer Play: which layer of a pad / key sounds (JUCE-free, header only; host test: Kick/tests/platform/LayerPlayTests.cpp).
//
//  CYCLE    - round robin over the layers that have a sound; wraps around (it used to stick on the last layer).
//  RANDOM   - one of the layers that have a sound.
//  VELOCITY - the layer whose velocity range holds the hit; the range is CLOSED (a 127 hit finds a 1..127 layer: the Full
//             Level velocity used to fall through to layer 0). No layer matches: the first layer with a sound.
// The result is always one of the layers that HAVE a sound (the position in the list was used as the layer number, so an
// empty layer in between played the wrong or an empty layer).
#pragma once

#include <vector>

namespace mt::layerplay
{
    enum class Mode
    {
        Cycle,
        Velocity,
        Random
    };

    struct Candidate
    {
        int layer;    // layer number 0..3
        int velStart; // 1..127
        int velEnd;
    };

    // cycleState: per pad / key, start at -1. randomIndex: any non-negative random number (taken modulo the count).
    // Returns the layer number, -1 when there is no candidate.
    inline int select( Mode mode, const std::vector<Candidate>& candidates, int velocity, int& cycleState, unsigned randomIndex )
    {
        if( candidates.empty() ) return -1;
        const int n = (int)candidates.size();
        switch( mode )
        {
            case Mode::Cycle:
                cycleState = ( cycleState + 1 ) % n;
                if( cycleState < 0 ) cycleState = 0;
                return candidates[(size_t)cycleState].layer;
            case Mode::Random:
                return candidates[(size_t)( randomIndex % (unsigned)n )].layer;
            case Mode::Velocity:
                for( const auto& c : candidates )
                    if( c.velStart <= velocity && velocity <= c.velEnd ) return c.layer;
                return candidates.front().layer;
        }
        return candidates.front().layer;
    }
}
