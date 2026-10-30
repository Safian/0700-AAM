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
#include "kick_hardware_controls.h"
#include "kick_mobile_t_layout.h"

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
        DisplayOnly,
        MobileTLayout
    };

    KickHardwareChassis()
    {
        kickHome = &kick_ui::KickHome::Instance();
        addAndMakeVisible( kickHome );
        loadImage();

        // 1. Reusable Pads
        for( int i = 0; i < 16; ++i )
        {
            pads[i] = std::make_unique<kick_hw_ui::HardwarePad>( i );
            addChildComponent( pads[i].get() );
        }

        // 2. Reusable Q-Links
        for( int i = 0; i < 4; ++i )
        {
            qlinks[i] = std::make_unique<kick_hw_ui::HardwareRotaryKnob>( contracts::DeviceControlContracts::EN_KNOB_1 + i, true );
            addChildComponent( qlinks[i].get() );
        }

        // 3. Reusable Master & Rec knobs
        masterKnob = std::make_unique<kick_hw_ui::HardwareRotaryKnob>( contracts::DeviceControlContracts::EN_KNOB_5, false, 0.85f );
        addChildComponent( masterKnob.get() );

        recKnob = std::make_unique<kick_hw_ui::HardwareRotaryKnob>( contracts::DeviceControlContracts::EN_KNOB_6, false, 0.5f );
        addChildComponent( recKnob.get() );

        // 4. Reusable Jog Wheel
        jogWheel = std::make_unique<kick_hw_ui::HardwareJogWheel>();
        addChildComponent( jogWheel.get() );

        // 5. Reusable Ribbon
        ribbon = std::make_unique<kick_hw_ui::HardwareRibbon>();
        addChildComponent( ribbon.get() );

        // 6. Reusable Bank A..D buttons
        const int bankCodes[4] = { contracts::DeviceControlContracts::EN_BANK_A, contracts::DeviceControlContracts::EN_BANK_B, contracts::DeviceControlContracts::EN_BANK_C, contracts::DeviceControlContracts::EN_BANK_D };
        for( int b = 0; b < 4; ++b )
        {
            bankButtons[b] = std::make_unique<kick_hw_ui::HardwareButton>( bankCodes[b], false, juce::Colour( 0xffff9900 ) );
            if( b == 0 ) bankButtons[b]->setLit( true );
            addChildComponent( bankButtons[b].get() );
        }

        // 7. Reusable Transport buttons
        const int transCodes[5] = { contracts::DeviceControlContracts::EN_RECORD, contracts::DeviceControlContracts::EN_OVERDUB, contracts::DeviceControlContracts::EN_STOP, contracts::DeviceControlContracts::EN_PLAY, contracts::DeviceControlContracts::EN_PLAYSTART };
        const juce::Colour transCols[5] = {
            juce::Colour( 0xffff3b30 ), juce::Colour( 0xffff9500 ), juce::Colour( 0xffffcc00 ),
            juce::Colour( 0xff34c759 ), juce::Colour( 0xff00d4ff )
        };
        for( int t = 0; t < 5; ++t )
        {
            transButtons[t] = std::make_unique<kick_hw_ui::HardwareButton>( transCodes[t], true, transCols[t] );
            addChildComponent( transButtons[t].get() );
        }
    }

    ~KickHardwareChassis() override
    {
        mobileLayout.reset();
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
            if( mode == ViewMode::MobileTLayout )
            {
                if( kickHome != nullptr )
                {
                    kickHome->setTransform( juce::AffineTransform() );
                    removeChildComponent( kickHome );
                }
                mobileLayout = std::make_unique<kick_mobile::KickMobileTLayout>();
                addAndMakeVisible( mobileLayout.get() );
            }
            else
            {
                if( mobileLayout != nullptr )
                {
                    mobileLayout.reset();
                    kickHome = &kick_ui::KickHome::Instance();
                    addAndMakeVisible( kickHome );
                }
            }

            currentMode = mode;
            resized();
            repaint();
        }
    }

    ViewMode getViewMode() const noexcept { return currentMode; }
    kick_mobile::KickMobileTLayout* getMobileLayout() const noexcept { return mobileLayout.get(); }

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
        if( currentMode == ViewMode::MobileTLayout )
        {
            hideHardwareControls();
            screenCutoutRect = {};
            renderedChassisRect = {};
            if( mobileLayout != nullptr )
            {
                mobileLayout->setBounds( getLocalBounds() );
            }
            return;
        }

        if( currentMode == ViewMode::DisplayOnly )
        {
            hideHardwareControls();
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

            auto toScreen = [&]( float bx, float by, float bw, float bh ) {
                return juce::Rectangle<int>(
                    (int)std::round( chassisRect.getX() + bx * scale ),
                    (int)std::round( chassisRect.getY() + by * scale ),
                    (int)std::round( bw * scale ),
                    (int)std::round( bh * scale )
                );
            };

            // Pads
            const float colX[4] = { 116.0f, 194.0f, 272.0f, 350.0f };
            const float rowY[4] = { 363.0f, 441.0f, 519.0f, 597.0f };
            for( int r = 0; r < 4; ++r )
            {
                for( int c = 0; c < 4; ++c )
                {
                    int padIdx = ( 3 - r ) * 4 + c;
                    if( pads[padIdx] )
                    {
                        pads[padIdx]->setVisible( true );
                        pads[padIdx]->setBounds( toScreen( colX[c], rowY[r], 59.0f, 61.0f ) );
                    }
                }
            }

            // Q-Links
            const float knobY[4] = { 114.0f, 178.0f, 244.0f, 308.0f };
            for( int i = 0; i < 4; ++i )
            {
                if( qlinks[i] )
                {
                    qlinks[i]->setVisible( true );
                    qlinks[i]->setBounds( toScreen( 621.0f - 24.0f, knobY[i] - 24.0f, 48.0f, 48.0f ) );
                }
            }

            // Master & Rec
            if( masterKnob )
            {
                masterKnob->setVisible( true );
                masterKnob->setBounds( toScreen( 76.0f - 24.0f, 114.0f - 24.0f, 48.0f, 48.0f ) );
            }
            if( recKnob )
            {
                recKnob->setVisible( true );
                recKnob->setBounds( toScreen( 76.0f - 24.0f, 178.0f - 24.0f, 48.0f, 48.0f ) );
            }

            // Jog Wheel
            if( jogWheel )
            {
                jogWheel->setVisible( true );
                jogWheel->setBounds( toScreen( 553.0f - 48.0f, 524.0f - 48.0f, 96.0f, 96.0f ) );
            }

            // Ribbon
            if( ribbon )
            {
                ribbon->setVisible( true );
                ribbon->setBounds( toScreen( 35.0f, 363.0f, 55.0f, 150.0f ) );
            }

            // Banks
            for( int b = 0; b < 4; ++b )
            {
                if( bankButtons[b] )
                {
                    bankButtons[b]->setVisible( true );
                    bankButtons[b]->setBounds( toScreen( 440.0f + b * 60.0f, 350.0f, 38.0f, 28.0f ) );
                }
            }

            // Transport
            const float transX[5] = { 440.0f, 484.0f, 534.0f, 579.0f, 623.0f };
            for( int t = 0; t < 5; ++t )
            {
                if( transButtons[t] )
                {
                    transButtons[t]->setVisible( true );
                    transButtons[t]->setBounds( toScreen( transX[t], 625.0f, 34.0f, 34.0f ) );
                }
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
            jogStartAngle = std::atan2( bp.y - 524.0f, bp.x - 553.0f );
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
            float angle = std::atan2( bp.y - 524.0f, bp.x - 553.0f );
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
        const float colX[4] = { 116.0f, 194.0f, 272.0f, 350.0f };
        const float padW[4] = { 59.0f,  59.0f,  59.0f,  60.0f };

        const float rowY[4] = { 363.0f, 441.0f, 519.0f, 597.0f };
        const float padH[4] = { 61.0f,  61.0f,  61.0f,  62.0f };
        const float padMargin = 4.0f;

        int foundCol = -1;
        for( int c = 0; c < 4; ++c )
        {
            if( bx >= colX[c] - padMargin && bx <= colX[c] + padW[c] + padMargin )
            {
                foundCol = c;
                break;
            }
        }
        if( foundCol < 0 ) return -1;

        int foundRow = -1;
        for( int r = 0; r < 4; ++r )
        {
            if( by >= rowY[r] - padMargin && by <= rowY[r] + padH[r] + padMargin )
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

        const float colX[4] = { 116.0f, 194.0f, 272.0f, 350.0f };
        const float padW[4] = { 59.0f,  59.0f,  59.0f,  60.0f };

        const float rowY[4] = { 363.0f, 441.0f, 519.0f, 597.0f };
        const float padH[4] = { 61.0f,  61.0f,  61.0f,  62.0f };

        return juce::Rectangle<float>(
            renderedChassisRect.getX() + colX[col] * scale,
            renderedChassisRect.getY() + rowY[row] * scale,
            padW[col] * scale,
            padH[row] * scale
        );
    }

    bool isJogWheelHit( float bx, float by ) const
    {
        return std::hypot( bx - 553.0f, by - 524.0f ) <= 48.0f;
    }

    int getKnobAt( float bx, float by ) const
    {
        const float knobY[4] = { 114.0f, 178.0f, 244.0f, 308.0f };
        for( int i = 0; i < 4; ++i )
        {
            if( std::hypot( bx - 621.0f, by - knobY[i] ) <= 24.0f )
                return i;
        }
        return -1;
    }

    int getButtonAt( float bx, float by ) const
    {
        using namespace contracts;

        // Bank A..D: Y in [348, 380]
        if( by >= 348.0f && by <= 380.0f )
        {
            if( bx >= 438.0f && bx <= 478.0f ) return DeviceControlContracts::EN_BANK_A;
            if( bx >= 498.0f && bx <= 538.0f ) return DeviceControlContracts::EN_BANK_B;
            if( bx >= 558.0f && bx <= 598.0f ) return DeviceControlContracts::EN_BANK_C;
            if( bx >= 618.0f && bx <= 658.0f ) return DeviceControlContracts::EN_BANK_D;
        }

        // Mode row: Y in [410, 440]
        if( by >= 410.0f && by <= 440.0f )
        {
            if( bx >= 435.0f && bx <= 472.0f ) return DeviceControlContracts::EN_MAINMODE;
            if( bx >= 478.0f && bx <= 515.0f ) return DeviceControlContracts::EN_BROWSE;
            if( bx >= 524.0f && bx <= 564.0f ) return DeviceControlContracts::EN_TRACK_MIX;
            if( bx >= 575.0f && bx <= 615.0f ) return DeviceControlContracts::EN_TRACK_MUTE;
            if( bx >= 621.0f && bx <= 661.0f ) return DeviceControlContracts::EN_NEXT_SEQ;
        }

        // Transport row: Y in [620, 665]
        if( by >= 620.0f && by <= 665.0f )
        {
            if( std::hypot( bx - 457.0f, by - 642.0f ) <= 20.0f ) return DeviceControlContracts::EN_RECORD;
            if( std::hypot( bx - 501.0f, by - 641.0f ) <= 20.0f ) return DeviceControlContracts::EN_OVERDUB;
            if( std::hypot( bx - 551.0f, by - 642.0f ) <= 20.0f ) return DeviceControlContracts::EN_STOP;
            if( std::hypot( bx - 596.0f, by - 642.0f ) <= 20.0f ) return DeviceControlContracts::EN_PLAY;
            if( std::hypot( bx - 640.0f, by - 642.0f ) <= 20.0f ) return DeviceControlContracts::EN_PLAYSTART;
        }

        // Left utility column: X in [435, 475]
        if( bx >= 435.0f && bx <= 475.0f )
        {
            if( by >= 452.0f && by <= 483.0f ) return DeviceControlContracts::EN_TAP_TEMPO;
            if( by >= 508.0f && by <= 538.0f ) return DeviceControlContracts::EN_NOTE_REPEAT;
            if( by >= 563.0f && by <= 594.0f ) return DeviceControlContracts::EN_STEP_SEQ;
        }

        // Right utility column: X in [618, 658]
        if( bx >= 618.0f && bx <= 658.0f )
        {
            if( by >= 452.0f && by <= 483.0f ) return DeviceControlContracts::EN_MENU;
            if( by >= 505.0f && by <= 538.0f ) return DeviceControlContracts::EN_SHIFT;
            if( by >= 563.0f && by <= 594.0f ) return DeviceControlContracts::EN_PROG_EDIT;
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
            case DeviceControlContracts::EN_BANK_A: return makeRect( 440, 350, 38, 28 );
            case DeviceControlContracts::EN_BANK_B: return makeRect( 500, 350, 38, 28 );
            case DeviceControlContracts::EN_BANK_C: return makeRect( 560, 350, 38, 28 );
            case DeviceControlContracts::EN_BANK_D: return makeRect( 620, 350, 38, 28 );

            case DeviceControlContracts::EN_MAINMODE: return makeRect( 435, 412, 38, 26 );
            case DeviceControlContracts::EN_BROWSE:   return makeRect( 478, 412, 38, 26 );
            case DeviceControlContracts::EN_TRACK_MIX: return makeRect( 524, 412, 40, 26 );
            case DeviceControlContracts::EN_TRACK_MUTE: return makeRect( 575, 412, 40, 26 );
            case DeviceControlContracts::EN_NEXT_SEQ: return makeRect( 621, 412, 40, 26 );

            case DeviceControlContracts::EN_RECORD:    return makeRect( 440, 625, 34, 34 );
            case DeviceControlContracts::EN_OVERDUB:   return makeRect( 484, 625, 34, 34 );
            case DeviceControlContracts::EN_STOP:      return makeRect( 534, 625, 34, 34 );
            case DeviceControlContracts::EN_PLAY:      return makeRect( 579, 625, 34, 34 );
            case DeviceControlContracts::EN_PLAYSTART: return makeRect( 623, 625, 34, 34 );

            case DeviceControlContracts::EN_TAP_TEMPO:   return makeRect( 435, 454, 38, 26 );
            case DeviceControlContracts::EN_NOTE_REPEAT: return makeRect( 435, 510, 38, 26 );
            case DeviceControlContracts::EN_STEP_SEQ:    return makeRect( 435, 565, 38, 26 );

            case DeviceControlContracts::EN_MENU:      return makeRect( 620, 454, 38, 26 );
            case DeviceControlContracts::EN_SHIFT:     return makeRect( 620, 508, 38, 30 );
            case DeviceControlContracts::EN_PROG_EDIT: return makeRect( 620, 565, 38, 26 );
            default: return {};
        }
    }

public:
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

    void hideHardwareControls()
    {
        for( auto& p : pads ) if( p ) p->setVisible( false );
        for( auto& q : qlinks ) if( q ) q->setVisible( false );
        if( masterKnob ) masterKnob->setVisible( false );
        if( recKnob ) recKnob->setVisible( false );
        if( jogWheel ) jogWheel->setVisible( false );
        if( ribbon ) ribbon->setVisible( false );
        for( auto& b : bankButtons ) if( b ) b->setVisible( false );
        for( auto& t : transButtons ) if( t ) t->setVisible( false );
    }

    kick_ui::KickHome*     kickHome{ nullptr };
    std::unique_ptr<kick_mobile::KickMobileTLayout> mobileLayout;
    juce::Image            chassisImage;
    ViewMode               currentMode{ ViewMode::HardwareChassis };
    juce::Rectangle<float> renderedChassisRect;
    juce::Rectangle<float> screenCutoutRect;

    // Reusable kick_hw_ui components
    std::unique_ptr<kick_hw_ui::HardwarePad>        pads[16];
    std::unique_ptr<kick_hw_ui::HardwareRotaryKnob> qlinks[4];
    std::unique_ptr<kick_hw_ui::HardwareRotaryKnob> masterKnob;
    std::unique_ptr<kick_hw_ui::HardwareRotaryKnob> recKnob;
    std::unique_ptr<kick_hw_ui::HardwareJogWheel>   jogWheel;
    std::unique_ptr<kick_hw_ui::HardwareRibbon>     ribbon;
    std::unique_ptr<kick_hw_ui::HardwareButton>     bankButtons[4];
    std::unique_ptr<kick_hw_ui::HardwareButton>     transButtons[5];

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
        idMobileView = 1005,
        idMobileBottomPads = 1006,
        idMobileMasterStrip = 1007,
        idMobileQLinkStrip = 1008,
        idMobileReturnDaw = 1009,
        idToggleView = 1003,
        idToggleDrawer = 1004,
        idScale100 = 1010,
        idScale120 = 1011,
        idScale140 = 1012,
        idScaleFit = 1013,
        idProjectDemo1 = 2001,
        idProjectBrowser = 2002,
        idProjectPlayPause = 2003
    };

    explicit KickMacMenuModel( std::function<void( int )> onCommand )
        : commandHandler( std::move( onCommand ) )
    {
    }

    ~KickMacMenuModel() override = default;

    juce::StringArray getMenuBarNames() override
    {
        return { juce::String::fromUTF8( "Nézet (View)" ),
                 juce::String::fromUTF8( "Projekt (Project)" ) };
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
            itemHw.isTicked = ( activeViewMode == 0 );
            menu.addItem( itemHw );

            juce::PopupMenu::Item itemDisp;
            itemDisp.itemID = idDisplayOnlyView;
            itemDisp.text = juce::String::fromUTF8( "Csak kijelző (Display Only)" );
            itemDisp.shortcutKeyDescription = "Cmd+2 / F2";
            itemDisp.isTicked = ( activeViewMode == 1 );
            menu.addItem( itemDisp );

            juce::PopupMenu::Item itemMob;
            itemMob.itemID = idMobileView;
            itemMob.text = juce::String::fromUTF8( "Mobil hardver nézet (iPhone Mobile View)" );
            itemMob.shortcutKeyDescription = "Cmd+3 / F3";
            itemMob.isTicked = ( activeViewMode == 2 );
            menu.addItem( itemMob );

            juce::PopupMenu mobileGesturesMenu;
            mobileGesturesMenu.addItem( idMobileBottomPads, juce::String::fromUTF8( "Padok & Kontrolok felcsúsztatása (3 ujjal fel / ↑)" ) );
            mobileGesturesMenu.addItem( idMobileMasterStrip, juce::String::fromUTF8( "Master & Rec sáv (3 ujjal jobbra / →)" ) );
            mobileGesturesMenu.addItem( idMobileQLinkStrip, juce::String::fromUTF8( "Q-Link potméterek (3 ujjal balra / ←)" ) );
            mobileGesturesMenu.addItem( idMobileReturnDaw, juce::String::fromUTF8( "Vissza a DAW-hoz (3 ujjal le / ↓ / Esc)" ) );
            menu.addSubMenu( juce::String::fromUTF8( "Mobil gesztusok (Mobile Gestures)" ), mobileGesturesMenu );

            menu.addSeparator();

            juce::PopupMenu::Item itemToggle;
            itemToggle.itemID = idToggleView;
            itemToggle.text = juce::String::fromUTF8( "Nézet váltása (Toggle View)" );
            itemToggle.shortcutKeyDescription = "F5";
            menu.addItem( itemToggle );

            juce::PopupMenu::Item itemDrawer;
            itemDrawer.itemID = idToggleDrawer;
            itemDrawer.text = juce::String::fromUTF8( "Virtuális kezelősáv (Virtual Controls Drawer)" );
            itemDrawer.isTicked = kick_ui::KickHome::Instance().isHardwareDrawerVisible();
            menu.addItem( itemDrawer );

            menu.addSeparator();

            juce::PopupMenu scaleMenu;
            scaleMenu.addItem( idScale100, juce::String::fromUTF8( "100% (Kompakt: 700×710 / 1024×600)" ) );
            scaleMenu.addItem( idScale120, juce::String::fromUTF8( "120% (Alapértelmezett: 840×852)" ) );
            scaleMenu.addItem( idScale140, juce::String::fromUTF8( "140% (Nagy: 980×994)" ) );
            scaleMenu.addItem( idScaleFit, juce::String::fromUTF8( "Képernyőhöz igazítás (Fit to Screen)" ) );
            menu.addSubMenu( juce::String::fromUTF8( "Ablak méret (Scale)" ), scaleMenu );
        }
        else if( topLevelMenuIndex == 1 )
        {
            juce::PopupMenu::Item itemDemo;
            itemDemo.itemID = idProjectDemo1;
            itemDemo.text = juce::String::fromUTF8( "Demo 01 betöltése (Drum & Bass - 174 BPM, 16 Pad)" );
            itemDemo.shortcutKeyDescription = "Cmd+D";
            menu.addItem( itemDemo );

            juce::PopupMenu::Item itemRecent;
            itemRecent.itemID = idProjectBrowser;
            itemRecent.text = juce::String::fromUTF8( "Projektek böngészője... (Projects & Demos)" );
            itemRecent.shortcutKeyDescription = "Cmd+O";
            menu.addItem( itemRecent );

            menu.addSeparator();

            juce::PopupMenu::Item itemPlay;
            itemPlay.itemID = idProjectPlayPause;
            itemPlay.text = juce::String::fromUTF8( "Lejátszás indítása / leállítás (Play / Stop)" );
            itemPlay.shortcutKeyDescription = "Space";
            menu.addItem( itemPlay );
        }
        return menu;
    }

    void menuItemSelected( int menuItemID, int /*topLevelMenuIndex*/ ) override
    {
        if( commandHandler )
            commandHandler( menuItemID );
    }

    void setActiveViewMode( int mode )
    {
        if( activeViewMode != mode )
        {
            activeViewMode = mode;
            menuItemsChanged();
        }
    }

    void setHardwareViewActive( bool active )
    {
        setActiveViewMode( active ? 0 : 1 );
    }

private:
    std::function<void( int )> commandHandler;
    int activeViewMode{ 0 };
};

} // namespace kick_mac

#endif // JUCE_MAC
