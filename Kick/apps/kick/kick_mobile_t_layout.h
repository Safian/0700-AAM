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
#include <map>
#include <set>
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
 * Renders the authentic Behringer Kick lower hardware surface (746x402 base coords).
 */
class MobileBottomChassisPanel : public juce::Component
{
public:
    MobileBottomChassisPanel()
    {
        bgImage = loadChassisAsset( "mobile_hw_bottom.png" );
        setOpaque( true );
    }

    void paint( juce::Graphics& g ) override
    {
        g.fillAll( juce::Colour( 0xff121215 ) );

        if( !bgImage.isValid() || renderedRect.isEmpty() )
            return;

        g.drawImage( bgImage, renderedRect, juce::RectanglePlacement::centred );

        const float scale = renderedRect.getWidth() / 746.0f;
        const float ox    = renderedRect.getX();
        const float oy    = renderedRect.getY();

        // 1. Draw glowing cyan frames on active pads
        for( int pad : activePads )
        {
            auto pr = getPadRect( pad );
            if( !pr.isEmpty() )
            {
                g.setColour( juce::Colour( 0x5500d4ff ) );
                g.fillRoundedRectangle( pr, 6.0f * scale );
                g.setColour( juce::Colour( 0xff00d4ff ) );
                g.drawRoundedRectangle( pr, 6.0f * scale, 3.0f * scale );
            }
        }

        // 2. Active Bank highlight
        if( activeBank >= 0 && activeBank < 4 )
        {
            const float bankCx[4] = { 493.0f, 561.0f, 629.0f, 697.0f };
            juce::Rectangle<float> br(
                ox + ( bankCx[activeBank] - 18.0f ) * scale,
                oy + ( 50.0f - 12.0f ) * scale,
                36.0f * scale,
                24.0f * scale
            );
            g.setColour( juce::Colour( 0x44ff9900 ) );
            g.fillRoundedRectangle( br, 4.0f * scale );
            g.setColour( juce::Colour( 0xffff9900 ) );
            g.drawRoundedRectangle( br, 4.0f * scale, 2.0f * scale );
        }

        // 3. Jog Wheel indicator
        const float jogCx = ox + 590.0f * scale;
        const float jogCy = oy + 184.0f * scale;
        const float jogR  = 40.0f * scale;
        const float indX  = jogCx + std::cos( jogAngle ) * ( jogR * 0.72f );
        const float indY  = jogCy + std::sin( jogAngle ) * ( jogR * 0.72f );
        g.setColour( juce::Colour( 0xffff3b30 ) );
        g.fillEllipse( indX - 4.0f * scale, indY - 4.0f * scale, 8.0f * scale, 8.0f * scale );

        // 4. Ribbon touch strip LED ladder
        if( ribbonTouching )
        {
            float rx = ox + 25.0f * scale;
            float ry = oy + 35.0f * scale;
            float rw = 67.0f * scale;
            float rh = 150.0f * scale;

            float ty = ry + rh * ( 1.0f - ribbonValue );
            g.setColour( juce::Colour( 0x44ff2d55 ) );
            g.fillRect( rx, ty, rw, ( ry + rh ) - ty );
            g.setColour( juce::Colour( 0xffff2d55 ) );
            g.fillRect( rx, ty - 2.0f * scale, rw, 4.0f * scale );
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
    }

    juce::Point<float> toBaseCoords( juce::Point<float> p ) const
    {
        if( renderedRect.isEmpty() ) return { -1.0f, -1.0f };
        float scale = renderedRect.getWidth() / 746.0f;
        if( scale <= 0.0f ) return { -1.0f, -1.0f };
        return { ( p.x - renderedRect.getX() ) / scale, ( p.y - renderedRect.getY() ) / scale };
    }

    int getPadAt( float bx, float by ) const
    {
        const float colX[4] = { 118.0f, 203.0f, 289.0f, 375.0f };
        const float padW = 67.0f;
        const float rowY[4] = { 31.0f, 116.0f, 203.0f, 288.0f };
        const float padH = 67.0f;
        const float padMargin = 6.0f;

        int foundCol = -1;
        for( int c = 0; c < 4; ++c )
        {
            if( bx >= colX[c] - padMargin && bx <= colX[c] + padW + padMargin )
            {
                foundCol = c;
                break;
            }
        }
        if( foundCol < 0 ) return -1;

        int foundRow = -1;
        for( int r = 0; r < 4; ++r )
        {
            if( by >= rowY[r] - padMargin && by <= rowY[r] + padH + padMargin )
            {
                foundRow = r;
                break;
            }
        }
        if( foundRow < 0 ) return -1;

        return ( 3 - foundRow ) * 4 + foundCol;
    }

    juce::Rectangle<float> getPadRect( int padIndex ) const
    {
        if( renderedRect.isEmpty() || padIndex < 0 || padIndex >= 16 ) return {};
        const float scale = renderedRect.getWidth() / 746.0f;
        int row = 3 - ( padIndex / 4 );
        int col = padIndex % 4;

        const float colX[4] = { 118.0f, 203.0f, 289.0f, 375.0f };
        const float rowY[4] = { 31.0f, 116.0f, 203.0f, 288.0f };

        return juce::Rectangle<float>(
            renderedRect.getX() + colX[col] * scale,
            renderedRect.getY() + rowY[row] * scale,
            67.0f * scale,
            67.0f * scale
        );
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        auto bp = toBaseCoords( e.position );
        if( bp.x < 0.0f ) return;

        int sourceIdx = e.source.getIndex();

        // 1. Check 16 MPC Pads
        int pad = getPadAt( bp.x, bp.y );
        if( pad >= 0 )
        {
            touchPadMap[sourceIdx] = pad;
            activePads.insert( pad );
            sendPadMessage( pad, true, 127 );
            repaint();
            return;
        }

        // 2. Check Jog Wheel
        float jDist = std::hypot( bp.x - 590.0f, bp.y - 184.0f );
        if( jDist <= 52.0f )
        {
            if( jDist <= 20.0f )
            {
                // Enter button center
                sendButtonMessage( DCC::EN_KNOB_CENTER, true );
            }
            else
            {
                draggingJog = true;
                jogLastAngle = std::atan2( bp.y - 184.0f, bp.x - 590.0f );
            }
            return;
        }

        // 3. Check Bank A..D
        for( int b = 0; b < 4; ++b )
        {
            const float bankCx[4] = { 493.0f, 561.0f, 629.0f, 697.0f };
            if( std::abs( bp.x - bankCx[b] ) <= 22.0f && std::abs( bp.y - 50.0f ) <= 16.0f )
            {
                activeBank = b;
                sendButtonMessage( DCC::EN_BANK_A + b, true );
                repaint();
                return;
            }
        }

        // 4. Check Transport row: REC, OVR, STOP, PLAY, PAUSE
        const float transCx[5] = { 490.0f, 543.0f, 593.0f, 645.0f, 695.0f };
        const int transBtns[5] = { DCC::EN_RECORD, DCC::EN_OVERDUB, DCC::EN_STOP, DCC::EN_PLAY, DCC::EN_PLAYSTART };
        for( int t = 0; t < 5; ++t )
        {
            if( std::hypot( bp.x - transCx[t], bp.y - 355.0f ) <= 20.0f )
            {
                activeTransBtn = transBtns[t];
                sendButtonMessage( activeTransBtn, true );
                repaint();
                return;
            }
        }

        // 5. Check Ribbon
        if( bp.x >= 25.0f && bp.x <= 92.0f && bp.y >= 35.0f && bp.y <= 185.0f )
        {
            ribbonTouching = true;
            ribbonValue = juce::jlimit( 0.0f, 1.0f, 1.0f - ( bp.y - 35.0f ) / 150.0f );
            sendRibbonMessage( ribbonValue );
            repaint();
            return;
        }

        // 6. Check Left utility buttons: FULL LEVEL, 16 LEVELS, ERASE
        if( bp.x >= 25.0f && bp.x <= 95.0f )
        {
            if( std::abs( bp.y - 234.0f ) <= 18.0f ) { sendButtonMessage( DCC::EN_FULL_LEVEL, true ); return; }
            if( std::abs( bp.y - 285.0f ) <= 18.0f ) { sendButtonMessage( DCC::EN_16LEVELS, true ); return; }
            if( std::abs( bp.y - 340.0f ) <= 18.0f ) { sendButtonMessage( DCC::EN_ERASE, true ); return; }
        }

        // 7. Check Mode Buttons: MAIN, BROWSE, TRACK MIX, TRACK MUTE, NEXT SEC
        const float modeCx[5] = { 482.0f, 532.0f, 584.0f, 638.0f, 692.0f };
        if( std::abs( bp.y - 108.0f ) <= 15.0f )
        {
            const int modeBtns[5] = { DCC::EN_MAINMODE, DCC::EN_BROWSE, DCC::EN_TRACK_MIX, DCC::EN_TRACK_MUTE, DCC::EN_NEXT_SEQ };
            for( int m = 0; m < 5; ++m )
            {
                if( std::abs( bp.x - modeCx[m] ) <= 22.0f ) { sendButtonMessage( modeBtns[m], true ); return; }
            }
        }

        // 8. Check Bottom Mode Buttons: STEP SEQ, TC, SAMPLER, SAMPLE EDIT, PROG EDIT
        if( std::abs( bp.y - 292.0f ) <= 15.0f )
        {
            const int btmBtns[5] = { DCC::EN_STEP_SEQ, DCC::EN_TC, DCC::EN_SAMPLER, DCC::EN_SAMPLE_EDIT, DCC::EN_PROG_EDIT };
            for( int m = 0; m < 5; ++m )
            {
                if( std::abs( bp.x - modeCx[m] ) <= 22.0f ) { sendButtonMessage( btmBtns[m], true ); return; }
            }
        }
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        auto bp = toBaseCoords( e.position );
        if( bp.x < 0.0f ) return;

        if( draggingJog )
        {
            float curAngle = std::atan2( bp.y - 184.0f, bp.x - 590.0f );
            float delta = curAngle - jogLastAngle;
            if( delta > juce::MathConstants<float>::pi )  delta -= juce::MathConstants<float>::twoPi;
            if( delta < -juce::MathConstants<float>::pi ) delta += juce::MathConstants<float>::twoPi;

            jogAngle += delta;
            jogAccum += delta;
            jogLastAngle = curAngle;

            const float step = juce::MathConstants<float>::twoPi / 24.0f;
            if( std::abs( jogAccum ) >= step )
            {
                int ticks = (int)( jogAccum / step );
                jogAccum -= ticks * step;
                sendWheelMessage( ticks );
            }
            repaint();
            return;
        }

        if( ribbonTouching && bp.x >= 20.0f && bp.x <= 97.0f )
        {
            ribbonValue = juce::jlimit( 0.0f, 1.0f, 1.0f - ( bp.y - 35.0f ) / 150.0f );
            sendRibbonMessage( ribbonValue );
            repaint();
        }
    }

    void mouseUp( const juce::MouseEvent& e ) override
    {
        int sourceIdx = e.source.getIndex();
        auto it = touchPadMap.find( sourceIdx );
        if( it != touchPadMap.end() )
        {
            int pad = it->second;
            sendPadMessage( pad, false, 0 );
            activePads.erase( pad );
            touchPadMap.erase( it );
            repaint();
        }

        if( draggingJog )
        {
            draggingJog = false;
        }

        if( activeTransBtn >= 0 )
        {
            sendButtonMessage( activeTransBtn, false );
            activeTransBtn = -1;
            repaint();
        }

        if( ribbonTouching )
        {
            ribbonTouching = false;
            repaint();
        }
    }

    void sendPadMessage( int padIndex, bool pressed, int velocity )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_PAD;
        msg.pktCmd  = DCC::PadCmd::EN_PAD_STATE;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { padIndex, pressed ? (int)DCC::BTN_PRESSED : (int)DCC::BTN_RELEASED, velocity, 0 };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

    void sendButtonMessage( int buttonCode, bool pressed )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_BUTTON;
        msg.pktCmd  = DCC::BtnCmd::EN_BTN_STATUS;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { buttonCode, pressed ? (int)DCC::BTN_PRESSED : (int)DCC::BTN_RELEASED };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

    void sendWheelMessage( int ticks )
    {
        int dir = ( ticks > 0 ) ? (int)DCC::EN_KNOB_CLOCKWISE : (int)DCC::EN_KNOB_ANTICLOCKWISE;
        sendKnobMessage( DCC::EN_KNOB_CENTER, dir, std::max( 1, std::abs( ticks ) ), false );
    }

    void sendKnobMessage( int knobCode, int direction, int ticks, bool isQlink )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_KNOBS;
        msg.pktCmd  = isQlink ? (int)DCC::KnobCmd::EN_KNOB_QLINK
                              : (int)DCC::KnobCmd::EN_KNOB_ROTARY;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { knobCode, direction, ticks };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

    void sendRibbonMessage( float val )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_SLIDER;
        msg.pktCmd  = DCC::SliderCmd::EN_SLIDER_TOUCH_INPUT;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { (int)( val * 100.0f ) };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

private:
    juce::Image            bgImage;
    juce::Rectangle<float> renderedRect;
    std::map<int, int>     touchPadMap;
    std::set<int>          activePads;
    int                    activeBank{ 0 };
    int                    activeTransBtn{ -1 };

    bool                   draggingJog{ false };
    float                  jogAngle{ 0.0f };
    float                  jogLastAngle{ 0.0f };
    float                  jogAccum{ 0.0f };

    bool                   ribbonTouching{ false };
    float                  ribbonValue{ 0.0f };
};

//==============================================================================
/**
 * Master Panel (Left edge: Power, Master Volume knob, Rec Gain knob, Link button).
 * Base dimensions: 106 x 334.
 */
class MobileMasterChassisPanel : public juce::Component
{
public:
    MobileMasterChassisPanel()
    {
        bgImage = loadChassisAsset( "mobile_hw_master.png" );
        setOpaque( true );
    }

