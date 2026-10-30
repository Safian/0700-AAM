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
#include <juce_gui_extra/juce_gui_extra.h>
#include "kick_ui/kick_ui.h"
#include "contracts/dcl/dcl_contracts.h"
#include "contracts/dcl/dcl_signals.h"
#include "PktMgr/PktMgr.hpp"
#include "common/common.h"

#if JUCE_MAC

namespace kick_mac
{

//==============================================================================
/**
 * Interactive Behringer Kick hardware chassis container for macOS.
 * Renders the authentic 3D CAD chassis around the live DAW screen and
 * translates clicks/drags on pads, knobs and Jog wheel into DCL hardware messages.
 */
class KickHardwareChassis : public juce::Component
{
public:
    enum class ViewMode
    {
        HardwareChassis = 0,
        DisplayOnly
    };

    KickHardwareChassis()
    {
        kickHome = &kick_ui::KickHome::Instance();
        addAndMakeVisible( kickHome );
        loadImage();
    }

    ~KickHardwareChassis() override
    {
        if( kickHome != nullptr )
        {
            kickHome->setTransform( juce::AffineTransform() );
            removeChildComponent( kickHome );
        }
    }

    void setViewMode( ViewMode mode )
    {
        if( currentMode != mode )
        {
            currentMode = mode;
            resized();
            repaint();
        }
    }

    ViewMode getViewMode() const noexcept { return currentMode; }

    void paint( juce::Graphics& g ) override
    {
        if( currentMode == ViewMode::HardwareChassis )
        {
            g.fillAll( juce::Colours::black );

            if( chassisImage.isValid() && !renderedChassisRect.isEmpty() )
            {
                g.setImageResamplingQuality( juce::Graphics::highResamplingQuality );
                g.drawImage( chassisImage, renderedChassisRect );
            }

            // Fill cutout with black behind the child component to prevent any bleed
            if( !screenCutoutRect.isEmpty() )
            {
                g.setColour( juce::Colours::black );
                g.fillRect( screenCutoutRect );
            }

            // Draw visual hit confirmation on active pad
            if( activePadIndex >= 0 && activePadIndex < 16 )
            {
                auto r = getPadRect( activePadIndex );
                if( !r.isEmpty() )
                {
                    g.setColour( juce::Colour( 0x6600E5FF ) ); // Cyan glow
                    g.fillRoundedRectangle( r, 4.0f );
                    g.setColour( juce::Colours::white.withAlpha( 0.85f ) );
                    g.drawRoundedRectangle( r, 4.0f, 2.0f );
                }
            }

            // Draw visual hit confirmation on active button
            if( activeButtonCode >= 0 )
            {
                auto r = getButtonRect( activeButtonCode );
                if( !r.isEmpty() )
                {
                    g.setColour( juce::Colour( 0x6600E5FF ) );
                    g.fillRoundedRectangle( r, 3.0f );
                    g.setColour( juce::Colours::white.withAlpha( 0.75f ) );
                    g.drawRoundedRectangle( r, 3.0f, 1.5f );
                }
            }
        }
        else
        {
            g.fillAll( juce::Colours::black );
        }
    }

