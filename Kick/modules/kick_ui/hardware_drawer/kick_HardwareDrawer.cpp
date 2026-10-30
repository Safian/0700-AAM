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

#include "kick_HardwareDrawer.h"
#include "kick_ui_controls/source/kick_Theme.h"
#include <cmath>

namespace kick_ui
{

static const int kHandleHeight = 34;
static const int kExpandedHeight = 270;

//==============================================================================
// Drawer Handle: persistent slim bar at top of drawer
//==============================================================================
class MobileHardwareDrawer::DrawerHandle : public juce::Component
{
public:
    explicit DrawerHandle( MobileHardwareDrawer& owner ) : owner_( owner )
    {
        setInterceptsMouseClicks( true, false );
    }

    void paint( juce::Graphics& g ) override
    {
        auto bounds = getLocalBounds().toFloat();

        // Background
        g.setColour( juce::Colour( 0xEE161A26 ) );
        g.fillRect( bounds );

        // Top accent line
        g.setColour( ui::Theme::titleColour( 1 ).withAlpha( 0.85f ) );
        g.fillRect( bounds.removeFromTop( 2.0f ) );

        // Center toggle pill
        auto pillArea = getLocalBounds().withSizeKeepingCentre( 180, 24 ).toFloat();
        g.setColour( juce::Colour( 0xFF2A3245 ) );
        g.fillRoundedRectangle( pillArea, 12.0f );

        g.setColour( ui::Theme::titleColour( 1 ) );
        g.drawRoundedRectangle( pillArea, 12.0f, 1.0f );

        g.setColour( juce::Colours::white );
        g.setFont( juce::Font( 12.0f, juce::Font::bold ) );
        const juce::String title = owner_.isExpanded() ? "PADS & CONTROLS  v"
                                                      : "PADS & CONTROLS  ^";
        g.drawText( title, pillArea, juce::Justification::centred, false );

        // Status indicators on left
        auto leftArea = getLocalBounds().removeFromLeft( 220 ).reduced( 8, 4 );
        g.setFont( juce::Font( 11.0f, juce::Font::plain ) );
        g.setColour( juce::Colour( 0xFF90A4AE ) );
        const char bankChar = (char)( 'A' + owner_.getBank() );
        juce::String status = "BANK " + juce::String::charToString( bankChar );
        if( owner_.isFullLevel() ) status += "  |  FULL LVL";
        g.drawText( status, leftArea, juce::Justification::centredLeft, false );
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        juce::ignoreUnused( e );
        owner_.toggleExpanded();
    }

private:
    MobileHardwareDrawer& owner_;
};

//==============================================================================
// TouchStrip: ribbon controller
//==============================================================================
class MobileHardwareDrawer::TouchStrip : public juce::Component
{
public:
    explicit TouchStrip( MobileHardwareDrawer& owner ) : owner_( owner ) {}

    void paint( juce::Graphics& g ) override
    {
        auto b = getLocalBounds().toFloat().reduced( 2.0f );
        g.setColour( juce::Colour( 0xFF181C28 ) );
        g.fillRoundedRectangle( b, 4.0f );

        g.setColour( juce::Colour( 0xFF303B52 ) );
        g.drawRoundedRectangle( b, 4.0f, 1.0f );

        if( touching_ )
        {
            auto fillH = b.getHeight() * ( (float)value_ / 100.0f );
            auto fillRect = b.removeFromBottom( fillH );
            g.setColour( ui::Theme::titleColour( 1 ).withAlpha( 0.7f ) );
            g.fillRoundedRectangle( fillRect, 4.0f );
        }

        g.setColour( juce::Colour( 0xFF78909C ) );
        g.setFont( juce::Font( 9.0f, juce::Font::bold ) );
        g.drawText( "RIBBON", getLocalBounds(), juce::Justification::centredBottom, false );
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        updateValue( e.y );
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        updateValue( e.y );
    }