    void paint( juce::Graphics& g ) override
    {
        g.fillAll( juce::Colour( 0xff16161a ) );

        if( !bgImage.isValid() || renderedRect.isEmpty() )
            return;

        g.drawImage( bgImage, renderedRect, juce::RectanglePlacement::centred );

        const float scale = renderedRect.getHeight() / 334.0f;
        const float ox    = renderedRect.getX();
        const float oy    = renderedRect.getY();

        // Draw indicator arc for Master Volume knob (53, 112)
        if( activeKnob == 0 )
        {
            drawKnobArc( g, ox + 53.0f * scale, oy + 112.0f * scale, 24.0f * scale, masterVal );
        }

        // Draw indicator arc for Rec Gain knob (53, 202)
        if( activeKnob == 1 )
        {
            drawKnobArc( g, ox + 53.0f * scale, oy + 202.0f * scale, 24.0f * scale, recVal );
        }

        // Draw glowing border on right edge
        g.setColour( juce::Colour( 0x4400d4ff ) );
        g.drawLine( getWidth() - 1.0f, 0.0f, getWidth() - 1.0f, (float)getHeight(), 2.0f );
    }

    void drawKnobArc( juce::Graphics& g, float cx, float cy, float r, float val )
    {
        juce::Path p;
        float startA = juce::MathConstants<float>::pi * 0.75f;
        float endA   = startA + juce::MathConstants<float>::pi * 1.5f * val;
        p.addCentredArc( cx, cy, r + 4.0f, r + 4.0f, 0.0f, startA, endA, true );
        g.setColour( juce::Colour( 0xff00d4ff ) );
        g.strokePath( p, juce::PathStrokeType( 3.0f ) );
    }

