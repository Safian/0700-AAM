// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) mapping of the XY screen: preset table, axis kinds, cell hit test, value text, attack / release curves and the
// gate state machine (touch / Latch). Host test: Kick/tests/platform/XyFxMapTests.cpp. The numeric mode / filter values are those of
// AudIO::TouchFx (the UI file that connects the two checks them with static_asserts).
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <string>

namespace kick_ui::xyfx
{
    enum class Axis
    {
        BeatDivision,
        Cutoff,
        Resonance,
        Depth
    };

    struct Preset
    {
        const char* name;
        Axis        x;
        Axis        y;
        int         mode;   // AudIO::TouchFx::Mode: 1 filter manual, 2 filter beat, 3 beat repeat, 4 phaser manual, 5 phaser beat
        int         filter; // AudIO::TouchFx::FilterType: 0 low, 1 band, 2 high
    };

    constexpr int kNumPresets = 11;

    inline const Preset& preset( int Index ) noexcept
    {
        static const Preset table[kNumPresets] = {
            { "Beat Repeat LPF", Axis::BeatDivision, Axis::Cutoff, 3, 0 },
            { "Beat Repeat BPF", Axis::BeatDivision, Axis::Cutoff, 3, 1 },
            { "Beat Repeat HPF", Axis::BeatDivision, Axis::Cutoff, 3, 2 },
            { "LPF Manual", Axis::Cutoff, Axis::Resonance, 1, 0 },
            { "BPF Manual", Axis::Cutoff, Axis::Resonance, 1, 1 },
            { "HPF Manual", Axis::Cutoff, Axis::Resonance, 1, 2 },
            { "LPF", Axis::BeatDivision, Axis::Depth, 2, 0 },
            { "BPF", Axis::BeatDivision, Axis::Depth, 2, 1 },
            { "HPF", Axis::BeatDivision, Axis::Depth, 2, 2 },
            { "Phaser Manual", Axis::Cutoff, Axis::Resonance, 4, 0 },
            { "Phaser", Axis::BeatDivision, Axis::Resonance, 5, 0 },
        };
        return table[std::clamp( Index, 0, kNumPresets - 1 )];
    }

    inline const char* axisName( Axis A ) noexcept
    {
        switch( A )
        {
            case Axis::BeatDivision: return "Beat Division";
            case Axis::Cutoff: return "Cutoff";
            case Axis::Resonance: return "Resonance";
            case Axis::Depth: return "Depth";
        }
        return "";
    }

    // ---- grid: 8 x 8 cells, `spacing` px between them (the drawing and the hit test use the same geometry)
    constexpr int kCells = 8;

    struct Cell
    {
        int column{ -1 };
        int row{ -1 };
    };

    inline float cellSize( float Extent, float Spacing ) noexcept
    {
        return ( Extent - ( kCells - 1 ) * Spacing ) / kCells;
    }

    // Cell under a point; a point in the gap belongs to the cell before it. Outside / no size: column -1.
    inline Cell cellAt( float X, float Y, float Width, float Height, float Spacing ) noexcept
    {
        if( !( Width > 0.f ) || !( Height > 0.f ) || X < 0.f || Y < 0.f || X > Width || Y > Height )
            return {};
        const float cw = cellSize( Width, Spacing ) + Spacing;
        const float ch = cellSize( Height, Spacing ) + Spacing;
        return { std::clamp( static_cast<int>( X / cw ), 0, kCells - 1 ), std::clamp( static_cast<int>( Y / ch ), 0, kCells - 1 ) };
    }

    // Position 0..1 across the grid; no size (the grid is not laid out yet) gives 0, never NaN.
    inline float normalised( float Position, float Extent ) noexcept
    {
        if( !( Extent > 0.f ) )
            return 0.f;
        return std::clamp( Position / Extent, 0.f, 1.f );
    }

    // X axis of a beat division grid is the column (0..7 = 2 bar .. 1/64).
    inline int divisionOfColumn( int Column ) noexcept { return std::clamp( Column, 0, kCells - 1 ); }

    // ---- value text
    inline const char* divisionName( int Division ) noexcept
    {
        static const char* names[kCells] = { "2 Bar", "1 Bar", "1/2", "1/4", "1/8", "1/16", "1/32", "1/64" };
        return names[std::clamp( Division, 0, kCells - 1 )];
    }

