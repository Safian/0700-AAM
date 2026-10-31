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
 * Unified Hardware Chassis Canvas.
 * Renders the authentic full hardware faceplate (700x710 base coordinates) with:
 * - Live Kick DAW embedded in the center screen cutout (159, 78, 382, 224)
 * - 16 velocity pads (Pads 1..16)
 * - Touch Strip Ribbon with authentic 16-segment dynamic LED ladder
 * - Utility buttons (FULL LEVEL / HALF LVL, 16 LEVELS / NOTES, ERASE)
 * - Jog Wheel with center Enter button
 * - 4 Q-Link rotary encoders (P1..P4)
 * - Master & Rec volume knobs and Link button
 * - Bank A..D buttons
 * - Transport controls (REC, OVERDUB, STOP, PLAY, PAUSE)
 * - Mode buttons (Main, Browse, Track Mix, TC, Sampler, Prog Edit, etc.)
 */
class UnifiedHardwareChassisCanvas : public juce::Component
{
public:
    static constexpr float kBaseW = 700.0f;
    static constexpr float kBaseH = 710.0f;

    UnifiedHardwareChassisCanvas()
    {
        setOpaque( true );
        bgImage = loadChassisAsset( "hardware_front.png" );

        // 1. Live Kick DAW in screen cutout
        kickHome = &kick_ui::KickHome::Instance();
        addAndMakeVisible( kickHome );

        // 2. 16 Reusable Velocity Pads
        for( int i = 0; i < 16; ++i )
        {
            pads[i] = std::make_unique<kick_hw_ui::HardwarePad>( i );
            addAndMakeVisible( pads[i].get() );
        }

        // 3. Touch Ribbon with 16-segment dynamic LED ladder
        ribbon = std::make_unique<kick_hw_ui::HardwareRibbon>();
        addAndMakeVisible( ribbon.get() );

        // 4. Utility buttons under Touch Ribbon (FULL LEVEL, 16 LEVELS, ERASE)
        // FULL LEVEL: toggle
        utilButtons[0] = std::make_unique<kick_hw_ui::HardwareButton>( DCC::EN_FULL_LEVEL, false, juce::Colour( 0xffffcc00 ), true );
        addAndMakeVisible( utilButtons[0].get() );

        // 16 LEVELS: toggle
        utilButtons[1] = std::make_unique<kick_hw_ui::HardwareButton>( DCC::EN_16LEVELS, false, juce::Colour( 0xffffcc00 ), true );
        addAndMakeVisible( utilButtons[1].get() );

        // ERASE: momentary (active red)
        utilButtons[2] = std::make_unique<kick_hw_ui::HardwareButton>( DCC::EN_ERASE, false, juce::Colour( 0xffff3b30 ), false );
        addAndMakeVisible( utilButtons[2].get() );

        // 5. Center Jog Wheel
        jogWheel = std::make_unique<kick_hw_ui::HardwareJogWheel>();
        addAndMakeVisible( jogWheel.get() );

        // 6. Q-Link Knobs (P1..P4)
        for( int i = 0; i < 4; ++i )
        {
            qlinks[i] = std::make_unique<kick_hw_ui::HardwareRotaryKnob>( DCC::EN_KNOB_1 + i, true, 0.5f );
            addAndMakeVisible( qlinks[i].get() );
        }

        // 7. Master & Rec Knobs
        masterKnob = std::make_unique<kick_hw_ui::HardwareRotaryKnob>( DCC::EN_KNOB_5, false, 0.85f );
        addAndMakeVisible( masterKnob.get() );

        recKnob = std::make_unique<kick_hw_ui::HardwareRotaryKnob>( DCC::EN_KNOB_6, false, 0.5f );
        addAndMakeVisible( recKnob.get() );

        linkBtn = std::make_unique<kick_hw_ui::HardwareButton>( DCC::EN_LINK, false, juce::Colour( 0xff00d4ff ), true );
        addAndMakeVisible( linkBtn.get() );

        // 8. Bank A..D buttons
        const int bankCodes[4] = { DCC::EN_BANK_A, DCC::EN_BANK_B, DCC::EN_BANK_C, DCC::EN_BANK_D };
        for( int b = 0; b < 4; ++b )
        {
            bankButtons[b] = std::make_unique<kick_hw_ui::HardwareButton>( bankCodes[b], false, juce::Colour( 0xffff9900 ), true );
            if( b == 0 ) bankButtons[b]->setLit( true );
            addAndMakeVisible( bankButtons[b].get() );
        }

        // 9. Transport buttons (REC, OVR, STOP, PLAY, PAUSE)
        const int transCodes[5] = { DCC::EN_RECORD, DCC::EN_OVERDUB, DCC::EN_STOP, DCC::EN_PLAY, DCC::EN_PLAYSTART };
        const juce::Colour transCols[5] = {
            juce::Colour( 0xffff3b30 ),
            juce::Colour( 0xffff9500 ),
            juce::Colour( 0xffffcc00 ),
            juce::Colour( 0xff34c759 ),
            juce::Colour( 0xff00d4ff )
        };
        for( int t = 0; t < 5; ++t )
        {
            transButtons[t] = std::make_unique<kick_hw_ui::HardwareButton>( transCodes[t], true, transCols[t], false );
            addAndMakeVisible( transButtons[t].get() );
        }

        // 10. Mode buttons Row 1 & 2
        const int modeCodesTop[5] = { DCC::EN_MAINMODE, DCC::EN_BROWSE, DCC::EN_TRACK_MIX, DCC::EN_TRACK_MUTE, DCC::EN_NEXT_SEQ };
        for( int m = 0; m < 5; ++m )
        {
            modeButtonsTop[m] = std::make_unique<kick_hw_ui::HardwareButton>( modeCodesTop[m], false, juce::Colour( 0xff30d158 ), false );
            addAndMakeVisible( modeButtonsTop[m].get() );
        }

        const int modeCodesBtm[5] = { DCC::EN_STEP_SEQ, DCC::EN_TC, DCC::EN_SAMPLER, DCC::EN_SAMPLE_EDIT, DCC::EN_PROG_EDIT };
        for( int m = 0; m < 5; ++m )
        {
            modeButtonsBtm[m] = std::make_unique<kick_hw_ui::HardwareButton>( modeCodesBtm[m], false, juce::Colour( 0xff30d158 ), false );
            addAndMakeVisible( modeButtonsBtm[m].get() );
        }
    }