    void resized() override
    {
        auto bounds = getLocalBounds().toFloat();
        const float targetAspect = 106.0f / 334.0f;
        float w = bounds.getHeight() * targetAspect;
        renderedRect = juce::Rectangle<float>( ( bounds.getWidth() - w ) * 0.5f, 0.0f, w, bounds.getHeight() );
    }

    juce::Point<float> toBaseCoords( juce::Point<float> p ) const
    {
        if( renderedRect.isEmpty() ) return { -1.0f, -1.0f };
        float scale = renderedRect.getHeight() / 334.0f;
        if( scale <= 0.0f ) return { -1.0f, -1.0f };
        return { ( p.x - renderedRect.getX() ) / scale, ( p.y - renderedRect.getY() ) / scale };
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        auto bp = toBaseCoords( e.position );
        if( bp.x < 0.0f ) return;

        // Master knob: (53, 112)
        if( std::hypot( bp.x - 53.0f, bp.y - 112.0f ) <= 28.0f )
        {
            activeKnob = 0;
            dragStartY = e.position.y;
            startVal   = masterVal;
            return;
        }

        // Rec knob: (53, 202)
        if( std::hypot( bp.x - 53.0f, bp.y - 202.0f ) <= 28.0f )
        {
            activeKnob = 1;
            dragStartY = e.position.y;
            startVal   = recVal;
            return;
        }

        // Link button: (25..85, 268..305)
        if( bp.x >= 25.0f && bp.x <= 85.0f && bp.y >= 268.0f && bp.y <= 305.0f )
        {
            sendButtonMessage( DCC::EN_LINK, true );
        }
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        if( activeKnob >= 0 )
        {
            float dy = dragStartY - e.position.y;
            float newVal = juce::jlimit( 0.0f, 1.0f, startVal + dy / 150.0f );
            if( activeKnob == 0 )
            {
                masterVal = newVal;
                int dir = ( dy > 0 ) ? (int)DCC::EN_KNOB_CLOCKWISE : (int)DCC::EN_KNOB_ANTICLOCKWISE;
                sendKnobMessage( DCC::EN_KNOB_5, dir, 1, false );
            }
            else
            {
                recVal = newVal;
                int dir = ( dy > 0 ) ? (int)DCC::EN_KNOB_CLOCKWISE : (int)DCC::EN_KNOB_ANTICLOCKWISE;
                sendKnobMessage( DCC::EN_KNOB_6, dir, 1, false );
            }
            repaint();
        }
    }

