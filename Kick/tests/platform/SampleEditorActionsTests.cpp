// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the sample editor's JUCE-free DSP helpers: fades (all curves, in / out), normalise gain (incl. silence),
// stereo -> mono sum, edit-format guard, result file names.

#include "../../modules/engine/audio_engine/engine/mt_sample_dsp.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void check( bool ok, const char* why )
{
    if( !ok )
    {
        std::fprintf( stderr, "FAIL: %s\n", why );
        std::exit( 1 );
    }
}

static bool near( float a, float b, float eps = 1e-5f ) { return std::fabs( a - b ) <= eps; }

using namespace mt::sample_dsp;

int main()
{
    // fade gain curves
    for( int curve = 0; curve < 3; ++curve )
    {
        check( fadeGain( curve, 0.0 ) == 0.0f, "fade in starts silent" );
        check( fadeGain( curve, 1.0 ) == 1.0f, "fade in ends at unity" );
        check( fadeGain( curve, -1.0 ) == 0.0f && fadeGain( curve, 2.0 ) == 1.0f, "clamped" );
        float prev = -1.0f;
        for( int i = 0; i <= 100; ++i )
        {
            const float g = fadeGain( curve, i / 100.0 );
            check( g >= prev, "monotonic" );
            prev = g;
        }
    }
    check( near( fadeGain( 0, 0.5 ), 0.5f ), "linear mid" );
    check( fadeGain( 1, 0.5 ) > 0.5f, "log fades in faster than linear" );
    check( fadeGain( 2, 0.5 ) < 0.5f, "exp fades in slower than linear" );

    // applyFade on a constant signal: it really changes the samples (the old no-op did not)
    std::vector<float> in( 101, 1.0f );
    applyFade( in.data(), (int)in.size(), 0, true );
    check( in.front() == 0.0f && near( in.back(), 1.0f ) && near( in[50], 0.5f ), "linear fade in" );
    std::vector<float> out( 101, 1.0f );
    applyFade( out.data(), (int)out.size(), 0, false );
    check( near( out.front(), 1.0f ) && out.back() == 0.0f && near( out[50], 0.5f ), "linear fade out" );
    std::vector<float> one( 1, 1.0f );
    applyFade( one.data(), 1, 0, true );
    check( one[0] == 0.0f, "single sample fade in" );
    applyFade( nullptr, 10, 0, true );
    applyFade( one.data(), 0, 0, true ); // must not crash

    // normalise
    check( near( normaliseGain( 0.5f ), 2.0f ), "half-scale peak doubles" );
    check( normaliseGain( 0.0f ) == 1.0f, "silence untouched (no division by zero)" );
    check( normaliseGain( 1e-12f ) == 1.0f, "denormal-level peak untouched" );
    check( normaliseGain( INFINITY ) == 1.0f && normaliseGain( NAN ) == 1.0f, "non-finite peak untouched" );
    check( near( normaliseGain( 0.25f, 0.5f ), 2.0f ), "target level" );

    // stereo -> mono sum is an average
    check( near( monoSum( 1.0f, 1.0f ), 1.0f ), "sum of two full-scale channels does not clip" );
    check( near( monoSum( 0.5f, -0.5f ), 0.0f ), "antiphase cancels" );
    check( near( monoSum( 0.2f, 0.4f ), 0.3f ), "average" );

    // edit-format guard
    check( isEditableExtension( ".wav" ) && isEditableExtension( ".WAV" ) && isEditableExtension( ".wave" ), "wav editable" );
    check( !isEditableExtension( ".mp3" ) && !isEditableExtension( ".flac" ) && !isEditableExtension( ".aif" ) && !isEditableExtension( "" ), "other containers refused" );

    // result names
    check( resultFileName( "Kick", "x_edit" ) == "Kick.wav", "adds .wav" );
    check( resultFileName( "Kick.wav", "x_edit" ) == "Kick.wav", "keeps .wav" );
    check( resultFileName( "", "x_edit" ) == "x_edit.wav", "empty -> fallback" );
    check( resultFileName( "  ..  ", "x_edit" ) == "x_edit.wav", "dots / spaces -> fallback" );
    check( resultFileName( "../../etc/passwd", "x_edit" ) == "passwd.wav", "folders stripped" );
    check( resultFileName( "a:b*c", "x_edit" ) == "a_b_c.wav", "illegal characters replaced" );
    check( resultFileName( "C:\\temp\\snare", "x_edit" ) == "snare.wav", "windows path stripped" );

    // SE-06: Bit reduce DSP tests
    check( bitReduce( 0.73f, 24 ) == 0.73f, "24 bits leaves sample untouched" );
    check( bitReduce( 0.73f, 0 ) == 0.73f, "invalid bits leaves sample untouched" );
    // 2 bits: 1 << 1 = 2 levels -> steps at -1.0, -0.5, 0.0, 0.5, 1.0
    check( near( bitReduce( 0.49f, 2 ), 0.5f ), "2 bits quantizes 0.49 to 0.5" );
    check( near( bitReduce( 0.1f, 2 ), 0.0f ), "2 bits quantizes 0.1 to 0.0" );
    check( bitReduce( 1.5f, 4 ) == 1.0f, "clamped to +1.0" );
    check( bitReduce( -1.5f, 4 ) == -1.0f, "clamped to -1.0" );

    std::vector<float> brBuf = { 0.12f, -0.48f, 0.91f };
    applyBitReduce( brBuf.data(), 3, 2 );
    check( near( brBuf[0], 0.0f ) && near( brBuf[1], -0.5f ) && near( brBuf[2], 1.0f ), "applyBitReduce in place" );
    applyBitReduce( nullptr, 5, 4 ); // no crash

    // SE-15: dB conversion & clip check
    check( near( dBToLinear( 0.0f ), 1.0f ), "0 dB is unity linear" );
    check( near( dBToLinear( 6.0206f ), 2.0f, 1e-3f ), "+6 dB doubles" );
    check( linearToDB( 0.0f ) == -144.0f, "silence is -144 dB floor" );
    check( near( linearToDB( 1.0f ), 0.0f ), "unity is 0 dB" );
    check( !willGainClip( 0.5f, 3.0f ), "0.5 with +3dB does not clip" );
    check( willGainClip( 0.8f, 3.0f ), "0.8 with +3dB clips" );
    check( !willGainClip( 0.0f, 18.0f ), "silence never clips" );
    check( !willGainClip( -0.5f, 10.0f ), "invalid peak does not clip" );

    // SE-09: Energy envelope autocorrelation BPM estimation
    // Create a 50 Hz frame rate pulse train at 120 BPM (period = 0.5 s -> 25 frames)
    std::vector<float> flux120( 200, 0.0f );
    for( size_t i = 0; i < flux120.size(); i += 25 )
        flux120[i] = 1.0f;
    double bpm120 = estimateBpmFromFlux( flux120.data(), (int)flux120.size(), 50.0 );
    check( std::fabs( bpm120 - 120.0 ) <= 1.0, "detected 120 BPM from pulse train" );

    // Silence or flat signal returns default 120.0 BPM without error
    std::vector<float> fluxSilent( 100, 0.0f );
    check( estimateBpmFromFlux( fluxSilent.data(), (int)fluxSilent.size(), 50.0 ) == 120.0, "silent flux fallback 120 BPM" );
    check( estimateBpmFromFlux( nullptr, 0, 0.0 ) == 120.0, "null safe" );

    std::puts( "SampleEditorActionsTests OK" );
    return 0;
}
