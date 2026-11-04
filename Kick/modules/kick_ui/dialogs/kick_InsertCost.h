// SPDX-License-Identifier: AGPL-3.0-or-later
// Which Airwindows inserts are expensive. The numbers are the fraction of real time one stereo insert needs at 48 kHz in 64 frame blocks,
// measured on the Mac host by Kick/tests/platform/AllEffectsTests.cpp (INS-19; the ARMv7 MPC is several times slower, so what is a
// small fraction here can matter there). The test fails when an effect that measures above kHeavy is missing from this table.
#pragma once

#include <cstring>

namespace kick_ui::insert_cost
{
    constexpr double kHeavy     = 0.03; // listed from here on
    constexpr double kVeryHeavy = 0.2;  // a single insert that may not fit the MPC at all

    struct Entry
    {
        const char* effect;
        double      hostFraction;
    };

    inline constexpr Entry kTable[] = {
        { "Pockey", 0.68 },     { "BitGlitter", 0.049 }, { "LeadAmp", 0.042 },  { "FireAmp", 0.040 }, { "GrindAmp", 0.036 },
        { "BigAmp", 0.036 },    { "PocketVerbs", 0.035 }, { "MidAmp", 0.034 },  { "LilAmp", 0.033 },
    };

    enum class Level
    {
        Normal,
        Heavy,
        VeryHeavy
    };

    inline Level levelOf( const char* Effect ) noexcept
    {
        if( Effect == nullptr )
            return Level::Normal;
        for( const auto& e : kTable )
            if( std::strcmp( e.effect, Effect ) == 0 )
                return e.hostFraction >= kVeryHeavy ? Level::VeryHeavy : Level::Heavy;
        return Level::Normal;
    }

    inline double hostFraction( const char* Effect ) noexcept
    {
        if( Effect != nullptr )
            for( const auto& e : kTable )
                if( std::strcmp( e.effect, Effect ) == 0 )
                    return e.hostFraction;
        return 0.0;
    }
}
