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
#include "kick_ui/kick_ui.h"
#include "kick_ui_controls/source/kick_Theme.h"
#include <map>
#include <cmath>
#include <functional>

namespace kick_mobile
{

//==============================================================================
// DCL Message Helpers
//==============================================================================
inline void sendPadMessage( int padIndex, bool pressed, int velocity )
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

inline void sendButtonMessage( int buttonCode, bool pressed )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_BUTTON;
    msg.pktCmd  = contracts::DeviceControlContracts::BtnCmd::EN_BTN_STATUS;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { buttonCode, pressed ? (int)contracts::DeviceControlContracts::BTN_PRESSED
                                        : (int)contracts::DeviceControlContracts::BTN_RELEASED };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

inline void sendKnobMessage( int knobCode, int direction, int ticks, bool isQlink )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_KNOBS;
    msg.pktCmd  = isQlink ? (int)contracts::DeviceControlContracts::KnobCmd::EN_KNOB_QLINK
                          : (int)contracts::DeviceControlContracts::KnobCmd::EN_KNOB_ROTARY;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { knobCode, direction, ticks };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

inline void sendSliderMessage( int percent )
{
    CPktMgr::HWMsg_t msg;
    msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_SLIDER;
    msg.pktCmd  = contracts::DeviceControlContracts::SliderCmd::EN_SLIDER_TOUCH_INPUT;
    msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
    msg.data    = { percent };
    contracts::DclSignals::hardwareInputTriggered( msg );
}

//==============================================================================
// View Enumeration in Spatial T-Formation
//==============================================================================
enum class TView
{
    DAW = 0,    // Center (0, 0)
    Pads,       // Right (+1, 0)
    Banks,      // Left (-1, 0)
    Controls    // Top (0, -1)
};

// Forward declaration
class KickMobileTLayout;

//==============================================================================
// Dedicated Interactive Navigation Pill Component
//==============================================================================
class NavPillButton : public juce::Component
{
public:
    enum ArrowDir { Left, Right, Up, Down, None };

    NavPillButton( const juce::String& text, ArrowDir arrow, juce::Colour glowColour, std::function<void()> onClick )
        : text_( text ), arrow_( arrow ), glowColour_( glowColour ), onClick_( std::move( onClick ) )
    {
        setInterceptsMouseClicks( true, false );
    }

    void paint( juce::Graphics& g ) override
    {
        auto r = getLocalBounds().toFloat().reduced( 1.0f );
        g.setColour( isDown_ ? juce::Colour( 0xFF2A364F ) : juce::Colour( 0xFA141926 ) );
        g.fillRoundedRectangle( r, 8.0f );

        g.setColour( isDown_ ? juce::Colours::white : glowColour_ );
        g.drawRoundedRectangle( r, 8.0f, isDown_ ? 2.5f : 1.5f );

        // Draw vector arrow
        g.setColour( isDown_ ? juce::Colours::white : glowColour_ );
        float cx = r.getCentreX();
        float cy = r.getCentreY();
        juce::Path p;
        if( arrow_ == Left )
        {
            p.addTriangle( r.getX() + 12.0f, cy, r.getX() + 20.0f, cy - 6.0f, r.getX() + 20.0f, cy + 6.0f );
            g.fillPath( p );
        }
        else if( arrow_ == Right )
        {
            p.addTriangle( r.getRight() - 12.0f, cy, r.getRight() - 20.0f, cy - 6.0f, r.getRight() - 20.0f, cy + 6.0f );
            g.fillPath( p );
        }
        else if( arrow_ == Up )
        {
            p.addTriangle( cx, r.getY() + 6.0f, cx - 6.0f, r.getY() + 14.0f, cx + 6.0f, r.getY() + 14.0f );
            g.fillPath( p );
        }
        else if( arrow_ == Down )
        {
            p.addTriangle( cx, r.getBottom() - 6.0f, cx - 6.0f, r.getBottom() - 14.0f, cx + 6.0f, r.getBottom() - 14.0f );
            g.fillPath( p );
        }

        // Draw text
        g.setColour( juce::Colours::white );
        g.setFont( juce::Font( 12.0f, juce::Font::bold ) );
        auto textRect = r;
        if( arrow_ == Left ) textRect.removeFromLeft( 18.0f );
        else if( arrow_ == Right ) textRect.removeFromRight( 18.0f );
        else if( arrow_ == Up ) textRect.removeFromTop( 8.0f );
        else if( arrow_ == Down ) textRect.removeFromBottom( 8.0f );
        g.drawText( text_, textRect, juce::Justification::centred, false );
    }

    void mouseDown( const juce::MouseEvent& ) override
    {
        isDown_ = true;
        repaint();
    }

    void mouseUp( const juce::MouseEvent& ) override
    {
        isDown_ = false;
        repaint();
        if( onClick_ ) onClick_();
    }

private:
    juce::String text_;
    ArrowDir arrow_{ None };
    juce::Colour glowColour_;
    std::function<void()> onClick_;
    bool isDown_{ false };
};

//==============================================================================
// 1. MobilePadsView: Full Screen 16 MPC Velocity Pads + Bank Bar
//==============================================================================
class MobilePadsView : public juce::Component
{
public:
    explicit MobilePadsView( KickMobileTLayout& layout );

    void paint( juce::Graphics& g ) override;
    void resized() override;

    void mouseDown( const juce::MouseEvent& e ) override;
    void mouseDrag( const juce::MouseEvent& e ) override;
    void mouseUp( const juce::MouseEvent& e ) override;

    void setBank( int bankIndex );
    int getBank() const noexcept { return currentBank_; }

    void setFullLevel( bool enabled );
    bool isFullLevel() const noexcept { return fullLevel_; }

private:
    int getPadAt( int x, int y ) const;
    int getBankTabAt( int x, int y ) const;
    juce::Rectangle<float> getPadBounds( int padIndex ) const;

    KickMobileTLayout& layout_;
    int currentBank_{ 0 };
    bool fullLevel_{ false };

    // Multi-touch tracking: touch index -> pad index
    std::map<int, int> activeTouches_;
    bool padActive_[16]{};
    juce::Colour padColours_[16];
    std::unique_ptr<NavPillButton> toDawBtn_;
};

//==============================================================================
// 2. MobileBanksView: Banks A..H, Ribbon Touch Strip, Performance Buttons
//==============================================================================
class MobileBanksView : public juce::Component
{
public:
    explicit MobileBanksView( KickMobileTLayout& layout );

    void paint( juce::Graphics& g ) override;
    void resized() override;

    void mouseDown( const juce::MouseEvent& e ) override;
    void mouseDrag( const juce::MouseEvent& e ) override;
    void mouseUp( const juce::MouseEvent& e ) override;

