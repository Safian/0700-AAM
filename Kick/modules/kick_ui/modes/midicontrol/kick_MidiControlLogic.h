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

    // ---- MC-23: Navigating button identification
    inline bool isNavigatingButton( const std::string& name ) noexcept
    {
        return name == "Step Seq" || name == "Next Seq" || name == "Sample Edit"
            || name == "Program Edit" || name == "Browser" || name == "Track Mix"
            || name == "Pad Mixer" || name == "sampler" || name == "XYFX";
    }

    // ---- MC-22: ControllerMap & MidiLearnEngine clean architecture
    struct ControllerMapItem
    {
        int         controlId{ -1 };
        std::string name;
        int         defaultCc{ 0 };
        int         midiChannel{ 1 };
        bool        momentary{ true };
        std::string ledMode{ "Never" }; // "Never", "Midi Input", "Local" (MC-20)
        int         lowRange{ 0 };
        int         highRange{ 127 };
        bool        isAbsolute{ true };
    };

    class ControllerMap
    {
    public:
        ControllerMap()
        {
            initDefaults();
        }

        void initDefaults()
        {
            items.clear();
            const auto& bTable = buttons();
            for( size_t i = 0; i < bTable.size(); ++i )
            {
                ControllerMapItem item;
                item.controlId   = static_cast<int>( i );
                item.name        = bTable[i].name;
                item.defaultCc   = bTable[i].defaultCc;
                item.momentary   = true;
                item.ledMode     = "Never";
                items.push_back( item );
            }
            for( int i = 0; i < kQLinks; ++i )
            {
                ControllerMapItem item;
                item.controlId   = kFirstQLink + i;
                item.name        = qlinkName( i );
                item.defaultCc   = 16 + i;
                item.isAbsolute  = true;
                items.push_back( item );
            }
            for( int i = 0; i < kPads; ++i )
            {
                ControllerMapItem item;
                item.controlId   = kFirstPad + i;
                item.name        = padName( i );
                item.defaultCc   = 36 + i;
                items.push_back( item );
            }
        }

        const std::vector<ControllerMapItem>& getItems() const noexcept { return items; }
        std::vector<ControllerMapItem>&       getItems() noexcept { return items; }

        const ControllerMapItem* findItem( int controlId ) const noexcept
        {
            for( const auto& item : items )
                if( item.controlId == controlId )
                    return &item;
            return nullptr;
        }

        ControllerMapItem* findItem( int controlId ) noexcept
        {
            for( auto& item : items )
                if( item.controlId == controlId )
                    return &item;
            return nullptr;
        }

    private:
        std::vector<ControllerMapItem> items;
    };

    struct MidiLearnBinding
    {
        std::string sourcePort; // empty = any
        std::string type;       // "cc", "Note", "PitchBend", "ProgramChange"
        int         channel{ 1 };
        int         number{ -1 }; // CC or Note number
        std::string target;       // target parameter description
        bool        flip{ false };
        bool        isToggle{ false };
        float       lastValue{ 0.0f };
    };

    struct ProcessedMapping
    {
        bool        matched{ false };
        std::string target;
        float       value{ 0.0f };
        bool        isToggle{ false };
    };

    class MidiLearnEngine
    {
    public:
        void clearBindings() noexcept { bindings.clear(); }

        void addBinding( const MidiLearnBinding& binding )
        {
            bindings.push_back( binding );
        }

        std::size_t numBindings() const noexcept { return bindings.size(); }
        const MidiLearnBinding& getBinding( std::size_t index ) const { return bindings.at( index ); }

        ProcessedMapping processIncoming( const std::string& type, int channel, int number, int rawValue127, const std::string& port = "" ) noexcept
        {
            for( auto& b : bindings )
            {
                if( !b.sourcePort.empty() && !port.empty() && b.sourcePort != port )
                    continue;
                if( b.type == type && b.channel == channel && ( b.number == number || b.number < 0 ) )
                {
                    const int   v    = flipped( rawValue127, b.flip );
                    const float norm = v / 127.0f;
                    b.lastValue      = norm;
                    return { true, b.target, norm, b.isToggle };
                }
            }
            return { false, "", 0.0f, false };
        }

    private:
        std::vector<MidiLearnBinding> bindings;
    };
}
