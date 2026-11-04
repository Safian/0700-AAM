// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) logic of the Q-Link Edit screen and of the Q-Link routing, so that it runs in the host test
// (Kick/tests/platform/QLinkLogicTests.cpp): title -> parameter table (one table for the lists and the handler),
// Q-Link steps with the control's own increment, toggle direction, bank-aware pad index, fast-turn acceleration,
// strict "CC<n>" property filter, and the decision where a hardware message goes.
#pragma once

#include "../../../common/source/kick_mixer_logic.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <cstdlib>
#include <string>

namespace kick_ui::knob_logic
{
    // What a Custom Q-Link can drive. None = the "OFF" entry or an unknown title: the Q-Link does nothing.
    enum class Param
    {
        None,
        // program scope
        Pan,
        Level,
        Fine,
        Semi,
        Poly,
        Mute,
        Solo,
        // pad scope
        PadLevel,
        PadPan,
        PadSemi,
        PadFine,
        PadMute,
        PadSolo
    };

    // Lower case, no blanks: "Global SemiTune" == "GlobalSemiTune" == " global semitune ".
    inline std::string normalise( const std::string& Title )
    {
        std::string out;
        for( unsigned char c : Title )
            if( !std::isspace( c ) )
                out += static_cast<char>( std::tolower( c ) );
        return out;
    }

    // The single title table of the screen (program scope). Both the parameter lists and the handler use it, so a
    // list entry without handler cannot exist.
    inline Param parseProgramParam( const std::string& Title )
    {
        const auto t = normalise( Title );
        if( t == "pan" ) return Param::Pan;
        if( t == "level" || t == "velocity" ) return Param::Level;
        if( t == "fine" || t == "globalfinetune" ) return Param::Fine;
        if( t == "semi" || t == "globalsemitune" ) return Param::Semi;
        if( t == "poly" || t == "polyphony" || t == "programpolyphony" ) return Param::Poly;
        if( t == "mute" ) return Param::Mute;
        if( t == "solo" ) return Param::Solo;
        return Param::None;
    }

    inline Param parsePadParam( const std::string& Title )
    {
        const auto t = normalise( Title );
        if( t == "level" ) return Param::PadLevel;
        if( t == "pan" ) return Param::PadPan;
        if( t == "semitune" ) return Param::PadSemi;
        if( t == "finetune" ) return Param::PadFine;
        if( t == "padmute" ) return Param::PadMute;
        if( t == "padsolo" ) return Param::PadSolo;
        return Param::None;
    }

    inline bool isSwitch( Param P ) noexcept
    {
        return P == Param::Mute || P == Param::Solo || P == Param::PadMute || P == Param::PadSolo;
    }

    // Value range of the knob display of a parameter.
    struct Range
    {
        double min{ 0 }, max{ 1 }, step{ 1 };
    };

    inline Range rangeOf( Param P ) noexcept
    {
        switch( P )
        {
            case Param::Pan:
            case Param::PadPan: return { -1.0, 1.0, 0.01 };
            case Param::Level:
            case Param::PadLevel: return { 0.0, 127.0, 1.0 };
            case Param::Fine:
            case Param::PadFine: return { -99.0, 99.0, 1.0 };
            case Param::Semi:
            case Param::PadSemi: return { -12.0, 12.0, 1.0 };
            case Param::Poly: return { 1.0, 64.0, 1.0 };
            default: return { 0.0, 1.0, 1.0 }; // switches, insert parameters use their own range
        }
    }

    // Fast turning: a Key 61 Q-Link reports N ticks for a quick turn. 1 stays 1, then it grows faster than linear.
    inline int acceleratedCount( int Count ) noexcept
    {
        const int n = std::abs( Count );
        if( n <= 1 )
            return Count;
        const int scaled = n * ( 1 + ( n - 1 ) / 2 );
        return Count < 0 ? -scaled : scaled;
    }

    // One tick = one increment of the control (same as the Data Dial), clamped to the range of the parameter.
    inline double stepValue( Param P, double Current, int Count ) noexcept
    {
        const auto r = rangeOf( P );
        const double v = mt::mixlogic::qlinkStep( Current, Count, r.step, r.min, r.max );
        // 0.01 steps drift in binary floating point; the stored pan is a two-decimal value.
        return r.step < 1.0 ? std::round( v * 100.0 ) / 100.0 : v;
    }

    // Insert parameter (normalised 0..1): 0.01 per tick.
    inline double stepInsert( double Current, int Count ) noexcept
    {
        return std::round( mt::mixlogic::qlinkStep( Current, Count, 0.01, 0.0, 1.0 ) * 1000.0 ) / 1000.0;
    }

    // Program note of a pad of the shown bank (0..15 within the bank). A negative bank / pad (nothing chosen yet) is 0.
    inline int padNote( int Bank, int Pad ) noexcept
    {
        return std::clamp( Bank, 0, 7 ) * 16 + std::clamp( Pad, 0, 15 );
    }

    // "CC<digits>" (upper case, as the engine writes them): the CC number, otherwise -1. Other properties of a MIDI
    // program (ProgCH, BankLSB, "Accent...") must never be sent as a controller.
    inline int ccNumberOfProperty( const std::string& Name ) noexcept
    {
        if( Name.size() < 3 || Name.size() > 5 || Name[0] != 'C' || Name[1] != 'C' )
            return -1;
        int n = 0;
        for( std::size_t i = 2; i < Name.size(); ++i )
        {
            if( Name[i] < '0' || Name[i] > '9' )
                return -1;
            n = n * 10 + ( Name[i] - '0' );
        }
        return n <= 127 ? n : -1;
    }