    void mouseUp( const juce::MouseEvent& ) override
    {
        touching_ = false;
        repaint();
    }

private:
    void updateValue( int y )
    {
        touching_ = true;
        auto h = getHeight();
        if( h <= 0 ) return;
        float norm = 1.0f - juce::jlimit( 0.0f, 1.0f, (float)y / (float)h );
        value_ = juce::roundToInt( norm * 100.0f );
        owner_.sendSliderMessage( value_ );
        repaint();
    }

    MobileHardwareDrawer& owner_;
    int value_{ 50 };
    bool touching_{ false };
};

//==============================================================================
// PadMatrix: 16 MPC pads
//==============================================================================
class MobileHardwareDrawer::PadMatrix : public juce::Component
{
public:
    explicit PadMatrix( MobileHardwareDrawer& owner ) : owner_( owner )
    {
        setInterceptsMouseClicks( true, true );
        for( int i = 0; i < 16; ++i )
        {
            padActive_[i] = false;
            padColours_[i] = juce::Colour( 0xFF00E5FF );
        }
    }

    void paint( juce::Graphics& g ) override
    {
        auto bounds = getLocalBounds().toFloat();
        float padW = ( bounds.getWidth() - 15.0f ) / 4.0f;
        float padH = ( bounds.getHeight() - 15.0f ) / 4.0f;

        for( int row = 0; row < 4; ++row )
        {
            for( int col = 0; col < 4; ++col )
            {
                // MPC layout: row 0 is top (pads 13..16), row 3 is bottom (pads 1..4)
                int padIndex = ( 3 - row ) * 4 + col;
                auto padRect = juce::Rectangle<float>(
                    bounds.getX() + col * ( padW + 5.0f ),
                    bounds.getY() + row * ( padH + 5.0f ),
                    padW, padH
                );

                bool active = padActive_[padIndex];
                juce::Colour baseColour = active ? padColours_[padIndex].brighter( 0.5f ) : juce::Colour( 0xFF202534 );
                juce::Colour borderColour = active ? juce::Colours::white : padColours_[padIndex].withAlpha( 0.85f );

                // Pad body
                g.setColour( baseColour );
                g.fillRoundedRectangle( padRect, 6.0f );

                // LED border
                g.setColour( borderColour );
                g.drawRoundedRectangle( padRect, 6.0f, active ? 3.0f : 1.5f );

                // Pad text
                g.setColour( active ? juce::Colours::white : juce::Colour( 0xFFECEFF1 ) );
                g.setFont( juce::Font( 13.0f, juce::Font::bold ) );
                g.drawText( juce::String( padIndex + 1 ), padRect.reduced( 4.0f ), juce::Justification::topLeft, false );
            }
        }
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        int pad = getPadAt( e.x, e.y );
        if( pad >= 0 && pad < 16 )
        {
            padActive_[pad] = true;
            int vel = owner_.isFullLevel() ? 127 : computeVelocity( e.y, pad );
            owner_.sendPadMessage( pad, true, vel );
            repaint();
        }
    }

    void mouseUp( const juce::MouseEvent& e ) override
    {
        int pad = getPadAt( e.x, e.y );
        if( pad >= 0 && pad < 16 )
        {
            padActive_[pad] = false;
            owner_.sendPadMessage( pad, false, 0 );
            repaint();
        }
        else
        {
            // Release any stuck active pads
            for( int i = 0; i < 16; ++i )
            {
                if( padActive_[i] )
                {
                    padActive_[i] = false;
                    owner_.sendPadMessage( i, false, 0 );
                }
            }
            repaint();
        }
    }

private:
    int getPadAt( int x, int y ) const
    {
        auto bounds = getLocalBounds().toFloat();
        float padW = ( bounds.getWidth() - 15.0f ) / 4.0f;
        float padH = ( bounds.getHeight() - 15.0f ) / 4.0f;
        if( padW <= 0.0f || padH <= 0.0f ) return -1;

        int col = juce::roundToInt( std::floor( ( (float)x - bounds.getX() ) / ( padW + 5.0f ) ) );
        int row = juce::roundToInt( std::floor( ( (float)y - bounds.getY() ) / ( padH + 5.0f ) ) );

        if( col >= 0 && col < 4 && row >= 0 && row < 4 )
        {
            return ( 3 - row ) * 4 + col;
        }
        return -1;
    }

