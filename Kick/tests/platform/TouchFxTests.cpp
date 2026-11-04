// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for AudIO::TouchFx (Dsp/TouchFx.h): bit-exact bypass, filter responses, gate attack / release times, tempo-synced
// periods, beat repeat slice, phaser, no NaN. Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined TouchFxTests.cpp
#include "../../../AudIO/AudIO/Dsp/TouchFx.h"

#include <cmath>
#include <cstdio>
#include <vector>

static int g_fail = 0;
#define CHECK( a )                                                                      \
    do {                                                                                \
        if( !( a ) ) { std::printf( "FAIL %s:%d  %s\n", __FILE__, __LINE__, #a ); ++g_fail; } \
    } while( 0 )

using AudIO::TouchFx;

namespace
{
    constexpr double kRate = 48000.0;
    constexpr int    kBlock = 256;

    std::vector<float> sine( double Hz, int N, float Amp = 0.5f, double Phase = 0.0 )
    {
        std::vector<float> v( static_cast<size_t>( N ) );
        for( int i = 0; i < N; ++i )
            v[static_cast<size_t>( i )] = Amp * static_cast<float>( std::sin( 2.0 * 3.14159265358979 * Hz * i / kRate + Phase ) );
        return v;
    }

    // Runs the effect over mono-in-stereo audio in blocks.
    std::vector<float> run( TouchFx& fx, std::vector<float> L )
    {
        std::vector<float> R = L;
        for( size_t i = 0; i < L.size(); i += kBlock )
        {
            const int n = static_cast<int>( std::min<size_t>( kBlock, L.size() - i ) );
            float*    ch[2] = { L.data() + i, R.data() + i };
            fx.process( ch, 2, n );
        }
        return L;
    }

    double rms( const std::vector<float>& v, size_t from )
    {
        double s = 0;
        for( size_t i = from; i < v.size(); ++i ) s += static_cast<double>( v[i] ) * v[i];
        return std::sqrt( s / static_cast<double>( v.size() - from ) );
    }
}

int main()
{
    // ---- division periods: 120 BPM, 1/4 = a quarter bar = 1 beat = 0.5 s; 2 bar = 4 s
    CHECK( std::fabs( TouchFx::divisionSeconds( 3, 120.0 ) - 0.5 ) < 1e-9 );
    CHECK( std::fabs( TouchFx::divisionSeconds( 0, 120.0 ) - 4.0 ) < 1e-9 );
    CHECK( std::fabs( TouchFx::divisionSeconds( 7, 120.0 ) - 0.03125 ) < 1e-9 );
    CHECK( std::fabs( TouchFx::divisionSeconds( 3, 60.0 ) - 1.0 ) < 1e-9 ); // half the tempo, twice the period
    CHECK( TouchFx::divisionSeconds( 3, 0.0 ) > 0.0 );                       // unknown tempo does not divide by zero
    CHECK( std::fabs( TouchFx::cutoffHz( 0.f ) - 20.f ) < 1e-3f );
    CHECK( std::fabs( TouchFx::cutoffHz( 1.f ) - 20000.f ) < 1.f );

    const auto input = sine( 5000.0, 48000 );

    // ---- wet = 0, gate closed, Off: the audio is bit exact
    {
        TouchFx fx;
        fx.prepare( kRate, kBlock );
        TouchFx::Settings s;
        s.mode = TouchFx::Mode::FilterManual; s.gate = true; s.wet = 0.f;
        fx.set( s );
        CHECK( run( fx, input ) == input );
        s.wet = 1.f; s.gate = false;
        fx.set( s );
        CHECK( run( fx, input ) == input ); // gate never opened: nothing is touched
        s.mode = TouchFx::Mode::Off; s.gate = true;
        fx.set( s );
        CHECK( run( fx, input ) == input );
    }

    // ---- manual low pass: 5 kHz sine through a 200 Hz cutoff is strongly attenuated, a 15 kHz cutoff passes it
    {
        TouchFx fx;
        fx.prepare( kRate, kBlock );
        TouchFx::Settings s;
        s.mode = TouchFx::Mode::FilterManual; s.filter = TouchFx::FilterType::LowPass; s.gate = true; s.attackMs = 1.f;
        s.x = 0.33f; s.y = 0.f; // ~200 Hz
        fx.set( s );
        const auto low = run( fx, input );
        CHECK( rms( low, 24000 ) < 0.1 * rms( input, 24000 ) );

        TouchFx fx2;
        fx2.prepare( kRate, kBlock );
        s.x = 0.95f; // ~14 kHz
        fx2.set( s );
        const auto open = run( fx2, input );
        CHECK( rms( open, 24000 ) > 0.8 * rms( input, 24000 ) );
    }

    // ---- manual high pass: the opposite
    {
        const auto bass = sine( 100.0, 48000 );
        TouchFx fx;
        fx.prepare( kRate, kBlock );
        TouchFx::Settings s;
        s.mode = TouchFx::Mode::FilterManual; s.filter = TouchFx::FilterType::HighPass; s.gate = true; s.attackMs = 1.f; s.x = 0.66f; // ~2.0 kHz
        fx.set( s );
        CHECK( rms( run( fx, bass ), 24000 ) < 0.1 * rms( bass, 24000 ) );
    }

    // ---- band pass: the centre passes, far-away frequencies do not
    {
        TouchFx fx;
        fx.prepare( kRate, kBlock );
        TouchFx::Settings s;
        s.mode = TouchFx::Mode::FilterManual; s.filter = TouchFx::FilterType::BandPass; s.gate = true; s.attackMs = 1.f; s.x = 0.566f; s.y = 0.8f; // ~1.0 kHz, resonant
        fx.set( s );
        const auto centre = run( fx, sine( 1000.0, 48000 ) );
        TouchFx fx2;
        fx2.prepare( kRate, kBlock );
        fx2.set( s );
        const auto far = run( fx2, sine( 8000.0, 48000 ) );
        CHECK( rms( centre, 24000 ) > 3.0 * rms( far, 24000 ) );
    }

    // ---- attack / release: 63 % after about the attack time, back to ~0 after the release
    {
        TouchFx fx;
        fx.prepare( kRate, kBlock );
        TouchFx::Settings s;
        s.mode = TouchFx::Mode::FilterManual; s.filter = TouchFx::FilterType::LowPass; s.x = 0.f; s.gate = true; s.attackMs = 100.f; s.releaseMs = 200.f;
        fx.set( s );
        std::vector<float> dc( 48000, 0.5f ); // a DC level: a 20 Hz low pass keeps it, so the dry/wet mix shows nothing; use 5 kHz
        const auto in = sine( 5000.0, 48000 );
        const auto out = run( fx, in );
        // after 100 ms the gate is ~63 % open: the wet (almost silent) part has replaced ~63 % of the signal
        const size_t i100 = 4800;
        const double r0 = rms( std::vector<float>( out.begin(), out.begin() + 480 ), 0 );
        const double r1 = rms( std::vector<float>( out.begin() + static_cast<long>( i100 ), out.begin() + static_cast<long>( i100 ) + 480 ), 0 );
        const double rEnd = rms( std::vector<float>( out.begin() + 40000, out.end() ), 0 );
        const double rIn = rms( in, 0 );
        CHECK( r0 > 0.9 * rIn );                       // right after the touch the dry signal still dominates
        CHECK( r1 < 0.6 * rIn && r1 > 0.2 * rIn );     // 100 ms later: roughly 37 % of the dry level is left
        CHECK( rEnd < 0.05 * rIn );                    // fully open: only the filtered (silent) signal remains
        // release
        s.gate = false;
        fx.set( s );
        std::vector<float> longIn = sine( 5000.0, 96000 );
        const auto out2 = run( fx, longIn );
        const double e0 = rms( std::vector<float>( out2.begin(), out2.begin() + 480 ), 0 );
        const double eEnd = rms( std::vector<float>( out2.begin() + 90000, out2.end() ), 0 );
        CHECK( e0 < 0.3 * rIn );
        CHECK( eEnd > 0.99 * rIn );                    // closed again: the dry signal is back (and untouched)
        CHECK( fx.envelope() == 0.f );
    }

    // ---- tempo-synced filter sweep: 1/16 at 120 BPM = 125 ms = 6000 samples. The input repeats every 6000 samples, so the
    // output repeats too exactly when the sweep period is 6000 samples (and does not for another division).
    {
        std::vector<float> in( 96000 );
        for( size_t i = 0; i < in.size(); ++i )
            in[i] = 0.2f * static_cast<float>( std::sin( 2.0 * 3.14159265358979 * 50.0 * static_cast<double>( i ) / 6000.0 ) + std::sin( 2.0 * 3.14159265358979 * 130.0 * static_cast<double>( i ) / 6000.0 ) + std::sin( 2.0 * 3.14159265358979 * 340.0 * static_cast<double>( i ) / 6000.0 ) );
        auto periodDiff = [&]( int division ) {
            TouchFx fx;
            fx.prepare( kRate, kBlock );
            TouchFx::Settings s;
            s.mode = TouchFx::Mode::FilterBeat; s.filter = TouchFx::FilterType::LowPass; s.division = division; s.y = 1.f; s.gate = true; s.attackMs = 1.f; s.bpm = 120.0;
            fx.set( s );
            const auto out = run( fx, in );
            double d = 0, e = 0;
            for( size_t i = 48000; i < 60000; ++i ) { d += std::fabs( static_cast<double>( out[i] ) - out[i + 6000] ); e += std::fabs( static_cast<double>( out[i] ) ); }
            return d / e;
        };
        CHECK( periodDiff( 5 ) < 0.01 ); // 1/16: period 6000 samples
        CHECK( periodDiff( 4 ) > 0.1 );  // 1/8: period 12000, not 6000
    }

    // ---- beat repeat: while the gate is open the output is a loop of the audio before the touch, with the slice length
    {
        TouchFx fx;
        fx.prepare( kRate, kBlock );
        TouchFx::Settings s;
        s.mode = TouchFx::Mode::BeatRepeat; s.filter = TouchFx::FilterType::LowPass; s.division = 7; // 1/64 at 120 BPM = 1500 samples
        s.y = 1.f; s.gate = false; s.attackMs = 1.f; s.bpm = 120.0;
        fx.set( s );
        // ramp audio (never repeats on its own), long enough to fill the ring
        std::vector<float> ramp( 24000 );
        for( size_t i = 0; i < ramp.size(); ++i ) ramp[i] = 0.4f * static_cast<float>( std::sin( 0.01 * static_cast<double>( i ) * static_cast<double>( i ) / 100.0 ) );
        run( fx, ramp );
        s.gate = true;
        fx.set( s );
        std::vector<float> next( 24000 );
        for( size_t i = 0; i < next.size(); ++i ) next[i] = 0.4f * static_cast<float>( std::sin( 0.37 * static_cast<double>( i ) ) + 0.5 * std::sin( 0.011 * static_cast<double>( i ) * static_cast<double>( i ) ) );
        const auto out = run( fx, next );
        // periodic with 1500 samples once the gate is open
        double diffRepeat = 0, diffInput = 0;
        for( size_t i = 12000; i < 18000; ++i )
        {
            diffRepeat += std::fabs( static_cast<double>( out[i] ) - out[i + 1500] );
            diffInput += std::fabs( static_cast<double>( next[i] ) - next[i + 1500] );
        }
        CHECK( diffRepeat < 0.05 * diffInput );
    }

    // ---- phaser: runs, changes the signal, stays finite and bounded
    {
        for( auto mode : { TouchFx::Mode::PhaserManual, TouchFx::Mode::PhaserBeat } )
        {
            TouchFx fx;
            fx.prepare( kRate, kBlock );
            TouchFx::Settings s;
            s.mode = mode; s.gate = true; s.attackMs = 1.f; s.x = 0.5f; s.y = 1.f; s.division = 3; s.bpm = 120.0;
            fx.set( s );
            const auto in = sine( 800.0, 48000 );
            const auto out = run( fx, in );
            bool finite = true, bounded = true, changed = false;
            for( size_t i = 0; i < out.size(); ++i )
            {
                if( !std::isfinite( out[i] ) ) finite = false;
                if( std::fabs( out[i] ) > 4.f ) bounded = false;
                if( i > 24000 && std::fabs( out[i] - in[i] ) > 0.01f ) changed = true;
            }
            CHECK( finite && bounded && changed );
        }
    }

    // ---- extreme settings never produce NaN / Inf
    {
        TouchFx fx;
        fx.prepare( kRate, kBlock );
        const auto in = sine( 440.0, 48000, 1.f );
        for( int m = 1; m <= 5; ++m )
            for( int f = 0; f < 3; ++f )
            {
                TouchFx::Settings s;
                s.mode = static_cast<TouchFx::Mode>( m ); s.filter = static_cast<TouchFx::FilterType>( f );
                s.x = m % 2 ? 0.f : 1.f; s.y = 1.f; s.division = 7; s.gate = true; s.attackMs = 0.f; s.releaseMs = 0.f; s.wet = 1.f; s.bpm = 1e6;
                fx.set( s );
                const auto out = run( fx, in );
                for( float v : out ) CHECK( std::isfinite( v ) );
                s.gate = false; s.bpm = 0.0; s.division = 99;
                fx.set( s );
                for( float v : run( fx, in ) ) CHECK( std::isfinite( v ) );
            }
    }

    // ---- a mono block works too
    {
        TouchFx fx;
        fx.prepare( kRate, kBlock );
        TouchFx::Settings s;
        s.mode = TouchFx::Mode::FilterManual; s.gate = true; s.attackMs = 1.f; s.x = 0.2f;
        fx.set( s );
        auto in = sine( 5000.0, 4800 );
        float* ch[1] = { in.data() };
        for( int i = 0; i < 4800; i += kBlock )
        {
            float* c[1] = { ch[0] + i };
            fx.process( c, 1, std::min( kBlock, 4800 - i ) );
        }
        CHECK( std::isfinite( in[4000] ) );
    }

    if( g_fail == 0 )
        std::printf( "TouchFxTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
