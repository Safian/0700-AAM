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

#include "kick_ui_settings_etnernet.h"
#include "../modules/hardware/source/kick_Hwpref.h"

namespace kick_ui
{

    ///////////////////////////////////////////////////////////////////////////////////////////////////////
    ////////////////////////////  SettingsEthernet
    ///////////////////////////////////////////////////////////////////////////////////////////////////////

    SettingsEthernet::SettingsEthernet()
        : SettingsInterface( 6, 3, true )
    {
        ui::KickGrid::setBackgroundColour( juce::Colours::transparentBlack );
        setRowTitles( { "ETHERNET SETTINGS", "IP ADDRESS", "SUBNET MASK", "GATEWAY", "DNS SERVER" } );

        addComponent( enabled, 0, 0 );
        addComponent( method, 0, 1 );
        addComponent( statusLabel, 0, 2 );

        statusLabel.setValueJustification( juce::Justification::centred );
        statusLabel.setValueFontHeight( 16 );

        addComponent( ipAddr, 1, 0, 2 );
        addComponent( subnet, 2, 0, 2 );
        addComponent( gateway, 3, 0, 2 );
        addComponent( dns, 4, 0, 2 );

        enabled.onClick = [&]() {
            handleEnabledClick();
        };

        method.onClick = [&]() {
            handleMethodClick();
        };

        ipAddr.onTextChange = [&]() {
            if( settings.eth.mode() == "MANUAL" )
            {
                settings.eth.ipAddress = ( ipAddr.getTextValue().toString() );
                update();
            }
        };

        gateway.onTextChange = [&]() {
            if( settings.eth.mode() == "MANUAL" )
            {
                settings.eth.gateway = ( gateway.getTextValue().toString() );
                update();
            }
        };

        subnet.onTextChange = [&]() {
            if( settings.eth.mode() == "MANUAL" )
            {
                settings.eth.subnetMask = ( subnet.getTextValue().toString() );
                update();
            }
        };

        dns.onTextChange = [&]() {
            if( settings.eth.mode() == "MANUAL" )
            {
                settings.eth.dns = ( dns.getTextValue().toString() );
                update();
            }
        };

        initializeInterfaceState();
    }

    void SettingsEthernet::handleEnabledClick()
    {
        bool isEthEnabled = enabled.getToggleState();
        settings.eth.enabled = isEthEnabled;
        bool isDhcp = ( settings.eth.mode() == "DHCP" );

        setComponentsEnabled( isEthEnabled && !isDhcp );
        method.setEnabled( isEthEnabled );
        mt::HwPref::getHwPref().ethernetEnable( isEthEnabled );

        if( isEthEnabled )
        {
            if( isDhcp )
                mt::HwPref::getHwPref().startDhcp();
            else
            {
                mt::HwPref::getHwPref().stopDhcp();
                update();
            }
        }
        else
        {
            mt::HwPref::getHwPref().stopDhcp();
        }
    }

    void SettingsEthernet::handleMethodClick()
    {
        // Toggle state ON means DHCP; OFF means MANUAL
        bool isDhcp = method.getToggleState();
        settings.eth.mode = ( isDhcp ? "DHCP" : "MANUAL" );

        if( isDhcp )
        {
            setComponentsEnabled( false );
            mt::HwPref::getHwPref().startDhcp();
        }
        else
        {
            setComponentsEnabled( enabled.getToggleState() );
            mt::HwPref::getHwPref().stopDhcp();
            update();
        }
    }

    void SettingsEthernet::setComponentsEnabled( bool Enabled )
    {
        ipAddr.setEditable( Enabled );
        gateway.setEditable( Enabled );
        subnet.setEditable( Enabled );
        dns.setEditable( Enabled );
    }

    void SettingsEthernet::refresh()
    {
        bool ifUp = mt::HwPref::getHwPref().isInterfaceUp();
        bool isEthEnabled = settings.eth.enabled();
        bool isDhcp = ( settings.eth.mode() == "DHCP" );

        if( !isEthEnabled )
        {
            statusLabel.setText( "Disabled", juce::dontSendNotification );
        }
        else if( !ifUp )
        {
            statusLabel.setText( "Link Down", juce::dontSendNotification );
        }
        else
        {
            mt::sysEthInfo info;
            mt::HwPref::getHwPref().getIP( "eth0", info );
            if( info.IP.empty() || info.IP == "0.0.0.0" )
            {
                statusLabel.setText( isDhcp ? "Acquiring IP..." : "No IP", juce::dontSendNotification );
            }
            else
            {
                statusLabel.setText( juce::String( "Online (" ) + ( isDhcp ? "DHCP" : "Manual" ) + ")", juce::dontSendNotification );
            }

            if( isDhcp )
            {
                ipAddr.setText( info.IP, juce::dontSendNotification );
                subnet.setText( info.subnet, juce::dontSendNotification );
                gateway.setText( info.gateway, juce::dontSendNotification );
            }
        }
    }

    void SettingsEthernet::initializeInterfaceState()
    {
        bool isEthEnabled = settings.eth.enabled();
        enabled.setToggleState( isEthEnabled, juce::dontSendNotification );
        method.setEnabled( isEthEnabled );

        juce::String mode = settings.eth.mode();
        bool isDhcp = ( mode == "DHCP" );
        method.setToggleState( isDhcp, juce::dontSendNotification );
        setComponentsEnabled( isEthEnabled && !isDhcp );

        ipAddr.setText( settings.eth.ipAddress(), juce::dontSendNotification );
        subnet.setText( settings.eth.subnetMask(), juce::dontSendNotification );
        gateway.setText( settings.eth.gateway(), juce::dontSendNotification );
        dns.setText( settings.eth.dns(), juce::dontSendNotification );

        mt::HwPref::getHwPref().ethernetEnable( isEthEnabled );
        if( isEthEnabled )
        {
            if( isDhcp )
            {
                mt::HwPref::getHwPref().startDhcp();
            }
            else
            {
                mt::HwPref::getHwPref().stopDhcp();
                update();
            }
        }
    }

    void SettingsEthernet::update()
    {
        if( !settings.eth.enabled() || settings.eth.mode() == "DHCP" )
            return;

        juce::String ip   = settings.eth.ipAddress();
        juce::String sub  = settings.eth.subnetMask();
        juce::String gate = settings.eth.gateway();
        juce::String dnsServer = settings.eth.dns();
        mt::HwPref::getHwPref().setIP( "eth0", ip.toStdString(), sub.toStdString(), gate.toStdString(), dnsServer.toStdString() );
    }

    void SettingsEthernet::setupHeader( kick_ui::KickModeHeader& Hdr )
    {
        Hdr.setTitle( "Preferences - Ethernet" );
    }

    void SettingsEthernet::handleCommand( CPktMgr::HWMsg_t& ) {}

}
