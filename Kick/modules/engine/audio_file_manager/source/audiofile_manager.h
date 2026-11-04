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

#include <climits>

namespace mt
{
    class ASoundCache
    {
    public:
        ASoundCache();
        ~ASoundCache();

        void clear();

        /// Sample memory held by the cache. `unused` = only the cache itself references the sound (no slice plays it).
        struct Stats
        {
            int         count       = 0;
            juce::int64 bytes       = 0;
            int         unusedCount = 0;
            juce::int64 unusedBytes = 0;
        };
        Stats getStats() const;

        /// Drops the sounds nobody else references (message thread; never touches a sound a slice holds, so the audio
        /// thread is not affected). A dropped sound is decoded again by the next getOrLoad(). Returns what was freed.
        Stats purgeUnused();

        ASound::Ptr exists( const juce::String& File );
        ASound::Ptr getOrLoad( const juce::String& File, double LengthInSeconds = -1 );
        ASound::Ptr add( const juce::String& File, double LengthInSeconds = -1 );
        void        remove( const juce::String& File );

        ASound::Ptr getOrLoadMetronomeSound( const juce::String& File );

    private:
        // The list is read from the UI and loaded into from a background thread (the Program Edit waveform pages load off the
        // message thread), so every access to it takes this lock. The decode itself runs outside the lock.
        mutable juce::CriticalSection       lock;
        juce::AudioFormatManager            fmtMgr;
        juce::ReferenceCountedArray<ASound> aSounds;
        juce::ReferenceCountedArray<ASound> metronomeSounds;
    };

    /// API to obtain static instance of all currently loaded Sound sample cache.
    ASoundCache& getAudioSoundCache();

} // namespace mt
