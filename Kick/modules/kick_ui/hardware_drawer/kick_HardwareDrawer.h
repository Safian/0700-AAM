/*
 * Copyright (c) 2024 MUSIC Tribe & Proper Studio
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

#include <juce_gui_basics/juce_gui_basics.h>
#include "contracts/dcl/dcl_contracts.h"
#include "contracts/dcl/dcl_contracts_helper.h"
#include "contracts/dcl/dcl_signals.h"
#include "PktMgr/PktMgr.hpp"

namespace kick_ui
{

/**
 * Slide-out hardware drawer providing 16 MPC velocity pads,
 * 4 Q-link rotary encoders, center jog wheel, and transport controls.
 * Designed for mobile touchscreens (iOS/Android) and desktop workflow without physical hardware.
 */
class MobileHardwareDrawer : public juce::Component, private juce::Timer
{
public:
    enum DrawerMode
    {
        ModePads = 0,
        ModeQLinks,
        ModeFull
    };

    MobileHardwareDrawer();
    ~MobileHardwareDrawer() override;

    void paint( juce::Graphics& g ) override;
    void resized() override;

    bool isExpanded() const noexcept { return expanded_; }
    void setExpanded( bool shouldExpand );
    void toggleExpanded();

    void setMode( DrawerMode newMode );
    DrawerMode getMode() const noexcept { return mode_; }

    void setBank( int bankIndex );
    int getBank() const noexcept { return currentBank_; }

    void setFullLevel( bool enabled );
    bool isFullLevel() const noexcept { return fullLevel_; }

private:
    void timerCallback() override;

    // Dispatch helpers for hardware contracts
    void sendPadMessage( int padIndex, bool pressed, int velocity );
    void sendButtonMessage( int buttonCode, bool pressed );
    void sendKnobMessage( int knobCode, int direction, int ticks, bool isQlink = false );
    void sendSliderMessage( int percent );

    // Sub-components
    class DrawerHandle;
    class PadMatrix;
    class QLinksPanel;
    class TouchStrip;
    class BankSelector;
    class TransportPanel;

    std::unique_ptr<DrawerHandle>   handle_;
    std::unique_ptr<juce::Component> panelContainer_;
    std::unique_ptr<PadMatrix>      padMatrix_;
    std::unique_ptr<QLinksPanel>    qlinksPanel_;
    std::unique_ptr<TouchStrip>     touchStrip_;
    std::unique_ptr<BankSelector>   bankSelector_;
    std::unique_ptr<TransportPanel> transportPanel_;

    bool expanded_{ true };
    float currentHeight_{ 270.0f };
    float targetHeight_{ 270.0f };
    DrawerMode mode_{ ModeFull };
    int currentBank_{ 0 };
    bool fullLevel_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( MobileHardwareDrawer )
};

} // namespace kick_ui
