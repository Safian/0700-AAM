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
#include <cmath>

namespace kick_hw_ui
{

using DCC = contracts::DeviceControlContracts;

//==============================================================================
// DCL Hardware Dispatch Helpers
//==============================================================================

inline void sendDclPad( int padIndex, bool pressed, int velocity )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_PAD;
    msg.pktCmd  = DCC::PadCmd::EN_PAD_STATE;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { padIndex, pressed ? (int)DCC::BTN_PRESSED : (int)DCC::BTN_RELEASED, velocity, 0 };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

inline void sendDclButton( int buttonCode, bool pressed )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_BUTTON;
    msg.pktCmd  = DCC::BtnCmd::EN_BTN_STATUS;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { buttonCode, pressed ? (int)DCC::BTN_PRESSED : (int)DCC::BTN_RELEASED };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

inline void sendDclKnob( int knobCode, int direction, int ticks, bool isQlink )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_KNOBS;
    msg.pktCmd  = isQlink ? (int)DCC::KnobCmd::EN_KNOB_QLINK
                          : (int)DCC::KnobCmd::EN_KNOB_ROTARY;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { knobCode, direction, ticks };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

inline void sendDclSlider( float val0to1 )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_SLIDER;
    msg.pktCmd  = DCC::SliderCmd::EN_SLIDER_TOUCH_INPUT;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { (int)( val0to1 * 100.0f ) };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

//==============================================================================
/**
 * 1. HardwarePad: Authentic MPC Drum Pad Component.
 * Self-contained touch/mouse interaction, velocity calculation, neon cyan glow.
 */
class HardwarePad : public juce::Component
{
public:
    HardwarePad( int index = 0 )
        : padIndex( index )
    {
        setOpaque( false );
    }

    void setPadIndex( int index ) { padIndex = index; }
    int  getPadIndex() const noexcept { return padIndex; }

    void setGlowColour( juce::Colour col ) { glowColour = col; repaint(); }

    void setPressed( bool pressed, int vel = 127 )
    {
        if( isDown != pressed )
        {
            isDown = pressed;
            velocity = vel;
            sendDclPad( padIndex, isDown, velocity );
            repaint();
        }
    }

    bool isPressed() const noexcept { return isDown; }

    void paint( juce::Graphics& g ) override
    {
        if( isDown )
        {
            auto r = getLocalBounds().toFloat().reduced( 2.0f );
            g.setColour( glowColour.withAlpha( 0.35f ) );
            g.fillRoundedRectangle( r, 6.0f );
            g.setColour( glowColour );
            g.drawRoundedRectangle( r, 6.0f, 2.5f );
        }
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        int vel = juce::jlimit( 60, 127, (int)( 80.0f + e.position.y / (float)getHeight() * 47.0f ) );
        setPressed( true, vel );
    }

    void mouseUp( const juce::MouseEvent& ) override
    {
        setPressed( false, 0 );
    }

private:
    int          padIndex{ 0 };
    bool         isDown{ false };
    int          velocity{ 0 };
    juce::Colour glowColour{ 0xff00d4ff }; // Cyan neon default
};

//==============================================================================
/**
 * 2. HardwareRotaryKnob: Continuous Rotary Encoder / Potentiometer.
 * Supports both intuitive vertical touch dragging and mouse wheel.
 * Draws dynamic circular LED indicator arc and value dot.
 */
class HardwareRotaryKnob : public juce::Component
{
public:
    HardwareRotaryKnob( int code = DCC::EN_KNOB_1, bool qlink = true, float initialVal = 0.5f )
        : knobCode( code ), isQlink( qlink ), value( initialVal )
    {
        setOpaque( false );
    }

    void setKnobCode( int code, bool qlink ) { knobCode = code; isQlink = qlink; }
    void setValue( float val ) { value = juce::jlimit( 0.0f, 1.0f, val ); repaint(); }
    float getValue() const noexcept { return value; }
    void setArcColour( juce::Colour col ) { arcColour = col; repaint(); }

