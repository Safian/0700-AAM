// SPDX-License-Identifier: AGPL-3.0-or-later
//
// User defaults of the pad colours per program type (the manual's "Make Default": the current colour settings become
// the defaults of all programs of the same type). Pure, JUCE-free: the text form stored in the settings file and the
// choice between the user default and the factory default. Covered by Kick/tests/platform/PadColourDefaultsTests.cpp.
#pragma once

#include <array>
#include <cstdlib>
#include <string>

namespace mt::padcolour
{
    constexpr int kNumPads     = 128;
    constexpr int kMaxColourId = 63;

    // Program types share their defaults in three groups (as the factory tables do): Drum, Clip, everything else.
    enum class Group
    {
        Drum,
        Clip,
        Other
    };

    // MTProgramType numbers: Drum = 0, Clip = 4; every other type is Other.
    inline Group groupOfType( int ProgramType ) { return ProgramType == 0 ? Group::Drum : ( ProgramType == 4 ? Group::Clip : Group::Other ); }

    inline const char* groupKey( Group G )
    {
        switch( G )
        {
            case Group::Drum: return "drum";
            case Group::Clip: return "clip";
            default: return "other";
        }
    }

    struct Defaults
    {
        std::array<int, kNumPads> colours{};
        int                       mode{ 4 };  // PadColorMode::DIM_VELOCITY
        int                       empty{ 0 }; // EmptyPadColorMode::Off
    };

    // "mode;empty;c0,c1,...,c127"
    inline std::string encode( const Defaults& D )
    {
        std::string s = std::to_string( D.mode ) + ";" + std::to_string( D.empty ) + ";";
        for( size_t i = 0; i < D.colours.size(); ++i )
        {
            if( i )
                s += ',';
            s += std::to_string( D.colours[i] );
        }
        return s;
    }

    // False (and Out untouched) for anything that is not exactly one mode, one empty mode and 128 colours in range.
    inline bool decode( const std::string& S, Defaults& Out )
    {
        Defaults    d;
        const char* p = S.c_str();
        char*       e = nullptr;
        long        v = std::strtol( p, &e, 10 );
        if( e == p || *e != ';' || v < 0 || v > 8 )
            return false;
        d.mode = (int)v;
        p      = e + 1;
        v      = std::strtol( p, &e, 10 );
        if( e == p || *e != ';' || v < 0 || v > 2 )
            return false;
        d.empty = (int)v;
        p       = e + 1;
        for( size_t i = 0; i < d.colours.size(); ++i )
        {
            v = std::strtol( p, &e, 10 );
            if( e == p || v < 0 || v > kMaxColourId )
                return false;
            d.colours[i] = (int)v;
            p            = e;
            if( i + 1 < d.colours.size() )
            {
                if( *p != ',' )
                    return false;
                ++p;
            }
        }
        if( *p != '\0' )
            return false;
        Out = d;
        return true;
    }

    // The default colour of a pad: the user default when one is stored, else the factory table.
    inline int colourFor( const Defaults* User, const std::array<int, kNumPads>& Factory, int Pad )
    {
        if( Pad < 0 || Pad >= kNumPads )
            return 0;
        return User != nullptr ? User->colours[(size_t)Pad] : Factory[(size_t)Pad];
    }
}