    void setBank( int bankIndex ) { currentBank_ = juce::jlimit( 0, 7, bankIndex ); repaint(); }
    int getBank() const noexcept { return currentBank_; }

private:
    void updateTouchStrip( int y );
    int getBankButtonAt( int x, int y ) const;
    int getPerfButtonAt( int x, int y ) const;

    KickMobileTLayout& layout_;
    int currentBank_{ 0 };
    int ribbonValue_{ 50 };
    bool ribbonTouching_{ false };
    int activePerfButton_{ -1 };
    std::unique_ptr<NavPillButton> toDawBtn_;
};

//==============================================================================
// 3. MobileControlsView: Q-Links, Jog Wheel, Transport, Hardware Buttons
//==============================================================================
class MobileControlsView : public juce::Component
{
public:
    explicit MobileControlsView( KickMobileTLayout& layout );

    void paint( juce::Graphics& g ) override;
    void resized() override;

    void mouseDown( const juce::MouseEvent& e ) override;
    void mouseDrag( const juce::MouseEvent& e ) override;
    void mouseUp( const juce::MouseEvent& e ) override;

private:
    void drawEncoder( juce::Graphics& g, juce::Rectangle<float> r, const juce::String& name, int val, bool active );
    void drawButton( juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour bg = juce::Colour( 0xFF242C3D ) );
    int hitTestControl( int x, int y ) const;
    float getJogAngle( int x, int y ) const;

    KickMobileTLayout& layout_;
    int qlinkValues_[4]{ 64, 64, 64, 64 };
    int activeControl_{ -1 };
    int dragStartY_{ 0 };
    float lastJogAngle_{ 0.0f };
    juce::Rectangle<float> jogBounds_;
    std::unique_ptr<NavPillButton> toDawBtn_;
};

//==============================================================================
// KickMobileTLayout: Spatial T-Layout Container & Navigator
//==============================================================================
class KickMobileTLayout : public juce::Component, private juce::Timer
{
public:
    KickMobileTLayout();
    ~KickMobileTLayout() override;

    void paint( juce::Graphics& g ) override;
    void resized() override;

    void mouseDown( const juce::MouseEvent& e ) override;
    void mouseDrag( const juce::MouseEvent& e ) override;
    void mouseUp( const juce::MouseEvent& e ) override;

    void navigateTo( TView view );
    TView getCurrentView() const noexcept { return targetView_; }

    void setBank( int bankIndex );
    int getBank() const noexcept { return currentBank_; }

    void setFullLevel( bool enabled );
    bool isFullLevel() const noexcept { return fullLevel_; }

private:
    void timerCallback() override;
    void updateViewPositions();

    kick_ui::KickHome* kickHome_{ nullptr };
    std::unique_ptr<MobilePadsView>     padsView_;
    std::unique_ptr<MobileBanksView>    banksView_;
    std::unique_ptr<MobileControlsView> controlsView_;

    // Edge navigation pill buttons
    std::unique_ptr<NavPillButton> toBanksPill_;
    std::unique_ptr<NavPillButton> toPadsPill_;
    std::unique_ptr<NavPillButton> toControlsPill_;

    TView currentView_{ TView::DAW };
    TView targetView_{ TView::DAW };

    float currentX_{ 0.0f };
    float currentY_{ 0.0f };
    float targetX_{ 0.0f };
    float targetY_{ 0.0f };

    int currentBank_{ 0 };
    bool fullLevel_{ false };

    // Gesture tracking
    int dragStartX_{ 0 };
    int dragStartY_{ 0 };
    bool isSwiping_{ false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickMobileTLayout )
};

//==============================================================================
// Implementations: MobilePadsView
//==============================================================================
inline MobilePadsView::MobilePadsView( KickMobileTLayout& layout ) : layout_( layout )
{
    setInterceptsMouseClicks( true, true );

    toDawBtn_ = std::make_unique<NavPillButton>( "DAW", NavPillButton::Left, juce::Colour( 0xFF00E5FF ), [this] {
        layout_.navigateTo( TView::DAW );
    } );
    addAndMakeVisible( toDawBtn_.get() );

    for( int i = 0; i < 16; ++i )
    {
        padActive_[i] = false;
        padColours_[i] = juce::Colour( 0xFF00E5FF ); // Electric Cyan
    }
}

inline void MobilePadsView::paint( juce::Graphics& g )
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll( juce::Colour( 0xFF10141E ) );

    // Top Bar
    auto topBar = bounds.removeFromTop( 38.0f );
    g.setColour( juce::Colour( 0xFF171D2B ) );
    g.fillRect( topBar );

    // Left area reserved for toDawBtn_ component
    topBar.removeFromLeft( 100.0f );

    // Full Level toggle on Right of Top Bar
    auto fullLvlBtn = topBar.removeFromRight( 90.0f ).reduced( 4.0f );
    g.setColour( fullLevel_ ? ui::Theme::titleColour( 1 ) : juce::Colour( 0xFF222938 ) );
    g.fillRoundedRectangle( fullLvlBtn, 6.0f );
    g.setColour( fullLevel_ ? juce::Colours::black : juce::Colours::white );
    g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
    g.drawText( "FULL LVL", fullLvlBtn, juce::Justification::centred, false );

    // Bank Tabs A..H in Center of Top Bar
    auto bankArea = topBar.reduced( 10.0f, 4.0f );
    float tabW = bankArea.getWidth() / 8.0f;
    for( int i = 0; i < 8; ++i )
    {
        auto tabRect = juce::Rectangle<float>( bankArea.getX() + i * tabW, bankArea.getY(), tabW - 3.0f, bankArea.getHeight() );
        bool isCurrent = ( i == currentBank_ );

        g.setColour( isCurrent ? ui::Theme::titleColour( 1 ) : juce::Colour( 0xFF1F2636 ) );
        g.fillRoundedRectangle( tabRect, 4.0f );

        g.setColour( isCurrent ? juce::Colours::black : juce::Colour( 0xFF90A4AE ) );
        g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
        const char c = (char)( 'A' + i );
        g.drawText( juce::String::charToString( c ), tabRect, juce::Justification::centred, false );
    }