    // Value shown for an axis: Beat Division takes the column, the others a 0..1 position.
    inline std::string axisText( Axis A, float Value )
    {
        char buf[48];
        switch( A )
        {
            case Axis::BeatDivision: return divisionName( static_cast<int>( Value ) );
            case Axis::Cutoff:
            {
                const double hz = 20.0 * std::pow( 1000.0, std::clamp( static_cast<double>( Value ), 0.0, 1.0 ) );
                if( hz >= 1000.0 ) std::snprintf( buf, sizeof buf, "%.1f kHz", hz / 1000.0 );
                else std::snprintf( buf, sizeof buf, "%.0f Hz", hz );
                return buf;
            }
            case Axis::Resonance:
            {
                const double v = std::clamp( static_cast<double>( Value ), 0.0, 1.0 );
                std::snprintf( buf, sizeof buf, "Q %.1f", 0.5 + 11.5 * v * v );
                return buf;
            }
            case Axis::Depth:
                std::snprintf( buf, sizeof buf, "%.0f %%", 100.0 * std::clamp( static_cast<double>( Value ), 0.0, 1.0 ) );
                return buf;
        }
        return {};
    }

    // Short label of one grid cell along an axis (the value at the cell's centre): XY-15. Index 0..7, from the low end.
    inline std::string cellLabel( Axis A, int Index )
    {
        const int    i = std::clamp( Index, 0, kCells - 1 );
        const double c = ( i + 0.5 ) / kCells;
        char         buf[24];
        switch( A )
        {
            case Axis::BeatDivision: return divisionName( i );
            case Axis::Cutoff:
            {
                const double hz = 20.0 * std::pow( 1000.0, c );
                if( hz >= 1000.0 ) std::snprintf( buf, sizeof buf, "%.1fk", hz / 1000.0 );
                else std::snprintf( buf, sizeof buf, "%.0f", hz );
                return buf;
            }
            case Axis::Resonance: std::snprintf( buf, sizeof buf, "Q%.1f", 0.5 + 11.5 * c * c ); return buf;
            case Axis::Depth: std::snprintf( buf, sizeof buf, "%.0f%%", 100.0 * c ); return buf;
        }
        return {};
    }

    // Axis end labels of the grid (left / right or bottom / top).
    inline std::string axisLow( Axis A ) { return A == Axis::BeatDivision ? "2 Bar" : A == Axis::Cutoff ? "20 Hz" : A == Axis::Depth ? "0 %" : "Q 0.5"; }
    inline std::string axisHigh( Axis A ) { return A == Axis::BeatDivision ? "1/64" : A == Axis::Cutoff ? "20 kHz" : A == Axis::Depth ? "100 %" : "Q 12"; }

    // ---- attack / release sliders (0..100 %) -> milliseconds: 1 ms .. 2 s, squared so the short end has resolution
    inline float envelopeMs( float Percent ) noexcept
    {
        const float p = std::clamp( Percent, 0.f, 100.f ) / 100.f;
        return 1.f + p * p * 1999.f;
    }

    inline std::string envelopeText( float Percent )
    {
        char       buf[32];
        const float ms = envelopeMs( Percent );
        if( ms >= 1000.f ) std::snprintf( buf, sizeof buf, "%.2f s", ms / 1000.f );
        else std::snprintf( buf, sizeof buf, "%.0f ms", ms );
        return buf;
    }

    // ---- gate: finger down opens it; without Latch the finger up closes it, with Latch it stays open (a new touch moves the
    // position) until Latch is switched off. Switching the effect off, or leaving the screen, closes it.
    class Gate
    {
    public:
        bool open() const noexcept { return enabled_ && ( touching_ || ( latch_ && latched_ ) ); }
        bool touching() const noexcept { return touching_; }
        bool latch() const noexcept { return latch_; }

        void setEnabled( bool On ) noexcept
        {
            enabled_ = On;
            if( !On )
            {
                touching_ = false;
                latched_  = false;
            }
        }
        void touchDown() noexcept
        {
            if( !enabled_ ) return;
            touching_ = true;
            latched_  = true;
        }
        void touchUp() noexcept { touching_ = false; }
        void setLatch( bool On ) noexcept
        {
            latch_ = On;
            if( !On )
                latched_ = touching_; // releasing Latch lets go of what was held
        }
        void closeAll() noexcept
        {
            touching_ = false;
            latched_  = false;
        }

    private:
        bool enabled_{ false }, touching_{ false }, latched_{ false }, latch_{ false };
    };
}