    void paint( juce::Graphics& g ) override
    {
        if( isDragging )
        {
            auto r = getLocalBounds().toFloat();
            float cx = r.getCentreX();
            float cy = r.getCentreY();
            float radius = ( juce::jmin( r.getWidth(), r.getHeight() ) * 0.5f ) - 3.0f;

            juce::Path p;
            float startA = juce::MathConstants<float>::pi * 0.75f;
            float endA   = startA + juce::MathConstants<float>::pi * 1.5f * value;
            p.addCentredArc( cx, cy, radius, radius, 0.0f, startA, endA, true );

            g.setColour( arcColour.withAlpha( 0.4f ) );
            g.strokePath( p, juce::PathStrokeType( 5.0f ) );

            g.setColour( arcColour );
            g.strokePath( p, juce::PathStrokeType( 2.5f ) );

            // Indicator dot at current value
            float dotX = cx + std::cos( endA ) * radius;
            float dotY = cy + std::sin( endA ) * radius;
            g.setColour( juce::Colours::white );
            g.fillEllipse( dotX - 3.0f, dotY - 3.0f, 6.0f, 6.0f );
        }
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        isDragging = true;
        dragStartY = e.position.y;
        dragStartVal = value;
        repaint();
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        if( isDragging )
        {
            float dy = dragStartY - e.position.y;
            float newVal = juce::jlimit( 0.0f, 1.0f, dragStartVal + dy / 150.0f );
            if( std::abs( newVal - value ) > 0.005f )
            {
                int dir = ( newVal > value ) ? (int)DCC::EN_KNOB_CLOCKWISE : (int)DCC::EN_KNOB_ANTICLOCKWISE;
                value = newVal;
                sendDclKnob( knobCode, dir, 1, isQlink );
                repaint();
            }
        }
    }

    void mouseUp( const juce::MouseEvent& ) override
    {
        isDragging = false;
        repaint();
    }

    void mouseWheelMove( const juce::MouseEvent&, const juce::MouseWheelDetails& wheel ) override
    {
        if( std::abs( wheel.deltaY ) > 0.02f )
        {
            int dir = ( wheel.deltaY > 0.0f ) ? (int)DCC::EN_KNOB_CLOCKWISE : (int)DCC::EN_KNOB_ANTICLOCKWISE;
            value = juce::jlimit( 0.0f, 1.0f, value + ( wheel.deltaY > 0 ? 0.03f : -0.03f ) );
            sendDclKnob( knobCode, dir, 1, isQlink );
            repaint();
        }
    }

private:
    int          knobCode{ DCC::EN_KNOB_1 };
    bool         isQlink{ true };
    float        value{ 0.5f };
    bool         isDragging{ false };
    float        dragStartY{ 0.0f };
    float        dragStartVal{ 0.5f };
    juce::Colour arcColour{ 0xff00d4ff };
};

//==============================================================================
/**
 * 3. HardwareJogWheel: MPC / Kick Central Jog Wheel Dial Component.
 * Supports rotary rim touch tracking, mouse wheel, and central ENTER button.
 */
class HardwareJogWheel : public juce::Component
{
public:
    HardwareJogWheel()
    {
        setOpaque( false );
    }

    void paint( juce::Graphics& g ) override
    {
        auto r = getLocalBounds().toFloat();
        float cx = r.getCentreX();
        float cy = r.getCentreY();
        float radius = juce::jmin( r.getWidth(), r.getHeight() ) * 0.5f;

        // Rotating red LED indicator dot on outer rim
        float dotR = radius * 0.72f;
        float dotX = cx + std::cos( angle ) * dotR;
        float dotY = cy + std::sin( angle ) * dotR;

        g.setColour( juce::Colour( 0xffff3b30 ) );
        g.fillEllipse( dotX - 4.0f, dotY - 4.0f, 8.0f, 8.0f );

        // Center ENTER button feedback
        if( isEnterDown )
        {
            float enterR = radius * 0.38f;
            g.setColour( juce::Colour( 0x55ffffff ) );
            g.fillEllipse( cx - enterR, cy - enterR, enterR * 2.0f, enterR * 2.0f );
        }
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        auto r = getLocalBounds().toFloat();
        float cx = r.getCentreX();
        float cy = r.getCentreY();
        float dist = std::hypot( e.position.x - cx, e.position.y - cy );
        float radius = juce::jmin( r.getWidth(), r.getHeight() ) * 0.5f;

        if( dist <= radius * 0.38f )
        {
            isEnterDown = true;
            sendDclButton( DCC::EN_KNOB_CENTER, true );
            repaint();
        }
        else
        {
            isRotating = true;
            lastAngle = std::atan2( e.position.y - cy, e.position.x - cx );
        }
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        if( isRotating )
        {
            auto r = getLocalBounds().toFloat();
            float cx = r.getCentreX();
            float cy = r.getCentreY();

            float curAngle = std::atan2( e.position.y - cy, e.position.x - cx );
            float delta = curAngle - lastAngle;
            if( delta > juce::MathConstants<float>::pi )  delta -= juce::MathConstants<float>::twoPi;
            if( delta < -juce::MathConstants<float>::pi ) delta += juce::MathConstants<float>::twoPi;

            angle += delta;
            accum += delta;
            lastAngle = curAngle;

            const float step = juce::MathConstants<float>::twoPi / 24.0f;
            if( std::abs( accum ) >= step )
            {
                int ticks = (int)( accum / step );
                accum -= ticks * step;
                int dir = ( ticks > 0 ) ? (int)DCC::EN_KNOB_CLOCKWISE : (int)DCC::EN_KNOB_ANTICLOCKWISE;
                sendDclKnob( DCC::EN_KNOB_CENTER, dir, std::max( 1, std::abs( ticks ) ), false );
            }
            repaint();
        }
    }

