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

#include "kick_ui_settings_wifi.h"
#include "../modules/hardware/source/kick_Hwpref.h"
#include "../modules/hardware/source/kick_WifiFirmware.h"

namespace kick_ui
{

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  SettingsWifi
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    SettingsWifi::SettingsWifi()
        : SettingsInterface( 4, 4, true )
    {
        ui::KickGrid::setBackgroundColour( juce::Colours::transparentBlack );
        setRowTitles( { "WIFI SETTINGS", "SCAN & SELECT", "PASSWORD", "CONNECTION" } );

        addComponent( enabled, 0, 0 );
        addComponent( statusLabel, 0, 1, 3 );

        statusLabel.setValueJustification( juce::Justification::centred );
        statusLabel.setValueFontHeight( 16 );

        addComponent( scanWifi, 1, 0 );
        addComponent( wifiList, 1, 1, 3 );

        addComponent( password, 2, 0, 2 );
        addComponent( setPasswordBtn, 2, 2, 1 );

        addComponent( connect, 3, 0 );
        addComponent( wifIP, 3, 1 );
        addComponent( subnet, 3, 2 );
        addComponent( gateway, 3, 3 );

        enabled.onClick = [&]() {
            handleEnabledClick();
        };

        scanWifi.onClick = [this]() {
            if( !enabled.getToggleState() )
            {
                enabled.setToggleState( true, juce::dontSendNotification );
                handleEnabledClick();
            }
            statusLabel.setText( "Scanning WiFi networks...", juce::dontSendNotification );
            wifiList.setListOptions( { "Scanning..." } );
            juce::Thread::launch( [this]() {
                std::vector<juce::String> list = mt::WifiManager().scanWiFiNetworks();
                juce::MessageManager::callAsync( [this, list]() {
                    if( list.empty() )
                    {
                        wifiList.setListOptions( { "(No networks found)" } );
                        statusLabel.setText( "0 networks found - Tap SCAN to retry", juce::dontSendNotification );
                    }
                    else
                    {
                        wifiList.setListOptions( list );
                        juce::String savedSsid = settings.wifi.ssid();
                        if( !savedSsid.isEmpty() )
                            wifiList.setSelected( savedSsid, juce::dontSendNotification );
                        statusLabel.setText( juce::String( (int)list.size() ) + " networks found", juce::dontSendNotification );
                    }
                } );
            } );
        };

        password.onClick = [this]() {
            openPasswordKeyboard();
        };

        setPasswordBtn.onClick = [this]() {
            openPasswordKeyboard();
        };

        wifiList.onSelectionChanged = [&]( int value, juce::String str ) {
            juce::ignoreUnused( value );
            if( str.isNotEmpty() && str != "Scanning..." && str != "(No networks found)" )
            {
                settings.wifi.ssid = str;
                statusLabel.setText( "Selected: " + str, juce::dontSendNotification );
            }
        };

        connect.onClick = [&]() {
            connectWifi( connect.getToggleState() );
        };

        initializeInterfaceState();
    }

    void SettingsWifi::openPasswordKeyboard()
    {
        auto& keyboard = ui::KickVirtualKeyBoard::getWithText( settings.wifi.pwd() );
        keyboard.onEnterPressed = [&keyboard, this]( int v ) {
            mt::utils::exitDialog( &keyboard, v );
        };
        keyboard.show(
            "WIFI PASSWORD",
            ui::KickVirtualKeyBoard::Type::KEYS_LOWERCASE_ALPHABETIC,
            [this, &keyboard]( int r ) {
                if( r == UI::BUTTONS::APPLY )
                {
                    juce::String newPass = keyboard.getInputText();
                    settings.wifi.pwd = newPass;
                    updatePasswordDisplay();
                    statusLabel.setText( "Password updated", juce::dontSendNotification );
                }
            },
            ui::layout::keyBoardPopup );
    }

    void SettingsWifi::updatePasswordDisplay()
    {
        juce::String pwd = settings.wifi.pwd();
        if( pwd.isEmpty() )
        {
            password.setText( "(Tap to enter password)", juce::dontSendNotification );
        }
        else
        {
            juce::String masked;
            for( int i = 0; i < pwd.length(); ++i )
                masked += "*";
            password.setText( masked + " (" + juce::String( pwd.length() ) + " chars)", juce::dontSendNotification );
        }
    }

