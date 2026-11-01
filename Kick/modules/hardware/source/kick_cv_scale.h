// SPDX-License-Identifier: AGPL-3.0-or-later
// MIDI step (0..127) -> 12 bit DAC value (0..4095). JUCE-free, header only.
#pragma once

#include <cstdint>

namespace mt
{
    inline int stepToDac( uint16_t step )
    {
        const int v = ( static_cast<int>( step ) * 4095 ) / 127;
        return v < 0 ? 0 : ( v > 4095 ? 4095 : v );
    }
}
