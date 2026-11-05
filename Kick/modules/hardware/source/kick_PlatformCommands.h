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

#include <string>
#include <vector>
#include <array>
#include <sstream>
#include <iomanip>
#include <algorithm>
#include <cstdint>
#include <cctype>

namespace mt
{
namespace platform_cmd
{
    // ------------------------------------------------------------------------
    // Validation & Sanitization Helpers
    // ------------------------------------------------------------------------

    // Validates a timezone identifier (e.g. "UTC", "Europe/Budapest", "America/New_York", "Etc/GMT+1")
    // Rejects any shell metacharacters, spaces, control characters, or newlines.
    inline bool validateTimezone( const std::string& tz )
    {
        if( tz.empty() || tz.length() > 64 )
            return false;

        for( char c : tz )
        {
            if( !std::isalnum( static_cast<unsigned char>( c ) ) &&
                c != '/' && c != '_' && c != '-' && c != '+' )
            {
                return false;
            }
        }
        return true;
    }

    // Validates standard IPv4 dotted-decimal address (e.g. "192.168.0.1")
    inline bool validateIpAddress( const std::string& ip )
    {
        if( ip.empty() || ip.length() > 15 )
            return false;

        int parts = 0;
        int currentVal = 0;
        bool hasDigits = false;

        for( size_t i = 0; i < ip.length(); ++i )
        {
            char c = ip[i];
            if( std::isdigit( static_cast<unsigned char>( c ) ) )
            {
                currentVal = currentVal * 10 + ( c - '0' );
                if( currentVal > 255 )
                    return false;
                hasDigits = true;
            }
            else if( c == '.' )
            {
                if( !hasDigits )
                    return false;
                parts++;
                if( parts > 3 )
                    return false;
                currentVal = 0;
                hasDigits = false;
            }
            else
            {
                return false;
            }
        }
        return ( parts == 3 && hasDigits );
    }

    // Validates standard IPv4 subnet mask (e.g. "255.255.255.0")
    inline bool validateSubnetMask( const std::string& mask )
    {
        if( !validateIpAddress( mask ) )
            return false;

        // Parse into 32-bit integer
        uint32_t val = 0;
        std::istringstream iss( mask );
        std::string token;
        while( std::getline( iss, token, '.' ) )
        {
            val = ( val << 8 ) | static_cast<uint32_t>( std::stoi( token ) );
        }

        // A valid mask has contiguous 1s followed by contiguous 0s
        if( val == 0 )
            return false;
        uint32_t inverted = ~val;
        return ( ( inverted & ( inverted + 1 ) ) == 0 );
    }

    // Strips problematic characters and escapes quotes in Wi-Fi SSID
    inline std::string sanitizeWifiSsid( const std::string& ssid )
    {
        std::string out;
        out.reserve( ssid.length() );
        for( char c : ssid )
        {
            // Drop control characters and newlines
            if( c == '\r' || c == '\n' || c == '\0' )
                continue;
            if( c == '"' || c == '\\' || c == '`' || c == '$' || c == '(' || c == ')' || c == ';' || c == '&' || c == '|' )
                out.push_back( '\\' );
            out.push_back( c );
        }
        return out;
    }

    // Strips newlines and escapes quotes in Wi-Fi Password
    inline std::string sanitizeWifiPassword( const std::string& psk )
    {
        std::string out;
        out.reserve( psk.length() );
        for( char c : psk )
        {
            if( c == '\r' || c == '\n' || c == '\0' )
                continue;
            if( c == '"' || c == '\\' || c == '`' || c == '$' || c == '(' || c == ')' || c == ';' || c == '&' || c == '|' )
                out.push_back( '\\' );
            out.push_back( c );
        }
        return out;
    }

    // ------------------------------------------------------------------------
    // Command Construction
    // ------------------------------------------------------------------------

    // Formats: date -s "YYYY-MM-DD"
    inline std::string formatDateCommand( int year, int month, int day )
    {
        year = std::max( 2000, std::min( 2099, year ) );
        month = std::max( 1, std::min( 12, month ) );
        day = std::max( 1, std::min( 31, day ) );

        std::ostringstream oss;
        oss << "date -s \""
            << std::setfill( '0' ) << std::setw( 4 ) << year << "-"
            << std::setfill( '0' ) << std::setw( 2 ) << month << "-"
            << std::setfill( '0' ) << std::setw( 2 ) << day
            << "\"";
        return oss.str();
    }

    // Formats: date -s "HH:MM:SS"
    inline std::string formatTimeCommand( int hour, int minute, int second = 0 )
    {
        hour = std::max( 0, std::min( 23, hour ) );
        minute = std::max( 0, std::min( 59, minute ) );
        second = std::max( 0, std::min( 59, second ) );

        std::ostringstream oss;
        oss << "date -s \""
            << std::setfill( '0' ) << std::setw( 2 ) << hour << ":"
            << std::setfill( '0' ) << std::setw( 2 ) << minute << ":"
            << std::setfill( '0' ) << std::setw( 2 ) << second
            << "\"";
        return oss.str();
    }

