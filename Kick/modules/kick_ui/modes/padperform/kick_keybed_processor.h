// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <vector>

namespace kick_hw
{
    // KeybedProcessor (PP-21, PP-21a-e, PP-30b): pure logic for keybed input processing.
    // Handles:
    // - Transpose (octave -3..+3, semitone -36..+36) and MIDI Channel (1..16) (PP-21a)
    // - Aftertouch enable / disable (PP-21a)
    // - Note-On velocity processing: Fixed / Variable, curves (Linear, Soft, Hard), gain,
    //   and white / black key sensitivity (PP-21b)
    // - Scale filter quantization (PP-21c)
    // - Internal / External routing (PP-21d)
    // - Pedal settings (sustain polarity, enable) (PP-21e)
    class KeybedProcessor
    {
    public:
        enum class VelocityMode
        {
            Variable = 0,
            Fixed
        };

        enum class VelocityCurve
        {
            Linear = 0,
            Soft1,
            Soft2,
            Hard1,
            Hard2
        };

        enum class Routing
        {
            Both = 0,
            InternalOnly,
            ExternalOnly
        };

        enum class PedalPolarity
        {
            Normal = 0, // open on release, closed on press (standard)
            Inverted    // closed on release, open on press
        };

        struct NoteResult
        {
            int  pitch{ 60 };
            int  velocity{ 100 };
            int  channel{ 1 };
            bool filtered{ false }; // true if dropped by routing or filter
        };

        KeybedProcessor() = default;

        // --- Transpose & Channel (PP-21a) ---
        void setOctave( int oct ) noexcept { octave_ = std::clamp( oct, -3, 3 ); }
        int  octave() const noexcept { return octave_; }

        void setTranspose( int semi ) noexcept { transpose_ = std::clamp( semi, -36, 36 ); }
        int  transpose() const noexcept { return transpose_; }

        void setMidiChannel( int ch ) noexcept { midiChannel_ = std::clamp( ch, 1, 16 ); }
        int  midiChannel() const noexcept { return midiChannel_; }

        void setAftertouchEnabled( bool en ) noexcept { aftertouchEnabled_ = en; }
        bool isAftertouchEnabled() const noexcept { return aftertouchEnabled_; }

        int soundingPitch( int rawKey ) const noexcept
        {
            return std::clamp( rawKey + octave_ * 12 + transpose_, 0, 127 );
        }

        // --- Velocity Processing (PP-21b) ---
        void         setVelocityMode( VelocityMode mode ) noexcept { velMode_ = mode; }
        VelocityMode velocityMode() const noexcept { return velMode_; }

        void setFixedVelocity( int vel ) noexcept { fixedVelocity_ = std::clamp( vel, 1, 127 ); }
        int  fixedVelocity() const noexcept { return fixedVelocity_; }

        void          setVelocityCurve( VelocityCurve curve ) noexcept { velCurve_ = curve; }
        VelocityCurve velocityCurve() const noexcept { return velCurve_; }

        void  setVelocityGain( float gain ) noexcept { velGain_ = std::clamp( gain, 0.1f, 4.0f ); }
        float velocityGain() const noexcept { return velGain_; }

        void  setWhiteKeySensitivity( float s ) noexcept { whiteKeySens_ = std::clamp( s, 0.25f, 2.0f ); }
        float whiteKeySensitivity() const noexcept { return whiteKeySens_; }

        void  setBlackKeySensitivity( float s ) noexcept { blackKeySens_ = std::clamp( s, 0.25f, 2.0f ); }
        float blackKeySensitivity() const noexcept { return blackKeySens_; }

        static bool isBlackKey( int pitch ) noexcept
        {
            const int pc = ( ( pitch % 12 ) + 12 ) % 12;
            return ( pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10 );
        }

        int processVelocity( int rawKey, int inputVel ) const noexcept
        {
            if( velMode_ == VelocityMode::Fixed )
                return fixedVelocity_;

            float v = static_cast<float>( std::clamp( inputVel, 1, 127 ) ) / 127.0f;

            // Apply key sensitivity (white vs black keys)
            const float sens = isBlackKey( rawKey ) ? blackKeySens_ : whiteKeySens_;
            v *= sens;

            // Apply curve
            switch( velCurve_ )
            {
                case VelocityCurve::Soft1:  v = std::sqrt( v ); break;
                case VelocityCurve::Soft2:  v = std::pow( v, 0.35f ); break;
                case VelocityCurve::Hard1:  v = v * v; break;
                case VelocityCurve::Hard2:  v = v * v * v; break;
                case VelocityCurve::Linear:
                default: break;
            }

            // Apply gain
            v *= velGain_;

            const int result = static_cast<int>( std::round( v * 127.0f ) );
            return std::clamp( result, 1, 127 );
        }