    // 16 MPC Pads Grid
    for( int i = 0; i < 16; ++i )
    {
        auto r = getPadBounds( i );
        bool active = padActive_[i];

        // Pad body
        juce::Colour baseColour = active ? juce::Colour( 0xFF4A6572 ) : juce::Colour( 0xFF1E2433 );
        g.setColour( baseColour );
        g.fillRoundedRectangle( r, 8.0f );

        // Glowing border
        juce::Colour borderColour = active ? juce::Colours::white : padColours_[i].withAlpha( 0.85f );
        g.setColour( borderColour );
        g.drawRoundedRectangle( r, 8.0f, active ? 3.5f : 1.5f );

        // Corner indicator
        auto corner = juce::Rectangle<float>( r.getX() + 4, r.getY() + 4, 10, 10 );
        g.setColour( active ? juce::Colours::white : padColours_[i] );
        g.fillRoundedRectangle( corner, 2.0f );

        // Pad Number
        g.setColour( active ? juce::Colours::white : juce::Colour( 0xFFECEFF1 ) );
        g.setFont( juce::Font( 18.0f, juce::Font::bold ) );
        g.drawText( juce::String( i + 1 ), r.reduced( 10.0f ), juce::Justification::bottomRight, false );
    }
}

inline void MobilePadsView::resized()
{
    if( toDawBtn_ != nullptr )
        toDawBtn_->setBounds( 6, 4, 88, 30 );
}

inline juce::Rectangle<float> MobilePadsView::getPadBounds( int padIndex ) const
{
    if( padIndex < 0 || padIndex >= 16 ) return {};
    auto gridArea = getLocalBounds().toFloat();
    gridArea.removeFromTop( 42.0f );
    gridArea = gridArea.reduced( 10.0f, 6.0f );

    float gap = 6.0f;
    float padW = ( gridArea.getWidth() - 3.0f * gap ) / 4.0f;
    float padH = ( gridArea.getHeight() - 3.0f * gap ) / 4.0f;

    // MPC layout: pad 13..16 on top row, 1..4 on bottom row
    int row = 3 - ( padIndex / 4 );
    int col = padIndex % 4;

    return juce::Rectangle<float>(
        gridArea.getX() + col * ( padW + gap ),
        gridArea.getY() + row * ( padH + gap ),
        padW, padH
    );
}

inline int MobilePadsView::getPadAt( int x, int y ) const
{
    for( int i = 0; i < 16; ++i )
    {
        if( getPadBounds( i ).contains( (float)x, (float)y ) )
            return i;
    }
    return -1;
}

inline int MobilePadsView::getBankTabAt( int x, int y ) const
{
    if( y > 38 ) return -1;
    auto topBar = getLocalBounds().toFloat().removeFromTop( 38.0f );
    topBar.removeFromLeft( 100.0f );
    topBar.removeFromRight( 90.0f );
    auto bankArea = topBar.reduced( 10.0f, 4.0f );
    if( !bankArea.contains( (float)x, (float)y ) ) return -1;

    float tabW = bankArea.getWidth() / 8.0f;
    if( tabW <= 0.0f ) return -1;
    int idx = (int)std::floor( ( (float)x - bankArea.getX() ) / tabW );
    return juce::jlimit( 0, 7, idx );
}

inline void MobilePadsView::mouseDown( const juce::MouseEvent& e )
{
    // Back to DAW button
    if( e.y <= 38 && e.x <= 100 )
    {
        layout_.navigateTo( TView::DAW );
        return;
    }

    // Full Level toggle
    if( e.y <= 38 && e.x >= getWidth() - 90 )
    {
        setFullLevel( !fullLevel_ );
        return;
    }

    // Bank tabs
    int bank = getBankTabAt( e.x, e.y );
    if( bank >= 0 )
    {
        setBank( bank );
        return;
    }

    // Pad hit (multi-touch supported)
    int pad = getPadAt( e.x, e.y );
    if( pad >= 0 && pad < 16 )
    {
        int touchId = e.source.getIndex();
        activeTouches_[touchId] = pad;
        padActive_[pad] = true;

        int velocity = fullLevel_ ? 127 : 110;
        sendPadMessage( pad, true, velocity );
        repaint();
    }
}

inline void MobilePadsView::mouseDrag( const juce::MouseEvent& e )
{
    int touchId = e.source.getIndex();
    auto it = activeTouches_.find( touchId );
    if( it != activeTouches_.end() )
    {
        int currentPad = it->second;
        int newPad = getPadAt( e.x, e.y );
        if( newPad >= 0 && newPad != currentPad )
        {
            // Released old pad
            padActive_[currentPad] = false;
            sendPadMessage( currentPad, false, 0 );

            // Hit new pad
            it->second = newPad;
            padActive_[newPad] = true;
            int velocity = fullLevel_ ? 127 : 110;
            sendPadMessage( newPad, true, velocity );
            repaint();
        }
    }
}

inline void MobilePadsView::mouseUp( const juce::MouseEvent& e )
{
    int touchId = e.source.getIndex();
    auto it = activeTouches_.find( touchId );
    if( it != activeTouches_.end() )
    {
        int pad = it->second;
        padActive_[pad] = false;
        sendPadMessage( pad, false, 0 );
        activeTouches_.erase( it );
        repaint();
    }
    else
    {
        // Safety: release any lingering active pads if no touches active
        if( activeTouches_.empty() )
        {
            for( int i = 0; i < 16; ++i )
            {
                if( padActive_[i] )
                {
                    padActive_[i] = false;
                    sendPadMessage( i, false, 0 );
                }
            }
            repaint();
        }
    }
}

inline void MobilePadsView::setBank( int bankIndex )
{
    currentBank_ = juce::jlimit( 0, 7, bankIndex );
    sendButtonMessage( contracts::DeviceControlContracts::EN_BANK_A + currentBank_, true );
    layout_.setBank( currentBank_ );
    repaint();
}

inline void MobilePadsView::setFullLevel( bool enabled )
{
    fullLevel_ = enabled;
    sendButtonMessage( contracts::DeviceControlContracts::EN_FULL_LEVEL, fullLevel_ );
    layout_.setFullLevel( fullLevel_ );
    repaint();
}

//==============================================================================
// Implementations: MobileBanksView
//==============================================================================
inline MobileBanksView::MobileBanksView( KickMobileTLayout& layout ) : layout_( layout )
{
    setInterceptsMouseClicks( true, true );

    toDawBtn_ = std::make_unique<NavPillButton>( "DAW", NavPillButton::Right, ui::Theme::titleColour( 1 ), [this] {
        layout_.navigateTo( TView::DAW );
    } );
    addAndMakeVisible( toDawBtn_.get() );
}