    // Formats: date -s "YYYY-MM-DD HH:MM:SS"
    inline std::string formatDateTimeCommand( int year, int month, int day, int hour, int minute, int second = 0 )
    {
        year = std::max( 2000, std::min( 2099, year ) );
        month = std::max( 1, std::min( 12, month ) );
        day = std::max( 1, std::min( 31, day ) );
        hour = std::max( 0, std::min( 23, hour ) );
        minute = std::max( 0, std::min( 59, minute ) );
        second = std::max( 0, std::min( 59, second ) );

        std::ostringstream oss;
        oss << "date -s \""
            << std::setfill( '0' ) << std::setw( 4 ) << year << "-"
            << std::setfill( '0' ) << std::setw( 2 ) << month << "-"
            << std::setfill( '0' ) << std::setw( 2 ) << day << " "
            << std::setfill( '0' ) << std::setw( 2 ) << hour << ":"
            << std::setfill( '0' ) << std::setw( 2 ) << minute << ":"
            << std::setfill( '0' ) << std::setw( 2 ) << second
            << "\"";
        return oss.str();
    }

    // Formats: timedatectl set-timezone '<tz>' (only if valid)
    inline std::string formatSetTimezoneCommand( const std::string& tz )
    {
        if( !validateTimezone( tz ) )
            return "";
        return "timedatectl set-timezone " + tz;
    }

    // Extracts timezone name from a zoneinfo symlink target (e.g. "/var/db/timezone/zoneinfo/Europe/Budapest" -> "Europe/Budapest")
    inline std::string parseTimezoneFromZoneinfoLink( const std::string& target )
    {
        const std::string marker = "zoneinfo/";
        auto pos = target.rfind( marker );
        if( pos == std::string::npos )
            return "";
        std::string tz = target.substr( pos + marker.length() );
        if( validateTimezone( tz ) )
            return tz;
        return "";
    }

    // Parses timezone from GeoIP JSON response (e.g. {"status":"success","timezone":"Europe/Budapest"})
    inline std::string parseTimezoneFromGeoIpJson( const std::string& json )
    {
        auto pos = json.find( "\"timezone\"" );
        if( pos == std::string::npos )
            return "";
        auto colon = json.find( ':', pos );
        if( colon == std::string::npos )
            return "";
        auto startQuote = json.find( '"', colon );
        if( startQuote == std::string::npos )
            return "";
        auto endQuote = json.find( '"', startQuote + 1 );
        if( endQuote == std::string::npos )
            return "";
        std::string tz = json.substr( startQuote + 1, endQuote - startQuote - 1 );
        if( validateTimezone( tz ) )
            return tz;
        return "";
    }

    // Formats IP assignment and optional default gateway
    inline std::string formatSetIpCommand( const std::string& iface, const std::string& ip,
                                          const std::string& netmask, const std::string& gateway = "" )
    {
        if( iface.empty() || !validateIpAddress( ip ) || !validateSubnetMask( netmask ) )
            return "";

        std::string cmd = "ifconfig " + iface + " " + ip + " netmask " + netmask;
        if( !gateway.empty() && validateIpAddress( gateway ) )
        {
            cmd += " && ip route replace default via " + gateway + " dev " + iface;
        }
        return cmd;
    }

    // Scales brightness percent (0..100) to hardware value range (e.g. 20..255)
    inline int formatBrightnessPercentToValue( int percent, int minVal = 20, int maxVal = 255 )
    {
        percent = std::max( 0, std::min( 100, percent ) );
        if( percent <= 0 )
            return minVal;
        return minVal + ( ( percent * ( maxVal - minVal ) ) / 100 );
    }

    // Scales hardware value range to brightness percent (0..100)
    inline int formatBrightnessValueToPercent( int value, int minVal = 20, int maxVal = 255 )
    {
        if( value <= minVal )
            return 0;
        if( value >= maxVal )
            return 100;
        return ( ( value - minVal ) * 100 ) / ( maxVal - minVal );
    }

    // Generates safe wpa_supplicant.conf network block with escaped credentials
    inline std::string formatWifiWpaSupplicantConfig( const std::string& ssid, const std::string& psk )
    {
        std::string safeSsid = sanitizeWifiSsid( ssid );
        std::string safePsk = sanitizeWifiPassword( psk );

        std::ostringstream oss;
        oss << "ctrl_interface=/var/run/wpa_supplicant\n"
            << "update_config=1\n\n"
            << "network={\n"
            << "    ssid=\"" << safeSsid << "\"\n";

        if( safePsk.empty() )
        {
            oss << "    key_mgmt=NONE\n";
        }
        else
        {
            oss << "    psk=\"" << safePsk << "\"\n"
                << "    key_mgmt=WPA-PSK\n";
        }
        oss << "}\n";
        return oss.str();
    }