    int computeVelocity( int y, int padIndex ) const
    {
        juce::ignoreUnused( y, padIndex );
        return 110;
    }

    MobileHardwareDrawer& owner_;
    bool padActive_[16]{};
    juce::Colour padColours_[16];
};

//==============================================================================
// BankSelector: Bank A..H and Level buttons
//==============================================================================
class MobileHardwareDrawer::BankSelector : public juce::Component
{
public:
    explicit BankSelector( MobileHardwareDrawer& owner ) : owner_( owner ) {}

    void paint( juce::Graphics& g ) override
    {
        auto bounds = getLocalBounds().toFloat();
        float btnW = bounds.getWidth();
        float btnH = ( bounds.getHeight() - 7.0f * 3.0f ) / 8.0f;

        for( int i = 0; i < 8; ++i )
        {
            auto r = juce::Rectangle<float>( bounds.getX(), bounds.getY() + i * ( btnH + 3.0f ), btnW, btnH );
            bool isCurrent = ( i == owner_.getBank() );

            g.setColour( isCurrent ? ui::Theme::titleColour( 1 ) : juce::Colour( 0xFF202636 ) );
            g.fillRoundedRectangle( r, 4.0f );

            g.setColour( isCurrent ? juce::Colours::black : juce::Colours::white );
            g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
            const char c = (char)( 'A' + i );
            g.drawText( juce::String::charToString( c ), r, juce::Justification::centred, false );
        }
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        auto bounds = getLocalBounds().toFloat();
        float btnH = ( bounds.getHeight() - 7.0f * 3.0f ) / 8.0f;
        if( btnH <= 0.0f ) return;
        int idx = juce::roundToInt( std::floor( ( (float)e.y - bounds.getY() ) / ( btnH + 3.0f ) ) );
        if( idx >= 0 && idx < 8 )
        {
            owner_.setBank( idx );
            repaint();
        }
    }

private:
    MobileHardwareDrawer& owner_;
};

//==============================================================================
// QLinksPanel: 4 Encoders, Jog Wheel, Enter/Back, Transport
//==============================================================================
class MobileHardwareDrawer::QLinksPanel : public juce::Component
{
public:
    explicit QLinksPanel( MobileHardwareDrawer& owner ) : owner_( owner )
    {
        for( int i = 0; i < 4; ++i ) qlinkValues_[i] = 64;
    }