    void mouseUp( const juce::MouseEvent& ) override
    {
        activeKnob = -1;
        repaint();
    }

    void sendButtonMessage( int buttonCode, bool pressed )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_BUTTON;
        msg.pktCmd  = DCC::BtnCmd::EN_BTN_STATUS;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { buttonCode, pressed ? (int)DCC::BTN_PRESSED : (int)DCC::BTN_RELEASED };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

    void sendKnobMessage( int knobCode, int direction, int ticks, bool isQlink )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_KNOBS;
        msg.pktCmd  = isQlink ? (int)DCC::KnobCmd::EN_KNOB_QLINK
                              : (int)DCC::KnobCmd::EN_KNOB_ROTARY;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { knobCode, direction, ticks };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

private:
    juce::Image            bgImage;
    juce::Rectangle<float> renderedRect;
    int                    activeKnob{ -1 };
    float                  dragStartY{ 0.0f };
    float                  startVal{ 0.8f };
    float                  masterVal{ 0.85f };
    float                  recVal{ 0.5f };
};

//==============================================================================
/**
 * Q-Link Potentiometer Panel (Right edge: P1, P2, P3, P4 knobs).
 * Base dimensions: 74 x 302.
 */
class MobileQLinkChassisPanel : public juce::Component
{
public:
    MobileQLinkChassisPanel()
    {
        bgImage = loadChassisAsset( "mobile_hw_qlinks.png" );
        setOpaque( true );
    }

