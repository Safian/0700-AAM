// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) mapping of a program's pan value to a MIDI CC10 value.
#pragma once

namespace mt::midipan
{
    // Program pan range is PAN::MIN..PAN::MAX = -1..1 (AudIO/Includes/Constants.h), 0 = centre.
    // MIDI CC10: 0 = hard left, 64 = centre, 127 = hard right. Out-of-range input is clamped, NaN -> centre.
    // Linear, monotonic: left half spans 0..64, right half 64..127.
    inline int panToCC10( double Pan )
    {
        if( !( Pan == Pan ) )
            return 64;
        if( Pan <= -1.0 )
            return 0;
        if( Pan >= 1.0 )
            return 127;

        const double v = Pan < 0.0 ? 64.0 + Pan * 64.0 : 64.0 + Pan * 63.0;
        int          r = (int)( v + 0.5 );
        return r < 0 ? 0 : ( r > 127 ? 127 : r );
    }
} // namespace mt::midipan
