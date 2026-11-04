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

// Pure (JUCE-free) logic shared by the mixer and mute screens, so it can run in the host tests
// (Kick/tests/platform/MixerLogicTests.cpp): Q-Link steps, toggle direction, row / index guards,
// gain <-> MIDI <-> dB conversion, row <-> track mapping.
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <string>
#include <vector>

namespace mt::mixlogic
{
    // A Q-Link / Data Dial delta is a signed tick count (clockwise = +n). One tick moves a control by one
    // increment of that control (gain / pan 0.01, level / int 1), exactly like the Data Dial.
    inline double qlinkDelta( int Count, double Increment ) noexcept
    {
        return static_cast<double>( Count ) * Increment;
    }

    inline double qlinkStep( double Current, int Count, double Increment, double Min, double Max ) noexcept
    {
        return std::clamp( Current + qlinkDelta( Count, Increment ), Min, Max );
    }

    inline int qlinkStepInt( int Current, int Count, int Min, int Max ) noexcept
    {
        return std::clamp( Current + Count, Min, Max );
    }

    // Mute / solo switches follow the direction: turn right = on, turn left = off, no movement = unchanged.
    inline bool toggleFromDelta( bool Current, int Count ) noexcept
    {
        return Count > 0 ? true : Count < 0 ? false :
                                              Current;
    }

    // Mute group step: 0 is "None" (the engine stores it as -1 or 0), 1..16 are the groups shown on the pad.
    inline int stepMuteGroup( int Current, int Count, int MaxGroup ) noexcept
    {
        return std::clamp( Current + Count, 0, MaxGroup );
    }

    // Row index guard: a row of a list of Size entries, or -1.
    inline int safeRow( int Row, int Size ) noexcept
    {
        return ( Row >= 0 && Row < Size ) ? Row : -1;
    }

    // Selected row for the Q-Link: the selected one when valid, else the first one, else -1 (empty list).
    inline int qlinkRow( int Selected, int Size ) noexcept
    {
        if( Size <= 0 )
            return -1;
        return safeRow( Selected, Size ) >= 0 ? Selected : 0;
    }

    // Gain (linear 0..1) -> dB. False for silence (display "-inf").
    inline bool gainToDb( double Gain, double& Db ) noexcept
    {
        if( !( Gain > 0.0 ) )
            return false;
        Db = 20.0 * std::log10( Gain );
        return true;
    }

    inline std::string formatGainDb( double Gain )
    {
        double db{ 0.0 };
        if( !gainToDb( Gain, db ) || db < -99.9 )
            return "-inf dB";
        char buf[32];
        std::snprintf( buf, sizeof( buf ), "%.1f dB", db == 0.0 ? 0.0 : db );
        return buf;
    }

    inline std::string formatPan( double Pan )
    {
        const int pct = static_cast<int>( std::lround( Pan * 100.0 ) );
        char      buf[16];
        if( pct == 0 )
            return "C";
        std::snprintf( buf, sizeof( buf ), "%c%d", pct < 0 ? 'L' : 'R', pct < 0 ? -pct : pct );
        return buf;
    }

    // Linear gain <-> MIDI 0..127 round trip (rounded, not truncated: 9 / 127 * 127 may be 8.99999).
    inline float midiToGain127( int Midi ) noexcept
    {
        return ( 1.f / 127.f ) * static_cast<float>( Midi );
    }

    inline int gainToMidi127( float Gain ) noexcept
    {
        return std::clamp( static_cast<int>( std::lround( Gain * 127.f ) ), 0, 127 );
    }

    // The Midi view of the channel mixer lists the non-audio tracks only. Row <-> track index mapping.
    // IsAudioTrack[i] says whether track i has an Audio program.
    inline std::vector<int> midiTrackRows( const std::vector<bool>& IsAudioTrack )
    {
        std::vector<int> rows;
        for( std::size_t i = 0; i < IsAudioTrack.size(); ++i )
            if( !IsAudioTrack[i] )
                rows.push_back( static_cast<int>( i ) );
        return rows;
    }

    inline int rowToTrack( const std::vector<int>& Rows, int Row ) noexcept
    {
        return safeRow( Row, static_cast<int>( Rows.size() ) ) >= 0 ? Rows[static_cast<std::size_t>( Row )] : -1;
    }

    inline int trackToRow( const std::vector<int>& Rows, int Track ) noexcept
    {
        auto it = std::find( Rows.begin(), Rows.end(), Track );
        return it == Rows.end() ? -1 : static_cast<int>( it - Rows.begin() );
    }

    // Bank offset of a Q-Link-addressed pad / track: Q-Link 1..4 address the first four pads of the shown bank.
    inline int bankPadIndex( int Bank, int Knob0to3 ) noexcept
    {
        return Bank * 16 + Knob0to3;
    }

    // Clear Solos model (track level): after clearing, no track is solo and no track is solo-muted.
    struct TrackSoloState
    {
        bool solo{ false };
        bool soloMuted{ false };
    };

    inline void clearAllSolos( std::vector<TrackSoloState>& Tracks ) noexcept
    {
        for( auto& t : Tracks )
            t = TrackSoloState{};
    }

    // Single solo behaviour: soloing one track un-solos the others; the others are solo-muted while any solo exists.
    inline void setSingleSolo( std::vector<TrackSoloState>& Tracks, int Track, bool On ) noexcept
    {
        for( auto& t : Tracks )
            t.solo = false;
        if( On && safeRow( Track, static_cast<int>( Tracks.size() ) ) >= 0 )
            Tracks[static_cast<std::size_t>( Track )].solo = true;
        bool any = false;
        for( auto& t : Tracks )
            any = any || t.solo;
        for( auto& t : Tracks )
            t.soloMuted = any && !t.solo;
    }
}
