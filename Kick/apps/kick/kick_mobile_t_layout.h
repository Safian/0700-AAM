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

    void resized() override
    {
        auto bounds = getLocalBounds().toFloat();
        const float targetAspect = 746.0f / 402.0f;
        const float currentAspect = bounds.getHeight() > 0 ? ( bounds.getWidth() / bounds.getHeight() ) : targetAspect;

        if( currentAspect > targetAspect )
        {
            float w = bounds.getHeight() * targetAspect;
            renderedRect = juce::Rectangle<float>( ( bounds.getWidth() - w ) * 0.5f, 0.0f, w, bounds.getHeight() );
        }
        else
        {
            float h = bounds.getWidth() / targetAspect;
            renderedRect = juce::Rectangle<float>( 0.0f, ( bounds.getHeight() - h ) * 0.5f, bounds.getWidth(), h );
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
        addAndMakeVisible( bottomPanel.get() );

        // Listen for gestures and touches across entire layout
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
        targetState = newState;
        repaint();
    }

    PanelState getPanelState() const noexcept { return targetState; }

    void paint( juce::Graphics& g ) override
    {
        // Continuous dark chassis metal background filling gaps
        g.fillAll( juce::Colour( 0xff121215 ) );
    }

    void resized() override
    {
        updateLayout();
    }

    void getDawGeometry( float& outDawW, float& outDawH ) const
    {
        int w = getWidth();
        int h = getHeight();
        if( w <= 0 || h <= 0 )
        {
            outDawW = outDawH = 0.0f;
            return;
        }

        const float targetAspect = 1024.0f / 600.0f;
        const float currentAspect = (float)w / (float)h;

        if( currentAspect > targetAspect )
        {
            outDawH = (float)h;
            outDawW = (float)h * targetAspect;
        }
        else
        {
            outDawW = (float)w;
            outDawH = (float)w / targetAspect;
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

    void getSnapTarget( PanelState state, float& outCamX, float& outCamY ) const
    {
        int w = getWidth();
        int h = getHeight();
        if( w <= 0 || h <= 0 ) return;

        float dawW = 0.0f, dawH = 0.0f;
        getDawGeometry( dawW, dawH );

        float masterW = getMasterPanelWidth();
        float qlinkW  = getQLinkPanelWidth();

        // Horizontal camera position so DAW is centered on screen:
        // Screen DAW X = 0 - camX = (w - dawW) * 0.5f => camX = -(w - dawW) * 0.5f
        float centerCamX = -( (float)w - dawW ) * 0.5f;

        switch( state )
        {
            case PanelState::DAW:
                outCamX = centerCamX;
                outCamY = 0.0f;
                break;

            case PanelState::MasterStrip:
                // Camera positioned so Master strip left edge is comfortably visible on screen (padding 16pt):
                // Screen Master X = -masterW - camX = 16.0f => camX = -masterW - 16.0f
                outCamX = -masterW - 16.0f;
                outCamY = 0.0f;
                break;

            case PanelState::QLinkStrip:
                // Camera positioned so Q-Link strip right edge is comfortably visible on screen (padding 16pt from right):
                // Screen Q-Link right = dawW + qlinkW - camX = w - 16.0f => camX = dawW + qlinkW - w + 16.0f
                outCamX = dawW + qlinkW - (float)w + 16.0f;
                outCamY = 0.0f;
                break;

            case PanelState::BottomPads:
                // Pad & Bank is situated directly under the DAW, so horizontal center is identical to DAW center!
                outCamX = centerCamX;
                outCamY = dawH;
                break;
        }
    }

    PanelState findClosestSnapState( float camX, float camY, float vx = 0.0f, float vy = 0.0f ) const
    {
        int w = getWidth();
        int h = getHeight();
        if( w <= 0 || h <= 0 ) return PanelState::DAW;

        float dawW = 0.0f, dawH = 0.0f;
        getDawGeometry( dawW, dawH );

        float masterW = getMasterPanelWidth();
        float qlinkW  = getQLinkPanelWidth();

        // Project position based on drag velocity
        float projX = camX - vx * 0.16f;
        float projY = camY - vy * 0.16f;

        // Vertical decision: closer to bottom row or top row?
        if( projY > dawH * 0.40f )
        {
            return PanelState::BottomPads;
        }

        // Horizontal decision in top row
        float targetMasterX = -masterW - 16.0f;
        float targetCenterX = -( (float)w - dawW ) * 0.5f;
        float targetQLinkX  = dawW + qlinkW - (float)w + 16.0f;

        float distMaster = std::abs( projX - targetMasterX );
        float distCenter = std::abs( projX - targetCenterX );
        float distQLink  = std::abs( projX - targetQLinkX );

        if( distMaster < distCenter && distMaster < distQLink )
            return PanelState::MasterStrip;

        if( distQLink < distCenter && distQLink < distMaster )
            return PanelState::QLinkStrip;

        return PanelState::DAW;
    }

    void timerCallback() override
    {
        float targetCamX = 0.0f;
        float targetCamY = 0.0f;
        getSnapTarget( targetState, targetCamX, targetCamY );

        if( !isUserDragging )
        {
            // Monotonic exponential ease-out with ZERO BOUNCE / ZERO OVERSHOOT!
            const float snapSpeed = 0.25f;

            float dx = targetCamX - currentCamX;
            float dy = targetCamY - currentCamY;

            if( std::abs( dx ) > 0.3f )
                currentCamX += dx * snapSpeed;
            else
                currentCamX = targetCamX;

            if( std::abs( dy ) > 0.3f )
                currentCamY += dy * snapSpeed;
            else
                currentCamY = targetCamY;

            updateLayout();
        }
    }

    void updateLayout()
    {
        int w = getWidth();
        int h = getHeight();
        if( w <= 0 || h <= 0 ) return;

        float dawW = 0.0f, dawH = 0.0f;
        getDawGeometry( dawW, dawH );

        float masterW = getMasterPanelWidth();
        float qlinkW  = getQLinkPanelWidth();

        // Single rigid 2D planar canvas coordinate mapping:
        // Screen Position = Canvas Position - Camera Position

        // 1. DAW Screen: Canvas X in [0, dawW], Canvas Y in [0, dawH]
        if( kickHome != nullptr )
        {
            float dawScreenX = 0.0f - currentCamX;
            float dawScreenY = 0.0f - currentCamY;

            kickHome->setBounds( 0, 0, 1024, 600 );
            kickHome->setTransform(
                juce::AffineTransform::scale( dawW / 1024.0f, dawH / 600.0f )
                                     .translated( dawScreenX, dawScreenY )
            );
        }

        // 2. Master Panel: Canvas X in [-masterW, 0], Canvas Y in [0, dawH]
        if( masterPanel != nullptr )
        {
            float mScreenX = -masterW - currentCamX;
            float mScreenY = 0.0f - currentCamY;
            masterPanel->setBounds( (int)std::round( mScreenX ),
                                    (int)std::round( mScreenY ),
                                    (int)std::round( masterW ),
                                    (int)std::round( dawH ) );
        }

        // 3. Q-Link Panel: Canvas X in [dawW, dawW + qlinkW], Canvas Y in [0, dawH]
        if( qlinkPanel != nullptr )
        {
            float qScreenX = dawW - currentCamX;
            float qScreenY = 0.0f - currentCamY;
            qlinkPanel->setBounds( (int)std::round( qScreenX ),
                                   (int)std::round( qScreenY ),
                                   (int)std::round( qlinkW ),
                                   (int)std::round( dawH ) );
        }

        // 4. Pad & Bank Panel (Directly under DAW!):
        // Canvas X in [0, dawW], Canvas Y in [dawH, 2*dawH]
        // Left gap is masterW wide, right gap is qlinkW wide!
        if( bottomPanel != nullptr )
        {
            float bScreenX = 0.0f - currentCamX;
            float bScreenY = dawH - currentCamY;
            bottomPanel->setBounds( (int)std::round( bScreenX ),
                                    (int)std::round( bScreenY ),
                                    (int)std::round( dawW ),
                                    (int)std::round( dawH ) );
        }
    }

    //==========================================================================
    // Gesture & Touch Handling
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

        dragStartCamX = currentCamX;
        dragStartCamY = currentCamY;
        lastTouchP    = screenP;
        dragVx = 0.0f;
        dragVy = 0.0f;

        // Check if touch occurred inside DAW screen or any of its controls (e.g. Back button, header)
        bool isDawTouch = ( kickHome != nullptr && ( e.eventComponent == kickHome || kickHome->isParentOf( e.eventComponent ) ) );

        // Check if touch occurred on Q-Link strip or any of its knobs
        bool isQLinkTouch = !isDawTouch && ( qlinkPanel != nullptr && ( e.eventComponent == qlinkPanel.get() || qlinkPanel->isParentOf( e.eventComponent ) ) );
        if( !isDawTouch && !isQLinkTouch && qlinkPanel != nullptr && qlinkPanel->isVisible() )
        {
            isQLinkTouch = qlinkPanel->getBounds().toFloat().contains( localP );
        }

        // Check if touch occurred on Master strip or any of its knobs
        bool isMasterTouch = !isDawTouch && ( masterPanel != nullptr && ( e.eventComponent == masterPanel.get() || masterPanel->isParentOf( e.eventComponent ) ) );
        if( !isDawTouch && !isMasterTouch && masterPanel != nullptr && masterPanel->isVisible() )
        {
            isMasterTouch = masterPanel->getBounds().toFloat().contains( localP );
        }

        // Tap on peeking control -> magnetizes to that panel
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
        // Tapping DAW does not jump back
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

        float dtVx = screenP.x - lastTouchP.x;
        float dtVy = screenP.y - lastTouchP.y;
        dragVx = dragVx * 0.6f + dtVx * 0.4f;
        dragVy = dragVy * 0.6f + dtVy * 0.4f;
        lastTouchP = screenP;

        bool has3Fingers = ( count >= 3 ) || ( e.mods.isShiftDown() && count >= 1 );

        bool startedOnChassis = false;
        if( it != activeTouches.end() )
        {
            auto* comp = it->second.startComponent;
            if( comp == this || comp == masterPanel.get() || comp == qlinkPanel.get() )
                startedOnChassis = true;
        }

        // 3 fingers drag ANYWHERE or 1 finger drag on background/chassis
        if( has3Fingers || ( startedOnChassis && count == 1 ) )
        {
            float avgDx = totalDx / (float)juce::jmax( 1, count );
            float avgDy = totalDy / (float)juce::jmax( 1, count );

            if( std::abs( avgDx ) > 12.0f || std::abs( avgDy ) > 12.0f )
            {
                isUserDragging = true;
                currentCamX = dragStartCamX - avgDx;
                currentCamY = dragStartCamY - avgDy;
                updateLayout();
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
            if( isUserDragging )
            {
                isUserDragging = false;
                // Magnetize / snap to the closest / most trivial point!
                PanelState best = findClosestSnapState( currentCamX, currentCamY, dragVx, dragVy );
                setPanelState( best );
            }
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

    float currentCamX{ 0.0f };
    float currentCamY{ 0.0f };

    bool  isUserDragging{ false };
    float dragStartCamX{ 0.0f };
    float dragStartCamY{ 0.0f };
    juce::Point<float> lastTouchP;
    float dragVx{ 0.0f };
    float dragVy{ 0.0f };

    std::map<int, TouchPoint> activeTouches;
    bool                      gestureTriggered{ false };
};

} // namespace kick_mobile