    void resized() override
    {
        if( currentMode == ViewMode::DisplayOnly )
        {
            screenCutoutRect = {};
            renderedChassisRect = {};
            if( kickHome != nullptr )
            {
                kickHome->setTransform( juce::AffineTransform() );
                kickHome->setBounds( getLocalBounds() );
            }
        }
        else
        {
            auto area = getLocalBounds().toFloat();
            const float targetAspect = 700.0f / 710.0f;
            const float currentAspect = area.getHeight() > 0 ? ( area.getWidth() / area.getHeight() ) : targetAspect;

            juce::Rectangle<float> chassisRect;
            if( currentAspect > targetAspect )
            {
                float w = area.getHeight() * targetAspect;
                chassisRect = juce::Rectangle<float>( ( area.getWidth() - w ) * 0.5f, 0.0f, w, area.getHeight() );
            }
            else
            {
                float h = area.getWidth() / targetAspect;
                chassisRect = juce::Rectangle<float>( 0.0f, ( area.getHeight() - h ) * 0.5f, area.getWidth(), h );
            }

            renderedChassisRect = chassisRect;
            const float scale = chassisRect.getWidth() / 700.0f;

            // Screen cutout in 700x710 base coordinates: (159, 78, 382, 224)
            float sx = chassisRect.getX() + 159.0f * scale;
            float sy = chassisRect.getY() + 78.0f * scale;
            float sw = 382.0f * scale;
            float sh = 224.0f * scale;

            screenCutoutRect = juce::Rectangle<float>( sx, sy, sw, sh );

            if( kickHome != nullptr )
            {
                kickHome->setBounds( 0, 0, 1024, 600 );
                kickHome->setTransform( juce::AffineTransform::scale( sw / 1024.0f, sh / 600.0f ).translated( sx, sy ) );
            }
        }
    }

    void mouseDown( const juce::MouseEvent& e ) override
    {
        if( currentMode != ViewMode::HardwareChassis ) return;

        auto bp = toBaseCoords( e.getPosition() );
        if( bp.x < 0.0f ) return;

        // Check 16 MPC Pads
        int pad = getPadAt( bp.x, bp.y );
        if( pad >= 0 )
        {
            activePadIndex = pad;
            sendPadMessage( pad, true, 127 );
            repaint();
            return;
        }

        // Check Jog Wheel
        if( isJogWheelHit( bp.x, bp.y ) )
        {
            draggingJog = true;
            jogStartAngle = std::atan2( bp.y - 518.0f, bp.x - 553.0f );
            return;
        }

        // Check Q-Link Knobs (P1..P4)
        int knob = getKnobAt( bp.x, bp.y );
        if( knob >= 0 )
        {
            activeKnobIndex = knob;
            dragStartY = e.getPosition().y;
            return;
        }

        // Check Buttons
        int btn = getButtonAt( bp.x, bp.y );
        if( btn >= 0 )
        {
            activeButtonCode = btn;
            sendButtonMessage( btn, true );
            repaint();
            return;
        }
    }

    void mouseDrag( const juce::MouseEvent& e ) override
    {
        if( currentMode != ViewMode::HardwareChassis ) return;

        if( draggingJog )
        {
            auto bp = toBaseCoords( e.getPosition() );
            float angle = std::atan2( bp.y - 518.0f, bp.x - 553.0f );
            float delta = angle - jogStartAngle;
            if( delta > juce::MathConstants<float>::pi ) delta -= juce::MathConstants<float>::twoPi;
            else if( delta < -juce::MathConstants<float>::pi ) delta += juce::MathConstants<float>::twoPi;

            if( std::abs( delta ) >= 0.12f )
            {
                int ticks = juce::roundToInt( delta / 0.12f );
                int dir = ( ticks > 0 ) ? (int)contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE
                                        : (int)contracts::DeviceControlContracts::EN_KNOB_ANTICLOCKWISE;
                sendKnobMessage( contracts::DeviceControlContracts::EN_KNOB_CENTER, dir, std::max( 1, std::abs( ticks ) ), false );
                jogStartAngle = angle;
            }
            return;
        }

        if( activeKnobIndex >= 0 )
        {
            int deltaY = dragStartY - e.getPosition().y;
            if( std::abs( deltaY ) >= 4 )
            {
                int ticks = deltaY / 4;
                int dir = ( ticks > 0 ) ? (int)contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE
                                        : (int)contracts::DeviceControlContracts::EN_KNOB_ANTICLOCKWISE;
                sendKnobMessage( contracts::DeviceControlContracts::EN_KNOB_1 + activeKnobIndex, dir, std::max( 1, std::abs( ticks ) ), true );
                dragStartY = e.getPosition().y;
            }
            return;
        }
    }