    void paint( juce::Graphics& g ) override
    {
        auto bounds = getLocalBounds().toFloat().reduced( 4.0f );

        // Left half: 4 Q-Links in a 2x2 grid
        auto leftArea = bounds.removeFromLeft( bounds.getWidth() * 0.48f );
        float kw = ( leftArea.getWidth() - 6.0f ) / 2.0f;
        float kh = ( leftArea.getHeight() - 6.0f ) / 2.0f;

        for( int i = 0; i < 4; ++i )
        {
            int col = i % 2;
            int row = i / 2;
            auto kr = juce::Rectangle<float>( leftArea.getX() + col * ( kw + 6.0f ),
                                              leftArea.getY() + row * ( kh + 6.0f ),
                                              kw, kh );

            drawEncoder( g, kr, "Q" + juce::String( i + 1 ), qlinkValues_[i], activeKnob_ == i );
        }

        // Right half: Jog Wheel + Enter/Back + Transport
        auto rightArea = bounds;
        auto jogArea = rightArea.removeFromTop( rightArea.getHeight() * 0.65f );

        // Jog Wheel (circle in center)
        auto wheelRect = jogArea.withSizeKeepingCentre( jogArea.getHeight() * 0.9f, jogArea.getHeight() * 0.9f );
        g.setColour( juce::Colour( 0xFF1B202D ) );
        g.fillEllipse( wheelRect );

        g.setColour( activeKnob_ == 4 ? ui::Theme::titleColour( 1 ) : juce::Colour( 0xFF3D4961 ) );
        g.drawEllipse( wheelRect, 3.0f );

        // Center ENTER button
        auto enterRect = wheelRect.withSizeKeepingCentre( wheelRect.getWidth() * 0.45f, wheelRect.getHeight() * 0.45f );
        g.setColour( juce::Colour( 0xFF2A3245 ) );
        g.fillEllipse( enterRect );
        g.setColour( juce::Colours::white );
        g.setFont( juce::Font( 10.0f, juce::Font::bold ) );
        g.drawText( "ENTER", enterRect, juce::Justification::centred, false );

        // BACK, +, - buttons on the side of jog
        auto sideBtns = rightArea;
        drawButton( g, sideBtns.removeFromLeft( sideBtns.getWidth() * 0.33f ), "BACK" );
        drawButton( g, sideBtns.removeFromLeft( sideBtns.getWidth() * 0.5f ), "-" );
        drawButton( g, sideBtns, "+" );
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        lastY_ = e.y;
        lastAngle_ = getAngleFromCentre( e.x, e.y );
        activeKnob_ = hitTestKnob( e.x, e.y );

        if( activeKnob_ == 5 ) // ENTER clicked
        {
            owner_.sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_KNOB_ENTER, true );
        }
        else if( activeKnob_ == 6 ) // BACK
        {
            owner_.sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_BACK, true );
        }
        else if( activeKnob_ == 7 ) // -
        {
            owner_.sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_MINUS, true );
        }
        else if( activeKnob_ == 8 ) // +
        {
            owner_.sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_PLUS, true );
        }
        repaint();
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        if( activeKnob_ >= 0 && activeKnob_ < 4 )
        {
            int dy = lastY_ - e.y;
            if( dy != 0 )
            {
                int dir = ( dy > 0 ) ? contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE
                                     : contracts::DeviceControlContracts::EN_KNOB_ANTICLOCKWISE;
                int ticks = std::abs( dy );
                qlinkValues_[activeKnob_] = juce::jlimit( 0, 127, qlinkValues_[activeKnob_] + ( dy > 0 ? 1 : -1 ) );
                owner_.sendKnobMessage( activeKnob_ + 1, dir, ticks, true );
                lastY_ = e.y;
                repaint();
            }
        }
        else if( activeKnob_ == 4 ) // Jog wheel
        {
            float angle = getAngleFromCentre( e.x, e.y );
            float diff = angle - lastAngle_;
            if( diff > juce::MathConstants<float>::pi ) diff -= juce::MathConstants<float>::twoPi;
            else if( diff < -juce::MathConstants<float>::pi ) diff += juce::MathConstants<float>::twoPi;

            if( std::abs( diff ) > 0.08f )
            {
                int dir = ( diff > 0 ) ? contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE
                                       : contracts::DeviceControlContracts::EN_KNOB_ANTICLOCKWISE;
                owner_.sendKnobMessage( contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER, dir, 1, false );
                lastAngle_ = angle;
                repaint();
            }
        }
    }

    void mouseUp( const juce::MouseEvent& ) override
    {
        activeKnob_ = -1;
        repaint();
    }

