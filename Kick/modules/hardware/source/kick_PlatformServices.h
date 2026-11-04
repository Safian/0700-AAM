/*
 * Copyright (c) 2024 MUSIC Tribe / Safian
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

#include "kick_Hwpref.h"
#include "kick_PlatformCommands.h"
#include <juce_core/juce_core.h>
#include <memory>
#include <vector>
#include <string>

namespace mt
{
    enum class PlatformType
    {
        Desktop,
        Behringer,
        AkaiMpc
    };

    class PlatformServices
    {
    public:
        virtual ~PlatformServices() = default;

        virtual PlatformType getPlatformType() const = 0;
        virtual const char* getPlatformName() const = 0;
        virtual bool isHardwareControlAvailable() const = 0;
        virtual juce::String getHardwareUnavailableReason() const = 0;

        // Display brightness (0..100)
        virtual int setBrightness( int percent ) = 0;
        virtual int getBrightness() = 0;

        // Date, time, timezone
        virtual int setDateTime( int day, int month, int year, int hour, int minute ) = 0;
        virtual int getDateTime( dateInfo& date, timeInfo& time, const juce::String& timezone ) = 0;
        virtual int setTimeZone( const juce::String& tz ) = 0;
        virtual juce::String getTimeZone() = 0;
        virtual int getDaysInMonth( int month, int year ) = 0;

        // Ethernet
        virtual std::string getEthernetInterfaceName() const = 0;
        virtual bool isEthernetCableConnected() = 0;
        virtual bool isEthernetInterfaceUp() = 0;
        virtual bool ethernetEnable( bool enable ) = 0;
        virtual void getEthernetIP( sysEthInfo& info ) = 0;
        virtual int setEthernetIP( const std::string& ipAddress, const std::string& subnetMask, const std::string& gateway, const std::string& dns ) = 0;
        virtual int startDhcp() = 0;
        virtual int stopDhcp() = 0;

        // WiFi
        virtual std::string getWiFiInterfaceName() const = 0;
        virtual bool isWiFiAvailable() const = 0;
        virtual bool enableWiFi() = 0;
        virtual bool disableWiFi() = 0;
        virtual std::vector<juce::String> scanWiFiNetworks() = 0;
        virtual bool connectToWiFi( const juce::String& ssid, const juce::String& password ) = 0;
        virtual bool disconnectWiFi() = 0;
        virtual void getWiFiIP( sysEthInfo& info ) = 0;

        // System actions
        virtual int factoryReset() = 0;
        virtual int reboot() = 0;
        virtual int powerOff() = 0;

        // Info & versions
        virtual DeviceInfo getDeviceInfo() = 0;
        virtual juce::String getFirmwareVersion() = 0;
        virtual juce::String getControlSurfaceVersion() = 0;
        virtual juce::String getBuildStamp() = 0;
        virtual juce::String getDevSerial() = 0;
        virtual void setControlSurfaceVersion( const juce::String& ver ) = 0;

        static PlatformServices& getInstance();
        static PlatformType detectPlatform();
        static void setPlatformOverride( PlatformType type );
    };
}
