/*
 * Copyright (c) 2021 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

#include <cstdint>
#include <string>

namespace mt::timedivmute
{
    // Default PPQN in Kick is 96 (96 ticks per quarter note).
    constexpr int kDefaultPPQN = 96;

    // Convert time division name to tick length at given PPQN.
    // Supports 1/16 to 2 Bar, including triplets (D-082, D-121).
    inline int divisionToTicks( const std::string& div, int ppqn = kDefaultPPQN )
    {
        // Straight notes
        if( div == "1/16" )                  return ppqn / 4;       // 24 ticks
        if( div == "1/8" )                   return ppqn / 2;       // 48 ticks
        if( div == "1/4" )                   return ppqn;           // 96 ticks
        if( div == "1/2" )                   return ppqn * 2;       // 192 ticks
        if( div == "1" || div == "1 Bar" )   return ppqn * 4;       // 384 ticks (in 4/4 time)
        if( div == "2" || div == "2 Bar" )   return ppqn * 8;       // 768 ticks (in 4/4 time)

        // Triplets: 3 triplet notes take the time of 2 straight notes of that value.
        if( div == "1/16T" || div == "1/16 T" ) return ( ppqn * 2 ) / ( 4 * 3 ); // 16 ticks
        if( div == "1/8T"  || div == "1/8 T" )  return ( ppqn * 2 ) / ( 2 * 3 ); // 32 ticks
        if( div == "1/4T"  || div == "1/4 T" )  return ( ppqn * 2 ) / 3;         // 64 ticks
        if( div == "1/2T"  || div == "1/2 T" )  return ( ppqn * 4 ) / 3;         // 128 ticks

        return ppqn / 4; // default to 1/16 (24 ticks)
    }

    // Calculates the next grid point strictly after currentTick (MU-12).
    // If gridTicks <= 0 or currentTick < 0, returns currentTick.
    inline int64_t nextGridPoint( int64_t currentTick, int gridTicks )
    {
        if( gridTicks <= 0 )
            return currentTick;
        if( currentTick < 0 )
            return 0;
        return ( ( currentTick / gridTicks ) + 1 ) * gridTicks;
    }

    // Checks whether a scheduled mute at scheduledTick should take effect at currentTick.
    inline bool shouldTrigger( int64_t currentTick, int64_t scheduledTick )
    {
        return scheduledTick >= 0 && currentTick >= scheduledTick;
    }
} // namespace mt::timedivmute