inline void MobileBanksView::paint( juce::Graphics& g )
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll( juce::Colour( 0xFF10141E ) );

    // Top Bar
    auto topBar = bounds.removeFromTop( 38.0f );
    g.setColour( juce::Colour( 0xFF171D2B ) );
    g.fillRect( topBar );

    // Title
    g.setColour( juce::Colours::white );
    g.setFont( juce::Font( 13.0f, juce::Font::bold ) );
    g.drawText( "BANKS & EXPRESSION", topBar.reduced( 12.0f, 0.0f ), juce::Justification::centredLeft, false );

    // Area reserved on Right for toDawBtn_
    topBar.removeFromRight( 100.0f );

    auto mainArea = bounds.reduced( 10.0f, 8.0f );

    // Column 1: Ribbon Touch Strip
    auto stripArea = mainArea.removeFromLeft( 60.0f );
    mainArea.removeFromLeft( 12.0f );

    g.setColour( juce::Colour( 0xFF181C28 ) );
    g.fillRoundedRectangle( stripArea, 6.0f );
    g.setColour( juce::Colour( 0xFF303B52 ) );
    g.drawRoundedRectangle( stripArea, 6.0f, 1.5f );

    // LED Ladder inside Ribbon
    int numLeds = 20;
    float ledGap = 3.0f;
    float ledH = ( stripArea.getHeight() - 28.0f - ( numLeds - 1 ) * ledGap ) / numLeds;
    int litLeds = juce::roundToInt( ( (float)ribbonValue_ / 100.0f ) * numLeds );

    for( int i = 0; i < numLeds; ++i )
    {
        // LED 0 is bottom, numLeds-1 is top
        int idx = ( numLeds - 1 ) - i;
        auto ledRect = juce::Rectangle<float>(
            stripArea.getX() + 8.0f,
            stripArea.getY() + 8.0f + idx * ( ledH + ledGap ),
            stripArea.getWidth() - 16.0f,
            ledH
        );

        bool isLit = i < litLeds;
        juce::Colour ledColor;
        if( i > 16 ) ledColor = juce::Colour( 0xFFE53935 );     // Red top
        else if( i > 12 ) ledColor = juce::Colour( 0xFFFFA000 ); // Amber mid
        else ledColor = juce::Colour( 0xFF00E676 );              // Green base

        g.setColour( isLit ? ledColor : ledColor.withAlpha( 0.2f ) );
        g.fillRoundedRectangle( ledRect, 2.0f );
    }

    g.setColour( juce::Colour( 0xFF78909C ) );
    g.setFont( juce::Font( 10.0f, juce::Font::bold ) );
    g.drawText( "RIBBON", stripArea.removeFromBottom( 20.0f ), juce::Justification::centred, false );

    // Column 2: Banks A..H (2 columns x 4 rows)
    auto bankCol = mainArea.removeFromLeft( mainArea.getWidth() * 0.52f );
    mainArea.removeFromLeft( 12.0f );

    float bGap = 6.0f;
    float bw = ( bankCol.getWidth() - bGap ) / 2.0f;
    float bh = ( bankCol.getHeight() - 3.0f * bGap ) / 4.0f;

    for( int i = 0; i < 8; ++i )
    {
        int col = i % 2;
        int row = i / 2;
        auto br = juce::Rectangle<float>( bankCol.getX() + col * ( bw + bGap ), bankCol.getY() + row * ( bh + bGap ), bw, bh );
        bool isCurrent = ( i == currentBank_ );

        g.setColour( isCurrent ? ui::Theme::titleColour( 1 ) : juce::Colour( 0xFF222938 ) );
        g.fillRoundedRectangle( br, 6.0f );

        g.setColour( isCurrent ? juce::Colours::black : juce::Colours::white );
        g.setFont( juce::Font( 14.0f, juce::Font::bold ) );
        const char c = (char)( 'A' + i );
        g.drawText( "BANK " + juce::String::charToString( c ), br, juce::Justification::centred, false );
    }

    // Column 3: Performance buttons (2 columns x 4 rows)
    auto perfCol = mainArea;
    float pw = ( perfCol.getWidth() - bGap ) / 2.0f;
    float ph = ( perfCol.getHeight() - 3.0f * bGap ) / 4.0f;

    const char* perfLabels[8] = {
        "FULL LVL", "16 LVL",
        "HALF LVL", "NOTES",
        "TRK MUTE", "PAD MUTE",
        "STEP SEQ", "ERASE"
    };

    for( int i = 0; i < 8; ++i )
    {
        int col = i % 2;
        int row = i / 2;
        auto pr = juce::Rectangle<float>( perfCol.getX() + col * ( pw + bGap ), perfCol.getY() + row * ( ph + bGap ), pw, ph );
        bool active = ( activePerfButton_ == i );

        juce::Colour btnBg = active ? juce::Colour( 0xFF00E5FF ) : juce::Colour( 0xFF202738 );
        if( i == 7 && !active ) btnBg = juce::Colour( 0xFF4A1A1A ); // ERASE is reddish

        g.setColour( btnBg );
        g.fillRoundedRectangle( pr, 6.0f );

        g.setColour( active ? juce::Colours::black : juce::Colours::white );
        g.setFont( juce::Font( 12.0f, juce::Font::bold ) );
        g.drawText( perfLabels[i], pr, juce::Justification::centred, false );
    }
}

inline void MobileBanksView::resized()
{
    if( toDawBtn_ != nullptr )
        toDawBtn_->setBounds( getWidth() - 94, 4, 88, 30 );
}

inline void MobileBanksView::updateTouchStrip( int y )
{
    ribbonTouching_ = true;
    auto stripArea = getLocalBounds().toFloat().reduced( 10.0f, 8.0f );
    stripArea.removeFromTop( 38.0f );
    float h = stripArea.getHeight();
    if( h <= 0.0f ) return;

    float norm = 1.0f - juce::jlimit( 0.0f, 1.0f, ( (float)y - stripArea.getY() ) / h );
    ribbonValue_ = juce::roundToInt( norm * 100.0f );
    sendSliderMessage( ribbonValue_ );
    repaint();
}

inline int MobileBanksView::getBankButtonAt( int x, int y ) const
{
    auto mainArea = getLocalBounds().toFloat().reduced( 10.0f, 8.0f );
    mainArea.removeFromTop( 38.0f );
    mainArea.removeFromLeft( 72.0f ); // Skip strip + gap
    auto bankCol = mainArea.removeFromLeft( mainArea.getWidth() * 0.52f );

    if( !bankCol.contains( (float)x, (float)y ) ) return -1;

    float bGap = 6.0f;
    float bw = ( bankCol.getWidth() - bGap ) / 2.0f;
    float bh = ( bankCol.getHeight() - 3.0f * bGap ) / 4.0f;
    if( bw <= 0.0f || bh <= 0.0f ) return -1;

    int col = juce::roundToInt( std::floor( ( (float)x - bankCol.getX() ) / ( bw + bGap ) ) );
    int row = juce::roundToInt( std::floor( ( (float)y - bankCol.getY() ) / ( bh + bGap ) ) );

    if( col >= 0 && col < 2 && row >= 0 && row < 4 )
    {
        return row * 2 + col;
    }
    return -1;
}