    ~UnifiedHardwareChassisCanvas() override
    {
        if( kickHome != nullptr )
        {
            kickHome->setTransform( juce::AffineTransform() );
            removeChildComponent( kickHome );
        }
    }

    void paint( juce::Graphics& g ) override
    {
        g.fillAll( juce::Colour( 0xff121215 ) );

        if( bgImage.isValid() )
        {
            g.setImageResamplingQuality( juce::Graphics::highResamplingQuality );
            g.drawImage( bgImage, getLocalBounds().toFloat() );
        }

        // Fill screen cutout with black behind DAW
        g.setColour( juce::Colours::black );
        g.fillRect( juce::Rectangle<float>( 159.0f, 78.0f, 382.0f, 224.0f ) );
    }

    void resized() override
    {
        // Screen cutout: 159, 78, 382, 224
        if( kickHome != nullptr )
        {
            kickHome->setBounds( 0, 0, 1024, 600 );
            kickHome->setTransform( juce::AffineTransform::scale( 382.0f / 1024.0f, 224.0f / 600.0f ).translated( 159.0f, 78.0f ) );
        }

        // 1. 16 Pads
        const float colX[4] = { 116.0f, 194.0f, 272.0f, 350.0f };
        const float rowY[4] = { 363.0f, 441.0f, 519.0f, 597.0f };
        for( int r = 0; r < 4; ++r )
        {
            for( int c = 0; c < 4; ++c )
            {
                int padIdx = ( 3 - r ) * 4 + c;
                pads[padIdx]->setBounds( (int)std::round( colX[c] ), (int)std::round( rowY[r] ), 59, 61 );
            }
        }

        // 2. Touch Ribbon with 16-segment dynamic LED ladder
        ribbon->setBounds( 35, 363, 55, 140 );

        // 3. Utility buttons under Touch Ribbon
        utilButtons[0]->setBounds( 45, 531, 38, 26 ); // FULL LEVEL
        utilButtons[1]->setBounds( 45, 593, 38, 26 ); // 16 LEVELS
        utilButtons[2]->setBounds( 45, 655, 38, 26 ); // ERASE

        // 4. Jog Wheel
        jogWheel->setBounds( 553 - 48, 524 - 48, 96, 96 );

        // 5. Q-Links (P1..P4)
        const float knobY[4] = { 114.0f, 178.0f, 244.0f, 308.0f };
        for( int i = 0; i < 4; ++i )
            qlinks[i]->setBounds( 621 - 24, (int)std::round( knobY[i] - 24.0f ), 48, 48 );

        // 6. Master & Rec Knobs
        masterKnob->setBounds( 76 - 24, 114 - 24, 48, 48 );
        recKnob->setBounds( 76 - 24, 178 - 24, 48, 48 );
        linkBtn->setBounds( 57, 218, 38, 22 );

        // 7. Bank A..D Buttons
        for( int b = 0; b < 4; ++b )
            bankButtons[b]->setBounds( 441 + b * 63, 362, 34, 25 );

        // 8. Transport Buttons (REC, OVR, STOP, PLAY, PAUSE)
        const int transX[5] = { 440, 484, 534, 579, 623 };
        for( int t = 0; t < 5; ++t )
            transButtons[t]->setBounds( transX[t], 625, 34, 34 );

        // 9. Mode Buttons Top & Bottom
        const int modeX[5] = { 440, 484, 534, 579, 623 };
        for( int m = 0; m < 5; ++m )
        {
            modeButtonsTop[m]->setBounds( modeX[m], 415, 38, 24 );
            modeButtonsBtm[m]->setBounds( modeX[m], 465, 38, 24 );
        }
    }

