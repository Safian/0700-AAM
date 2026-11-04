/*
 * Copyright (c) 2024 MUSIC Tribe
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

#include "kick_ui_settings_info.h"
#include "../modules/hardware/source/kick_Hwpref.h"
#include "../modules/hardware/source/kick_PlatformServices.h"
#include "../modules/engine/interface/source/mt_audio_ids.h"

namespace kick_ui
{
    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  KickChannelMixer
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    SettingsInfo::SettingsInfo()
        : SettingsInterface( 7, 5, false )
    {
        ui::KickGrid::setBackgroundColour( juce::Colours::transparentBlack );

        fReset.setValueColour( ui::Theme::textColour( 1 ) );
        fwUpdate.setValueColour( ui::Theme::textColour( 1 ) );
        rDefaults.setValueColour( ui::Theme::textColour( 1 ) );

        fReset.setValueFontHeight( 20 );
        fwUpdate.setValueFontHeight( 20 );
        rDefaults.setValueFontHeight( 20 );

        addComponent( logo, 0, 0, 3, 4 );
        addComponent( srNo, 0, 3, 2, 1 );
        addComponent( fVer, 1, 3, 2, 1 );
        addComponent( cVer, 2, 3, 2, 1 );
        addComponent( date, 3, 3, 2, 1 );
        addComponent( fReset, 5, 1, 1, 1 );
        addComponent( fwUpdate, 5, 2, 1, 1 );
        addComponent( rDefaults, 5, 3, 1, 1 );

        ui::themeChanged.connect( [&]() {
            logo.setImage( "LOGO" );
        } );

        fwUpdate.onClick = [&]() {
            auto& ps = mt::PlatformServices::getInstance();
            // The Behringer image updater needs the Behringer hardware; everywhere else the Kick Update screen applies.
            signals::sigScreenOpen( ps.isHardwareControlAvailable() ? kick_ui::Screen::SWUpdateView : kick_ui::Screen::KickUpdate );
        };
        fReset.onClick = [this]() {
            mt::utils::showDialogOwned<ConfirmationPopup>(
                "CONFIRMATION!", std::make_unique<ConfirmationPopup>( "Factory Reset will erase all projects, settings, and WiFi credentials. Continue?", false, false ),
                []( int R, ConfirmationPopup& ) {
                    if( R == UI::APPLY )
                        mt::PlatformServices::getInstance().factoryReset();
                },
                ui::layout::warningPopup, true );
        };
        rDefaults.onClick = [this]() {
            mt::utils::showDialogOwned<ConfirmationPopup>(
                "CONFIRMATION!", std::make_unique<ConfirmationPopup>( "Are You Sure You Want To Restore Defaults ?", false, false ),
                [this]( int R, ConfirmationPopup& ) {
                    if( R == UI::APPLY )
                    {
                        mt::getSettings().makeDefaults();
                        updateInfo();
                    }
                },
                ui::layout::warningPopup, true );
        };

        updateInfo();
    }

    void SettingsInfo::updateInfo()
    {
        auto& ps = mt::PlatformServices::getInstance();

        juce::String serialNumber          = ps.getDevSerial();
        juce::String ControlSurfaceVersion = ps.getControlSurfaceVersion();
        juce::String firmWareVersion       = ps.getFirmwareVersion();
        juce::String buildStamp            = ps.getBuildStamp();

        srNo.value.setText( serialNumber, juce::dontSendNotification );
        cVer.value.setText( ControlSurfaceVersion, juce::dontSendNotification );
        fVer.value.setText( firmWareVersion, juce::dontSendNotification );
        date.value.setText( buildStamp, juce::dontSendNotification );
    }

    void SettingsInfo::refresh()
    {
        updateInfo();
    }

    void SettingsInfo::setupHeader( kick_ui::KickModeHeader& Hdr ) {}

    void SettingsInfo::handleCommand( CPktMgr::HWMsg_t& ) {}

    ConfirmationPopup::ConfirmationPopup( juce::String Info, bool isYes, bool isNo, int x, int y, int width )
        : info( Info ), startX( x ), startY( y ), width( width )
    {
        getProperties().set( "outsideTapCancels", true ); // a confirmation: a tap beside it is a "No"
        mt::utils::addChildComponents( this, yes, no, info );
        yes.setVisible( isYes );
        no.setVisible( isNo );
        info.setVisible( true );
        yes.onClick = [&]() { close( UI::APPLY ); };
        no.onClick  = [&]() { close( UI::CANCEL ); };
        info.setFontHeight( 24.0f );
        info.setTextColour( ui::Theme::textColour( 2 ) );
        info.setValueBackgroundColour( ui::Theme::backgroundColour( 9 ) );
        info.setValueJustification( juce::Justification::centred );

        themeConn = ui::themeChanged.connect( [this]() {
            no.setImages( "No" );
            yes.setImages( "Yes" );
        } );
    }
    void ConfirmationPopup::close( int State )
    {
        mt::utils::exitDialog( this, State );
    }
    void ConfirmationPopup::resized()
    {
        info.setBounds( startX, startY, width, 25 );
        no.setBoundsRelative( 0.05f, 0.86f, 0.4f, 0.106f );
        yes.setBoundsRelative( 0.55f, 0.86f, 0.4f, 0.106f );
    }
}