    void mouseUp( const juce::MouseEvent& e ) override
    {
        juce::ignoreUnused( e );

        if( activePadIndex >= 0 )
        {
            sendPadMessage( activePadIndex, false, 0 );
            activePadIndex = -1;
            repaint();
        }

        if( activeButtonCode >= 0 )
        {
            sendButtonMessage( activeButtonCode, false );
            activeButtonCode = -1;
            repaint();
        }

        draggingJog = false;
        activeKnobIndex = -1;
    }

    void mouseWheelMove( const juce::MouseEvent& e, const juce::MouseWheelDetails& wheel ) override
    {
        if( currentMode != ViewMode::HardwareChassis ) return;

        auto bp = toBaseCoords( e.getPosition() );
        if( bp.x < 0.0f ) return;

        int ticks = std::abs( wheel.deltaY ) > 0.05f ? ( wheel.deltaY > 0.0f ? 1 : -1 ) : 0;
        if( ticks == 0 ) return;

        int dir = ( ticks > 0 ) ? (int)contracts::DeviceControlContracts::EN_KNOB_CLOCKWISE
                                : (int)contracts::DeviceControlContracts::EN_KNOB_ANTICLOCKWISE;

        if( isJogWheelHit( bp.x, bp.y ) )
        {
            sendKnobMessage( contracts::DeviceControlContracts::EN_KNOB_CENTER, dir, 1, false );
            return;
        }

        int knob = getKnobAt( bp.x, bp.y );
        if( knob >= 0 )
        {
            sendKnobMessage( contracts::DeviceControlContracts::EN_KNOB_1 + knob, dir, 1, true );
            return;
        }
    }

private:
    void loadImage()
    {
        auto assetsDir = getAppDirs( MTAssets );
        auto imgFile = assetsDir.getChildFile( "hardware_front.png" );
        if( !imgFile.exists() )
        {
            auto appBundle = juce::File::getSpecialLocation( juce::File::currentApplicationFile );
            auto resFile = appBundle.getChildFile( "Contents/Resources/hardware_front.png" );
            if( resFile.exists() )
                imgFile = resFile;
            else
            {
                auto cwdFile = juce::File::getCurrentWorkingDirectory().getChildFile( "Kick/assets/hardware_front.png" );
                if( cwdFile.exists() )
                    imgFile = cwdFile;
            }
        }

        if( imgFile.exists() )
        {
            chassisImage = juce::ImageFileFormat::loadFrom( imgFile );
        }
    }

    juce::Point<float> toBaseCoords( juce::Point<int> p ) const
    {
        if( renderedChassisRect.isEmpty() ) return { -1.0f, -1.0f };
        float scale = renderedChassisRect.getWidth() / 700.0f;
        if( scale <= 0.0f ) return { -1.0f, -1.0f };
        return { ( p.x - renderedChassisRect.getX() ) / scale, ( p.y - renderedChassisRect.getY() ) / scale };
    }

    int getPadAt( float bx, float by ) const
    {
        const float colX[4] = { 118.0f, 202.0f, 285.0f, 369.0f };
        const float rowY[4] = { 362.0f, 443.0f, 524.0f, 605.0f };
        const float padSize = 64.0f;
        const float padMargin = 4.0f;

        int foundCol = -1;
        for( int c = 0; c < 4; ++c )
        {
            if( bx >= colX[c] - padMargin && bx <= colX[c] + padSize + padMargin )
            {
                foundCol = c;
                break;
            }
        }
        if( foundCol < 0 ) return -1;

        int foundRow = -1;
        for( int r = 0; r < 4; ++r )
        {
            if( by >= rowY[r] - padMargin && by <= rowY[r] + padSize + padMargin )
            {
                foundRow = r;
                break;
            }
        }
        if( foundRow < 0 ) return -1;

        // Row 0 = Pads 13..16 (index 12..15), Row 3 = Pads 1..4 (index 0..3)
        return ( 3 - foundRow ) * 4 + foundCol;
    }