    kick_ui::KickHome*                              kickHome{ nullptr };
    std::unique_ptr<kick_hw_ui::HardwarePad>        pads[16];
    std::unique_ptr<kick_hw_ui::HardwareRibbon>     ribbon;
    std::unique_ptr<kick_hw_ui::HardwareButton>     utilButtons[3];
    std::unique_ptr<kick_hw_ui::HardwareJogWheel>   jogWheel;
    std::unique_ptr<kick_hw_ui::HardwareRotaryKnob> qlinks[4];
    std::unique_ptr<kick_hw_ui::HardwareRotaryKnob> masterKnob;
    std::unique_ptr<kick_hw_ui::HardwareRotaryKnob> recKnob;
    std::unique_ptr<kick_hw_ui::HardwareButton>     linkBtn;
    std::unique_ptr<kick_hw_ui::HardwareButton>     bankButtons[4];
    std::unique_ptr<kick_hw_ui::HardwareButton>     transButtons[5];
    std::unique_ptr<kick_hw_ui::HardwareButton>     modeButtonsTop[5];
    std::unique_ptr<kick_hw_ui::HardwareButton>     modeButtonsBtm[5];

private:
    juce::Image bgImage;
};

//==============================================================================
/**
 * Root Mobile Container: Unified Hardware Chassis with Camera Viewport Navigation.
 * - Center: Full live Kick DAW in screen cutout.
 * - Bottom Left: 16 Pads + Touch Ribbon with 16-segment LED ladder + Utility buttons.
 * - Bottom Right: Jog Wheel + Bank Buttons + Transport (Sideways swipe from Pads!).
 * - Top Left: Master Volume & Rec Gain.
 * - Top Right: Q-Link rotary encoders.
 * - Quick Zone Switcher: 1-tap pill bar at the screen edge.
 */
