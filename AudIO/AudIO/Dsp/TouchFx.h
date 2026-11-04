// SPDX-License-Identifier: AGPL-3.0-or-later
// Touch FX: the effect behind the XY screen. Header only, platform independent (no JUCE), no allocation or lock on the
// audio thread: every buffer is sized in prepare(), the controls are atomics written by the UI thread.
//
// Modes (the XY presets): a manual state-variable filter (X = cutoff, Y = resonance), the same filter swept by a tempo-synced
// LFO (X = beat division, Y = depth), beat repeat (a slice of the last audio loops while the gate is open; X = slice length as
// a beat division, Y = filter cutoff), and a 4-stage phaser (manual: X = centre frequency, Y = feedback; synced: X = beat
// division of the sweep, Y = feedback). The gate (finger down, or held by Latch) opens the wet signal over `attack` ms and
// closes it over `release` ms. With wet = 0 or the gate fully closed the audio is not touched at all (bit exact).
#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace AudIO
{
    class TouchFx
    {
        struct RepeatBuffers
        {
            std::vector<float> ring, rep; // interleaved stereo
            int                size{ 0 }; // frames
        };

    public:
        enum class Mode : int
        {
            Off = 0,
            FilterManual,
            FilterBeat,
            BeatRepeat,
            PhaserManual,
            PhaserBeat
        };
        enum class FilterType : int
        {
            LowPass = 0,
            BandPass,
            HighPass
        };

        struct Settings
        {
            Mode       mode{ Mode::Off };
            FilterType filter{ FilterType::LowPass };
            float      x{ 0.f };          // 0..1 (manual cutoff / phaser centre) or ignored for beat modes
            float      y{ 0.f };          // 0..1
            int        division{ 0 };     // 0..7 = 2 bar .. 1/64 (beat modes)
            bool       gate{ false };     // finger down or latched
            float      attackMs{ 10.f };
            float      releaseMs{ 100.f };
            float      wet{ 1.f };        // 0..1
            double     bpm{ 120.0 };
        };

        // Length of a beat division in bars: 2 bar, 1 bar, 1/2, 1/4, 1/8, 1/16, 1/32, 1/64.
        static constexpr int   kNumDivisions = 8;
        static constexpr float kDivisionBars[kNumDivisions] = { 2.f, 1.f, .5f, .25f, .125f, .0625f, .03125f, .015625f };
        static constexpr float kMaxRepeatSeconds = 4.f;

        // Period of a division in seconds for a tempo (4 beats per bar).
        static double divisionSeconds( int Division, double Bpm ) noexcept
        {
            const int    d   = std::clamp( Division, 0, kNumDivisions - 1 );
            const double bpm = Bpm > 1.0 ? Bpm : 120.0;
            return static_cast<double>( kDivisionBars[d] ) * 4.0 * 60.0 / bpm;
        }

        // Cutoff of an X / Y position: exponential 20 Hz .. 20 kHz.
        static float cutoffHz( float Position ) noexcept
        {
            const float p = std::clamp( Position, 0.f, 1.f );
            return 20.f * std::pow( 1000.f, p );
        }

        // ---- UI thread
        void set( const Settings& S ) noexcept
        {
            // The slice buffers (about 3 MB at 48 kHz) are only built for a chain that uses beat repeat, here on the UI
            // thread, and published to the audio thread through an atomic pointer; they are never freed while it can run.
            if( S.mode == Mode::BeatRepeat && !repeat_.load( std::memory_order_acquire ) && rate_ > 0.0 )
            {
                auto buffers = std::make_unique<RepeatBuffers>();
                buffers->size = static_cast<int>( rate_ * kMaxRepeatSeconds );
                buffers->ring.assign( 2u * static_cast<std::size_t>( buffers->size ) + 2u, 0.f );
                buffers->rep.assign( buffers->ring.size(), 0.f );
                ownedRepeat_ = std::move( buffers );
                repeat_.store( ownedRepeat_.get(), std::memory_order_release );
            }
            mode_.store( static_cast<int>( S.mode ), std::memory_order_relaxed );
            filter_.store( static_cast<int>( S.filter ), std::memory_order_relaxed );
            x_.store( S.x, std::memory_order_relaxed );
            y_.store( S.y, std::memory_order_relaxed );
            division_.store( S.division, std::memory_order_relaxed );
            attack_.store( S.attackMs, std::memory_order_relaxed );
            release_.store( S.releaseMs, std::memory_order_relaxed );
            wet_.store( S.wet, std::memory_order_relaxed );
            bpm_.store( S.bpm, std::memory_order_relaxed );
            gate_.store( S.gate, std::memory_order_release );
        }

        // Automation playback: only the gesture (position and gate) changes, the preset and the envelope stay as set().
        void setGesture( float X, float Y, bool Gate ) noexcept
        {
            x_.store( X, std::memory_order_relaxed );
            y_.store( Y, std::memory_order_relaxed );
            gate_.store( Gate, std::memory_order_release );
        }

        // Switches the effect off (gate closed, nothing audible after the release).
        void close() noexcept { gate_.store( false, std::memory_order_release ); }

        // ---- audio thread
        void prepare( double SampleRate, int MaxBlock )
        {
            // Called with the audio stopped (device start): a changed rate drops buffers built for the old one.
            const double newRate = SampleRate > 1000.0 ? SampleRate : 48000.0;
            if( newRate != rate_ )
            {
                repeat_.store( nullptr, std::memory_order_release );
                ownedRepeat_.reset();
            }
            rate_      = newRate;
            ringPos_   = 0;
            repLen_    = 0;
            repeating_ = false;
            reset();
            (void)MaxBlock;
        }

        // True while the effect can change the audio (so a host may skip it otherwise).
        bool isActive() const noexcept
        {
            return mode_.load( std::memory_order_relaxed ) != static_cast<int>( Mode::Off ) && wet_.load( std::memory_order_relaxed ) > 0.f && ( gate_.load( std::memory_order_acquire ) || env_ > 1e-4f );
        }

        void process( float* const* Channels, int NumChannels, int NumSamples ) noexcept
        {
            if( NumChannels < 1 || NumSamples <= 0 )
                return;
            auto* rb = repeat_.load( std::memory_order_acquire ); // null until a beat repeat preset was chosen
            if( !isActive() )
            {
                // Not audible: the audio is left alone. Beat repeat still listens, so the slice it captures when the finger
                // goes down is the audio from just before that.
                wasGate_ = gate_.load( std::memory_order_acquire );
                if( rb && mode_.load( std::memory_order_relaxed ) == static_cast<int>( Mode::BeatRepeat ) )
                    for( int n = 0; n < NumSamples; ++n )
                    {
                        for( int c = 0; c < 2; ++c )
                            rb->ring[static_cast<std::size_t>( ringPos_ ) * 2u + static_cast<std::size_t>( c )] = Channels[std::min( c, NumChannels - 1 )][n];
                        ringPos_ = ( ringPos_ + 1 ) % rb->size;
                    }
                return;
            }
            const int  ch    = std::min( NumChannels, 2 );
            auto       mode  = static_cast<Mode>( mode_.load( std::memory_order_relaxed ) );
            if( mode == Mode::BeatRepeat && rb == nullptr )
                mode = Mode::Off; // buffers not published yet: leave the audio alone
            const auto ftype = static_cast<FilterType>( filter_.load( std::memory_order_relaxed ) );
            const float x    = x_.load( std::memory_order_relaxed );
            const float y    = y_.load( std::memory_order_relaxed );
            const int   div  = division_.load( std::memory_order_relaxed );
            const double bpm = bpm_.load( std::memory_order_relaxed );
            const float wet  = std::clamp( wet_.load( std::memory_order_relaxed ), 0.f, 1.f );
            const bool  gate = gate_.load( std::memory_order_acquire );
            const float aA   = coeff( attack_.load( std::memory_order_relaxed ) );
            const float aR   = coeff( release_.load( std::memory_order_relaxed ) );

            if( gate && !wasGate_ && mode == Mode::BeatRepeat )
                captureSlice( *rb, ch, std::clamp( static_cast<int>( divisionSeconds( div, bpm ) * rate_ ), 64, rb->size ) );
            wasGate_ = gate;

            const double lfoInc = 1.0 / std::max( 1.0, divisionSeconds( div, bpm ) * rate_ ); // cycles per sample

            // Pre-calculate block-invariant filter parameters outside the per-sample loop
            if( mode == Mode::FilterManual )
            {
                setFilter( ftype, cutoffHz( x ), 0.5f + 11.5f * y * y );
            }
            else if( mode == Mode::BeatRepeat )
            {
                setFilter( ftype, cutoffHz( y ), 1.f );
            }

            float phaserManualA = 0.f, phaserManualFb = 0.f;
            if( mode == Mode::PhaserManual )
            {
                const float centre = 100.f * std::pow( 80.f, std::clamp( x, 0.f, 1.f ) );
                const float t      = std::tan( 3.14159265f * std::min( centre, 0.45f * static_cast<float>( rate_ ) ) / static_cast<float>( rate_ ) );
                phaserManualA      = ( t - 1.f ) / ( t + 1.f );
                phaserManualFb     = 0.9f * std::clamp( y, 0.f, 1.f );
            }

            for( int n = 0; n < NumSamples; ++n )
            {
                const float target = gate ? 1.f : 0.f;
                env_ += ( target - env_ ) * ( target > env_ ? aA : aR );
                const float w = env_ * wet;

                float in[2] = { Channels[0][n], ch > 1 ? Channels[1][n] : Channels[0][n] };
                float fx[2] = { in[0], in[1] };

                lfoPhase_ += lfoInc;
                if( lfoPhase_ >= 1.0 ) lfoPhase_ -= 1.0;
                const float lfo = 0.5f + 0.5f * std::sin( 6.283185307f * static_cast<float>( lfoPhase_ ) ); // 0..1

                switch( mode )
                {
                    case Mode::FilterManual:
                        for( int c = 0; c < ch; ++c ) fx[c] = svf( c, in[c] );
                        break;
                    case Mode::FilterBeat:
                        setFilter( ftype, 150.f * std::pow( 2.f, 7.f * std::clamp( y, 0.f, 1.f ) * lfo ), 2.f );
                        for( int c = 0; c < ch; ++c ) fx[c] = svf( c, in[c] );
                        break;
                    case Mode::BeatRepeat:
                    {
                        // the live input keeps feeding the ring; what is heard is the captured slice
                        for( int c = 0; c < ch; ++c ) rb->ring[static_cast<std::size_t>( ringPos_ ) * 2u + static_cast<std::size_t>( c )] = in[c];
                        ringPos_ = ( ringPos_ + 1 ) % rb->size;
                        if( repeating_ && repLen_ > 0 )
                        {
                            for( int c = 0; c < ch; ++c ) fx[c] = rb->rep[static_cast<std::size_t>( repPos_ ) * 2u + static_cast<std::size_t>( c )];
                            repPos_ = ( repPos_ + 1 ) % repLen_;
                        }
                        for( int c = 0; c < ch; ++c ) fx[c] = svf( c, fx[c] );
                        break;
                    }
                    case Mode::PhaserManual:
                    {
                        for( int c = 0; c < ch; ++c )
                        {
                            float s = in[c] + phaserManualFb * phaseFb_[c];
                            for( int k = 0; k < 4; ++k )
                            {
                                const float out = -phaserManualA * s + ap_[c][k];
                                ap_[c][k]       = s + phaserManualA * out;
                                s               = out;
                            }
                            phaseFb_[c] = s;
                            fx[c]       = 0.5f * ( in[c] + s );
                        }
                        break;
                    }
                    case Mode::PhaserBeat:
                    {
                        const float centre = 300.f * std::pow( 8.f, lfo );
                        const float t      = std::tan( 3.14159265f * std::min( centre, 0.45f * static_cast<float>( rate_ ) ) / static_cast<float>( rate_ ) );
                        const float a      = ( t - 1.f ) / ( t + 1.f );
                        const float fb     = 0.9f * std::clamp( y, 0.f, 1.f );
                        for( int c = 0; c < ch; ++c )
                        {
                            float s = in[c] + fb * phaseFb_[c];
                            for( int k = 0; k < 4; ++k )
                            {
                                const float out = -a * s + ap_[c][k];
                                ap_[c][k]       = s + a * out;
                                s               = out;
                            }
                            phaseFb_[c] = s;
                            fx[c]       = 0.5f * ( in[c] + s );
                        }
                        break;
                    }
                    case Mode::Off: break;
                }

                for( int c = 0; c < ch; ++c )
                    Channels[c][n] = in[c] + ( fx[c] - in[c] ) * w;
                if( ch == 1 ) { /* mono block: only channel 0 was processed */ }
            }

            if( !gate && env_ < 1e-4f )
            {
                env_       = 0.f;
                repeating_ = false;
                reset();
            }
        }

        float envelope() const noexcept { return env_; }

    private:
        float coeff( float Ms ) const noexcept
        {
            const float samples = std::max( 1.f, Ms * 0.001f * static_cast<float>( rate_ ) );
            return 1.f - std::exp( -1.f / samples );
        }

        void reset() noexcept
        {
            for( auto& c : ic1_ ) c = 0.f;
            for( auto& c : ic2_ ) c = 0.f;
            for( auto& c : phaseFb_ ) c = 0.f;
            for( auto& c : ap_ ) for( auto& k : c ) k = 0.f;
        }

        void captureSlice( RepeatBuffers& B, int Channels, int Length ) noexcept
        {
            repLen_ = std::min( Length, B.size );
            // the last repLen_ frames before the current write position
            for( int i = 0; i < repLen_; ++i )
            {
                const int src = ( ringPos_ - repLen_ + i + B.size ) % B.size;
                for( int c = 0; c < 2; ++c )
                    B.rep[static_cast<std::size_t>( i ) * 2u + static_cast<std::size_t>( c )] = B.ring[static_cast<std::size_t>( src ) * 2u + static_cast<std::size_t>( std::min( c, Channels - 1 ) )];
            }
            repPos_    = 0;
            repeating_ = true;
        }

        void setFilter( FilterType T, float Fc, float Q ) noexcept
        {
            const float fc = std::clamp( Fc, 20.f, 0.45f * static_cast<float>( rate_ ) );
            const float g  = std::tan( 3.14159265f * fc / static_cast<float>( rate_ ) );
            k_             = 1.f / std::max( 0.3f, Q );
            a1_            = 1.f / ( 1.f + g * ( g + k_ ) );
            a2_            = g * a1_;
            a3_            = g * a2_;
            type_          = T;
        }

        float svf( int C, float V0 ) noexcept
        {
            const float v3 = V0 - ic2_[C];
            const float v1 = a1_ * ic1_[C] + a2_ * v3;
            const float v2 = ic2_[C] + a2_ * ic1_[C] + a3_ * v3;
            ic1_[C]        = 2.f * v1 - ic1_[C];
            ic2_[C]        = 2.f * v2 - ic2_[C];
            switch( type_ )
            {
                case FilterType::LowPass: return v2;
                case FilterType::BandPass: return v1;
                case FilterType::HighPass: return V0 - k_ * v1 - v2;
            }
            return V0;
        }

        // controls (UI thread writes)
        std::atomic<int>    mode_{ 0 };
        std::atomic<int>    filter_{ 0 };
        std::atomic<float>  x_{ 0.f }, y_{ 0.f };
        std::atomic<int>    division_{ 0 };
        std::atomic<bool>   gate_{ false };
        std::atomic<float>  attack_{ 10.f }, release_{ 100.f }, wet_{ 1.f };
        std::atomic<double> bpm_{ 120.0 };

        // state (audio thread only)
        double rate_{ 48000.0 };
        float  env_{ 0.f };
        bool   wasGate_{ false };
        double lfoPhase_{ 0.0 };
        float  a1_{ 0.f }, a2_{ 0.f }, a3_{ 0.f }, k_{ 1.f };
        FilterType type_{ FilterType::LowPass };
        float  ic1_[2]{}, ic2_[2]{}, phaseFb_[2]{};
        float  ap_[2][4]{};
        std::unique_ptr<RepeatBuffers>  ownedRepeat_;
        std::atomic<RepeatBuffers*>     repeat_{ nullptr };
        int    ringPos_{ 0 }, repLen_{ 0 }, repPos_{ 0 };
        bool   repeating_{ false };
    };
}