    void paint( juce::Graphics& g ) override
    {
        g.fillAll( juce::Colour( 0xff16161a ) );

        if( !bgImage.isValid() || renderedRect.isEmpty() )
            return;

        g.drawImage( bgImage, renderedRect, juce::RectanglePlacement::centred );

        const float scale = renderedRect.getHeight() / 302.0f;
        const float ox    = renderedRect.getX();
        const float oy    = renderedRect.getY();

        const float knobY[4] = { 36.0f, 105.0f, 174.0f, 243.0f };
        for( int i = 0; i < 4; ++i )
        {
            if( activeKnob == i )
            {
                drawKnobArc( g, ox + 37.0f * scale, oy + knobY[i] * scale, 22.0f * scale, knobVals[i] );
            }
        }

        // Draw glowing border on left edge
        g.setColour( juce::Colour( 0x4400d4ff ) );
        g.drawLine( 0.0f, 0.0f, 0.0f, (float)getHeight(), 2.0f );
    }

    void drawKnobArc( juce::Graphics& g, float cx, float cy, float r, float val )
    {
        juce::Path p;
        float startA = juce::MathConstants<float>::pi * 0.75f;
        float endA   = startA + juce::MathConstants<float>::pi * 1.5f * val;
        p.addCentredArc( cx, cy, r + 4.0f, r + 4.0f, 0.0f, startA, endA, true );
        g.setColour( juce::Colour( 0xff00d4ff ) );
        g.strokePath( p, juce::PathStrokeType( 3.0f ) );
    }