private:
    void drawEncoder( juce::Graphics& g, juce::Rectangle<float> r, const juce::String& name, int val, bool active )
    {
        g.setColour( juce::Colour( 0xFF191D2A ) );
        g.fillRoundedRectangle( r, 6.0f );

        auto dialArea = r.reduced( 4.0f ).removeFromTop( r.getHeight() * 0.72f );
        float radius = std::min( dialArea.getWidth(), dialArea.getHeight() ) * 0.45f;
        auto centre = dialArea.getCentre();

        // Track arc
        float startAngle = juce::MathConstants<float>::pi * 0.75f;
        float endAngle = juce::MathConstants<float>::pi * 2.25f;
        float curAngle = startAngle + ( (float)val / 127.0f ) * ( endAngle - startAngle );

        juce::Path track;
        track.addCentredArc( centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true );
        g.setColour( juce::Colour( 0xFF2A3245 ) );
        g.strokePath( track, juce::PathStrokeType( 3.5f ) );

        juce::Path valPath;
        valPath.addCentredArc( centre.x, centre.y, radius, radius, 0.0f, startAngle, curAngle, true );
        g.setColour( active ? juce::Colours::white : ui::Theme::titleColour( 1 ) );
        g.strokePath( valPath, juce::PathStrokeType( 3.5f ) );

        // Label
        g.setColour( juce::Colours::white );
        g.setFont( juce::Font( 10.0f, juce::Font::bold ) );
        g.drawText( name, r.removeFromBottom( 14.0f ), juce::Justification::centred, false );
    }

    void drawButton( juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text )
    {
        auto box = r.reduced( 2.0f );
        g.setColour( juce::Colour( 0xFF2A3245 ) );
        g.fillRoundedRectangle( box, 4.0f );
        g.setColour( juce::Colours::white );
        g.setFont( juce::Font( 10.0f, juce::Font::bold ) );
        g.drawText( text, box, juce::Justification::centred, false );
    }

    int hitTestKnob( int x, int y ) const
    {
        auto bounds = getLocalBounds().toFloat().reduced( 4.0f );
        auto leftArea = bounds.removeFromLeft( bounds.getWidth() * 0.48f );
        float kw = ( leftArea.getWidth() - 6.0f ) / 2.0f;
        float kh = ( leftArea.getHeight() - 6.0f ) / 2.0f;

        for( int i = 0; i < 4; ++i )
        {
            int col = i % 2;
            int row = i / 2;
            auto kr = juce::Rectangle<float>( leftArea.getX() + col * ( kw + 6.0f ),
                                              leftArea.getY() + row * ( kh + 6.0f ),
                                              kw, kh );
            if( kr.contains( (float)x, (float)y ) ) return i;
        }

        auto rightArea = bounds;
        auto jogArea = rightArea.removeFromTop( rightArea.getHeight() * 0.65f );
        auto wheelRect = jogArea.withSizeKeepingCentre( jogArea.getHeight() * 0.9f, jogArea.getHeight() * 0.9f );
        auto enterRect = wheelRect.withSizeKeepingCentre( wheelRect.getWidth() * 0.45f, wheelRect.getHeight() * 0.45f );

        if( enterRect.contains( (float)x, (float)y ) ) return 5;
        if( wheelRect.contains( (float)x, (float)y ) ) return 4;

        auto sideBtns = rightArea;
        auto b1 = sideBtns.removeFromLeft( sideBtns.getWidth() * 0.33f );
        if( b1.contains( (float)x, (float)y ) ) return 6;
        auto b2 = sideBtns.removeFromLeft( sideBtns.getWidth() * 0.5f );
        if( b2.contains( (float)x, (float)y ) ) return 7;
        if( sideBtns.contains( (float)x, (float)y ) ) return 8;

        return -1;
    }

    float getAngleFromCentre( int x, int y ) const
    {
        auto bounds = getLocalBounds().toFloat().reduced( 4.0f );
        bounds.removeFromLeft( bounds.getWidth() * 0.48f );
        auto jogArea = bounds.removeFromTop( bounds.getHeight() * 0.65f );
        auto centre = jogArea.getCentre();
        return std::atan2( (float)y - centre.y, (float)x - centre.x );
    }

    MobileHardwareDrawer& owner_;
    int qlinkValues_[4]{};
    int activeKnob_{ -1 };
    int lastY_{ 0 };
    float lastAngle_{ 0.0f };
};

//==============================================================================
// TransportPanel: Play, Stop, Rec, Overdub, Tap
//==============================================================================
class MobileHardwareDrawer::TransportPanel : public juce::Component
{
public:
    explicit TransportPanel( MobileHardwareDrawer& owner ) : owner_( owner ) {}

