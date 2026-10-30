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
#include "contracts/dcl/dcl_signals.h"
#include "PktMgr/PktMgr.hpp"
#include "common/common.h"
#include "kick_ui/kick_ui.h"
#include "kick_hardware_controls.h"
#include <map>
#include <vector>
#include <memory>
#include <cmath>

namespace kick_mobile
{

using DCC = contracts::DeviceControlContracts;

//==============================================================================
/**
 * Loads authentic hardware graphic assets from the app bundle or source directory.
 */
inline juce::Image loadChassisAsset( const juce::String& filename )
{
    auto assetsDir = getAppDirs( MTAssets );
    auto imgFile = assetsDir.getChildFile( filename );
    if( imgFile.exists() )
        return juce::ImageFileFormat::loadFrom( imgFile );

    auto appBundle = juce::File::getSpecialLocation( juce::File::currentApplicationFile );
    auto resFile = appBundle.getChildFile( "Contents/Resources/" + filename );
    if( resFile.exists() )
        return juce::ImageFileFormat::loadFrom( resFile );

    auto resFile2 = appBundle.getChildFile( "assets/" + filename );
    if( resFile2.exists() )
        return juce::ImageFileFormat::loadFrom( resFile2 );

    auto cwdFile = juce::File::getCurrentWorkingDirectory().getChildFile( "Kick/assets/" + filename );
    if( cwdFile.exists() )
        return juce::ImageFileFormat::loadFrom( cwdFile );

    return {};
}

//==============================================================================
/**
 * Bottom Hardware Chassis Panel (Pads, Bank A..D, Jog Wheel, Transport bar, Ribbon).
 * Composed 100% from reusable kick_hw_ui controls over authentic hardware graphics.
 */
class MobileBottomChassisPanel : public juce::Component
{
public:
    MobileBottomChassisPanel()
    {
        bgImage = loadChassisAsset( "mobile_hw_bottom.png" );
        setOpaque( true );

        // 1. Create 16 reusable HardwarePad components
        for( int i = 0; i < 16; ++i )
        {
            pads[i] = std::make_unique<kick_hw_ui::HardwarePad>( i );
            addAndMakeVisible( pads[i].get() );
        }

        // 2. Create reusable HardwareJogWheel component
        jogWheel = std::make_unique<kick_hw_ui::HardwareJogWheel>();
        addAndMakeVisible( jogWheel.get() );

        // 3. Create reusable HardwareRibbon component
        ribbon = std::make_unique<kick_hw_ui::HardwareRibbon>();
        addAndMakeVisible( ribbon.get() );

        // 4. Create reusable Bank A..D buttons
        const int bankCodes[4] = { DCC::EN_BANK_A, DCC::EN_BANK_B, DCC::EN_BANK_C, DCC::EN_BANK_D };
        for( int b = 0; b < 4; ++b )
        {
            bankButtons[b] = std::make_unique<kick_hw_ui::HardwareButton>( bankCodes[b], false, juce::Colour( 0xffff9900 ) );
            if( b == 0 ) bankButtons[b]->setLit( true ); // Bank A active by default
            addAndMakeVisible( bankButtons[b].get() );
        }

        // 5. Create reusable Transport buttons (REC, OVR, STOP, PLAY, PAUSE)
        const int transCodes[5] = { DCC::EN_RECORD, DCC::EN_OVERDUB, DCC::EN_STOP, DCC::EN_PLAY, DCC::EN_PLAYSTART };
        const juce::Colour transCols[5] = {
            juce::Colour( 0xffff3b30 ), // REC: Red
            juce::Colour( 0xffff9500 ), // OVR: Amber
            juce::Colour( 0xffffcc00 ), // STOP: Yellow
            juce::Colour( 0xff34c759 ), // PLAY: Green
            juce::Colour( 0xff00d4ff )  // PAUSE: Cyan
        };
        for( int t = 0; t < 5; ++t )
        {
            transButtons[t] = std::make_unique<kick_hw_ui::HardwareButton>( transCodes[t], true, transCols[t] );
            addAndMakeVisible( transButtons[t].get() );
        }

        // 6. Create reusable Utility buttons (FULL LEVEL, 16 LEVELS, ERASE)
        const int utilCodes[3] = { DCC::EN_FULL_LEVEL, DCC::EN_16LEVELS, DCC::EN_ERASE };
        for( int u = 0; u < 3; ++u )
        {
            utilButtons[u] = std::make_unique<kick_hw_ui::HardwareButton>( utilCodes[u], false, juce::Colour( 0xffffcc00 ) );
            addAndMakeVisible( utilButtons[u].get() );
        }

        // 7. Create reusable Mode buttons (Row 1: MAIN, BROWSE, TRACK MIX, TRACK MUTE, NEXT SEC)
        const int modeCodesTop[5] = { DCC::EN_MAINMODE, DCC::EN_BROWSE, DCC::EN_TRACK_MIX, DCC::EN_TRACK_MUTE, DCC::EN_NEXT_SEQ };
        for( int m = 0; m < 5; ++m )
        {
            modeButtonsTop[m] = std::make_unique<kick_hw_ui::HardwareButton>( modeCodesTop[m], false, juce::Colour( 0xff30d158 ) );
            addAndMakeVisible( modeButtonsTop[m].get() );
        }

        // 8. Create reusable Mode buttons (Row 2: STEP SEQ, TC, SAMPLER, SAMPLE EDIT, PROG EDIT)
        const int modeCodesBtm[5] = { DCC::EN_STEP_SEQ, DCC::EN_TC, DCC::EN_SAMPLER, DCC::EN_SAMPLE_EDIT, DCC::EN_PROG_EDIT };
        for( int m = 0; m < 5; ++m )
        {
            modeButtonsBtm[m] = std::make_unique<kick_hw_ui::HardwareButton>( modeCodesBtm[m], false, juce::Colour( 0xff30d158 ) );
            addAndMakeVisible( modeButtonsBtm[m].get() );
        }
    }