        // --- Scale Filter (PP-21c) ---
        void setScaleFilterEnabled( bool en ) noexcept { scaleFilterEnabled_ = en; }
        bool isScaleFilterEnabled() const noexcept { return scaleFilterEnabled_; }

        void setScale( int rootPc, const std::vector<int>& intervals )
        {
            rootPitchClass_ = std::clamp( rootPc, 0, 11 );
            scaleIntervals_ = intervals;
            rebuildScalePitchClasses();
        }

        int quantizeToScale( int pitch ) const noexcept
        {
            if( ! scaleFilterEnabled_ || validPitchClasses_.empty() )
                return pitch;

            const int pc = ( ( pitch % 12 ) + 12 ) % 12;
            const int oct = pitch / 12;

            // Find closest pitch class in scale
            int bestPc = validPitchClasses_[0];
            int bestDist = std::abs( pc - bestPc );
            for( int spc : validPitchClasses_ )
            {
                const int dist = std::abs( pc - spc );
                if( dist < bestDist )
                {
                    bestDist = dist;
                    bestPc = spc;
                }
            }

            int quantized = oct * 12 + bestPc;
            return std::clamp( quantized, 0, 127 );
        }

        // --- Routing (PP-21d) ---
        void    setRouting( Routing r ) noexcept { routing_ = r; }
        Routing routing() const noexcept { return routing_; }

        bool routeToInternal() const noexcept
        {
            return routing_ == Routing::Both || routing_ == Routing::InternalOnly;
        }

        bool routeToExternal() const noexcept
        {
            return routing_ == Routing::Both || routing_ == Routing::ExternalOnly;
        }

        // --- Pedal Settings (PP-21e) ---
        void          setSustainPedalPolarity( PedalPolarity pol ) noexcept { pedalPolarity_ = pol; }
        PedalPolarity sustainPedalPolarity() const noexcept { return pedalPolarity_; }

        void setSustainPedalEnabled( bool en ) noexcept { sustainPedalEnabled_ = en; }
        bool isSustainPedalEnabled() const noexcept { return sustainPedalEnabled_; }

        // Maps hardware pedal state (true = closed/active) considering polarity
        bool processSustainPedal( bool hwClosed ) const noexcept
        {
            if( ! sustainPedalEnabled_ ) return false;
            return ( pedalPolarity_ == PedalPolarity::Normal ) ? hwClosed : ! hwClosed;
        }

        // --- Full Note-On Pipeline ---
        NoteResult processNoteOn( int rawKey, int inputVel ) const noexcept
        {
            NoteResult res;
            res.channel = midiChannel_;
            res.pitch = quantizeToScale( soundingPitch( rawKey ) );
            res.velocity = processVelocity( rawKey, inputVel );
            res.filtered = ( routing_ == Routing::ExternalOnly );
            return res;
        }

        // Process aftertouch (returns 0 if disabled, else clamped 0..127)
        int processAftertouch( int rawPressure ) const noexcept
        {
            if( ! aftertouchEnabled_ ) return 0;
            return std::clamp( rawPressure, 0, 127 );
        }

    private:
        void rebuildScalePitchClasses()
        {
            validPitchClasses_.clear();
            if( scaleIntervals_.empty() ) return;

            int current = rootPitchClass_;
            validPitchClasses_.push_back( current % 12 );
            for( size_t i = 0; i < scaleIntervals_.size() - 1; ++i )
            {
                current += scaleIntervals_[i];
                validPitchClasses_.push_back( current % 12 );
            }
            std::sort( validPitchClasses_.begin(), validPitchClasses_.end() );
            validPitchClasses_.erase( std::unique( validPitchClasses_.begin(), validPitchClasses_.end() ), validPitchClasses_.end() );
        }

        int  octave_{ 0 };
        int  transpose_{ 0 };
        int  midiChannel_{ 1 };
        bool aftertouchEnabled_{ true };

        VelocityMode  velMode_{ VelocityMode::Variable };
        int           fixedVelocity_{ 127 };
        VelocityCurve velCurve_{ VelocityCurve::Linear };
        float         velGain_{ 1.0f };
        float         whiteKeySens_{ 1.0f };
        float         blackKeySens_{ 1.0f };

        bool             scaleFilterEnabled_{ false };
        int              rootPitchClass_{ 0 }; // C
        std::vector<int> scaleIntervals_{ 2, 2, 1, 2, 2, 2, 1 };
        std::vector<int> validPitchClasses_{ 0, 2, 4, 5, 7, 9, 11 };

        Routing       routing_{ Routing::Both };
        PedalPolarity pedalPolarity_{ PedalPolarity::Normal };
        bool          sustainPedalEnabled_{ true };
    };
} // namespace kick_hw
