// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) logic of the Sampler screen: slice maths in the recording's real sample rate (no truncation),
// time-ordered Pad Tap / Pad Hold slices with complete end points, collision-free file names.
// Host-tested in Kick/tests/platform/SamplerLogicTests.cpp.
#pragma once

#include <algorithm>
#include <cmath>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace kick_ui::sampler_logic
{
    // Seconds -> sample frame at the recording's real rate; rounded (never truncated to whole seconds), clamped to [0, totalFrames].
    inline int secondsToFrame( double seconds, double sampleRate, int totalFrames = 0x7fffffff )
    {
        if( !( seconds > 0.0 ) || !( sampleRate > 0.0 ) )
            return 0;
        const double f = std::floor( seconds * sampleRate + 0.5 );
        if( f >= (double)totalFrames )
            return totalFrames;
        return (int)f;
    }

    struct Slice
    {
        int pad;   // pad / key note the slice belongs to (-1 for plain Slice mode ordering)
        int start; // frames
        int end;   // frames, > start
    };

    // Slice mode: markers (seconds, ascending as recorded) cut the recording; the last slice ends at endSeconds.
    // Empty / inverted slices are dropped. Slice i belongs to pad i.
    inline std::vector<Slice> sliceModeSlices( std::vector<double> markers, double endSeconds, double sampleRate, int totalFrames )
    {
        std::sort( markers.begin(), markers.end() );
        std::vector<Slice> out;
        int                pad = 0;
        for( size_t i = 0; i < markers.size(); ++i )
        {
            const double endSec = ( i + 1 < markers.size() ) ? markers[i + 1] : endSeconds;
            const int    s      = secondsToFrame( markers[i], sampleRate, totalFrames );
            const int    e      = secondsToFrame( endSec, sampleRate, totalFrames );
            if( e > s )
                out.push_back( { pad++, s, e } );
        }
        return out;
    }

    // Pad Tap / Pad Hold: start (and optional end) time per pad. Slices come out in TIME order (not pad order). A missing end point
    // (bank change, recording stopped before the pad was released) ends at the next slice start, or at the recording end.
    inline std::vector<Slice> padSlices( const std::map<int, double>& starts, const std::map<int, double>& ends, double endSeconds, double sampleRate, int totalFrames )
    {
        std::vector<std::pair<double, int>> order; // start time, pad
        for( const auto& kv : starts )
            order.push_back( { kv.second, kv.first } );
        std::stable_sort( order.begin(), order.end(), []( const auto& a, const auto& b ) { return a.first < b.first; } );

        std::vector<Slice> out;
        for( size_t i = 0; i < order.size(); ++i )
        {
            const int pad     = order[i].second;
            double    endSec  = endSeconds;
            const auto endIt  = ends.find( pad );
            const bool hasEnd = endIt != ends.end() && endIt->second > order[i].first;
            if( hasEnd )
                endSec = endIt->second;
            else if( i + 1 < order.size() )
                endSec = order[i + 1].first;
            if( endSec > endSeconds )
                endSec = endSeconds;
            const int s = secondsToFrame( order[i].first, sampleRate, totalFrames );
            const int e = secondsToFrame( endSec, sampleRate, totalFrames );
            if( e > s )
                out.push_back( { pad, s, e } );
        }
        return out;
    }

    // "name.wav", then "name 1.wav", "name 2.wav" ... ; `exists` tells whether a file name is taken. Path separators and
    // characters that are illegal in file names are replaced; an empty name falls back to "New Sample".
    inline std::string sanitiseName( std::string name )
    {
        for( auto& c : name )
            if( c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || (unsigned char)c < 32 )
                c = '_';
        const auto b = name.find_first_not_of( " ." );
        if( b == std::string::npos )
            return "New Sample";
        const auto e = name.find_last_not_of( " ." );
        return name.substr( b, e - b + 1 );
    }

    inline std::string uniqueFileName( const std::string& name, const std::string& extension, const std::function<bool( const std::string& )>& exists )
    {
        const auto base = sanitiseName( name );
        auto       cand = base + extension;
        for( int n = 1; exists( cand ) && n < 100000; ++n )
            cand = base + " " + std::to_string( n ) + extension;
        return cand;
    }

    // Maximum recording length: the engine clamps to 600 s; the UI must offer exactly that.
    constexpr double kMaxRecordSeconds = 600.0;
    inline double    clampRecordSeconds( double seconds )
    {
        if( !std::isfinite( seconds ) || seconds < 1.0 )
            return 1.0;
        return seconds > kMaxRecordSeconds ? kMaxRecordSeconds : seconds;
    }
}
