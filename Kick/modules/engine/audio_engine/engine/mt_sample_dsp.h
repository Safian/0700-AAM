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

    // SE-06: Bit reduce DSP quantizer. Quantizes linear float sample [-1.0, 1.0] to `bits` resolution (1 to 16 bits).
    inline float bitReduce( float sample, int bits )
    {
        if( bits >= 24 || bits < 1 )
            return sample;
        const float levels = (float)( 1 << ( bits - 1 ) );
        if( !( levels > 0.0f ) )
            return sample;
        float q = std::round( sample * levels ) / levels;
        if( q > 1.0f ) q = 1.0f;
        if( q < -1.0f ) q = -1.0f;
        return q;
    }

    inline void applyBitReduce( float* data, int n, int bits )
    {
        if( data == nullptr || n <= 0 || bits >= 24 || bits < 1 )
            return;
        for( int i = 0; i < n; ++i )
            data[i] = bitReduce( data[i], bits );
    }

    // SE-15: dB to linear and clipping check.
    inline float dBToLinear( float db )
    {
        return std::pow( 10.0f, db / 20.0f );
    }

    inline float linearToDB( float lin )
    {
        if( !( lin > 1.0e-9f ) || !std::isfinite( lin ) )
            return -144.0f;
        return 20.0f * std::log10( lin );
    }

    inline bool willGainClip( float peakLinear, float gainDb )
    {
        if( !( peakLinear > 0.0f ) || !std::isfinite( peakLinear ) )
            return false;
        const float scaled = peakLinear * dBToLinear( gainDb );
        return scaled > 1.0001f;
    }

    // SE-09: Energy envelope autocorrelation BPM estimation.
    // Given an array of RMS/energy flux blocks computed at frameRateHz (e.g. sampleRate / blockSize),
    // finds the most periodic tempo between minBpm (default 60) and maxBpm (default 180).
    inline double estimateBpmFromFlux( const float* flux, int numFrames, double frameRateHz, double minBpm = 60.0, double maxBpm = 180.0 )
    {
        if( flux == nullptr || numFrames < 16 || !( frameRateHz > 0.0 ) )
            return 120.0;

        const int minLag = (int)std::floor( ( 60.0 / maxBpm ) * frameRateHz );
        const int maxLag = (int)std::ceil( ( 60.0 / minBpm ) * frameRateHz );
        if( minLag < 1 || maxLag >= numFrames )
            return 120.0;

        int bestLag = minLag;
        float bestCorr = -1.0f;

        for( int lag = minLag; lag <= maxLag; ++lag )
        {
            float sum = 0.0f;
            int count = 0;
            for( int i = 0; i + lag < numFrames; ++i )
            {
                sum += flux[i] * flux[i + lag];
                ++count;
            }
            if( count > 0 )
            {
                sum /= (float)count;
                if( sum > bestCorr )
                {
                    bestCorr = sum;
                    bestLag = lag;
                }
            }
        }

        if( bestCorr <= 1.0e-6f || bestLag <= 0 )
            return 120.0;

        return ( 60.0 * frameRateHz ) / (double)bestLag;
    }

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