    // Where a hardware message of KickHome::handleCommand goes.
    enum class Route
    {
        KnobEditCustom, // Custom Q-Link mode: every Q-Link goes to the Knob Edit grid
        FocusedControl, // data dial / +/- / ENTER / slider of the touched control
        Screen,         // the current screen (Q-Link included, also after a touch on a control)
        None
    };

    struct RouteInput
    {
        bool isQLink{ false };          // knob message, not the data dial
        bool isNavigation{ false };     // arrows, +/-, data dial, knobs ...
        bool isPadButton{ false };
        bool hasFocusedControl{ false };
        int  qlinkMode{ 1 };            // 0 = Custom, 1 = Screen
        bool screenHandlesCommands{ false };
    };

    inline Route routeHardwareMessage( const RouteInput& In ) noexcept
    {
        if( In.isQLink && In.qlinkMode == 0 )
            return Route::KnobEditCustom;
        if( In.isNavigation && In.hasFocusedControl && !In.isPadButton && !In.isQLink )
            return Route::FocusedControl;
        if( In.screenHandlesCommands )
            return Route::Screen;
        return Route::None;
    }

    // ---- QL-10: what Q-Link 1..4 do on a screen in Screen mode. Every row below was read from that screen's knobCommand; a screen
    // that is not listed says so instead of guessing (its Q-Links may still work: Channel Mixer, the Sequence / Track / Program lists).
    struct ScreenQLinkRole
    {
        std::string name;        // short, fits a card
        std::string description; // what one click does
    };

    struct ScreenQLinkRoles
    {
        bool                             known{ false };
        std::string                      screen; // readable
        std::array<ScreenQLinkRole, 4>   roles;
    };

    // Key: the kick_ui::Screen enumerator name ("HomeGridView", "MenuLooper"), case and blanks ignored.
    inline ScreenQLinkRoles screenQLinkRoles( const std::string& ScreenKey )
    {
        const auto k = normalise( ScreenKey );
        using R      = ScreenQLinkRoles;
        const auto make = []( const char* Screen, ScreenQLinkRole A, ScreenQLinkRole B, ScreenQLinkRole C, ScreenQLinkRole D ) {
            return R{ true, Screen, { A, B, C, D } };
        };
        if( k == "homegridview" )
            return make( "Grid", { "Scroll vertical", "Scrolls the grid up / down" }, { "Scroll horizontal", "Scrolls the grid left / right" },
                         { "Zoom horizontal", "Time zoom" }, { "Zoom vertical", "Key zoom" } );
        if( k == "homexyview" )
            return make( "XY FX", { "Preset", "Steps the effect preset" }, { "Attack", "Opening time of the effect" }, { "Release", "Closing time of the effect" },
                         { "Wet / Dry", "Effect level" } );
        if( k == "menulooper" )
            return make( "Looper", { "Threshold", "Level that starts the recording" }, { "Monitor", "Input monitor level" }, { "Output gain", "Playback level of the loop" },
                         { "Output pan", "Playback pan of the loop" } );
        if( k == "menusampler" )
            return make( "Sampler", { "Max length", "Longest recording, coarse" }, { "Max length fine", "Longest recording, fine" }, { "Threshold", "Level that starts the recording" },
                         { "Monitor", "Input monitor level" } );
        if( k == "menusampleedit" )
            return make( "Sample Edit (Chop)", { "Start coarse", "Moves the start point in big steps" }, { "End coarse", "Moves the end point in big steps" },
                         { "Start fine", "Moves the start point in small steps" }, { "End fine", "Moves the end point in small steps" } );
        if( k == "menupadmixer" )
            return make( "Pad Mixer", { "Pad level", "Level of the selected pad" }, { "Pad pan", "Pan of the selected pad" }, { "Pad mute", "Right = muted, left = not" },
                         { "Pad solo", "Right = solo, left = not" } );
        if( k == "menutrackmute" )
            return make( "Track Mute", { "Track 1", "Mutes / unmutes the 1st track of the bank" }, { "Track 2", "Mutes / unmutes the 2nd track of the bank" },
                         { "Track 3", "Mutes / unmutes the 3rd track of the bank" }, { "Track 4", "Mutes / unmutes the 4th track of the bank" } );
        if( k == "menupadmute" )
            return make( "Pad Mute", { "Pad 1", "Mutes / unmutes the 1st pad of the bank" }, { "Pad 2", "Mutes / unmutes the 2nd pad of the bank" },
                         { "Pad 3", "Mutes / unmutes the 3rd pad of the bank" }, { "Pad 4", "Mutes / unmutes the 4th pad of the bank" } );
        if( k == "menupadperform" )
            return make( "Pad Perform", { "Scale", "Scale of the pads" }, { "Root note", "Root note of the scale" }, { "Octave", "Octave of the pads" },
                         { "Chord / step", "Chord when chords are shown, otherwise the step mode" } );
        if( k == "menulistedit" )
            return make( "List Edit", { "Pad", "Pad of the selected event" }, { "Length", "Length of the selected event" }, { "Velocity", "Velocity of the selected event" },
                         { "Probability", "Probability of the selected event" } );
        if( k == "menunextsequence" || k == "menusong" )
            return make( k == "menusong" ? "Song" : "Next Sequence", { "Tempo", "BPM of the selected sequence" }, { "Length", "Bars of the selected sequence" },
                         { "Loop start", "Loop start of the sequence" }, { "Loop end", "Loop end of the sequence" } );
        if( k == "inserts" )
            return make( "Inserts", { "Parameter 1", "First parameter of the shown insert" }, { "Parameter 2", "Second parameter of the shown insert" },
                         { "Parameter 3", "Third parameter of the shown insert" }, { "Parameter 4", "Fourth parameter of the shown insert" } );
        return R{};
    }
}