inline int MobileBanksView::getPerfButtonAt( int x, int y ) const
{
    auto mainArea = getLocalBounds().toFloat().reduced( 10.0f, 8.0f );
    mainArea.removeFromTop( 38.0f );
    mainArea.removeFromLeft( 72.0f );
    mainArea.removeFromLeft( mainArea.getWidth() * 0.52f + 12.0f );
    auto perfCol = mainArea;

    if( !perfCol.contains( (float)x, (float)y ) ) return -1;

    float bGap = 6.0f;
    float pw = ( perfCol.getWidth() - bGap ) / 2.0f;
    float ph = ( perfCol.getHeight() - 3.0f * bGap ) / 4.0f;
    if( pw <= 0.0f || ph <= 0.0f ) return -1;

    int col = juce::roundToInt( std::floor( ( (float)x - perfCol.getX() ) / ( pw + bGap ) ) );
    int row = juce::roundToInt( std::floor( ( (float)y - perfCol.getY() ) / ( ph + bGap ) ) );

    if( col >= 0 && col < 2 && row >= 0 && row < 4 )
    {
        return row * 2 + col;
    }
    return -1;
}

inline void MobileBanksView::mouseDown( const juce::MouseEvent& e )
{
    // Return to DAW button
    if( e.y <= 38 && e.x >= getWidth() - 100 )
    {
        layout_.navigateTo( TView::DAW );
        return;
    }

    // Touch Strip
    if( e.x >= 10 && e.x <= 70 && e.y > 38 )
    {
        updateTouchStrip( e.y );
        return;
    }

    // Bank buttons
    int bank = getBankButtonAt( e.x, e.y );
    if( bank >= 0 )
    {
        setBank( bank );
        sendButtonMessage( contracts::DeviceControlContracts::EN_BANK_A + bank, true );
        layout_.setBank( bank );
        return;
    }

    // Performance buttons
    int perf = getPerfButtonAt( e.x, e.y );
    if( perf >= 0 )
    {
        activePerfButton_ = perf;
        using DCC = contracts::DeviceControlContracts;
        int code = -1;
        switch( perf )
        {
            case 0: code = DCC::EN_FULL_LEVEL; break;
            case 1: code = DCC::EN_16LEVELS; break;
            case 2: code = DCC::EN_HALF_LEVEL; break;
            case 3: code = DCC::EN_NOTES; break;
            case 4: code = DCC::EN_TRACK_MUTE; break;
            case 5: code = DCC::EN_PAD_MUTE; break;
            case 6: code = DCC::EN_STEP_SEQ; break;
            case 7: code = DCC::EN_ERASE; break;
            default: break;
        }
        if( code >= 0 ) sendButtonMessage( code, true );
        repaint();
    }
}

inline void MobileBanksView::mouseDrag( const juce::MouseEvent& e )
{
    if( ribbonTouching_ )
    {
        updateTouchStrip( e.y );
    }
}

inline void MobileBanksView::mouseUp( const juce::MouseEvent& )
{
    ribbonTouching_ = false;
    if( activePerfButton_ >= 0 )
    {
        activePerfButton_ = -1;
        repaint();
    }
}

//==============================================================================
// Implementations: MobileControlsView
//==============================================================================
inline MobileControlsView::MobileControlsView( KickMobileTLayout& layout ) : layout_( layout )
{
    setInterceptsMouseClicks( true, true );

    toDawBtn_ = std::make_unique<NavPillButton>( "DAW", NavPillButton::Down, juce::Colour( 0xFFFFAB00 ), [this] {
        layout_.navigateTo( TView::DAW );
    } );
    addAndMakeVisible( toDawBtn_.get() );
}

inline void MobileControlsView::paint( juce::Graphics& g )
{
    auto bounds = getLocalBounds().toFloat();
    g.fillAll( juce::Colour( 0xFF10141E ) );

    // Top Bar (Center area reserved for toDawBtn_)
    auto topBar = bounds.removeFromTop( 38.0f );
    g.setColour( juce::Colour( 0xFF171D2B ) );
    g.fillRect( topBar );

    auto mainArea = bounds.reduced( 10.0f, 6.0f );

    // Section 1 (Left): 4 Q-Link Encoders (2x2 grid)
    auto qlinkArea = mainArea.removeFromLeft( mainArea.getWidth() * 0.36f );
    mainArea.removeFromLeft( 12.0f );

    float qw = ( qlinkArea.getWidth() - 6.0f ) / 2.0f;
    float qh = ( qlinkArea.getHeight() - 6.0f ) / 2.0f;
    for( int i = 0; i < 4; ++i )
    {
        int col = i % 2;
        int row = i / 2;
        auto qr = juce::Rectangle<float>( qlinkArea.getX() + col * ( qw + 6.0f ), qlinkArea.getY() + row * ( qh + 6.0f ), qw, qh );
        drawEncoder( g, qr, "Q" + juce::String( i + 1 ), qlinkValues_[i], activeControl_ == i );
    }

    // Section 2 (Center): Mode buttons (2 columns x 4 rows)
    auto modeArea = mainArea.removeFromLeft( mainArea.getWidth() * 0.44f );
    mainArea.removeFromLeft( 12.0f );

    float mw = ( modeArea.getWidth() - 6.0f ) / 2.0f;
    float mh = ( modeArea.getHeight() - 3.0f * 6.0f ) / 4.0f;
    const char* modeLabels[8] = {
        "MAIN", "BROWSE",
        "TRACK MIX", "PAD MIX",
        "GRID", "SAVE",
        "MENU", "SHIFT"
    };

    for( int i = 0; i < 8; ++i )
    {
        int col = i % 2;
        int row = i / 2;
        auto mr = juce::Rectangle<float>( modeArea.getX() + col * ( mw + 6.0f ), modeArea.getY() + row * ( mh + 6.0f ), mw, mh );
        drawButton( g, mr, modeLabels[i], ( activeControl_ == 10 + i ) ? juce::Colour( 0xFF00E5FF ) : juce::Colour( 0xFF222938 ) );
    }

    // Section 3 (Right): Jog Wheel, ENTER/BACK, and Transport
    auto rightArea = mainArea;
    auto transportArea = rightArea.removeFromBottom( 36.0f );
    rightArea.removeFromBottom( 6.0f );

    // Transport Bar
    float tw = ( transportArea.getWidth() - 4.0f * 6.0f ) / 5.0f;
    const char* tNames[5] = { "REC", "OVR", "STOP", "PLAY", "TAP" };
    juce::Colour tColors[5] = {
        juce::Colour( 0xFFD32F2F ), // REC
        juce::Colour( 0xFFF57C00 ), // OVR
        juce::Colour( 0xFF455A64 ), // STOP
        juce::Colour( 0xFF388E3C ), // PLAY
        juce::Colour( 0xFF1976D2 )  // TAP
    };
    for( int i = 0; i < 5; ++i )
    {
        auto tr = juce::Rectangle<float>( transportArea.getX() + i * ( tw + 6.0f ), transportArea.getY(), tw, transportArea.getHeight() );
        drawButton( g, tr, tNames[i], tColors[i] );
    }

    // Jog Wheel & Side Buttons
    float jogSize = std::min( rightArea.getWidth() * 0.62f, rightArea.getHeight() );
    jogBounds_ = rightArea.removeFromLeft( jogSize ).withSizeKeepingCentre( jogSize, jogSize );

    g.setColour( juce::Colour( 0xFF1C2230 ) );
    g.fillEllipse( jogBounds_ );
    g.setColour( ( activeControl_ == 4 ) ? ui::Theme::titleColour( 1 ) : juce::Colour( 0xFF3E4A62 ) );
    g.drawEllipse( jogBounds_, 3.0f );

    // Center ENTER
    auto enterBounds = jogBounds_.withSizeKeepingCentre( jogSize * 0.45f, jogSize * 0.45f );
    g.setColour( ( activeControl_ == 5 ) ? ui::Theme::titleColour( 1 ) : juce::Colour( 0xFF2A344A ) );
    g.fillEllipse( enterBounds );
    g.setColour( ( activeControl_ == 5 ) ? juce::Colours::black : juce::Colours::white );
    g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
    g.drawText( "ENTER", enterBounds, juce::Justification::centred, false );

    // BACK, -, + on side of jog
    auto sideBtns = rightArea;
    float sbH = ( sideBtns.getHeight() - 2.0f * 6.0f ) / 3.0f;
    drawButton( g, juce::Rectangle<float>( sideBtns.getX(), sideBtns.getY() + 0 * ( sbH + 6.0f ), sideBtns.getWidth(), sbH ), "BACK" );
    drawButton( g, juce::Rectangle<float>( sideBtns.getX(), sideBtns.getY() + 1 * ( sbH + 6.0f ), sideBtns.getWidth(), sbH ), "-" );
    drawButton( g, juce::Rectangle<float>( sideBtns.getX(), sideBtns.getY() + 2 * ( sbH + 6.0f ), sideBtns.getWidth(), sbH ), "+" );
}