    // Generates iwd / iwctl connect command
    inline std::string formatIwdConnectCommand( const std::string& iface, const std::string& ssid, const std::string& passphrase )
    {
        std::string safeSsid = sanitizeWifiSsid( ssid );
        std::string safePass = sanitizeWifiPassword( passphrase );

        std::ostringstream oss;
        if( safePass.empty() )
        {
            oss << "iwctl station " << iface << " connect \"" << safeSsid << "\"";
        }
        else
        {
            oss << "iwctl --passphrase \"" << safePass << "\" station " << iface << " connect \"" << safeSsid << "\"";
        }
        return oss.str();
    }

    // ------------------------------------------------------------------------
    // Factory Reset Commands
    // ------------------------------------------------------------------------

    inline std::string formatMpcFactoryResetCommand()
    {
        return "rm -rf /data/prefs /data/projects /var/lib/iwd/*.psk /data/kick-boot.log* && sync";
    }

    inline std::string formatBehringerFactoryResetCommand()
    {
        return "/bin/rm -rf /home/Kick/projects* /home/Kick/prefs* /home/Kick/wifi_credentials.txt /home/Kick/wpa_supplicant.conf && sync";
    }

    // ------------------------------------------------------------------------
    // VERSION File Parsing (Akai MPC pendrive / local flash format)
    // ------------------------------------------------------------------------

    struct ParsedVersionInfo
    {
        std::string version = "0.0.0";
        std::string commit = "unknown";
        std::string buildDate = "";
        std::string sha256 = "";
        bool isValid = false;
    };

    inline ParsedVersionInfo parseVersionContent( const std::string& content )
    {
        ParsedVersionInfo info;
        std::istringstream iss( content );
        std::string line;

        while( std::getline( iss, line ) )
        {
            // Trim carriage return if present
            if( !line.empty() && line.back() == '\r' )
                line.pop_back();

            size_t eqPos = line.find( '=' );
            if( eqPos != std::string::npos )
            {
                std::string key = line.substr( 0, eqPos );
                std::string val = line.substr( eqPos + 1 );

                if( key == "VERSION" )
                {
                    info.version = val;
                    info.isValid = true;
                }
                else if( key == "GIT_COMMIT" )
                {
                    info.commit = val;
                }
                else if( key == "BUILD_DATE" )
                {
                    info.buildDate = val;
                }
                else if( key == "KICK_SHA256" )
                {
                    info.sha256 = val;
                }
            }
        }
        return info;
    }

    // ------------------------------------------------------------------------
    // CV Port & Channel Mapping
    // ------------------------------------------------------------------------

    // Physical DAC channel mapping table:
    // Port 1 -> DAC 4, Port 2 -> DAC 5, Port 3 -> DAC 6, Port 4 -> DAC 7,
    // Port 5 -> DAC 0, Port 6 -> DAC 1, Port 7 -> DAC 2, Port 8 -> DAC 3
    constexpr std::array<int, 9> kCvPortToDacMap = { -1, 4, 5, 6, 7, 0, 1, 2, 3 };

    // Maps 0-based (0..7) or 1-based (1..8) port index to DAC channel (0..7). Returns -1 on invalid.
    inline int mapCvPortToDacChannel( int port )
    {
        if( port >= 1 && port <= 8 )
            return kCvPortToDacMap[port];
        if( port >= 0 && port < 8 )
            return kCvPortToDacMap[port + 1];
        return -1;
    }

    // Computes 61-point DAC lookup table (5 octaves, 12 notes per octave + note 60 = 4095)
    // from 6 voltage points: 0V (0), 1V, 2V, 3V, 4V, 5V (4095).
    inline void computeCvCalibrationTable( const std::array<uint16_t, 6>& calibrationPoints,
                                           std::array<uint16_t, 61>& dacTable )
    {
        int arrayIndex = 0;
        for( int i = 0; i < 5; ++i )
        {
            float stepSize = static_cast<float>( calibrationPoints[i + 1] - calibrationPoints[i] ) / 12.0f;
            for( int j = 0; j < 12; ++j )
            {
                dacTable[arrayIndex++] = static_cast<uint16_t>( calibrationPoints[i] + static_cast<uint16_t>( stepSize * j ) );
            }
        }
        dacTable[60] = 4095;
    }

    // Default calibration values for 0V, 1V, 2V, 3V, 4V, 5V
    constexpr std::array<uint16_t, 6> kDefaultCvCalibration = { 0, 820, 1640, 2460, 3280, 4095 };

} // namespace platform_cmd
} // namespace mt
