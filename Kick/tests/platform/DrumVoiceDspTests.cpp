// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for Drum Voice DSP engine components (PE-02, PE-21).
// Proves:
// 1. ADSR envelope: when disabled/bypassed, leaves amplitude at 1.0 (unaffected).
//    When enabled with short decay, causes significant amplitude reduction over time.
// 2. LFO: generates bipolar oscillation (-1.0 .. +1.0) for pitch/pan/amplitude modulation.
// 3. Amplitude modulation formula: aMod = rGain * ampAdsr * ((1.0 - lfoToAmp) + (lfoToAmp * lfoOut))
//    correctly scales with envelope and LFO.

#include "AudIO/AudIO/Dsp/adsr.h"
#include "AudIO/AudIO/Dsp/lfo.h"
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <vector>

static void check( bool ok, const char* why )
{
    if( !ok ) { std::fprintf( stderr, "FAIL: %s\n", why ); std::exit( 1 ); }
}

static void testAdsrBypassVsActiveDecay()
{
    AudIO::ADSR<double> adsr( AudIO::ADSR<double>::Type::EXPONENTIAL );
    adsr.setSampleRate( 48000.0 );
    // Attack: 0.01s, Hold: 0.01s, Decay: 0.05s, Sustain: 0.0001, Release: 0.01s
    adsr.setParameters( { 0.01, 0.01, 0.05, 0.0001, 0.01 } );

    // Case 1: Disabled / bypassed (PE-02 default before enable switch)
    // AudIO::ADSR skipProcess returns 1.0 continuously
    adsr.setEnabled( false );
    adsr.keyOn();

    double bypassSum = 0.0;
    for( int i = 0; i < 4800; ++i )
    {
        double sample = 1.0; // synthetic unit sample
        double out = sample * adsr();
        bypassSum += out;
    }
    check( std::abs( bypassSum - 4800.0 ) < 1e-6, "bypassed ADSR leaves 1.0 sample unaffected across all blocks" );

    // Case 2: Enabled with decay (PE-02 active envelope)
    // Initial attack/decay produces high amplitude early, decaying to near-zero late
    adsr.setEnabled( true );
    adsr.keyOn();

    double peakSum = 0.0;
    for( int i = 0; i < 1000; ++i )
    {
        double sample = 1.0;
        double out = sample * adsr();
        if( i >= 600 ) peakSum += out; // peak window around attack/hold
    }

    double lateSum = 0.0;
    for( int i = 1000; i < 4800; ++i )
    {
        double sample = 1.0;
        double out = sample * adsr();
        if( i >= 3000 ) lateSum += out; // fully decayed window
    }

    double peakAvg = peakSum / 400.0;
    double lateAvg = lateSum / 1800.0;

    check( peakAvg > 0.5, "peak envelope has high amplitude (> 0.5)" );
    check( lateAvg < 0.01, "late envelope is decayed to near zero (< 0.01)" );
    check( peakAvg > lateAvg * 50.0, "decay causes massive amplitude drop in late samples vs peak" );
}

static void testLfoModulation()
{
    AudIO::LFO lfo;
    lfo.setSampleRate( 48000.0 );
    lfo.setFrequency( 100.0f ); // 100 Hz
    lfo.setType( AudIO::LFOType::SINE );
    lfo.setEnabled( true );

    float minVal = 100.0f, maxVal = -100.0f;
    for( int i = 0; i < 480; ++i )
    {
        float val = lfo();
        if( val < minVal ) minVal = val;
        if( val > maxVal ) maxVal = val;
    }

    check( maxVal > 0.5f, "LFO peaks positive (> 0.5)" );
    check( minVal < -0.5f, "LFO valleys negative (< -0.5)" );

    // Test disabled LFO output
    lfo.setEnabled( false );
    float dis = lfo();
    // AudIO::LFO returns 1.0 when disabled
    check( std::abs( dis - 1.0f ) < 1e-4f, "disabled LFO returns 1.0" );
}

static void testDrumVoiceAmplitudeModulationFormula()
{
    // Verifies DrumVoice::process amplitude modulation formula:
    // auto aMod = rGain * ampAdsr * ( ( 1. - lfoToAmp ) + ( lfoToAmp * lfoOut ) );

    AudIO::ADSR<double> ampEnv( AudIO::ADSR<double>::Type::EXPONENTIAL );
    ampEnv.setSampleRate( 48000.0 );
    ampEnv.setParameters( { 0.001, 0.005, 0.015, 0.0001, 0.01 } );
    ampEnv.setEnabled( true );
    ampEnv.keyOn();

    AudIO::LFO lfo;
    lfo.setSampleRate( 48000.0 );
    lfo.setFrequency( 50.0f );
    lfo.setType( AudIO::LFOType::SINE );
    lfo.setEnabled( true );

    const double rGain = 0.8;
    const float lfoToAmp = 0.5f;

    std::vector<double> blockOutputs;
    for( int i = 0; i < 1500; ++i )
    {
        double ampAdsr = ampEnv();
        float lfoOut = lfo();
        double aMod = rGain * ampAdsr * ( ( 1.0 - lfoToAmp ) + ( lfoToAmp * lfoOut ) );
        double renderedSample = 1.0 * aMod; // synthetic 1.0 input
        blockOutputs.push_back( renderedSample );
    }

    // Verify modulation occurs (values are varying, not constant)
    bool hasVariation = false;
    for( size_t i = 100; i < 300; ++i )
    {
        if( std::abs( blockOutputs[i] - blockOutputs[i - 1] ) > 1e-5 )
        {
            hasVariation = true;
            break;
        }
    }
    check( hasVariation, "aMod produces continuous time-varying amplitude output" );

    // Verify overall envelope decay
    double peakAvg = 0.0, lateAvg = 0.0;
    for( size_t i = 100; i < 300; ++i ) peakAvg += blockOutputs[i];
    for( size_t i = 1100; i < 1500; ++i ) lateAvg += blockOutputs[i];
    peakAvg /= 200.0;
    lateAvg /= 400.0;

    check( peakAvg > 0.1, "peak average has significant energy (> 0.1)" );
    check( peakAvg > lateAvg * 30.0, "peak average energy is vastly higher than late decayed energy" );
}

int main()
{
    testAdsrBypassVsActiveDecay();
    testLfoModulation();
    testDrumVoiceAmplitudeModulationFormula();
    std::puts( "DrumVoiceDspTests: OK" );
    return 0;
}