    void SettingsWifi::connectWifi( bool Connect )
    {
        if( Connect )
        {
            juce::String ssid = wifiList.getSelectedValue();
            if( ssid.isEmpty() || ssid == "Scanning..." || ssid == "(No networks found)" )
                ssid = settings.wifi.ssid();

            if( ssid.isEmpty() || ssid == "Scanning..." || ssid == "(No networks found)" )
            {
                statusLabel.setText( "Select a network first!", juce::dontSendNotification );
                connect.setToggleState( false, juce::dontSendNotification );
                return;
            }

            juce::String pwd = settings.wifi.pwd();
            statusLabel.setText( "Connecting to " + ssid + "...", juce::dontSendNotification );

            juce::Thread::launch( [this, ssid, pwd]() {
                bool ok = mt::WifiManager().getWifiInterface()->connectToWiFi( ssid, pwd );
                juce::MessageManager::callAsync( [this, ok, ssid]() {
                    if( ok )
                    {
                        refresh();
                        juce::String ip = wifIP.getText();
                        if( ip.isNotEmpty() && ip != "0.0.0.0" )
                            statusLabel.setText( "Connected: " + ip, juce::dontSendNotification );
                        else
                            statusLabel.setText( "Connected to " + ssid, juce::dontSendNotification );
                        connect.setToggleState( true, juce::dontSendNotification );

                        // If timezone has not been explicitly set by user, detect via network
                        if( !settings.hardware.timeZoneUserSet() )
                        {
                            mt::HwPref::getHwPref().detectTimezoneAsync( [this]( const juce::String& netTz ) {
                                if( !settings.hardware.timeZoneUserSet() && netTz.isNotEmpty() )
                                {
                                    settings.hardware.timezone = netTz;
                                    mt::HwPref::getHwPref().setTimeZone( netTz );
                                }
                            } );
                        }
                    }
                    else
                    {
                        statusLabel.setText( "Connection failed. Check password or signal.", juce::dontSendNotification );
                        connect.setToggleState( false, juce::dontSendNotification );
                    }
                } );
            } );
        }
        else
        {
            mt::WifiManager().getWifiInterface()->disconnectWiFi();
            statusLabel.setText( "Disconnected", juce::dontSendNotification );
            connect.setToggleState( false, juce::dontSendNotification );
            refresh();
        }
    }

    void SettingsWifi::handleEnabledClick()
    {
        bool status = enabled.getToggleState();
        settings.wifi.enabled = ( status );
        if( status )
        {
            mt::WifiManager().enableWiFi();
            statusLabel.setText( "WiFi Enabled", juce::dontSendNotification );
        }
        else
        {
            mt::WifiManager().disableWiFi();
            statusLabel.setText( "WiFi Disabled", juce::dontSendNotification );
            connect.setToggleState( false, juce::dontSendNotification );
        }
    }

    void SettingsWifi::refresh()
    {
        mt::sysEthInfo info;
        mt::HwPref::getHwPref().getIP( "wlan0", info );

        wifIP.setText( info.IP.empty() ? "0.0.0.0" : info.IP, juce::dontSendNotification );
        subnet.setText( info.subnet.empty() ? "0.0.0.0" : info.subnet, juce::dontSendNotification );
        gateway.setText( info.gateway.empty() ? "0.0.0.0" : info.gateway, juce::dontSendNotification );

        if( !info.IP.empty() && info.IP != "0.0.0.0" )
        {
            connect.setToggleState( true, juce::dontSendNotification );
        }
    }

    void SettingsWifi::initializeInterfaceState()
    {
        bool isWifiEnabled = settings.wifi.enabled();
        enabled.setToggleState( isWifiEnabled, juce::dontSendNotification );
        updatePasswordDisplay();

        juce::String savedSsid = settings.wifi.ssid();
        if( !savedSsid.isEmpty() )
        {
            wifiList.setListOptions( { savedSsid } );
            wifiList.setSelected( savedSsid, juce::dontSendNotification );
        }

        if( isWifiEnabled )
        {
            mt::WifiManager().enableWiFi();
            statusLabel.setText( "WiFi Ready", juce::dontSendNotification );
        }
        else
        {
            statusLabel.setText( "WiFi Disabled", juce::dontSendNotification );
        }
        refresh();
    }

    void SettingsWifi::setupHeader( kick_ui::KickModeHeader& Hdr )
    {
        Hdr.setTitle( "Preferences - Wi-Fi" );
    }

    void SettingsWifi::handleCommand( CPktMgr::HWMsg_t& ) {}

}
