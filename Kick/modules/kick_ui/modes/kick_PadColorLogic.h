/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 *
 * Pad colour mode mapping of the Color Edit screen: (Pad Off, Pad On, Empty Pad) <-> one of the nine engine
 * modes. No JUCE, no engine: header only, covered by Kick/tests/platform/PadColorLogicTests.cpp.
 * The numbers equal mt::PadColorMode / mt::PadModeState (checked by static_asserts in kick_PadColor.cpp).
 */
#pragma once

namespace kick_ui
{
    namespace pad_color_logic
    {
        enum Mode : int
        {
            Invalid = -1,
            Off     = 0,
            OffVelocity,
            OffFull,
            DimOff, // Pad Off = Dim: empty pads follow the Empty Pad Dim setting (empty dim)
            DimVelocity,
            DimFull,
            OffDimOff, // Pad Off = Dim, empty pads off
            OffDimVelocity,
            OffDimFull
        };

        constexpr int kPadOffDim = 0; // PAD_OFF_STATE::PAD_OFF_DIM
        constexpr int kPadOffOff = 1; // PAD_OFF_STATE::PAD_OFF_OFF
        constexpr int kOnVelocity = 0; // PAD_ON_STATE::PAD_ON_VELOCITY
        constexpr int kOnFull     = 1;
        constexpr int kOnOff      = 2;

        // The Empty Pad choice only exists when Pad Off is Dim; with Pad Off = Off empty pads are off anyway.
        // Never returns Invalid: an unknown selection falls back to the nearest valid one.
        inline Mode encode( int PadOff, int PadOn, bool EmptyDim )
        {
            const int on = PadOn < kOnVelocity ? kOnVelocity : ( PadOn > kOnOff ? kOnOff : PadOn );
            if( PadOff == kPadOffDim )
            {
                if( EmptyDim )
                    return on == kOnVelocity ? DimVelocity : ( on == kOnFull ? DimFull : DimOff );
                return on == kOnVelocity ? OffDimVelocity : ( on == kOnFull ? OffDimFull : OffDimOff );
            }
            return on == kOnVelocity ? OffVelocity : ( on == kOnFull ? OffFull : Off );
        }

        inline int decodePadOff( int M )
        {
            switch( M )
            {
                case DimOff:
                case DimVelocity:
                case DimFull:
                case OffDimOff:
                case OffDimVelocity:
                case OffDimFull: return kPadOffDim;
                default: return kPadOffOff;
            }
        }

        inline int decodePadOn( int M )
        {
            switch( M )
            {
                case OffVelocity:
                case DimVelocity:
                case OffDimVelocity: return kOnVelocity;
                case OffFull:
                case DimFull:
                case OffDimFull: return kOnFull;
                default: return kOnOff;
            }
        }

        // True = empty pads dim. Only DIM_* modes dim them.
        inline bool decodeEmptyDim( int M ) { return M == DimOff || M == DimVelocity || M == DimFull; }

        inline bool isValidMode( int M ) { return M >= Off && M <= OffDimFull; }

        // Colour palette stepping with the Data Dial: 18 colours, stops at both ends.
        inline int stepColour( int Current, int Delta, int NumColours = 18 )
        {
            const int c = Current + Delta;
            return c < 0 ? 0 : ( c >= NumColours ? NumColours - 1 : c );
        }
    }
}
