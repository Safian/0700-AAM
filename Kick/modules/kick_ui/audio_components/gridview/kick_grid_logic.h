/*
 * Copyright (c) 2023 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>

namespace kick_ui::grid_logic
{
    // =========================================================================
    // Pad to MIDI note calculation
    // =========================================================================
    // Each bank has 16 pads. Pad numbers from DCL are 0..15.
    // Bank offset is bank * 16 (Bank A = 0, Bank B = 16, Bank C = 32, ..., Bank H = 112).
    // In kick_AppGrid.cpp (~542 and ~558), bank offset was added twice:
    //   padNo += bankOffset; // 542
    //   notesComp.selectNote( padNo + ae.getBankOffset(), ... ); // 558
    // padToNote computes the correct note number once.
    inline int padToNote( int padNo, int bankOffset )
    {
        return padNo + bankOffset;
    }

    // =========================================================================
    // Button state machine for Grid hardware commands
    // =========================================================================
    // COPY and ERASE were previously executed without checking button state,
    // so both BTN_PRESSED and BTN_RELEASED triggered the action twice.
    // GridButtonStateMachine triggers actions strictly on the PRESSED transition.

    namespace dcl_buttons
    {
        constexpr int EN_COPY       = 20;
        constexpr int EN_KNOB_CCW   = 28;
        constexpr int EN_KNOB_CW    = 30;
        constexpr int EN_STOP       = 44;
        constexpr int EN_PLAY       = 45;
        constexpr int EN_PLAYSTART  = 46;
        constexpr int EN_ERASE      = 52;
    }

    enum class ButtonAction
    {
        None = 0,
        Copy,
        Erase,
        KnobCw,
        KnobCcw,
        Play,
        PlayStart,
        Stop
    };

    enum class ButtonState
    {
        Released = 0,
        Pressed  = 1
    };

    class GridButtonStateMachine
    {
    public:
        // Processes a button event: returns the ButtonAction to perform on press,
        // or ButtonAction::None if the button was released or already held down.
        ButtonAction processButton( int buttonCode, ButtonState state )
        {
            if( state == ButtonState::Released )
            {
                if( buttonCode >= 0 && buttonCode < kMaxButtons )
                    isDown[buttonCode] = false;
                return ButtonAction::None;
            }

            // Pressed state: edge trigger (ignore held repeat)
            if( buttonCode >= 0 && buttonCode < kMaxButtons )
            {
                if( isDown[buttonCode] )
                    return ButtonAction::None;
                isDown[buttonCode] = true;
            }

            switch( buttonCode )
            {
                case dcl_buttons::EN_COPY:      return ButtonAction::Copy;
                case dcl_buttons::EN_ERASE:     return ButtonAction::Erase;
                case dcl_buttons::EN_KNOB_CW:   return ButtonAction::KnobCw;
                case dcl_buttons::EN_KNOB_CCW:  return ButtonAction::KnobCcw;
                case dcl_buttons::EN_PLAY:      return ButtonAction::Play;
                case dcl_buttons::EN_PLAYSTART: return ButtonAction::PlayStart;
                case dcl_buttons::EN_STOP:      return ButtonAction::Stop;
                default:                        return ButtonAction::None;
            }
        }

        void reset()
        {
            isDown.fill( false );
        }

        bool isButtonPressed( int buttonCode ) const
        {
            if( buttonCode >= 0 && buttonCode < kMaxButtons )
                return isDown[buttonCode];
            return false;
        }

    private:
        static constexpr int kMaxButtons = 64;
        std::array<bool, kMaxButtons> isDown{};
    };

    // =========================================================================
    // Q-Link zoom scaling
    // =========================================================================
    // Key 61 rotary encoders send small delta values (typically +/-1).
    // The previous code divided value by 100, resulting in delta of 0.01 px
    // which caused imperceptible zoom on a default 432px bar width.
    // kDefaultHorizontalZoomStep (20.0f px) and kDefaultVerticalZoomStep (1.0f px)
    // match the +/- zoom button steps in AppGrid.
    constexpr float kDefaultHorizontalZoomStep = 20.0f;
    constexpr float kDefaultVerticalZoomStep   = 1.0f;

    inline float calculateHorizontalZoomDelta( int encoderDelta, float stepSize = kDefaultHorizontalZoomStep )
    {
        return static_cast<float>( encoderDelta ) * stepSize;
    }

    inline float calculateVerticalZoomDelta( int encoderDelta, float stepSize = kDefaultVerticalZoomStep )
    {
        return static_cast<float>( encoderDelta ) * stepSize;
    }

    // =========================================================================
    // Q-Link 5..10 mapping helpers (BBT & Metronome)
    // =========================================================================
    // EN_KNOB_5 (5) -> 0 (Bar), EN_KNOB_6 (6) -> 1 (Beat), EN_KNOB_7 (7) -> 2 (Tick)
    inline int knobToBbtIndex( int knobCode )
    {
        return knobCode - 5;
    }

    // EN_KNOB_8 (8) -> 0 (Level), EN_KNOB_9 (9) -> 1 (Rate), EN_KNOB_10 (10) -> 2 (OnOff)
    inline int knobToMetronomeIndex( int knobCode )
    {
        return knobCode - 8;
    }

    // =========================================================================
    // Touch identity, Y<->note mapping, pinch-zoom & pan, and note hit-testing
    // =========================================================================

    inline int resolveTouchId( int touchSlot, int sourceIndex ) noexcept
    {
        return ( touchSlot >= 0 ) ? touchSlot : sourceIndex;
    }

    inline float noteToY( int note, float noteHeight, int maxNotes = 128 ) noexcept
    {
        if( maxNotes <= 0 || noteHeight <= 0.0f )
            return 0.0f;
        return static_cast<float>( maxNotes - note - 1 ) * noteHeight;
    }

    inline int yToNote( float y, float noteHeight, int maxNotes = 128 ) noexcept
    {
        if( maxNotes <= 0 || noteHeight <= 0.0f )
            return 0;
        const int rowFromTop = static_cast<int>( std::floor( y / noteHeight ) );
        const int rawNote    = ( maxNotes - 1 ) - rowFromTop;
        return std::clamp( rawNote, 0, maxNotes - 1 );
    }

    struct TouchPoint
    {
        float x = 0.0f;
        float y = 0.0f;
    };

    struct GridViewportState
    {
        float viewX      = 0.0f;
        float viewY      = 0.0f;
        float viewWidth  = 0.0f;
        float viewHeight = 0.0f;
        float barWidth   = 432.0f;
        float noteHeight = 26.0f;
        int   numBars    = 1;
        int   maxNotes   = 128;
    };

    struct GridZoomBounds
    {
        float minBarWidth   = 1.0f;
        float maxBarWidth   = 16000.0f;
        float minNoteHeight = 4.0f;
        float maxNoteHeight = 128.0f;
    };

    struct PinchPanResult
    {
        float barWidth   = 432.0f;
        float noteHeight = 26.0f;
        float viewX      = 0.0f;
        float viewY      = 0.0f;
    };

    constexpr float kMinPinchAxisSpanPx      = 24.0f;
    constexpr float kPinchSpanDenominatorMin = 32.0f;

    inline float computeAxisPinchScale( float prevSpan, float curSpan ) noexcept
    {
        if( std::max( prevSpan, curSpan ) < kMinPinchAxisSpanPx )
            return 1.0f;
        const float denom = std::max( prevSpan, kPinchSpanDenominatorMin );
        return std::clamp( 1.0f + ( curSpan - prevSpan ) / denom, 0.5f, 2.0f );
    }

    inline PinchPanResult computeSingleFingerPan( const GridViewportState& state,
                                                  TouchPoint               prevVpPos,
                                                  TouchPoint               curVpPos ) noexcept
    {
        const float contentW = static_cast<float>( std::max( 1, state.numBars ) ) * state.barWidth;
        const float contentH = static_cast<float>( std::max( 1, state.maxNotes ) ) * state.noteHeight;
        const float maxViewX = std::max( 0.0f, contentW - state.viewWidth );
        const float maxViewY = std::max( 0.0f, contentH - state.viewHeight );

        PinchPanResult out;
        out.barWidth   = state.barWidth;
        out.noteHeight = state.noteHeight;
        out.viewX      = std::clamp( state.viewX - ( curVpPos.x - prevVpPos.x ), 0.0f, maxViewX );
        out.viewY      = std::clamp( state.viewY - ( curVpPos.y - prevVpPos.y ), 0.0f, maxViewY );
        return out;
    }

    inline PinchPanResult computePinchPan( const GridViewportState& state,
                                           const GridZoomBounds&    bounds,
                                           TouchPoint               prevP1,
                                           TouchPoint               prevP2,
                                           TouchPoint               curP1,
                                           TouchPoint               curP2 ) noexcept
    {
        const float prevSpanX = std::abs( prevP1.x - prevP2.x );
        const float curSpanX  = std::abs( curP1.x - curP2.x );
        const float prevSpanY = std::abs( prevP1.y - prevP2.y );
        const float curSpanY  = std::abs( curP1.y - curP2.y );

        const float scaleX = computeAxisPinchScale( prevSpanX, curSpanX );
        const float scaleY = computeAxisPinchScale( prevSpanY, curSpanY );

        const float clampedBarW = std::clamp( state.barWidth * scaleX,
                                              bounds.minBarWidth,
                                              std::max( bounds.minBarWidth, bounds.maxBarWidth ) );
        const float clampedNoteH = std::clamp( state.noteHeight * scaleY,
                                               bounds.minNoteHeight,
                                               std::max( bounds.minNoteHeight, bounds.maxNoteHeight ) );

        const float actualRatioX = ( state.barWidth > 0.0f ) ? ( clampedBarW / state.barWidth ) : 1.0f;
        const float actualRatioY = ( state.noteHeight > 0.0f ) ? ( clampedNoteH / state.noteHeight ) : 1.0f;

        const float prevMidX = 0.5f * ( prevP1.x + prevP2.x );
        const float prevMidY = 0.5f * ( prevP1.y + prevP2.y );
        const float curMidX  = 0.5f * ( curP1.x + curP2.x );
        const float curMidY  = 0.5f * ( curP1.y + curP2.y );

        const float newContentW = static_cast<float>( std::max( 1, state.numBars ) ) * clampedBarW;
        const float newContentH = static_cast<float>( std::max( 1, state.maxNotes ) ) * clampedNoteH;
        const float maxViewX    = std::max( 0.0f, newContentW - state.viewWidth );
        const float maxViewY    = std::max( 0.0f, newContentH - state.viewHeight );

        PinchPanResult out;
        out.barWidth   = clampedBarW;
        out.noteHeight = clampedNoteH;
        out.viewX      = std::clamp( ( state.viewX + prevMidX ) * actualRatioX - curMidX, 0.0f, maxViewX );
        out.viewY      = std::clamp( ( state.viewY + prevMidY ) * actualRatioY - curMidY, 0.0f, maxViewY );
        return out;
    }

    struct NoteSpan
    {
        int note   = 0;
        int start  = 0;
        int length = 0;
        int id     = 0;
    };

    inline int ticksFromPixelSlop( float slopPx, float barWidth, int ticksPerBar ) noexcept
    {
        if( slopPx <= 0.0f || barWidth <= 0.0f || ticksPerBar <= 0 )
            return 0;
        return std::max( 1, static_cast<int>( std::lround( slopPx * static_cast<float>( ticksPerBar ) / barWidth ) ) );
    }

    inline int findBestNoteMatch( const NoteSpan* notes, int count, int targetNote, int tick, int tickSlop ) noexcept
    {
        if( !notes || count <= 0 || targetNote < 0 || tick < 0 )
            return -1;

        const int slop      = std::max( 0, tickSlop );
        int       bestIndex = -1;
        int       bestDist  = slop + 1;
        int       bestSpan  = 0;

        for( int i = 0; i < count; ++i )
        {
            if( notes[i].note != targetNote )
                continue;

            const int len   = std::max( 1, notes[i].length );
            const int start = notes[i].start;
            const int end   = start + len;

            int dist = 0;
            if( tick < start )
                dist = start - tick;
            else if( tick >= end )
                dist = ( tick - end ) + 1;

            if( dist > slop )
                continue;

            const bool better = ( bestIndex < 0 )
                             || ( dist < bestDist )
                             || ( dist == bestDist && len < bestSpan )
                             || ( dist == bestDist && len == bestSpan && i > bestIndex );
            if( better )
            {
                bestIndex = i;
                bestDist  = dist;
                bestSpan  = len;
            }
        }
        return bestIndex;
    }

    // =========================================================================
    // Long-press note inspector, clamping, and touch edge-resize hit testing
    // =========================================================================
    constexpr int   kLongPressDelayMs    = 450;
    constexpr float kLongPressMaxMovePx  = 10.0f;

    inline bool shouldTriggerLongPress( bool isDown, float moveDistPx, int elapsedMs ) noexcept
    {
        return isDown && moveDistPx <= kLongPressMaxMovePx && elapsedMs >= kLongPressDelayMs;
    }

    struct NotePropertiesState
    {
        int  note        = 60;
        int  start       = 0;
        int  length      = 96;
        int  velocity    = 100;
        int  probability = 100;
        int  ratchet     = 1;
        bool mute        = false;
    };

    inline NotePropertiesState clampNoteProperties( NotePropertiesState s, int maxStartTicks ) noexcept
    {
        const int maxStart = std::max( 0, maxStartTicks );
        s.note             = std::clamp( s.note, 0, 127 );
        s.start            = std::clamp( s.start, 0, maxStart );
        s.length           = std::max( 1, s.length );
        s.velocity         = std::clamp( s.velocity, 1, 127 );
        s.probability      = std::clamp( s.probability, 0, 100 );
        s.ratchet          = std::clamp( s.ratchet, 1, 8 );
        return s;
    }

    inline int stepNoteProperty( int cur, int delta, int minVal, int maxVal ) noexcept
    {
        return std::clamp( cur + delta, minVal, std::max( minVal, maxVal ) );
    }

    enum class NoteEdgeHit
    {
        None = 0,
        Left,
        Right
    };

    inline NoteEdgeHit hitTestNoteEdge( float x, float startX, float endX, bool editStart, bool editEnd, bool widenForTouch ) noexcept
    {
        if( !editStart && !editEnd )
            return NoteEdgeHit::None;

        const float width  = std::max( 0.0f, endX - startX );
        const float handle = widenForTouch ? std::min( 16.0f, std::max( 5.0f, width * 0.35f ) ) : 5.0f;

        if( editStart && x >= ( startX - 2.0f ) && x <= ( startX + handle ) )
            return NoteEdgeHit::Left;
        if( editEnd && x <= ( endX + 2.0f ) && x >= ( endX - handle ) )
            return NoteEdgeHit::Right;
        return NoteEdgeHit::None;
    }

} // namespace kick_ui::grid_logic