    juce::Rectangle<float> getPadRect( int padIndex ) const
    {
        if( renderedChassisRect.isEmpty() || padIndex < 0 || padIndex >= 16 ) return {};
        const float scale = renderedChassisRect.getWidth() / 700.0f;
        int row = 3 - ( padIndex / 4 );
        int col = padIndex % 4;

        const float colX[4] = { 118.0f, 202.0f, 285.0f, 369.0f };
        const float rowY[4] = { 362.0f, 443.0f, 524.0f, 605.0f };
        const float padSize = 64.0f;

        return juce::Rectangle<float>(
            renderedChassisRect.getX() + colX[col] * scale,
            renderedChassisRect.getY() + rowY[row] * scale,
            padSize * scale,
            padSize * scale
        );
    }

    bool isJogWheelHit( float bx, float by ) const
    {
        return std::hypot( bx - 553.0f, by - 518.0f ) <= 42.0f;
    }

    int getKnobAt( float bx, float by ) const
    {
        const float knobY[4] = { 90.0f, 155.0f, 220.0f, 285.0f };
        for( int i = 0; i < 4; ++i )
        {
            if( std::hypot( bx - 635.0f, by - knobY[i] ) <= 24.0f )
                return i;
        }
        return -1;
    }

    int getButtonAt( float bx, float by ) const
    {
        using namespace contracts;

        // Bank A..D: Y in [360, 388]
        if( by >= 358.0f && by <= 390.0f )
        {
            if( bx >= 435.0f && bx <= 478.0f ) return DeviceControlContracts::EN_BANK_A;
            if( bx >= 495.0f && bx <= 538.0f ) return DeviceControlContracts::EN_BANK_B;
            if( bx >= 555.0f && bx <= 598.0f ) return DeviceControlContracts::EN_BANK_C;
            if( bx >= 615.0f && bx <= 658.0f ) return DeviceControlContracts::EN_BANK_D;
        }

        // Mode row: Y in [415, 445]
        if( by >= 412.0f && by <= 448.0f )
        {
            if( bx >= 435.0f && bx <= 478.0f ) return DeviceControlContracts::EN_MAINMODE;
            if( bx >= 480.0f && bx <= 523.0f ) return DeviceControlContracts::EN_BROWSE;
            if( bx >= 525.0f && bx <= 568.0f ) return DeviceControlContracts::EN_TRACK_MIX;
            if( bx >= 570.0f && bx <= 613.0f ) return DeviceControlContracts::EN_TRACK_MUTE;
            if( bx >= 615.0f && bx <= 658.0f ) return DeviceControlContracts::EN_NEXT_SEQ;
        }

        // Transport row: Y in [620, 658]
        if( by >= 618.0f && by <= 660.0f )
        {
            if( bx >= 440.0f && bx <= 478.0f ) return DeviceControlContracts::EN_RECORD;
            if( bx >= 485.0f && bx <= 523.0f ) return DeviceControlContracts::EN_OVERDUB;
            if( bx >= 530.0f && bx <= 568.0f ) return DeviceControlContracts::EN_STOP;
            if( bx >= 575.0f && bx <= 613.0f ) return DeviceControlContracts::EN_PLAY;
            if( bx >= 620.0f && bx <= 658.0f ) return DeviceControlContracts::EN_PLAYSTART;
        }

        // Utility column right:
        if( bx >= 615.0f && bx <= 658.0f )
        {
            if( by >= 465.0f && by <= 495.0f ) return DeviceControlContracts::EN_MENU;
            if( by >= 515.0f && by <= 552.0f ) return DeviceControlContracts::EN_SHIFT;
            if( by >= 570.0f && by <= 600.0f ) return DeviceControlContracts::EN_PROG_EDIT;
        }

        // Left utility column:
        if( bx >= 435.0f && bx <= 478.0f )
        {
            if( by >= 465.0f && by <= 495.0f ) return DeviceControlContracts::EN_TAP_TEMPO;
            if( by >= 515.0f && by <= 545.0f ) return DeviceControlContracts::EN_NOTE_REPEAT;
            if( by >= 570.0f && by <= 600.0f ) return DeviceControlContracts::EN_STEP_SEQ;
        }

        return -1;
    }

