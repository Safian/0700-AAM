// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) logic of the MIDI Control screen, so that it runs in the host test
// (Kick/tests/platform/MidiControlLogicTests.cpp): the control table (one table for the list, the enum order and the
// defaults), Q-Link Absolute / Relative CC values, toggle state, MIDI Learn row matching, flip, switch edge,
// CC -> pan / gain, XY de-duplication, monitor percentage.
#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace kick_ui::midictl
{
    // The 18 front-panel buttons in list order. The order of `controlEnums` in kick_MidiControl.h is the same (a static
    // check there compares the size); the list used to be one position off from the enum, so a physical + selected "Tap".
    struct ButtonDef
    {
        const char* name;
        int         defaultCc;
    };

    inline const std::array<ButtonDef, 18>& buttons()
    {
        static const std::array<ButtonDef, 18> table = { { { "Step Seq", 2 }, { "Next Seq", 3 }, { "Track Mute", 4 }, { "Sample Edit", 5 }, { "Program Edit", 6 }, { "Browser", 7 },
                                                           { "Tap", 9 }, { "Enter", 10 }, { "-", 11 }, { "+", 26 }, { "Erase", 12 }, { "Track Mix", 13 },
                                                           { "TC", 14 }, { "PAD Mute", 15 }, { "Copy", 16 }, { "Pad Mixer", 17 }, { "sampler", 18 }, { "XYFX", 19 } } };
        return table;
    }

    constexpr int kButtons = 18;
    constexpr int kQLinks  = 4;
    constexpr int kPads    = 16;

    // List index ranges of the Hardware tab list: buttons, Q-Links, pads, the two XY axes, Touch Strip.
    constexpr int kFirstQLink = kButtons;
    constexpr int kFirstPad   = kButtons + kQLinks;
    constexpr int kXAxis      = kFirstPad + kPads;
    constexpr int kYAxis      = kXAxis + 1;
    constexpr int kTouchStrip = kYAxis + 1;
    constexpr int kListSize   = kTouchStrip + 1;

    inline std::string qlinkName( int Index0 ) { return "Q-Link " + std::to_string( Index0 + 1 ); }
    inline std::string padName( int Index0 ) { return "Pad " + std::to_string( Index0 + 1 ); }

    inline std::vector<std::string> controlListNames()
    {
        std::vector<std::string> names;
        for( const auto& b : buttons() )
            names.emplace_back( b.name );
        for( int i = 0; i < kQLinks; ++i )
            names.push_back( qlinkName( i ) );
        for( int i = 0; i < kPads; ++i )
            names.push_back( padName( i ) );
        names.emplace_back( "XYFXX-Axis" );
        names.emplace_back( "XYFXY-Axis" );
        names.emplace_back( "Touch Strip" );
        return names;
    }

    enum class ListPanel
    {
        Button,
        QLink,
        Pad,
        Xy,
        Strip
    };

    // Which right-hand panel belongs to a list row (the XY rows used to be unreachable: `index == 37 || 38` after the pad range).
    inline ListPanel panelOfIndex( int Index ) noexcept
    {
        if( Index < kFirstQLink ) return ListPanel::Button;
        if( Index < kFirstPad ) return ListPanel::QLink;
        if( Index < kXAxis ) return ListPanel::Pad;
        if( Index <= kYAxis ) return ListPanel::Xy;
        return ListPanel::Strip;
    }

    // ---- Q-Link CC values
    // Relative CC: two's complement in 7 bits, 0x01 = +1, 0x7F = -1 (the Key 61 encoding), at most +-63 per message.
    inline int relativeCc( int Delta ) noexcept
    {
        const int d = std::clamp( Delta, -63, 63 );
        return d >= 0 ? d : 128 + d;
    }

    // Absolute CC: the running value moves by the delta and stays inside Low..High.
    inline int absoluteCc( int Current, int Delta, int Low, int High ) noexcept
    {
        if( High < Low ) std::swap( Low, High );
        return std::clamp( Current + Delta, std::clamp( Low, 0, 127 ), std::clamp( High, 0, 127 ) );
    }

    // ---- buttons
    // Toggle: a press flips the state and sends 127 / 0, the release sends nothing. Momentary: 127 on press, 0 on release.
    struct ButtonOut
    {
        bool send{ false };
        int  value{ 0 };
    };

    inline ButtonOut buttonCc( bool Momentary, bool Pressed, bool& ToggleState ) noexcept
    {
        if( Momentary )
            return { true, Pressed ? 127 : 0 };
        if( !Pressed )
            return { false, 0 };
        ToggleState = !ToggleState;
        return { true, ToggleState ? 127 : 0 };
    }

    // ---- pads
    // Pad velocity "Off" = fixed full velocity, "On" = the hit velocity. Never above 1.
    inline float padVelocity( bool VelocityOn, float Hit01 ) noexcept
    {
        return VelocityOn ? std::clamp( Hit01, 0.0f, 1.0f ) : 1.0f;
    }

    // ---- XY
    inline int scaleAxis( float Normalised, int Low, int High ) noexcept
    {
        const float n = std::clamp( Normalised, 0.0f, 1.0f );
        return std::clamp( static_cast<int>( std::lround( n * static_cast<float>( High - Low ) ) ) + Low, 0, 127 );
    }

    // Sends an axis value only when it changed (a drag used to send two CC per mouse move, ~1000 messages/s).
    struct AxisFilter
    {
        int last{ -1 };
        bool changed( int Value ) noexcept
        {
            if( Value == last )
                return false;
            last = Value;
            return true;
        }
        void reset() noexcept { last = -1; }
    };

    // The two axes of the XY screens (MIDI Control > XY tab and XY FX with the "MIDI CC" location) as controller messages: one engine,
    // so both send the same CC numbers, channels and ranges, each axis only when its value changed, nothing for a muted axis.
    struct XyAxisConfig
    {
        int  channel{ 1 };
        int  cc{ 24 };
        int  low{ 0 };
        int  high{ 127 };
        bool muted{ false };
    };
    struct XyCc
    {
        int channel{ 1 };
        int cc{ 0 };
        int value{ 0 };
    };
    class XyCcEngine
    {
    public:
        // Writes the messages to send for the position (0..1 each) into Out[0..1] and returns how many.
        int process( float X, float Y, const XyAxisConfig& Cx, const XyAxisConfig& Cy, XyCc* Out ) noexcept
        {
            int n = 0;
            n += axis( X, Cx, filterX_, Out + n );
            n += axis( Y, Cy, filterY_, Out + n );
            return n;
        }
        void reset() noexcept
        {
            filterX_.reset();
            filterY_.reset();
        }

    private:
        static int axis( float V, const XyAxisConfig& C, AxisFilter& F, XyCc* Out ) noexcept
        {
            if( C.muted )
                return 0;
            const int mapped = scaleAxis( V, C.low, C.high );
            if( !F.changed( mapped ) )
                return 0;
            *Out = { std::clamp( C.channel, 1, 16 ), std::clamp( C.cc, 0, 127 ), mapped };
            return 1;
        }
        AxisFilter filterX_, filterY_;
    };

    // ---- MC-18: input ports follow the devices that are plugged in / out while mapping is on
    struct PortDiff
    {
        std::vector<std::string> toOpen;  // available, not open yet
        std::vector<std::string> toClose; // open, no longer available
    };
    inline PortDiff diffPorts( const std::vector<std::string>& Open, const std::vector<std::string>& Available )
    {
        PortDiff d;
        for( const auto& a : Available )
            if( std::find( Open.begin(), Open.end(), a ) == Open.end() && std::find( d.toOpen.begin(), d.toOpen.end(), a ) == d.toOpen.end() )
                d.toOpen.push_back( a );
        for( const auto& o : Open )
            if( std::find( Available.begin(), Available.end(), o ) == Available.end() )
                d.toClose.push_back( o );
        return d;
    }

    // ---- MIDI Learn
    struct LearnRow
    {
        std::string type;   // "cc", "Note" or "None"
        int         channel{ 0 };
        int         number{ -1 };
        bool        flip{ false };
    };

    // A learned row reacts to its own controller only: same type, channel and number.
    inline bool learnMatches( const LearnRow& Row, const std::string& Type, int Channel, int Number ) noexcept
    {
        return Row.type == Type && Row.channel == Channel && Row.number == Number;
    }

    inline int flipped( int Value, bool Flip ) noexcept
    {
        const int v = std::clamp( Value, 0, 127 );
        return Flip ? 127 - v : v;
    }

    // Mute / Solo from a CC: edge, not toggle on every message: >= 64 is on, below is off.
    inline bool switchFromCc( int Value ) noexcept { return Value >= 64; }

    inline double ccToPan( int Value ) noexcept { return std::clamp( Value, 0, 127 ) / 127.0 * 2.0 - 1.0; }
    inline double ccToGain( int Value ) noexcept { return std::clamp( Value, 0, 127 ) / 127.0; }

    // CC 0..127 spread over an integer range (semi -12..12, fine -99..99, polyphony 1..64), rounded.
    inline int ccToRange( int Value, int Low, int High ) noexcept
    {
        const double n = std::clamp( Value, 0, 127 ) / 127.0;
        return std::clamp( static_cast<int>( std::lround( Low + n * ( High - Low ) ) ), std::min( Low, High ), std::max( Low, High ) );
    }

    // ---- monitor
    inline int velocityPercent( int Velocity127 ) noexcept { return std::clamp( ( std::clamp( Velocity127, 0, 127 ) * 100 + 63 ) / 127, 0, 100 ); }

    // Only the last N entries stay in the monitor.
    template<typename T>
    inline void trimTo( std::vector<T>& Entries, std::size_t Max )
    {
        if( Entries.size() > Max )
            Entries.erase( Entries.begin(), Entries.begin() + static_cast<std::ptrdiff_t>( Entries.size() - Max ) );
    }

    // ---- MC-22: the MIDI Learn matcher. The table rows (type, channel, number, flip) are turned into bindings; a message is matched
    // against all of them: every row that learned this controller reacts (two rows may share one), each with its own flip. Pure and
    // allocation-free per message except for the result; the screen rebuilds the bindings when the table changes (message thread).
    struct LearnBinding
    {
        int         row{ -1 };
        std::string type;
        int         channel{ -1 };
        int         number{ -1 };
        bool        flip{ false };
        std::string port; // empty = any input port
    };

    struct LearnHit
    {
        int row{ -1 };
        int value{ 0 }; // 0..127, flipped when the row says so
    };

    class MidiLearnEngine
    {
    public:
        void rebuild( std::vector<LearnBinding> Bindings ) { bindings_ = std::move( Bindings ); }
        std::size_t size() const noexcept { return bindings_.size(); }

        std::vector<LearnHit> match( const std::string& Type, int Channel, int Number, int RawValue127, const std::string& Port = {} ) const
        {
            std::vector<LearnHit> hits;
            for( const auto& b : bindings_ )
            {
                if( !b.port.empty() && !Port.empty() && b.port != Port )
                    continue;
                if( learnMatches( LearnRow{ b.type, b.channel, b.number, b.flip }, Type, Channel, Number ) )
                    hits.push_back( { b.row, flipped( RawValue127, b.flip ) } );
            }
            return hits;
        }

    private:
        std::vector<LearnBinding> bindings_;
    };
}