class KickMobileTLayout : public juce::Component,
                          public juce::Timer
{
public:
    enum class PanelState
    {
        DAW = 0,
        BottomPads,
        BottomControls,
        MasterStrip,
        QLinkStrip
    };

    KickMobileTLayout()
    {
        setOpaque( true );

        // 1. Create the unified full hardware chassis canvas
        canvas = std::make_unique<UnifiedHardwareChassisCanvas>();
        addAndMakeVisible( canvas.get() );

        // 2. Initial camera frames the DAW screen
        targetState = PanelState::DAW;
        camRect = getTargetRectForState( targetState );

        // 3. Listen for touches & drag gestures across layout
        addMouseListener( this, true );

        startTimerHz( 60 );
    }

    ~KickMobileTLayout() override
    {
        stopTimer();
        removeMouseListener( this );
        canvas.reset();
    }

    void setPanelState( PanelState newState )
    {
        targetState = newState;
        repaint();
    }

    PanelState getPanelState() const noexcept { return targetState; }

    void paint( juce::Graphics& g ) override
    {
        g.fillAll( juce::Colour( 0xff0f0f12 ) );
    }

    void paintOverChildren( juce::Graphics& g ) override
    {
        // Draw sleek floating Quick Zone Switcher pill bar at bottom edge
        auto b = getLocalBounds();
        int pillBarH = 34;
        int pillBarW = juce::jmin( 380, b.getWidth() - 20 );
        auto pillBarRect = juce::Rectangle<int>( ( b.getWidth() - pillBarW ) / 2, b.getHeight() - pillBarH - 8, pillBarW, pillBarH );

        g.setColour( juce::Colour( 0xdd16161c ) );
        g.fillRoundedRectangle( pillBarRect.toFloat(), 17.0f );

        g.setColour( juce::Colour( 0x44ffffff ) );
        g.drawRoundedRectangle( pillBarRect.toFloat(), 17.0f, 1.0f );

        const char* tabNames[5] = { "DAW", "PADS", "CTRL", "Q-LINK", "MASTER" };
        const PanelState tabStates[5] = { PanelState::DAW, PanelState::BottomPads, PanelState::BottomControls, PanelState::QLinkStrip, PanelState::MasterStrip };

        float tabW = (float)pillBarW / 5.0f;
        for( int i = 0; i < 5; ++i )
        {
            auto tabRect = juce::Rectangle<float>( pillBarRect.getX() + i * tabW, (float)pillBarRect.getY(), tabW, (float)pillBarH ).reduced( 2.0f );
            bool isSelected = ( targetState == tabStates[i] );

            if( isSelected )
            {
                g.setColour( juce::Colour( 0x3300d4ff ) );
                g.fillRoundedRectangle( tabRect, 14.0f );
                g.setColour( juce::Colour( 0xff00d4ff ) );
                g.drawRoundedRectangle( tabRect, 14.0f, 1.5f );
                g.setColour( juce::Colour( 0xff00d4ff ) );
            }
            else
            {
                g.setColour( juce::Colour( 0x88ffffff ) );
            }

            g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
            g.drawText( tabNames[i], tabRect, juce::Justification::centred, false );
        }
    }

    void resized() override
    {
        applyCameraTransform();
    }

    juce::Rectangle<float> getTargetRectForState( PanelState state ) const
    {
        switch( state )
        {
            case PanelState::DAW:
                // Full DAW Screen Cutout
                return { 159.0f, 78.0f, 382.0f, 224.0f };

            case PanelState::BottomPads:
                // Touch Strip + Utility Buttons + 16 Pads
                return { 25.0f, 345.0f, 395.0f, 360.0f };

            case PanelState::BottomControls:
                // Jog Wheel + Bank Buttons + Modes + Transport (Sideways swipe view)
                return { 420.0f, 345.0f, 275.0f, 360.0f };

            case PanelState::MasterStrip:
                // Master Knob, Rec Knob, Link Button
                return { 20.0f, 60.0f, 135.0f, 275.0f };

            case PanelState::QLinkStrip:
                // 4 Q-Link Rotary Encoders
                return { 555.0f, 60.0f, 135.0f, 275.0f };
        }
        return { 159.0f, 78.0f, 382.0f, 224.0f };
    }

    void timerCallback() override
    {
        auto target = getTargetRectForState( targetState );

        if( !isUserDragging )
        {
            const float snapSpeed = 0.28f;
            float dx = ( target.getX() - camRect.getX() ) * snapSpeed;
            float dy = ( target.getY() - camRect.getY() ) * snapSpeed;
            float dw = ( target.getWidth() - camRect.getWidth() ) * snapSpeed;
            float dh = ( target.getHeight() - camRect.getHeight() ) * snapSpeed;

            if( std::abs( dx ) > 0.2f || std::abs( dy ) > 0.2f || std::abs( dw ) > 0.2f || std::abs( dh ) > 0.2f )
            {
                camRect.setX( camRect.getX() + dx );
                camRect.setY( camRect.getY() + dy );
                camRect.setWidth( camRect.getWidth() + dw );
                camRect.setHeight( camRect.getHeight() + dh );
                applyCameraTransform();
                repaint();
            }
            else
            {
                camRect = target;
                applyCameraTransform();
            }
        }
    }

    void applyCameraTransform()
    {
        if( canvas == nullptr ) return;
        int vw = getWidth();
        int vh = getHeight();
        if( vw <= 0 || vh <= 0 ) return;

        float sx = (float)vw / camRect.getWidth();
        float sy = (float)vh / camRect.getHeight();
        float s  = juce::jmin( sx, sy );

        float screenW = camRect.getWidth() * s;
        float screenH = camRect.getHeight() * s;
        float ox = ( (float)vw - screenW ) * 0.5f;
        float oy = ( (float)vh - screenH ) * 0.5f;

        juce::AffineTransform t = juce::AffineTransform::translation( -camRect.getX(), -camRect.getY() )
                                                      .scaled( s )
                                                      .translated( ox, oy );

        canvas->setBounds( 0, 0, (int)UnifiedHardwareChassisCanvas::kBaseW, (int)UnifiedHardwareChassisCanvas::kBaseH );
        canvas->setTransform( t );
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        // 1. Check if tap is inside bottom Quick Zone Switcher pill bar
        auto b = getLocalBounds();
        int pillBarH = 34;
        int pillBarW = juce::jmin( 380, b.getWidth() - 20 );
        auto pillBarRect = juce::Rectangle<int>( ( b.getWidth() - pillBarW ) / 2, b.getHeight() - pillBarH - 8, pillBarW, pillBarH );

        if( pillBarRect.contains( e.getPosition() ) )
        {
            float tabW = (float)pillBarW / 5.0f;
            int tabIdx = juce::jlimit( 0, 4, (int)( ( e.position.x - pillBarRect.getX() ) / tabW ) );
            const PanelState tabStates[5] = { PanelState::DAW, PanelState::BottomPads, PanelState::BottomControls, PanelState::QLinkStrip, PanelState::MasterStrip };
            setPanelState( tabStates[tabIdx] );
            return;
        }

        // 2. Record drag gesture start
        dragStartPos = e.position;
        lastTouchPos = e.position;
        hasDragMoved = false;
        isUserDragging = false;
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        float dx = e.position.x - dragStartPos.x;
        float dy = e.position.y - dragStartPos.y;

        if( std::abs( dx ) > 12.0f || std::abs( dy ) > 12.0f )
        {
            hasDragMoved = true;
        }
        lastTouchPos = e.position;
    }

    void mouseUp( const juce::MouseEvent& e ) override
    {
        if( hasDragMoved )
        {
            float dx = e.position.x - dragStartPos.x;
            float dy = e.position.y - dragStartPos.y;

            // Gesture logic:
            if( targetState == PanelState::DAW )
            {
                if( dy < -40.0f )
                    setPanelState( PanelState::BottomPads );
                else if( dx > 40.0f )
                    setPanelState( PanelState::MasterStrip );
                else if( dx < -40.0f )
                    setPanelState( PanelState::QLinkStrip );
            }
            else if( targetState == PanelState::BottomPads )
            {
                // Swipe left moves sideways to Jog/Transport controls!
                if( dx < -40.0f )
                    setPanelState( PanelState::BottomControls );
                else if( dy > 40.0f )
                    setPanelState( PanelState::DAW );
            }
            else if( targetState == PanelState::BottomControls )
            {
                // Swipe right moves sideways back to Pads!
                if( dx > 40.0f )
                    setPanelState( PanelState::BottomPads );
                else if( dy > 40.0f )
                    setPanelState( PanelState::DAW );
            }
            else if( targetState == PanelState::MasterStrip )
            {
                if( dx < -30.0f )
                    setPanelState( PanelState::DAW );
            }
            else if( targetState == PanelState::QLinkStrip )
            {
                if( dx > 30.0f )
                    setPanelState( PanelState::DAW );
            }
        }
        hasDragMoved = false;
        isUserDragging = false;
    }

    bool handleKeyboardNav( const juce::KeyPress& key )
    {
        if( key.isKeyCode( juce::KeyPress::upKey ) )
        {
            setPanelState( PanelState::BottomPads );
            return true;
        }
        if( key.isKeyCode( juce::KeyPress::downKey ) )
        {
            setPanelState( PanelState::DAW );
            return true;
        }
        if( key.isKeyCode( juce::KeyPress::leftKey ) )
        {
            if( targetState == PanelState::BottomControls )
                setPanelState( PanelState::BottomPads );
            else
                setPanelState( PanelState::QLinkStrip );
            return true;
        }
        if( key.isKeyCode( juce::KeyPress::rightKey ) )
        {
            if( targetState == PanelState::BottomPads )
                setPanelState( PanelState::BottomControls );
            else
                setPanelState( PanelState::MasterStrip );
            return true;
        }
        return false;
    }

    UnifiedHardwareChassisCanvas* getCanvas() const noexcept { return canvas.get(); }

private:
    std::unique_ptr<UnifiedHardwareChassisCanvas> canvas;
    PanelState                                   targetState{ PanelState::DAW };
    juce::Rectangle<float>                       camRect;

    juce::Point<float>                           dragStartPos;
    juce::Point<float>                           lastTouchPos;
    bool                                         hasDragMoved{ false };
    bool                                         isUserDragging{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickMobileTLayout )
};

} // namespace kick_mobile