    void paint( juce::Graphics& g ) override
    {
        g.fillAll( juce::Colour( 0xff121215 ) );

        if( bgImage.isValid() && !renderedRect.isEmpty() )
        {
            g.drawImage( bgImage, renderedRect, juce::RectanglePlacement::centred );
        }
    }

    void setHorizontalOffset( float offset )
    {
        if( std::abs( horizontalOffset - offset ) > 0.1f )
        {
            horizontalOffset = offset;
            updateControlPositions();
            repaint();
        }
    }

    void resized() override
    {
        updateControlPositions();
    }

    void updateControlPositions()
    {
        auto bounds = getLocalBounds().toFloat();
        const float targetAspect = 746.0f / 402.0f;
        const float currentAspect = bounds.getHeight() > 0 ? ( bounds.getWidth() / bounds.getHeight() ) : targetAspect;

        if( currentAspect > targetAspect )
        {
            float w = bounds.getHeight() * targetAspect;
            renderedRect = juce::Rectangle<float>( ( bounds.getWidth() - w ) * 0.5f + horizontalOffset, 0.0f, w, bounds.getHeight() );
        }
        else
        {
            float h = bounds.getWidth() / targetAspect;
            renderedRect = juce::Rectangle<float>( horizontalOffset, ( bounds.getHeight() - h ) * 0.5f, bounds.getWidth(), h );
        }

        if( renderedRect.isEmpty() ) return;

        const float scale = renderedRect.getWidth() / 746.0f;
        const float ox    = renderedRect.getX();
        const float oy    = renderedRect.getY();

        auto toScreen = [&]( float bx, float by, float bw, float bh ) {
            return juce::Rectangle<int>(
                (int)std::round( ox + bx * scale ),
                (int)std::round( oy + by * scale ),
                (int)std::round( bw * scale ),
                (int)std::round( bh * scale )
            );
        };

        // 1. Position 16 Pads
        const float colX[4] = { 118.0f, 203.0f, 289.0f, 375.0f };
        const float rowY[4] = { 31.0f, 116.0f, 203.0f, 288.0f };
        for( int r = 0; r < 4; ++r )
        {
            for( int c = 0; c < 4; ++c )
            {
                int padIdx = ( 3 - r ) * 4 + c;
                pads[padIdx]->setBounds( toScreen( colX[c], rowY[r], 67.0f, 67.0f ) );
            }
        }

        // 2. Position Jog Wheel
        jogWheel->setBounds( toScreen( 590.0f - 52.0f, 184.0f - 52.0f, 104.0f, 104.0f ) );

        // 3. Position Ribbon
        ribbon->setBounds( toScreen( 25.0f, 35.0f, 67.0f, 150.0f ) );

        // 4. Position Bank A..D buttons
        const float bankCx[4] = { 493.0f, 561.0f, 629.0f, 697.0f };
        for( int b = 0; b < 4; ++b )
        {
            bankButtons[b]->setBounds( toScreen( bankCx[b] - 18.0f, 50.0f - 12.0f, 36.0f, 24.0f ) );
        }

        // 5. Position Transport buttons (REC, OVR, STOP, PLAY, PAUSE)
        const float transCx[5] = { 490.0f, 543.0f, 593.0f, 645.0f, 695.0f };
        for( int t = 0; t < 5; ++t )
        {
            transButtons[t]->setBounds( toScreen( transCx[t] - 17.0f, 355.0f - 17.0f, 34.0f, 34.0f ) );
        }

        // 6. Position Utility buttons (FULL LEVEL, 16 LEVELS, ERASE)
        const float utilY[3] = { 234.0f, 285.0f, 340.0f };
        for( int u = 0; u < 3; ++u )
        {
            utilButtons[u]->setBounds( toScreen( 35.0f, utilY[u] - 14.0f, 50.0f, 28.0f ) );
        }

        // 7. Position Mode Buttons Top
        const float modeCx[5] = { 482.0f, 532.0f, 584.0f, 638.0f, 692.0f };
        for( int m = 0; m < 5; ++m )
        {
            modeButtonsTop[m]->setBounds( toScreen( modeCx[m] - 20.0f, 108.0f - 12.0f, 40.0f, 24.0f ) );
        }

        // 8. Position Mode Buttons Bottom
        for( int m = 0; m < 5; ++m )
        {
            modeButtonsBtm[m]->setBounds( toScreen( modeCx[m] - 20.0f, 292.0f - 12.0f, 40.0f, 24.0f ) );
        }
    }

private:
    juce::Image bgImage;
    juce::Rectangle<float> renderedRect;
    float horizontalOffset{ 0.0f };