    void resized() override
    {
        auto bounds = getLocalBounds().toFloat();
        const float targetAspect = 74.0f / 302.0f;
        float w = bounds.getHeight() * targetAspect;
        renderedRect = juce::Rectangle<float>( ( bounds.getWidth() - w ) * 0.5f, 0.0f, w, bounds.getHeight() );
    }

    juce::Point<float> toBaseCoords( juce::Point<float> p ) const
    {
        if( renderedRect.isEmpty() ) return { -1.0f, -1.0f };
        float scale = renderedRect.getHeight() / 302.0f;
        if( scale <= 0.0f ) return { -1.0f, -1.0f };
        return { ( p.x - renderedRect.getX() ) / scale, ( p.y - renderedRect.getY() ) / scale };
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        auto bp = toBaseCoords( e.position );
        if( bp.x < 0.0f ) return;

        const float knobY[4] = { 36.0f, 105.0f, 174.0f, 243.0f };
        for( int i = 0; i < 4; ++i )
        {
            if( std::hypot( bp.x - 37.0f, bp.y - knobY[i] ) <= 24.0f )
            {
                activeKnob = i;
                dragStartY = e.position.y;
                startVal   = knobVals[i];
                repaint();
                return;
            }
        }
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        if( activeKnob >= 0 && activeKnob < 4 )
        {
            float dy = dragStartY - e.position.y;
            float newVal = juce::jlimit( 0.0f, 1.0f, startVal + dy / 150.0f );
            knobVals[activeKnob] = newVal;
            int dir = ( dy > 0 ) ? (int)DCC::EN_KNOB_CLOCKWISE : (int)DCC::EN_KNOB_ANTICLOCKWISE;
            sendKnobMessage( DCC::EN_KNOB_1 + activeKnob, dir, 1, true );
            repaint();
        }
    }

