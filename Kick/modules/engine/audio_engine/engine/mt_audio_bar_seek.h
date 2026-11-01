// SPDX-License-Identifier: AGPL-3.0-or-later
//
// Pure arithmetic of the bar seek (no JUCE, no engine types) so it can be unit tested on a host:
// Kick/tests/platform/BarSeekTests.cpp.
//
// Numbering: the engine counts bars from 0 (MTBarNBeats::bar, MTTempoMgr::getTicksUpto(bar)); the UI shows
// bar + 1. Everything in this header is 0-based.
//
// Tick model: the global CMidiClock hands RAW ticks to the sequence. The sequence works with
// SHIFTED = RAW + shift (mod 2^32), where shift is chosen at a seek so that SHIFTED(RAW_now) == target tick.
// All unsigned arithmetic is modulo 2^32 on purpose.

#pragma once

#include <cstdint>

namespace mt
{
    namespace bar_seek
    {
        // Clamp a 0-based bar into [0, lengthInBars - 1]. A length below 1 collapses to bar 0.
        constexpr int clampBar( int bar0, int lengthInBars ) noexcept
        {
            const int last = lengthInBars < 1 ? 0 : lengthInBars - 1;
            return bar0 < 0 ? 0 : ( bar0 > last ? last : bar0 );
        }

        // current + delta, clamped (no wrap around). Overflow safe.
        constexpr int relativeBar( int currentBar0, int deltaBars, int lengthInBars ) noexcept
        {
            const long long target = static_cast<long long>( currentBar0 ) + static_cast<long long>( deltaBars );
            const long long last   = lengthInBars < 1 ? 0 : static_cast<long long>( lengthInBars ) - 1;
            return static_cast<int>( target < 0 ? 0 : ( target > last ? last : target ) );
        }

        // Start of the LAST bar of the playable range, 0-based.
        //  - looping: loopEnd is an EXCLUSIVE 0-based end (MTSequence::onBar wraps when bar >= loopEnd, and the
        //    song code uses loopEnd - loopStart as the loop length in bars), so the last bar is loopEnd - 1,
        //    limited to the sequence length (loopEnd is stored independently of the length) and to >= 0.
        //  - not looping: length - 1.
        constexpr int lastBar( bool looping, int loopEnd, int lengthInBars ) noexcept
        {
            const int len = lengthInBars < 1 ? 1 : lengthInBars;
            const int end = looping ? ( loopEnd < 1 ? 1 : ( loopEnd > len ? len : loopEnd ) ) : len;
            return end - 1;
        }

        // shift such that shifted(rawNow, shift) == targetTick.
        constexpr uint32_t shiftFor( uint32_t targetTick, uint32_t rawNow ) noexcept { return static_cast<uint32_t>( targetTick - rawNow ); }

        constexpr uint32_t shifted( uint32_t rawTick, uint32_t shift ) noexcept { return static_cast<uint32_t>( rawTick + shift ); }

        // MTTrack::fillMidiMessages plays the positions (prevTick, tick], i.e. starting at prevTick + 1.
        // To make the first position played after a seek exactly the target (the bar's downbeat note), the track's
        // prevTick is target - 1; for target 0 this wraps to 0xFFFFFFFF and prevTick + 1 wraps back to 0.
        constexpr uint32_t prevTickFor( uint32_t targetTick ) noexcept { return static_cast<uint32_t>( targetTick - 1u ); }

        // Position inside a track / sequence of the given length in ticks (0 for an empty one).
        constexpr uint32_t wrapToLength( uint32_t posTicks, uint32_t lengthInTicks ) noexcept { return lengthInTicks ? posTicks % lengthInTicks : 0u; }
    } // namespace bar_seek
} // namespace mt