    juce::Rectangle<float> getButtonRect( int buttonCode ) const
    {
        if( renderedChassisRect.isEmpty() || buttonCode < 0 ) return {};
        const float scale = renderedChassisRect.getWidth() / 700.0f;

        auto makeRect = [&]( float bx, float by, float bw, float bh ) {
            return juce::Rectangle<float>(
                renderedChassisRect.getX() + bx * scale,
                renderedChassisRect.getY() + by * scale,
                bw * scale,
                bh * scale
            );
        };

        using namespace contracts;
        switch( buttonCode )
        {
            case DeviceControlContracts::EN_BANK_A: return makeRect( 438, 362, 40, 24 );
            case DeviceControlContracts::EN_BANK_B: return makeRect( 498, 362, 40, 24 );
            case DeviceControlContracts::EN_BANK_C: return makeRect( 558, 362, 40, 24 );
            case DeviceControlContracts::EN_BANK_D: return makeRect( 618, 362, 40, 24 );
            case DeviceControlContracts::EN_MAINMODE: return makeRect( 438, 418, 40, 26 );
            case DeviceControlContracts::EN_BROWSE: return makeRect( 483, 418, 40, 26 );
            case DeviceControlContracts::EN_TRACK_MIX: return makeRect( 528, 418, 40, 26 );
            case DeviceControlContracts::EN_TRACK_MUTE: return makeRect( 573, 418, 40, 26 );
            case DeviceControlContracts::EN_NEXT_SEQ: return makeRect( 618, 418, 40, 26 );
            case DeviceControlContracts::EN_RECORD: return makeRect( 445, 625, 34, 30 );
            case DeviceControlContracts::EN_OVERDUB: return makeRect( 490, 625, 34, 30 );
            case DeviceControlContracts::EN_STOP: return makeRect( 535, 625, 34, 30 );
            case DeviceControlContracts::EN_PLAY: return makeRect( 580, 625, 34, 30 );
            case DeviceControlContracts::EN_PLAYSTART: return makeRect( 625, 625, 34, 30 );
            case DeviceControlContracts::EN_MENU: return makeRect( 618, 468, 40, 26 );
            case DeviceControlContracts::EN_SHIFT: return makeRect( 618, 518, 40, 32 );
            default: return {};
        }
    }

    void sendPadMessage( int padIndex, bool pressed, int velocity )
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

    void sendButtonMessage( int buttonCode, bool pressed )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_BUTTON;
        msg.pktCmd  = contracts::DeviceControlContracts::BtnCmd::EN_BTN_STATUS;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { buttonCode, pressed ? (int)contracts::DeviceControlContracts::BTN_PRESSED
                                            : (int)contracts::DeviceControlContracts::BTN_RELEASED };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

    void sendKnobMessage( int knobCode, int direction, int ticks, bool isQlink )
    {
        CPktMgr::HWMsg_t msg;
        msg.pktType = CPktMgr::enumPacketType::EN_PKT_TYPE_KNOBS;
        msg.pktCmd  = isQlink ? (int)contracts::DeviceControlContracts::KnobCmd::EN_KNOB_QLINK
                              : (int)contracts::DeviceControlContracts::KnobCmd::EN_KNOB_ROTARY;
        msg.pktDir  = CPktMgr::enumPacketDir::EN_PKT_REQUEST;
        msg.data    = { knobCode, direction, ticks };
        contracts::DclSignals::hardwareInputTriggered( msg );
    }