inline void MobileControlsView::resized()
{
    if( toDawBtn_ != nullptr )
        toDawBtn_->setBounds( ( getWidth() - 120 ) / 2, 4, 120, 30 );
}

inline void MobileControlsView::drawEncoder( juce::Graphics& g, juce::Rectangle<float> r, const juce::String& name, int val, bool active )
{
    g.setColour( juce::Colour( 0xFF1A202C ) );
    g.fillRoundedRectangle( r, 6.0f );

    auto dialArea = r.reduced( 4.0f ).removeFromTop( r.getHeight() * 0.72f );
    float radius = std::min( dialArea.getWidth(), dialArea.getHeight() ) * 0.42f;
    auto centre = dialArea.getCentre();

    float startAngle = juce::MathConstants<float>::pi * 0.75f;
    float endAngle = juce::MathConstants<float>::pi * 2.25f;
    float curAngle = startAngle + ( (float)val / 127.0f ) * ( endAngle - startAngle );

    juce::Path track;
    track.addCentredArc( centre.x, centre.y, radius, radius, 0.0f, startAngle, endAngle, true );
    g.setColour( juce::Colour( 0xFF2A344A ) );
    g.strokePath( track, juce::PathStrokeType( 3.5f ) );

    juce::Path valPath;
    valPath.addCentredArc( centre.x, centre.y, radius, radius, 0.0f, startAngle, curAngle, true );
    g.setColour( active ? juce::Colours::white : ui::Theme::titleColour( 1 ) );
    g.strokePath( valPath, juce::PathStrokeType( 3.5f ) );

    g.setColour( juce::Colours::white );
    g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
    g.drawText( name + " " + juce::String( val ), r.removeFromBottom( 16.0f ), juce::Justification::centred, false );
}

inline void MobileControlsView::drawButton( juce::Graphics& g, juce::Rectangle<float> r, const juce::String& text, juce::Colour bg )
{
    g.setColour( bg );
    g.fillRoundedRectangle( r, 5.0f );
    g.setColour( juce::Colours::white );
    g.setFont( juce::Font( 11.0f, juce::Font::bold ) );
    g.drawText( text, r, juce::Justification::centred, false );
}

inline float MobileControlsView::getJogAngle( int x, int y ) const
{
    auto centre = jogBounds_.getCentre();
    return std::atan2( (float)y - centre.y, (float)x - centre.x );
}

