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
        addComponent( statusLabel, 0, 1, 2 );

        statusLabel.setValueJustification( juce::Justification::centred );
        statusLabel.setValueFontHeight( 16 );

        addComponent( scanWifi, 1, 0 );
        addComponent( wifiList, 1, 1, 2 );

        addComponent( password, 2, 0, 2 );
        password.setEditable( false, true );

        addComponent( connect, 3, 0 );
        addComponent( wifIP, 3, 1 );
        addComponent( subnet, 3, 2 );
        addComponent( gateway, 3, 3 );

        enabled.onClick = [&]() {
            handleEnabledClick();
        };

        scanWifi.onClick = [this]() {
            statusLabel.setText( "Scanning WiFi...", juce::dontSendNotification );
            juce::Thread::launch( [this]() {
                std::vector<juce::String> list = mt::WifiManager().scanWiFiNetworks();
                juce::MessageManager::callAsync( [this, list]() {
                    wifiList.setListOptions( list );
                    juce::String savedSsid = settings.wifi.ssid();
                    if( !savedSsid.isEmpty() )
                        wifiList.setSelected( savedSsid, juce::dontSendNotification );
                    statusLabel.setText( juce::String( (int)list.size() ) + " networks found", juce::dontSendNotification );
                } );
            } );
        };

        password.onTextChange = [&]() {
            juce::String text = password.getText();
            settings.wifi.pwd = ( text );
        };

        wifiList.onSelectionChanged = [&]( int value, juce::String str ) {
            juce::ignoreUnused( value );
            settings.wifi.ssid = ( str );
        };

        connect.onClick = [&]() {
            connectWifi( connect.getToggleState() );
        };

        initializeInterfaceState();
    }

    void SettingsWifi::connectWifi( bool Connect )
    {
        if( Connect )
        {
            juce::String ssid = wifiList.getSelectedValue();
            if( ssid.isEmpty() )
                ssid = settings.wifi.ssid();
            juce::String pwd = settings.wifi.pwd();

            statusLabel.setText( "Connecting to " + ssid + "...", juce::dontSendNotification );

            juce::Thread::launch( [this, ssid, pwd]() {
                bool ok = mt::WifiManager().getWifiInterface()->connectToWiFi( ssid, pwd );
                juce::MessageManager::callAsync( [this, ok, ssid]() {
                    if( ok )
                    {
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
                        statusLabel.setText( "Failed to connect to " + ssid, juce::dontSendNotification );
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
    }

    void SettingsWifi::initializeInterfaceState()
    {
        bool isWifiEnabled = settings.wifi.enabled();
        enabled.setToggleState( isWifiEnabled, juce::dontSendNotification );
        password.setText( settings.wifi.pwd(), juce::dontSendNotification );

        if( isWifiEnabled )
        {
            mt::WifiManager().enableWiFi();
            statusLabel.setText( "WiFi Ready", juce::dontSendNotification );
        }
        else
        {
            statusLabel.setText( "WiFi Disabled", juce::dontSendNotification );
        }
    }

    void SettingsWifi::setupHeader( kick_ui::KickModeHeader& Hdr )
    {
        Hdr.setTitle( "Preferences - Wi-Fi" );
    }

    void SettingsWifi::handleCommand( CPktMgr::HWMsg_t& ) {}

}