    // Reusable kick_hw_ui components
    std::unique_ptr<kick_hw_ui::HardwarePad>        pads[16];
    std::unique_ptr<kick_hw_ui::HardwareJogWheel>   jogWheel;
    std::unique_ptr<kick_hw_ui::HardwareRibbon>     ribbon;
    std::unique_ptr<kick_hw_ui::HardwareButton>     bankButtons[4];
    std::unique_ptr<kick_hw_ui::HardwareButton>     transButtons[5];
    std::unique_ptr<kick_hw_ui::HardwareButton>     utilButtons[3];
    std::unique_ptr<kick_hw_ui::HardwareButton>     modeButtonsTop[5];
    std::unique_ptr<kick_hw_ui::HardwareButton>     modeButtonsBtm[5];
};

//==============================================================================
/**
 * Master Panel (Left edge: Power, Master Volume knob, Rec Gain knob, Link button).
 * Composed from reusable HardwareRotaryKnob and HardwareButton components.
 */
class MobileMasterChassisPanel : public juce::Component
{
public:
    MobileMasterChassisPanel()
    {
        bgImage = loadChassisAsset( "mobile_hw_master.png" );
        setOpaque( true );

        masterKnob = std::make_unique<kick_hw_ui::HardwareRotaryKnob>( DCC::EN_KNOB_5, false, 0.85f );
        addAndMakeVisible( masterKnob.get() );

        recKnob = std::make_unique<kick_hw_ui::HardwareRotaryKnob>( DCC::EN_KNOB_6, false, 0.5f );
        addAndMakeVisible( recKnob.get() );

        linkBtn = std::make_unique<kick_hw_ui::HardwareButton>( DCC::EN_LINK, false, juce::Colour( 0xff00d4ff ) );
        addAndMakeVisible( linkBtn.get() );
    }

    void paint( juce::Graphics& g ) override
    {
        g.fillAll( juce::Colour( 0xff16161a ) );

        if( bgImage.isValid() && !renderedRect.isEmpty() )
        {
            g.drawImage( bgImage, renderedRect, juce::RectanglePlacement::centred );
        }

        // Draw glowing border on right edge
        g.setColour( juce::Colour( 0x4400d4ff ) );
        g.drawLine( getWidth() - 1.0f, 0.0f, getWidth() - 1.0f, (float)getHeight(), 2.0f );
    }