    void mouseUp( const juce::MouseEvent& ) override
    {
        activeKnob = -1;
        repaint();
    }

    void sendKnobMessage( int knobCode, int direction, int ticks, bool isQlink )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_KNOBS;
        msg.pktCmd  = isQlink ? (int)DCC::KnobCmd::EN_KNOB_QLINK
                              : (int)DCC::KnobCmd::EN_KNOB_ROTARY;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { knobCode, direction, ticks };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

private:
    juce::Image            bgImage;
    juce::Rectangle<float> renderedRect;
    int                    activeKnob{ -1 };
    float                  dragStartY{ 0.0f };
    float                  startVal{ 0.5f };
    float                  knobVals[4]{ 0.5f, 0.5f, 0.5f, 0.5f };
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

        bottomPanel = std::make_unique<MobileBottomChassisPanel>();
        addChildComponent( bottomPanel.get() );

        masterPanel = std::make_unique<MobileMasterChassisPanel>();
        addChildComponent( masterPanel.get() );

        qlinkPanel = std::make_unique<MobileQLinkChassisPanel>();
        addChildComponent( qlinkPanel.get() );

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
                bottomPanel->setVisible( true );
                bottomPanel->toFront( true );
            }
            else if( targetState == PanelState::MasterStrip )
            {
                masterPanel->setVisible( true );
                masterPanel->toFront( true );
            }
            else if( targetState == PanelState::QLinkStrip )
            {
                qlinkPanel->setVisible( true );
                qlinkPanel->toFront( true );
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

    void timerCallback() override
    {
        const float snap = 0.28f;
        bool needUpdate = false;

        float targetDawY = ( targetState == PanelState::BottomPads ) ? -(float)getHeight() : 0.0f;
        float targetDawX = 0.0f;
        if( targetState == PanelState::MasterStrip ) targetDawX = getMasterPanelWidth();
        if( targetState == PanelState::QLinkStrip )  targetDawX = -getQLinkPanelWidth();

        if( std::abs( currentDawY - targetDawY ) > 0.5f )
        {
            currentDawY += ( targetDawY - currentDawY ) * snap;
            needUpdate = true;
        }
        else
        {
            currentDawY = targetDawY;
        }

        if( std::abs( currentDawX - targetDawX ) > 0.5f )
        {
            currentDawX += ( targetDawX - currentDawX ) * snap;
            needUpdate = true;
        }
        else
        {
            currentDawX = targetDawX;
        }

        if( needUpdate )
        {
            updateLayout();
        }
        else
        {
            // Animation finished: hide invisible panels
            if( targetState == PanelState::DAW )
            {
                if( bottomPanel != nullptr && bottomPanel->isVisible() ) bottomPanel->setVisible( false );
                if( masterPanel != nullptr && masterPanel->isVisible() ) masterPanel->setVisible( false );
                if( qlinkPanel != nullptr && qlinkPanel->isVisible() )  qlinkPanel->setVisible( false );
            }
            else if( targetState == PanelState::BottomPads )
            {
                if( masterPanel != nullptr && masterPanel->isVisible() ) masterPanel->setVisible( false );
                if( qlinkPanel != nullptr && qlinkPanel->isVisible() )  qlinkPanel->setVisible( false );
            }
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

    void updateLayout()
    {
        int w = getWidth();
        int h = getHeight();
        if( w <= 0 || h <= 0 ) return;

        // Position DAW with translation (currentDawX, currentDawY)
        if( kickHome != nullptr )
        {
            const float targetAspect = 1024.0f / 600.0f;
            const float currentAspect = (float)w / (float)h;

            float dawW = (float)w;
            float dawH = (float)h;
            float dawX = 0.0f;
            float dawY = 0.0f;

            if( currentAspect > targetAspect )
            {
                dawW = (float)h * targetAspect;
                dawX = ( (float)w - dawW ) * 0.5f;
            }
            else
            {
                dawH = (float)w / targetAspect;
                dawY = ( (float)h - dawH ) * 0.5f;
            }

            kickHome->setBounds( 0, 0, 1024, 600 );
            kickHome->setTransform(
                juce::AffineTransform::scale( dawW / 1024.0f, dawH / 600.0f )
                                     .translated( dawX + currentDawX, dawY + currentDawY )
            );
        }

        // Position Bottom Hardware Panel (slides from y = h to y = 0)
        if( bottomPanel != nullptr )
        {
            bottomPanel->setBounds( 0, (int)( currentDawY + (float)h ), w, h );
        }

        // Position Master Panel (slides from x = -masterW to x = 0)
        float masterW = getMasterPanelWidth();
        if( masterPanel != nullptr )
        {
            float mX = currentDawX - masterW;
            masterPanel->setBounds( (int)mX, 0, (int)masterW, h );
        }

        // Position Q-Link Panel (slides from x = w to x = w - qlinkW)
        float qlinkW = getQLinkPanelWidth();
        if( qlinkPanel != nullptr )
        {
            float qX = (float)w + currentDawX;
            qlinkPanel->setBounds( (int)qX, 0, (int)qlinkW, h );
        }
    }

    //==========================================================================
    // Multi-touch 3-finger gesture recognition
    //==========================================================================
    struct TouchPoint
    {
        bool               isDown{ false };
        juce::Point<float> startPos;
        juce::Point<float> currentPos;
    };

    void mouseDown( const juce::MouseEvent& e ) override
    {
        int sourceIdx = e.source.getIndex();
        auto screenP  = e.getScreenPosition().toFloat();
        activeTouches[sourceIdx] = { true, screenP, screenP };
        gestureTriggered = false;

        // If a side drawer is open, tapping anywhere outside the drawer closes it
        if( targetState == PanelState::MasterStrip && e.position.x > getMasterPanelWidth() )
        {
            setPanelState( PanelState::DAW );
        }
        else if( targetState == PanelState::QLinkStrip && e.position.x < (float)getWidth() - getQLinkPanelWidth() )
        {
            setPanelState( PanelState::DAW );
        }
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

        // Count active fingers
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

        // 3-Finger Gesture Detection (or Shift+drag on desktop)
        bool has3Fingers = ( count >= 3 ) || ( e.mods.isShiftDown() && count >= 1 );

        if( has3Fingers )
        {
            float avgDx = totalDx / (float)juce::jmax( 1, count );
            float avgDy = totalDy / (float)juce::jmax( 1, count );

            const float threshold = 38.0f;

            if( std::abs( avgDy) > threshold && std::abs( avgDy ) > std::abs( avgDx ) * 1.3f )
            {
                gestureTriggered = true;
                if( avgDy < 0.0f )
                {
                    // 3-finger swipe UP -> Show Bottom Hardware Panel
                    setPanelState( PanelState::BottomPads );
                }
                else
                {
                    // 3-finger swipe DOWN -> Return to DAW
                    setPanelState( PanelState::DAW );
                }
            }
            else if( std::abs( avgDx ) > threshold && std::abs( avgDx ) > std::abs( avgDy ) * 1.3f )
            {
                gestureTriggered = true;
                if( avgDx > 0.0f )
                {
                    // 3-finger swipe RIGHT
                    if( targetState == PanelState::DAW )
                        setPanelState( PanelState::MasterStrip );
                    else if( targetState == PanelState::QLinkStrip )
                        setPanelState( PanelState::DAW );
                }
                else
                {
                    // 3-finger swipe LEFT
                    if( targetState == PanelState::DAW )
                        setPanelState( PanelState::QLinkStrip );
                    else if( targetState == PanelState::MasterStrip )
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

    // Keyboard navigation helper for desktop testing
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

    std::map<int, TouchPoint> activeTouches;
    bool                      gestureTriggered{ false };
};

} // namespace kick_mobile