    void paint( juce::Graphics& g ) override
    {
        auto bounds = getLocalBounds().toFloat().reduced( 2.0f );
        float btnW = ( bounds.getWidth() - 5.0f * 4.0f ) / 5.0f;

        drawBtn( g, juce::Rectangle<float>( bounds.getX() + 0 * ( btnW + 5.0f ), bounds.getY(), btnW, bounds.getHeight() ),
                 "REC", juce::Colour( 0xFFD32F2F ) );
        drawBtn( g, juce::Rectangle<float>( bounds.getX() + 1 * ( btnW + 5.0f ), bounds.getY(), btnW, bounds.getHeight() ),
                 "OVR", juce::Colour( 0xFFF57C00 ) );
        drawBtn( g, juce::Rectangle<float>( bounds.getX() + 2 * ( btnW + 5.0f ), bounds.getY(), btnW, bounds.getHeight() ),
                 "STOP", juce::Colour( 0xFF455A64 ) );
        drawBtn( g, juce::Rectangle<float>( bounds.getX() + 3 * ( btnW + 5.0f ), bounds.getY(), btnW, bounds.getHeight() ),
                 "PLAY", juce::Colour( 0xFF388E3C ) );
        drawBtn( g, juce::Rectangle<float>( bounds.getX() + 4 * ( btnW + 5.0f ), bounds.getY(), btnW, bounds.getHeight() ),
                 "TAP", juce::Colour( 0xFF1976D2 ) );
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        auto bounds = getLocalBounds().toFloat().reduced( 2.0f );
        float btnW = ( bounds.getWidth() - 5.0f * 4.0f ) / 5.0f;
        if( btnW <= 0.0f ) return;

        int idx = juce::roundToInt( std::floor( ( (float)e.x - bounds.getX() ) / ( btnW + 5.0f ) ) );
        switch( idx )
        {
            case 0: owner_.sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_RECORD, true ); break;
            case 1: owner_.sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_OVERDUB, true ); break;
            case 2: owner_.sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_STOP, true ); break;
            case 3: owner_.sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_PLAY, true ); break;
            case 4: owner_.sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_TAP_TEMPO, true ); break;
            default: break;
        }
    }

private:
    void drawBtn( juce::Graphics& g, juce::Rectangle<float> r, const juce::String& name, juce::Colour c )
    {
        g.setColour( c );
        g.fillRoundedRectangle( r, 4.0f );
        g.setColour( juce::Colours::white );
        g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
        g.drawText( name, r, juce::Justification::centred, false );
    }

    MobileHardwareDrawer& owner_;
};

//==============================================================================
// MobileHardwareDrawer implementation
//==============================================================================
MobileHardwareDrawer::MobileHardwareDrawer()
{
    setInterceptsMouseClicks( false, true );

    handle_ = std::make_unique<DrawerHandle>( *this );
    addAndMakeVisible( handle_.get() );

    panelContainer_ = std::make_unique<juce::Component>();
    panelContainer_->setInterceptsMouseClicks( false, true );
    addAndMakeVisible( panelContainer_.get() );

    touchStrip_ = std::make_unique<TouchStrip>( *this );
    padMatrix_ = std::make_unique<PadMatrix>( *this );
    bankSelector_ = std::make_unique<BankSelector>( *this );
    qlinksPanel_ = std::make_unique<QLinksPanel>( *this );
    transportPanel_ = std::make_unique<TransportPanel>( *this );

    panelContainer_->addAndMakeVisible( touchStrip_.get() );
    panelContainer_->addAndMakeVisible( padMatrix_.get() );
    panelContainer_->addAndMakeVisible( bankSelector_.get() );
    panelContainer_->addAndMakeVisible( qlinksPanel_.get() );
    panelContainer_->addAndMakeVisible( transportPanel_.get() );
}

MobileHardwareDrawer::~MobileHardwareDrawer()
{
    stopTimer();
}

void MobileHardwareDrawer::setExpanded( bool shouldExpand )
{
    expanded_ = shouldExpand;
    targetHeight_ = expanded_ ? (float)kExpandedHeight : (float)kHandleHeight;
    startTimerHz( 60 );
}

void MobileHardwareDrawer::toggleExpanded()
{
    setExpanded( !expanded_ );
}

void MobileHardwareDrawer::setMode( DrawerMode newMode )
{
    mode_ = newMode;
    resized();
}

void MobileHardwareDrawer::setBank( int bankIndex )
{
    currentBank_ = juce::jlimit( 0, 7, bankIndex );
    sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_BANK_A + currentBank_, true );
    if( handle_ ) handle_->repaint();
    if( bankSelector_ ) bankSelector_->repaint();
}