inline int MobileControlsView::hitTestControl( int x, int y ) const
{
    // Return DAW button at top
    if( y <= 38 )
    {
        auto dawBtn = getLocalBounds().toFloat().removeFromTop( 38.0f ).withSizeKeepingCentre( 130.0f, 28.0f );
        if( dawBtn.contains( (float)x, (float)y ) ) return 999;
    }

    auto mainArea = getLocalBounds().toFloat().reduced( 10.0f, 6.0f );
    mainArea.removeFromTop( 38.0f );

    // Q-Links
    auto qlinkArea = mainArea.removeFromLeft( mainArea.getWidth() * 0.36f );
    mainArea.removeFromLeft( 12.0f );
    if( qlinkArea.contains( (float)x, (float)y ) )
    {
        float qw = ( qlinkArea.getWidth() - 6.0f ) / 2.0f;
        float qh = ( qlinkArea.getHeight() - 6.0f ) / 2.0f;
        int col = juce::roundToInt( std::floor( ( (float)x - qlinkArea.getX() ) / ( qw + 6.0f ) ) );
        int row = juce::roundToInt( std::floor( ( (float)y - qlinkArea.getY() ) / ( qh + 6.0f ) ) );
        if( col >= 0 && col < 2 && row >= 0 && row < 2 ) return row * 2 + col;
    }

    // Mode buttons
    auto modeArea = mainArea.removeFromLeft( mainArea.getWidth() * 0.44f );
    mainArea.removeFromLeft( 12.0f );
    if( modeArea.contains( (float)x, (float)y ) )
    {
        float mw = ( modeArea.getWidth() - 6.0f ) / 2.0f;
        float mh = ( modeArea.getHeight() - 3.0f * 6.0f ) / 4.0f;
        int col = juce::roundToInt( std::floor( ( (float)x - modeArea.getX() ) / ( mw + 6.0f ) ) );
        int row = juce::roundToInt( std::floor( ( (float)y - modeArea.getY() ) / ( mh + 6.0f ) ) );
        if( col >= 0 && col < 2 && row >= 0 && row < 4 ) return 10 + row * 2 + col;
    }

    // Transport buttons
    auto rightArea = mainArea;
    auto transportArea = rightArea.removeFromBottom( 36.0f );
    if( transportArea.contains( (float)x, (float)y ) )
    {
        float tw = ( transportArea.getWidth() - 4.0f * 6.0f ) / 5.0f;
        int idx = juce::roundToInt( std::floor( ( (float)x - transportArea.getX() ) / ( tw + 6.0f ) ) );
        if( idx >= 0 && idx < 5 ) return 20 + idx;
    }

    // Center ENTER & Jog Wheel
    auto enterBounds = jogBounds_.withSizeKeepingCentre( jogBounds_.getWidth() * 0.45f, jogBounds_.getHeight() * 0.45f );
    if( enterBounds.contains( (float)x, (float)y ) ) return 5;
    if( jogBounds_.contains( (float)x, (float)y ) ) return 4;

    // Side buttons: BACK (6), - (7), + (8)
    auto sideBtns = rightArea;
    if( sideBtns.contains( (float)x, (float)y ) )
    {
        float sbH = ( sideBtns.getHeight() - 2.0f * 6.0f ) / 3.0f;
        int idx = juce::roundToInt( std::floor( ( (float)y - sideBtns.getY() ) / ( sbH + 6.0f ) ) );
        if( idx >= 0 && idx < 3 ) return 6 + idx;
    }

    return -1;
}

inline void MobileControlsView::mouseDown( const juce::MouseEvent& e )
{
    activeControl_ = hitTestControl( e.x, e.y );
    dragStartY_ = e.y;
    lastJogAngle_ = getJogAngle( e.x, e.y );

    if( activeControl_ == 999 ) // DAW Return
    {
        layout_.navigateTo( TView::DAW );
        return;
    }

    using DCC = contracts::DeviceControlContracts;
    if( activeControl_ == 5 ) // ENTER
    {
        sendButtonMessage( DCC::EN_KNOB_ENTER, true );
    }
    else if( activeControl_ == 6 ) // BACK
    {
        sendButtonMessage( DCC::EN_BACK, true );
    }
    else if( activeControl_ == 7 ) // -
    {
        sendButtonMessage( DCC::EN_MINUS, true );
    }
    else if( activeControl_ == 8 ) // +
    {
        sendButtonMessage( DCC::EN_PLUS, true );
    }
    else if( activeControl_ >= 10 && activeControl_ < 18 ) // Mode buttons
    {
        int code = -1;
        switch( activeControl_ - 10 )
        {
            case 0: code = DCC::EN_MAINMODE; break;
            case 1: code = DCC::EN_BROWSE; break;
            case 2: code = DCC::EN_TRACK_MIX; break;
            case 3: code = DCC::EN_PAD_MIX; break;
            case 4: code = DCC::EN_GRID; break;
            case 5: code = DCC::EN_SAVE; break;
            case 6: code = DCC::EN_MENU; break;
            case 7: code = DCC::EN_SHIFT; break;
            default: break;
        }
        if( code >= 0 ) sendButtonMessage( code, true );
    }
    else if( activeControl_ >= 20 && activeControl_ < 25 ) // Transport
    {
        int code = -1;
        switch( activeControl_ - 20 )
        {
            case 0: code = DCC::EN_RECORD; break;
            case 1: code = DCC::EN_OVERDUB; break;
            case 2: code = DCC::EN_STOP; break;
            case 3: code = DCC::EN_PLAY; break;
            case 4: code = DCC::EN_TAP_TEMPO; break;
            default: break;
        }
        if( code >= 0 ) sendButtonMessage( code, true );
    }
    repaint();
}

inline void MobileControlsView::mouseDrag( const juce::MouseEvent& e )
{
    if( activeControl_ >= 0 && activeControl_ < 4 )
    {
        int dy = dragStartY_ - e.y;
        if( dy != 0 )
        {
            int dir = ( dy > 0 ) ? (int)contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE
                                 : (int)contracts::DeviceControlContracts::EN_KNOB_ANTICLOCKWISE;
            int ticks = std::abs( dy );
            qlinkValues_[activeControl_] = juce::jlimit( 0, 127, qlinkValues_[activeControl_] + ( dy > 0 ? 1 : -1 ) );
            sendKnobMessage( activeControl_ + 1, dir, ticks, true );
            dragStartY_ = e.y;
            repaint();
        }
    }
    else if( activeControl_ == 4 ) // Jog wheel
    {
        float angle = getJogAngle( e.x, e.y );
        float diff = angle - lastJogAngle_;
        if( diff > juce::MathConstants<float>::pi ) diff -= juce::MathConstants<float>::twoPi;
        else if( diff < -juce::MathConstants<float>::pi ) diff += juce::MathConstants<float>::twoPi;

        if( std::abs( diff ) > 0.08f )
        {
            int dir = ( diff > 0 ) ? (int)contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE
                                   : (int)contracts::DeviceControlContracts::EN_KNOB_ANTICLOCKWISE;
            sendKnobMessage( contracts::DeviceControlContracts::EN_KNOB_CENTER, dir, 1, false );
            lastJogAngle_ = angle;
            repaint();
        }
    }
}

inline void MobileControlsView::mouseUp( const juce::MouseEvent& )
{
    activeControl_ = -1;
    repaint();
}

//==============================================================================
// Implementations: KickMobileTLayout Container
//==============================================================================
inline KickMobileTLayout::KickMobileTLayout()
{
    setInterceptsMouseClicks( false, true );

    kickHome_ = &kick_ui::KickHome::Instance();
    addChildComponent( kickHome_ );
    kickHome_->setVisible( true );

    padsView_ = std::make_unique<MobilePadsView>( *this );
    addAndMakeVisible( padsView_.get() );

    banksView_ = std::make_unique<MobileBanksView>( *this );
    addAndMakeVisible( banksView_.get() );

    controlsView_ = std::make_unique<MobileControlsView>( *this );
    addAndMakeVisible( controlsView_.get() );

    // Dedicated Edge Navigation Pill Buttons
    toBanksPill_ = std::make_unique<NavPillButton>( "BANKS", NavPillButton::Left, ui::Theme::titleColour( 1 ), [this] {
        navigateTo( TView::Banks );
    } );
    addAndMakeVisible( toBanksPill_.get() );

    toPadsPill_ = std::make_unique<NavPillButton>( "16 PADS", NavPillButton::Right, juce::Colour( 0xFF00E5FF ), [this] {
        navigateTo( TView::Pads );
    } );
    addAndMakeVisible( toPadsPill_.get() );

    toControlsPill_ = std::make_unique<NavPillButton>( "CONTROLS", NavPillButton::Up, juce::Colour( 0xFFFFAB00 ), [this] {
        navigateTo( TView::Controls );
    } );
    addAndMakeVisible( toControlsPill_.get() );

    navigateTo( TView::DAW );
}