    void mouseUp( const juce::MouseEvent& ) override
    {
        if( isEnterDown )
        {
            isEnterDown = false;
            sendDclButton( DCC::EN_KNOB_CENTER, false );
            repaint();
        }
        isRotating = false;
    }

    void mouseWheelMove( const juce::MouseEvent&, const juce::MouseWheelDetails& wheel ) override
    {
        if( std::abs( wheel.deltaY ) > 0.05f )
        {
            int dir = ( wheel.deltaY > 0.0f ) ? (int)DCC::EN_KNOB_CLOCKWISE : (int)DCC::EN_KNOB_ANTICLOCKWISE;
            angle += ( wheel.deltaY > 0.0f ? 0.25f : -0.25f );
            sendDclKnob( DCC::EN_KNOB_CENTER, dir, 1, false );
            repaint();
        }
    }

private:
    float angle{ 0.0f };
    float lastAngle{ 0.0f };
    float accum{ 0.0f };
    bool  isRotating{ false };
    bool  isEnterDown{ false };
};

//==============================================================================
/**
 * 4. HardwareButton: Physical Backlit Button Component.
 * Supports circular transport buttons and rectangular mode/bank buttons.
 */
class HardwareButton : public juce::Component
{
public:
    HardwareButton( int code = 0, bool circular = false, juce::Colour col = juce::Colour( 0xffff9900 ) )
        : buttonCode( code ), isCircular( circular ), ledColour( col )
    {
        setOpaque( false );
    }

    void setButtonCode( int code ) { buttonCode = code; }
    int  getButtonCode() const noexcept { return buttonCode; }

    void setLit( bool lit ) { isLit = lit; repaint(); }
    bool getLit() const noexcept { return isLit; }

    void setLedColour( juce::Colour col ) { ledColour = col; repaint(); }

    void paint( juce::Graphics& g ) override
    {
        if( isDown || isLit )
        {
            auto r = getLocalBounds().toFloat().reduced( 1.5f );
            if( isCircular )
            {
                g.setColour( ledColour.withAlpha( isDown ? 0.45f : 0.25f ) );
                g.fillEllipse( r );
                g.setColour( ledColour );
                g.drawEllipse( r, 2.0f );
            }
            else
            {
                g.setColour( ledColour.withAlpha( isDown ? 0.40f : 0.20f ) );
                g.fillRoundedRectangle( r, 3.0f );
                g.setColour( ledColour.withAlpha( isDown ? 1.0f : 0.85f ) );
                g.drawRoundedRectangle( r, 3.0f, 1.0f );
            }
        }
    }

    void mouseDown( const juce::MouseEvent& ) override
    {
        isDown = true;
        sendDclButton( buttonCode, true );
        repaint();
    }

    void mouseUp( const juce::MouseEvent& ) override
    {
        isDown = false;
        sendDclButton( buttonCode, false );
        repaint();
    }

private:
    int          buttonCode{ 0 };
    bool         isCircular{ false };
    bool         isDown{ false };
    bool         isLit{ false };
    juce::Colour ledColour{ 0xffff9900 };
};

//==============================================================================
/**
 * 5. HardwareRibbon: Touch Ribbon Strip Component with 20-segment LED ladder.
 */
class HardwareRibbon : public juce::Component
{
public:
    HardwareRibbon()
    {
        setOpaque( false );
    }

    void paint( juce::Graphics& g ) override
    {
        if( isTouching )
        {
            auto r = getLocalBounds().toFloat();
            float fillH = r.getHeight() * touchValue;
            float topY  = r.getBottom() - fillH;

            // Draw dynamic LED ladder
            g.setColour( juce::Colour( 0x44ff2d55 ) );
            g.fillRect( r.getX(), topY, r.getWidth(), fillH );

            g.setColour( juce::Colour( 0xffff2d55 ) );
            g.fillRect( r.getX(), topY - 2.0f, r.getWidth(), 4.0f );
        }
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        isTouching = true;
        updateValue( e.position.y );
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        if( isTouching )
            updateValue( e.position.y );
    }

    void mouseUp( const juce::MouseEvent& ) override
    {
        isTouching = false;
        repaint();
    }

private:
    void updateValue( float y )
    {
        touchValue = juce::jlimit( 0.0f, 1.0f, 1.0f - ( y / (float)getHeight() ) );
        sendDclSlider( touchValue );
        repaint();
    }

    bool  isTouching{ false };
    float touchValue{ 0.0f };
};

} // namespace kick_hw_ui
