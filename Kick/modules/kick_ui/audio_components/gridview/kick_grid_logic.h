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

#include <array>
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

} // namespace kick_ui::grid_logic
