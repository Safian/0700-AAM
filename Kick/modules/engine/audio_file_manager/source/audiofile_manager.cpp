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

namespace mt
{
    /// <summary>
    ///  ASoundCache
    /// </summary>
    ///
    ASoundCache& getAudioSoundCache()
    {
        static ASoundCache cache;
        return cache;
    }

    ASoundCache::ASoundCache()
    {
        fmtMgr.registerBasicFormats();
    }

    ASoundCache::~ASoundCache()
    {
        clear();
    }

    void ASoundCache::clear()
    {
        const juce::ScopedLock sl( lock );
        aSounds.clear();
    }

    ASoundCache::Stats ASoundCache::getStats() const
    {
        const juce::ScopedLock sl( lock );
        Stats                  st;
        for( const auto& it : aSounds )
        {
            const auto bytes = it->getMemoryBytes();
            ++st.count;
            st.bytes += bytes;
            if( it->getReferenceCount() == 1 ) // the cache's own reference
            {
                ++st.unusedCount;
                st.unusedBytes += bytes;
            }
        }
        return st;
    }

    ASoundCache::Stats ASoundCache::purgeUnused()
    {
        const juce::ScopedLock sl( lock );
        Stats                  freed;
        for( int i = aSounds.size(); --i >= 0; )
        {
            auto* sound = aSounds.getObjectPointerUnchecked( i );
            if( sound->getReferenceCount() != 1 )
                continue;
            ++freed.count;
            freed.bytes += sound->getMemoryBytes();
            aSounds.remove( i );
        }
        freed.unusedCount = freed.count;
        freed.unusedBytes = freed.bytes;
        return freed;
    }

    ASound::Ptr ASoundCache::exists( const juce::String& File )
    {
        const juce::ScopedLock sl( lock );
        for( auto it : aSounds )
        {
            if( 0 == File.compare( it->getFilePath() ) )
                return it;
            if( juce::File( File ).getFileName() == it->getFileName() )
                return it;
        }
        return nullptr;
    }

    ASound::Ptr ASoundCache::getOrLoad( const juce::String& File, double LengthInSeconds )
    {
        if( auto ptr = exists( File ) )
            return ptr;

        return add( File );
    }

    void ASoundCache::remove( const juce::String& File )
    {
        const juce::ScopedLock sl( lock );
        aSounds.removeObject( exists( File ) );
    }

#define ASOUND_UPSAMPLE_RATIO ( 1 )

    ASound::Ptr ASoundCache::add( const juce::String& File, double LengthInSeconds )
    {
        if( File.isNotEmpty() )
        {
            if( juce::File( File ).existsAsFile() )
            {
                if( auto ptr = new ASound( File, fmtMgr, ASOUND_UPSAMPLE_RATIO, LengthInSeconds ) )
                {
                    const juce::ScopedLock sl( lock );
                    return aSounds.add( ptr );
                }
            }
        }
        return nullptr;
    }

    ASound::Ptr ASoundCache::getOrLoadMetronomeSound( const juce::String& File )
    {
        for( auto it : metronomeSounds )
        {
            if( 0 == File.compare( it->getFilePath() ) )
                return it;
        }

        if( File.isNotEmpty() && juce::File( File ).existsAsFile() )
        {
            if( auto ptr = new ASound( File, fmtMgr, ASOUND_UPSAMPLE_RATIO ) )
            {
                return metronomeSounds.add( ptr );
            }
        }
        return nullptr;
    }

#undef ASOUND_UPSAMPLE_RATIO

} // namespace mt
