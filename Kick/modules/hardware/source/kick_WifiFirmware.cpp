#include "kick_HardwareMode.h"
#include "kick_PlatformServices.h"
/*
 * Copyright (c) 2021 MUSIC Tribe
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

#if( JUCE_LINUX || __linux__ )
    #include <iostream>
    #include <fcntl.h>
    #include <linux/i2c-dev.h>
    #include <sys/ioctl.h>
    #include <unistd.h>
#endif

#define WIFI_CRED_FILE "/home/Kick/wifi_credentials.txt"
#define WIFI_CONF_FILE "/home/Kick/wpa_supplicant.conf"

namespace mt
{
    WifiManager::WifiManager()
    {
    }

    WifiManager::~WifiManager()
    {
    }

    juce::String WifiManager::executeCommand( const juce::String& command )
    {
        if (!hardwareEnabled()) return {};
        juce::ChildProcess process;
        process.start( command );
        process.waitForProcessToFinish( -1 );
        return process.readAllProcessOutput();
    }

    bool WifiManager::fileExists( const std::string& filename )
    {
        return juce::File(juce::String(filename)).existsAsFile();
    }

    std::map<std::string, std::string> WifiManager::loadWifiCredentials()
    {
        std::ifstream                      file( WIFI_CRED_FILE );
        std::map<std::string, std::string> wifiMap;
        std::string                        line;

        while( std::getline( file, line ) )
        {
            std::stringstream ss( line );
            std::string       ssid, password;
            if( std::getline( ss, ssid, ':' ) && std::getline( ss, password ) )
            {
                wifiMap[ssid] = password;
            }
        }
        return wifiMap;
    }

    void WifiManager::saveWifiCredential( const std::string& ssid, const std::string& password )
    {
        if (!hardwareEnabled()) return;
        if( !fileExists( WIFI_CRED_FILE ) )
        {
            std::ofstream file( WIFI_CRED_FILE );
            file.close();
        }

        std::ofstream file( WIFI_CRED_FILE, std::ios::app );
        file << ssid << ":" << password << std::endl;
    }

    const std::vector<juce::String> WifiManager::scanWiFiNetworks()
    {
        return PlatformServices::getInstance().scanWiFiNetworks();
    }

    bool WifiManager::enableWiFi()
    {
        return PlatformServices::getInstance().enableWiFi();
    }

    bool WifiManager::disableWiFi()
    {
        return PlatformServices::getInstance().disableWiFi();
    }

    void WifiManager::displayWiFiNetworks( const std::vector<juce::String>& Networks )
    {
        for( const auto& network : Networks )
        {
            std::cout << "Network: " << network << std::endl;
        }
    }

    bool WifiManager::connectToWiFi( const juce::String& ssid, const juce::String& password, int priority )
    {
        juce::ignoreUnused( priority );
        return PlatformServices::getInstance().connectToWiFi( ssid, password );
    }

    bool WifiManager::disconnectWiFi()
    {
        return PlatformServices::getInstance().disconnectWiFi();
    }

    void WifiManager::addSSID( const juce::String& ssid, const juce::String& password, int priority )
    {
        if (!hardwareEnabled()) return;
        if( !fileExists( WIFI_CONF_FILE ) )
        {
            std::ofstream file( WIFI_CONF_FILE );
            file.close();
        }
        juce::File configFile( WIFI_CONF_FILE );
        if( configFile.existsAsFile() )
        {
            juce::String networkConfig =
                "\nnetwork={\n"
                "    ssid=\""
                + ssid + "\"\n"
                         "    psk=\""
                + password + "\"\n"
                             "    key_mgmt=WPA-PSK  "
                + "\n"
                  "}\n\n";

            configFile.replaceWithText( networkConfig );
        }
        else
        {
            std::cerr << "Error: wpa_supplicant.conf file not found or not accessible." << std::endl;
        }
    }
}