    void resized() override
    {
        auto bounds = getLocalBounds().toFloat();
        const float targetAspect = 106.0f / 334.0f;
        float w = bounds.getHeight() * targetAspect;
        renderedRect = juce::Rectangle<float>( ( bounds.getWidth() - w ) * 0.5f, 0.0f, w, bounds.getHeight() );

        if( renderedRect.isEmpty() ) return;

        const float scale = renderedRect.getHeight() / 334.0f;
        const float ox    = renderedRect.getX();
        const float oy    = renderedRect.getY();

        auto toScreen = [&]( float bx, float by, float bw, float bh ) {
            return juce::Rectangle<int>(
                (int)std::round( ox + bx * scale ),
                (int)std::round( oy + by * scale ),
                (int)std::round( bw * scale ),
                (int)std::round( bh * scale )
            );
        };

        // Master knob at (53, 112), radius ~26
        masterKnob->setBounds( toScreen( 53.0f - 26.0f, 112.0f - 26.0f, 52.0f, 52.0f ) );

        // Rec knob at (53, 202), radius ~26
        recKnob->setBounds( toScreen( 53.0f - 26.0f, 202.0f - 26.0f, 52.0f, 52.0f ) );

        // Link button at (25..85, 268..305)
        linkBtn->setBounds( toScreen( 25.0f, 268.0f, 60.0f, 37.0f ) );
    }

private:
    juce::Image bgImage;
    juce::Rectangle<float> renderedRect;

    std::unique_ptr<kick_hw_ui::HardwareRotaryKnob> masterKnob;
    std::unique_ptr<kick_hw_ui::HardwareRotaryKnob> recKnob;
    std::unique_ptr<kick_hw_ui::HardwareButton>     linkBtn;
};

//==============================================================================
/**
 * Q-Link Potentiometer Panel (Right edge: P1, P2, P3, P4 knobs).
 * Composed from 4 reusable HardwareRotaryKnob components.
 */
class MobileQLinkChassisPanel : public juce::Component
{
public:
    MobileQLinkChassisPanel()
    {
        bgImage = loadChassisAsset( "mobile_hw_qlinks.png" );
        setOpaque( true );

        for( int i = 0; i < 4; ++i )
        {
            qlinkKnobs[i] = std::make_unique<kick_hw_ui::HardwareRotaryKnob>( DCC::EN_KNOB_1 + i, true, 0.5f );
            addAndMakeVisible( qlinkKnobs[i].get() );
        }
    }

    void paint( juce::Graphics& g ) override
    {
        g.fillAll( juce::Colour( 0xff16161a ) );

        if( bgImage.isValid() && !renderedRect.isEmpty() )
        {
            g.drawImage( bgImage, renderedRect, juce::RectanglePlacement::centred );
        }

        // Draw glowing border on left edge
        g.setColour( juce::Colour( 0x4400d4ff ) );
        g.drawLine( 0.0f, 0.0f, 0.0f, (float)getHeight(), 2.0f );
    }

    void resized() override
    {
        auto bounds = getLocalBounds().toFloat();
        const float targetAspect = 74.0f / 302.0f;
        float w = bounds.getHeight() * targetAspect;
        renderedRect = juce::Rectangle<float>( ( bounds.getWidth() - w ) * 0.5f, 0.0f, w, bounds.getHeight() );

        if( renderedRect.isEmpty() ) return;

        const float scale = renderedRect.getHeight() / 302.0f;
        const float ox    = renderedRect.getX();
        const float oy    = renderedRect.getY();

        const float knobY[4] = { 36.0f, 105.0f, 174.0f, 243.0f };
        for( int i = 0; i < 4; ++i )
        {
            qlinkKnobs[i]->setBounds(
                (int)std::round( ox + ( 37.0f - 24.0f ) * scale ),
                (int)std::round( oy + ( knobY[i] - 24.0f ) * scale ),
                (int)std::round( 48.0f * scale ),
                (int)std::round( 48.0f * scale )
            );
        }
    }

private:
    juce::Image bgImage;
    juce::Rectangle<float> renderedRect;

