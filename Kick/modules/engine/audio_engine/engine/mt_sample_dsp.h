// SPDX-License-Identifier: AGPL-3.0-or-later
// JUCE-free sample-editor DSP helpers (fades, normalise gain, stereo -> mono, file-name rules, edit-format guard).
// Host-tested in Kick/tests/platform/SampleEditorActionsTests.cpp.
#pragma once

#include <cmath>
#include <string>

namespace mt::sample_dsp
{
    // Fade curve ids as offered by the Process dialog: 0 Linear, 1 Log, 2 Exp.
    // Gain of a FADE IN at position t in [0,1]; a fade out uses fadeGain( curve, 1 - t ).
    //   Linear: t        Log: 1 - (1-t)^2 (fast rise, slow finish)        Exp: t^2 (slow start, fast finish)
    inline float fadeGain( int curve, double t )
    {
        if( !( t > 0.0 ) )
            return 0.0f;
        if( t >= 1.0 )
            return 1.0f;
        switch( curve )
        {
            case 1: return (float)( 1.0 - ( 1.0 - t ) * ( 1.0 - t ) );
            case 2: return (float)( t * t );
            default: return (float)t;
        }
    }

    // In-place fade over n samples of one channel: the first sample of a fade in is silent, the last is untouched.
    inline void applyFade( float* data, int n, int curve, bool fadeIn )
    {
        if( data == nullptr || n <= 0 )
            return;
        const double span = n > 1 ? (double)( n - 1 ) : 1.0;
        for( int i = 0; i < n; ++i )
        {
            const double t = (double)i / span;
            data[i] *= fadeGain( curve, fadeIn ? t : 1.0 - t );
        }
    }

    // Gain that brings `maxLevel` (linear peak, >= 0) to `targetLinear`; silence (peak 0) is left untouched instead of dividing by 0.
    inline float normaliseGain( float maxLevel, float targetLinear = 1.0f )
    {
        if( !( maxLevel > 1.0e-9f ) || !std::isfinite( maxLevel ) )
            return 1.0f;
        return targetLinear / maxLevel;
    }

    // Stereo -> mono "Sum": average of both channels (a plain sum would add 6 dB and clip).
    inline float monoSum( float left, float right ) { return 0.5f * ( left + right ); }

    // The editor rewrites files as WAV; any other container would be corrupted in place, so only WAV is edited.
    inline bool isEditableExtension( const std::string& extensionWithDot )
    {
        std::string e;
        for( char c : extensionWithDot )
            e += (char)( c >= 'A' && c <= 'Z' ? c - 'A' + 'a' : c );
        return e == ".wav" || e == ".wave";
    }

    // User supplied result name -> plain file name (no folders, no illegal characters), always ending in .wav; empty -> fallback.
    inline std::string resultFileName( std::string name, const std::string& fallback )
    {
        const auto slash = name.find_last_of( "/\\" );
        if( slash != std::string::npos )
            name = name.substr( slash + 1 );
        for( auto& c : name )
            if( c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|' || (unsigned char)c < 32 )
                c = '_';
        const auto b = name.find_first_not_of( " ." );
        name         = ( b == std::string::npos ) ? std::string() : name.substr( b, name.find_last_not_of( " ." ) - b + 1 );
        if( name.empty() )
            name = fallback;
        const auto n = name.size();
        const bool wav = n >= 4 && ( name.substr( n - 4 ) == ".wav" || name.substr( n - 4 ) == ".WAV" );
        return wav ? name : name + ".wav";
    }
}