void MobileHardwareDrawer::setFullLevel( bool enabled )
{
    fullLevel_ = enabled;
    sendButtonMessage( contracts::DeviceControlContracts::BtnLedSequence::EN_FULL_LEVEL, fullLevel_ );
    if( handle_ ) handle_->repaint();
}

void MobileHardwareDrawer::timerCallback()
{
    float diff = targetHeight_ - currentHeight_;
    if( std::abs( diff ) < 1.0f )
    {
        currentHeight_ = targetHeight_;
        stopTimer();
    }
    else
    {
        currentHeight_ += diff * 0.35f;
    }
    resized();
}

void MobileHardwareDrawer::paint( juce::Graphics& g )
{
    if( currentHeight_ > (float)kHandleHeight )
    {
        auto bounds = getLocalBounds().toFloat();
        auto drawerBounds = bounds.removeFromBottom( currentHeight_ );

        // Semi-transparent frosted background
        g.setColour( juce::Colour( 0xFA0E121B ) );
        g.fillRect( drawerBounds );

        // Subtle top glow
        g.setColour( ui::Theme::titleColour( 1 ).withAlpha( 0.4f ) );
        g.drawHorizontalLine( juce::roundToInt( drawerBounds.getY() ), drawerBounds.getX(), drawerBounds.getRight() );
    }
}

void MobileHardwareDrawer::resized()
{
    auto totalArea = getLocalBounds();
    int h = juce::roundToInt( currentHeight_ );
    auto drawerArea = totalArea.removeFromBottom( h );

    handle_->setBounds( drawerArea.removeFromTop( kHandleHeight ) );
    panelContainer_->setBounds( drawerArea );

    if( drawerArea.getHeight() > 10 )
    {
        auto pArea = panelContainer_->getLocalBounds().reduced( 6 );
        auto transportArea = pArea.removeFromBottom( 28 );
        pArea.removeFromBottom( 6 );

        transportPanel_->setBounds( transportArea );

        // Layout: Left = TouchStrip (26px), BankSelector (32px), PadMatrix (remainder ~50%), QLinksPanel (remainder ~45%)
        int touchW = 26;
        int bankW = 32;

        touchStrip_->setBounds( pArea.removeFromLeft( touchW ) );
        pArea.removeFromLeft( 6 );

        bankSelector_->setBounds( pArea.removeFromLeft( bankW ) );
        pArea.removeFromLeft( 8 );

        int remainingW = pArea.getWidth();
        int padW = juce::roundToInt( (float)remainingW * 0.54f );

        padMatrix_->setBounds( pArea.removeFromLeft( padW ) );
        pArea.removeFromLeft( 8 );

        qlinksPanel_->setBounds( pArea );
    }
}

void MobileHardwareDrawer::sendPadMessage( int padIndex, bool pressed, int velocity )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_PAD;
    msg.pktCmd  = contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { padIndex, pressed ? (int)contracts::DeviceControlContracts::BTN_PRESSED
                                      : (int)contracts::DeviceControlContracts::BTN_RELEASED,
                    velocity, 0 };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

void MobileHardwareDrawer::sendButtonMessage( int buttonCode, bool pressed )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_BUTTON;
    msg.pktCmd  = contracts::DeviceControlContracts::BtnCmd::EN_BTN_STATUS;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { buttonCode, pressed ? (int)contracts::DeviceControlContracts::BTN_PRESSED
                                        : (int)contracts::DeviceControlContracts::BTN_RELEASED };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

void MobileHardwareDrawer::sendKnobMessage( int knobCode, int direction, int ticks, bool isQlink )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_KNOBS;
    msg.pktCmd  = isQlink ? (int)contracts::DeviceControlContracts::KnobCmd::EN_KNOB_QLINK
                          : (int)contracts::DeviceControlContracts::KnobCmd::EN_KNOB_ROTARY;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { knobCode, direction, ticks };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

void MobileHardwareDrawer::sendSliderMessage( int percent )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_SLIDER;
    msg.pktCmd  = contracts::DeviceControlContracts::SliderCmd::EN_SLIDER_TOUCH_INPUT;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { percent };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

} // namespace kick_ui