    std::unique_ptr<kick_hw_ui::HardwareRotaryKnob> qlinkKnobs[4];
};

//==============================================================================
/**
 * Root Mobile Container: Native Hardware Design with 3-Finger Gesture Navigation.
 * - Center: Clean DAW full screen (no edge buttons).
 * - 3-finger swipe UP: Bottom Hardware Panel (Pads, Bank, Jog, Transport) slides up.
 * - 3-finger swipe DOWN: Slides back down to DAW.
 * - 3-finger swipe RIGHT: Master strip slides in from left, DAW shifts right.
 * - 3-finger swipe LEFT: Q-Link strip slides in from right, DAW shifts left.
 * - Tap exposed DAW: Immediately docks back to full-screen DAW.
 */
class KickMobileTLayout : public juce::Component,
                          public juce::Timer
{
public:
    enum class PanelState
    {
        DAW = 0,
        BottomPads,
        MasterStrip,
        QLinkStrip
    };

    KickMobileTLayout()
    {
        setOpaque( true );

        kickHome = &kick_ui::KickHome::Instance();
        addAndMakeVisible( kickHome );

        masterPanel = std::make_unique<MobileMasterChassisPanel>();
        addAndMakeVisible( masterPanel.get() );

        qlinkPanel = std::make_unique<MobileQLinkChassisPanel>();
        addAndMakeVisible( qlinkPanel.get() );

        bottomPanel = std::make_unique<MobileBottomChassisPanel>();
        addChildComponent( bottomPanel.get() );

        // Listen for multi-touch gestures across all children
        addMouseListener( this, true );

        startTimerHz( 60 );
    }

    ~KickMobileTLayout() override
    {
        stopTimer();
        removeMouseListener( this );

        bottomPanel.reset();
        masterPanel.reset();
        qlinkPanel.reset();

        if( kickHome != nullptr )
        {
            kickHome->setTransform( juce::AffineTransform() );
            removeChildComponent( kickHome );
        }
    }

    void setPanelState( PanelState newState )
    {
        if( targetState != newState )
        {
            targetState = newState;

            if( targetState == PanelState::BottomPads )
            {
                if( bottomPanel != nullptr )
                {
                    bottomPanel->setVisible( true );
                    bottomPanel->toFront( true );
                }
            }
            else
            {
                if( masterPanel != nullptr ) masterPanel->setVisible( true );
                if( qlinkPanel != nullptr )  qlinkPanel->setVisible( true );
            }

            repaint();
        }
    }

    PanelState getPanelState() const noexcept { return targetState; }

    void paint( juce::Graphics& g ) override
    {
        g.fillAll( juce::Colours::black );
    }

    void resized() override
    {
        updateLayout();
    }

    void getDawGeometry( float& outDawX, float& outDawY, float& outDawW, float& outDawH ) const
    {
        int w = getWidth();
        int h = getHeight();
        if( w <= 0 || h <= 0 )
        {
            outDawX = outDawY = outDawW = outDawH = 0.0f;
            return;
        }

        const float targetAspect = 1024.0f / 600.0f;
        const float currentAspect = (float)w / (float)h;

        if( currentAspect > targetAspect )
        {
            outDawH = (float)h;
            outDawW = (float)h * targetAspect;
            outDawX = ( (float)w - outDawW ) * 0.5f;
            outDawY = 0.0f;
        }
        else
        {
            outDawW = (float)w;
            outDawH = (float)w / targetAspect;
            outDawX = 0.0f;
            outDawY = ( (float)h - outDawH ) * 0.5f;
        }
    }

    float getMasterPanelWidth() const
    {
        return (float)getHeight() * ( 106.0f / 334.0f );
    }

    float getQLinkPanelWidth() const
    {
        return (float)getHeight() * ( 74.0f / 302.0f );
    }

    float getMasterRevealShift() const
    {
        float dawX = 0.0f, dawY = 0.0f, dawW = 0.0f, dawH = 0.0f;
        getDawGeometry( dawX, dawY, dawW, dawH );
        float masterW = getMasterPanelWidth();
        // Shift DAW to the right so Master panel is completely visible with comfortable margin
        float shift = masterW - dawX + 24.0f;
        return juce::jmax( 55.0f, shift );
    }

    float getQLinkRevealShift() const
    {
        float dawX = 0.0f, dawY = 0.0f, dawW = 0.0f, dawH = 0.0f;
        getDawGeometry( dawX, dawY, dawW, dawH );
        float qlinkW = getQLinkPanelWidth();
        // Shift DAW to the left so Q-Link panel is comfortably inside the screen away from bezel
        float shift = qlinkW - dawX + 24.0f;
        return juce::jmax( 75.0f, shift );
    }

    void timerCallback() override
    {
        float targetDawY = ( targetState == PanelState::BottomPads ) ? -(float)getHeight() : 0.0f;
        float targetDawX = 0.0f;

        if( targetState == PanelState::MasterStrip )
        {
            targetDawX = getMasterRevealShift();
        }
        else if( targetState == PanelState::QLinkStrip )
        {
            targetDawX = -getQLinkRevealShift();
        }
        else
        {
            targetDawX = 0.0f;
        }

        // Damped spring physics for X (soft carousel bounce)
        const float tensionX = 0.14f;
        const float dampingX = 0.72f;

        float forceX = ( targetDawX - currentDawX ) * tensionX;
        vx = ( vx + forceX ) * dampingX;
        currentDawX += vx;

        // Damped spring physics for Y (smooth vertical slide)
        const float tensionY = 0.18f;
        const float dampingY = 0.74f;

        float forceY = ( targetDawY - currentDawY ) * tensionY;
        vy = ( vy + forceY ) * dampingY;
        currentDawY += vy;

        bool settledX = ( std::abs( targetDawX - currentDawX ) < 0.2f && std::abs( vx ) < 0.2f );
        bool settledY = ( std::abs( targetDawY - currentDawY ) < 0.2f && std::abs( vy ) < 0.2f );

        if( settledX )
        {
            currentDawX = targetDawX;
            vx = 0.0f;
        }
        if( settledY )
        {
            currentDawY = targetDawY;
            vy = 0.0f;
        }

        updateLayout();

        // Control visibility when animation settles
        if( targetState == PanelState::BottomPads && settledY )
        {
            if( masterPanel != nullptr && masterPanel->isVisible() ) masterPanel->setVisible( false );
            if( qlinkPanel != nullptr && qlinkPanel->isVisible() )   qlinkPanel->setVisible( false );
        }
        else
        {
            if( masterPanel != nullptr && !masterPanel->isVisible() ) masterPanel->setVisible( true );
            if( qlinkPanel != nullptr && !qlinkPanel->isVisible() )   qlinkPanel->setVisible( true );
        }

        if( targetState != PanelState::BottomPads && settledY )
        {
            if( bottomPanel != nullptr && bottomPanel->isVisible() ) bottomPanel->setVisible( false );
        }
    }

    void updateLayout()
    {
        int w = getWidth();
        int h = getHeight();
        if( w <= 0 || h <= 0 ) return;

        float dawX = 0.0f, dawY = 0.0f, dawW = 0.0f, dawH = 0.0f;
        getDawGeometry( dawX, dawY, dawW, dawH );

        // 1. DAW Screen
        if( kickHome != nullptr )
        {
            kickHome->setBounds( 0, 0, 1024, 600 );
            kickHome->setTransform(
                juce::AffineTransform::scale( dawW / 1024.0f, dawH / 600.0f )
                                     .translated( dawX + currentDawX, dawY + currentDawY )
            );
        }

        // 2. Master Panel: Attached directly to left edge of DAW
        float masterW = getMasterPanelWidth();
        if( masterPanel != nullptr )
        {
            float mX = dawX + currentDawX - masterW;
            masterPanel->setBounds( (int)std::round( mX ), (int)std::round( dawY ), (int)std::round( masterW ), (int)std::round( dawH ) );
        }

        // 3. Q-Link Panel: Attached directly to right edge of DAW
        float qlinkW = getQLinkPanelWidth();
        if( qlinkPanel != nullptr )
        {
            float qX = dawX + dawW + currentDawX;
            qlinkPanel->setBounds( (int)std::round( qX ), (int)std::round( dawY ), (int)std::round( qlinkW ), (int)std::round( dawH ) );
        }

        // 4. Bottom Hardware Panel: Aligned horizontally with currentDawX, slides up with currentDawY
        if( bottomPanel != nullptr )
        {
            bottomPanel->setHorizontalOffset( currentDawX );
            bottomPanel->setBounds( 0, (int)std::round( currentDawY + (float)h ), w, h );
        }
    }

    //==========================================================================
    // Multi-touch & gesture recognition
    //==========================================================================
    struct TouchPoint
    {
        bool               isDown{ false };
        juce::Point<float> startPos;
        juce::Point<float> currentPos;
        juce::Component*   startComponent{ nullptr };
    };

    void mouseDown( const juce::MouseEvent& e ) override
    {
        int sourceIdx = e.source.getIndex();
        auto screenP  = e.getScreenPosition().toFloat();
        auto localP   = e.getEventRelativeTo( this ).position;
        activeTouches[sourceIdx] = { true, screenP, screenP, e.eventComponent };
        gestureTriggered = false;

        // Check if touch occurred on Q-Link strip or any of its knobs
        bool isQLinkTouch = ( qlinkPanel != nullptr && ( e.eventComponent == qlinkPanel.get() || qlinkPanel->isParentOf( e.eventComponent ) ) );
        if( !isQLinkTouch && qlinkPanel != nullptr && qlinkPanel->isVisible() )
        {
            isQLinkTouch = qlinkPanel->getBounds().toFloat().contains( localP );
        }

        // Check if touch occurred on Master strip or any of its knobs
        bool isMasterTouch = ( masterPanel != nullptr && ( e.eventComponent == masterPanel.get() || masterPanel->isParentOf( e.eventComponent ) ) );
        if( !isMasterTouch && masterPanel != nullptr && masterPanel->isVisible() )
        {
            isMasterTouch = masterPanel->getBounds().toFloat().contains( localP );
        }

        // Automatically reveal side panel when user touches / interacts with it
        if( isQLinkTouch )
        {
            if( targetState != PanelState::QLinkStrip && targetState != PanelState::BottomPads )
            {
                setPanelState( PanelState::QLinkStrip );
            }
        }
        else if( isMasterTouch )
        {
            if( targetState != PanelState::MasterStrip && targetState != PanelState::BottomPads )
            {
                setPanelState( PanelState::MasterStrip );
            }
        }
        // Explicit user requirement: DAW must NOT jump back on click/tap, only on swipe!
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        int sourceIdx = e.source.getIndex();
        auto screenP  = e.getScreenPosition().toFloat();

        auto it = activeTouches.find( sourceIdx );
        if( it != activeTouches.end() && it->second.isDown )
        {
            it->second.currentPos = screenP;
        }

        if( gestureTriggered ) return;

        int count = 0;
        float totalDx = 0.0f;
        float totalDy = 0.0f;

        for( auto& kv : activeTouches )
        {
            if( kv.second.isDown )
            {
                count++;
                totalDx += ( kv.second.currentPos.x - kv.second.startPos.x );
                totalDy += ( kv.second.currentPos.y - kv.second.startPos.y );
            }
        }

        bool has3Fingers = ( count >= 3 ) || ( e.mods.isShiftDown() && count >= 1 );

        bool startedOnQLink = false;
        bool startedOnMaster = false;
        if( it != activeTouches.end() )
        {
            if( qlinkPanel != nullptr && ( it->second.startComponent == qlinkPanel.get() || qlinkPanel->isParentOf( it->second.startComponent ) ) )
                startedOnQLink = true;
            if( masterPanel != nullptr && ( it->second.startComponent == masterPanel.get() || masterPanel->isParentOf( it->second.startComponent ) ) )
                startedOnMaster = true;
        }

        const float threshold = 38.0f;

        // 1. Swipe UP on side controls or 3 fingers anywhere -> Open Bottom Pads with carousel bounce!
        if( has3Fingers || ( ( startedOnQLink || startedOnMaster ) && count == 1 ) )
        {
            float singleDy = it != activeTouches.end() ? ( it->second.currentPos.y - it->second.startPos.y ) : totalDy;
            float singleDx = it != activeTouches.end() ? ( it->second.currentPos.x - it->second.startPos.x ) : totalDx;

            if( singleDy < -threshold && std::abs( singleDy ) > std::abs( singleDx ) * 1.3f )
            {
                gestureTriggered = true;
                setPanelState( PanelState::BottomPads );
                return;
            }
        }

        // 2. 3-Finger Gestures anywhere on screen
        if( has3Fingers )
        {
            float avgDx = totalDx / (float)juce::jmax( 1, count );
            float avgDy = totalDy / (float)juce::jmax( 1, count );

            if( std::abs( avgDy ) > threshold && std::abs( avgDy ) > std::abs( avgDx ) * 1.3f )
            {
                gestureTriggered = true;
                if( avgDy < 0.0f )
                {
                    setPanelState( PanelState::BottomPads );
                }
                else
                {
                    setPanelState( PanelState::DAW );
                }
            }
            else if( std::abs( avgDx ) > threshold && std::abs( avgDx ) > std::abs( avgDy ) * 1.3f )
            {
                gestureTriggered = true;
                if( avgDx > 0.0f )
                {
                    // Swipe right
                    if( targetState == PanelState::DAW )
                        setPanelState( PanelState::MasterStrip );
                    else if( targetState == PanelState::QLinkStrip )
                        setPanelState( PanelState::DAW );
                }
                else
                {
                    // Swipe left
                    if( targetState == PanelState::DAW )
                        setPanelState( PanelState::QLinkStrip );
                    else if( targetState == PanelState::MasterStrip )
                        setPanelState( PanelState::DAW );
                }
            }
        }
        // 3. Single-finger swipe horizontally on side panels to dock back
        else if( count == 1 && ( startedOnQLink || startedOnMaster ) )
        {
            float singleDx = it->second.currentPos.x - it->second.startPos.x;
            float singleDy = it->second.currentPos.y - it->second.startPos.y;

            if( std::abs( singleDx ) > threshold && std::abs( singleDx ) > std::abs( singleDy ) * 1.3f )
            {
                if( startedOnQLink && singleDx > 0.0f && targetState == PanelState::QLinkStrip )
                {
                    gestureTriggered = true;
                    setPanelState( PanelState::DAW );
                }
                else if( startedOnMaster && singleDx < 0.0f && targetState == PanelState::MasterStrip )
                {
                    gestureTriggered = true;
                    setPanelState( PanelState::DAW );
                }
            }
        }
    }

    void mouseUp( const juce::MouseEvent& e ) override
    {
        int sourceIdx = e.source.getIndex();
        auto it = activeTouches.find( sourceIdx );
        if( it != activeTouches.end() )
        {
            it->second.isDown = false;
        }

        int remaining = 0;
        for( auto& kv : activeTouches )
            if( kv.second.isDown ) remaining++;

        if( remaining == 0 )
        {
            gestureTriggered = false;
        }
    }

    bool handleKeyboardNav( const juce::KeyPress& key )
    {
        if( key == juce::KeyPress::upKey )
        {
            setPanelState( PanelState::BottomPads );
            return true;
        }
        if( key == juce::KeyPress::downKey || key == juce::KeyPress::escapeKey )
        {
            setPanelState( PanelState::DAW );
            return true;
        }
        if( key == juce::KeyPress::rightKey )
        {
            if( targetState == PanelState::DAW ) setPanelState( PanelState::MasterStrip );
            else if( targetState == PanelState::QLinkStrip ) setPanelState( PanelState::DAW );
            return true;
        }
        if( key == juce::KeyPress::leftKey )
        {
            if( targetState == PanelState::DAW ) setPanelState( PanelState::QLinkStrip );
            else if( targetState == PanelState::MasterStrip ) setPanelState( PanelState::DAW );
            return true;
        }
        return false;
    }

private:
    kick_ui::KickHome*                            kickHome{ nullptr };
    std::unique_ptr<MobileBottomChassisPanel>     bottomPanel;
    std::unique_ptr<MobileMasterChassisPanel>     masterPanel;
    std::unique_ptr<MobileQLinkChassisPanel>      qlinkPanel;

    PanelState targetState{ PanelState::DAW };

    float currentDawY{ 0.0f };
    float currentDawX{ 0.0f };
    float vx{ 0.0f };
    float vy{ 0.0f };

    std::map<int, TouchPoint> activeTouches;
    bool                      gestureTriggered{ false };
};

} // namespace kick_mobile
