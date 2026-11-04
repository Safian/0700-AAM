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

    std::puts( "SampleEditorActionsTests OK" );
    return 0;
}