    kick_ui::KickHome*     kickHome{ nullptr };
    juce::Image            chassisImage;
    ViewMode               currentMode{ ViewMode::HardwareChassis };
    juce::Rectangle<float> renderedChassisRect;
    juce::Rectangle<float> screenCutoutRect;

    int   activePadIndex{ -1 };
    int   activeButtonCode{ -1 };
    int   activeKnobIndex{ -1 };
    bool  draggingJog{ false };
    float jogStartAngle{ 0.0f };
    int   dragStartY{ 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickHardwareChassis )
};

//==============================================================================
/**
 * Native macOS Main Menu Bar model with View switching and scale presets.
 */
class KickMacMenuModel : public juce::MenuBarModel
{
public:
    enum MenuIDs
    {
        idHardwareView = 1001,
        idDisplayOnlyView = 1002,
        idToggleView = 1003,
        idScale100 = 1010,
        idScale120 = 1011,
        idScale140 = 1012,
        idScaleFit = 1013
    };

    explicit KickMacMenuModel( std::function<void( int )> onCommand )
        : commandHandler( std::move( onCommand ) )
    {
    }

    ~KickMacMenuModel() override = default;

    juce::StringArray getMenuBarNames() override
    {
        return { juce::String::fromUTF8( "Nézet (View)" ) };
    }

    juce::PopupMenu getMenuForIndex( int topLevelMenuIndex, const juce::String& /*menuName*/ ) override
    {
        juce::PopupMenu menu;
        if( topLevelMenuIndex == 0 )
        {
            juce::PopupMenu::Item itemHw;
            itemHw.itemID = idHardwareView;
            itemHw.text = juce::String::fromUTF8( "Hardver nézet (Hardware Chassis)" );
            itemHw.shortcutKeyDescription = "Cmd+1 / F1";
            itemHw.isTicked = isHardwareView;
            menu.addItem( itemHw );

            juce::PopupMenu::Item itemDisp;
            itemDisp.itemID = idDisplayOnlyView;
            itemDisp.text = juce::String::fromUTF8( "Csak kijelző (Display Only)" );
            itemDisp.shortcutKeyDescription = "Cmd+2 / F2";
            itemDisp.isTicked = !isHardwareView;
            menu.addItem( itemDisp );

            menu.addSeparator();

            juce::PopupMenu::Item itemToggle;
            itemToggle.itemID = idToggleView;
            itemToggle.text = juce::String::fromUTF8( "Nézet váltása (Toggle View)" );
            itemToggle.shortcutKeyDescription = "F5";
            menu.addItem( itemToggle );

            menu.addSeparator();

            juce::PopupMenu scaleMenu;
            scaleMenu.addItem( idScale100, juce::String::fromUTF8( "100% (Kompakt: 700×710 / 1024×600)" ) );
            scaleMenu.addItem( idScale120, juce::String::fromUTF8( "120% (Alapértelmezett: 840×852)" ) );
            scaleMenu.addItem( idScale140, juce::String::fromUTF8( "140% (Nagy: 980×994)" ) );
            scaleMenu.addItem( idScaleFit, juce::String::fromUTF8( "Képernyőhöz igazítás (Fit to Screen)" ) );
            menu.addSubMenu( juce::String::fromUTF8( "Ablak méret (Scale)" ), scaleMenu );
        }
        return menu;
    }

    void menuItemSelected( int menuItemID, int /*topLevelMenuIndex*/ ) override
    {
        if( commandHandler )
            commandHandler( menuItemID );
    }

    void setHardwareViewActive( bool active )
    {
        if( isHardwareView != active )
        {
            isHardwareView = active;
            menuItemsChanged();
        }
    }

private:
    std::function<void( int )> commandHandler;
    bool isHardwareView{ true };
};

} // namespace kick_mac

#endif // JUCE_MAC