inline KickMobileTLayout::~KickMobileTLayout()
{
    stopTimer();
    if( kickHome_ != nullptr )
    {
        kickHome_->setTransform( juce::AffineTransform() );
        removeChildComponent( kickHome_ );
    }
}

inline void KickMobileTLayout::navigateTo( TView view )
{
    targetView_ = view;
    switch( view )
    {
        case TView::DAW:      targetX_ = 0.0f;  targetY_ = 0.0f;  break;
        case TView::Pads:     targetX_ = 1.0f;  targetY_ = 0.0f;  break;
        case TView::Banks:    targetX_ = -1.0f; targetY_ = 0.0f;  break;
        case TView::Controls: targetX_ = 0.0f;  targetY_ = -1.0f; break;
    }
    startTimerHz( 60 );
}

inline void KickMobileTLayout::setBank( int bankIndex )
{
    currentBank_ = juce::jlimit( 0, 7, bankIndex );
    if( padsView_ ) padsView_->setBank( currentBank_ );
    if( banksView_ ) banksView_->setBank( currentBank_ );
}

inline void KickMobileTLayout::setFullLevel( bool enabled )
{
    fullLevel_ = enabled;
    if( padsView_ ) padsView_->setFullLevel( fullLevel_ );
}

inline void KickMobileTLayout::timerCallback()
{
    float dx = targetX_ - currentX_;
    float dy = targetY_ - currentY_;

    if( std::abs( dx ) < 0.002f && std::abs( dy ) < 0.002f )
    {
        currentX_ = targetX_;
        currentY_ = targetY_;
        currentView_ = targetView_;
        stopTimer();
    }
    else
    {
        currentX_ += dx * 0.28f;
        currentY_ += dy * 0.28f;
    }

    updateViewPositions();
    repaint();
}

inline void KickMobileTLayout::updateViewPositions()
{
    int w = getWidth();
    int h = getHeight();
    if( w <= 0 || h <= 0 ) return;

    float offsetX = -currentX_ * (float)w;
    float offsetY = -currentY_ * (float)h;

    // 1. DAW View at Center (0, 0)
    if( kickHome_ != nullptr )
    {
        const float targetAspect = 1024.0f / 600.0f;
        int dawH = h;
        int dawW = juce::roundToInt( (float)dawH * targetAspect );
        if( dawW > w )
        {
            dawW = w;
            dawH = juce::roundToInt( (float)dawW / targetAspect );
        }

        float dawX = offsetX + ( (float)w - (float)dawW ) * 0.5f;
        float dawY = offsetY + ( (float)h - (float)dawH ) * 0.5f;

        kickHome_->setBounds( 0, 0, 1024, 600 );
        kickHome_->setTransform( juce::AffineTransform::scale( (float)dawW / 1024.0f, (float)dawH / 600.0f ).translated( dawX, dawY ) );
    }

    // 2. Pads View at Right (+1, 0)
    if( padsView_ != nullptr )
    {
        padsView_->setBounds( juce::roundToInt( offsetX + (float)w ), juce::roundToInt( offsetY ), w, h );
    }

    // 3. Banks View at Left (-1, 0)
    if( banksView_ != nullptr )
    {
        banksView_->setBounds( juce::roundToInt( offsetX - (float)w ), juce::roundToInt( offsetY ), w, h );
    }

    // 4. Controls View at Top (0, -1)
    if( controlsView_ != nullptr )
    {
        controlsView_->setBounds( juce::roundToInt( offsetX ), juce::roundToInt( offsetY - (float)h ), w, h );
    }

    // Edge Navigation Pills
    if( toBanksPill_ != nullptr && toPadsPill_ != nullptr && toControlsPill_ != nullptr )
    {
        toBanksPill_->setBounds( 8, juce::roundToInt( ( (float)h - 42.0f ) * 0.5f ), 96, 42 );
        toPadsPill_->setBounds( w - 104, juce::roundToInt( ( (float)h - 42.0f ) * 0.5f ), 96, 42 );
        toControlsPill_->setBounds( juce::roundToInt( ( (float)w - 140.0f ) * 0.5f ), 8, 140, 32 );

        bool showPills = ( targetView_ == TView::DAW );
        toBanksPill_->setVisible( showPills );
        toPadsPill_->setVisible( showPills );
        toControlsPill_->setVisible( showPills );

        if( showPills )
        {
            toBanksPill_->toFront( false );
            toPadsPill_->toFront( false );
            toControlsPill_->toFront( false );
        }
    }
}

inline void KickMobileTLayout::resized()
{
    updateViewPositions();
}

inline void KickMobileTLayout::paint( juce::Graphics& g )
{
    g.fillAll( juce::Colours::black );
}

inline void KickMobileTLayout::mouseDown( const juce::MouseEvent& e )
{
    dragStartX_ = e.x;
    dragStartY_ = e.y;
    isSwiping_ = true;
}

inline void KickMobileTLayout::mouseDrag( const juce::MouseEvent& )
{
}

inline void KickMobileTLayout::mouseUp( const juce::MouseEvent& e )
{
    if( isSwiping_ )
    {
        isSwiping_ = false;
        int dx = e.x - dragStartX_;
        int dy = e.y - dragStartY_;

        // Swipe threshold
        if( std::abs( dx ) > 60 || std::abs( dy ) > 60 )
        {
            if( currentView_ == TView::DAW )
            {
                if( std::abs( dx ) > std::abs( dy ) )
                {
                    if( dx < -60 ) navigateTo( TView::Pads );
                    else if( dx > 60 ) navigateTo( TView::Banks );
                }
                else
                {
                    if( dy > 60 ) navigateTo( TView::Controls );
                }
            }
            else if( currentView_ == TView::Pads && dx > 60 )
            {
                navigateTo( TView::DAW );
            }
            else if( currentView_ == TView::Banks && dx < -60 )
            {
                navigateTo( TView::DAW );
            }
            else if( currentView_ == TView::Controls && dy < -60 )
            {
                navigateTo( TView::DAW );
            }
        }
    }
}

} // namespace kick_mobile
