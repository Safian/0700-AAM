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

    // Converts hex string (e.g. "53616669616e") to ASCII ("Safian")
    inline std::string hexToString( const std::string& hex )
    {
        if( hex.empty() || ( hex.length() % 2 ) != 0 )
            return "";
        std::string result;
        result.reserve( hex.length() / 2 );
        for( size_t i = 0; i < hex.length(); i += 2 )
        {
            char h1 = hex[i];
            char h2 = hex[i + 1];
            if( !std::isxdigit( static_cast<unsigned char>( h1 ) ) || !std::isxdigit( static_cast<unsigned char>( h2 ) ) )
                return "";
            int v1 = ( h1 >= 'a' && h1 <= 'f' ) ? ( h1 - 'a' + 10 ) : ( h1 >= 'A' && h1 <= 'F' ) ? ( h1 - 'A' + 10 ) : ( h1 - '0' );
            int v2 = ( h2 >= 'a' && h2 <= 'f' ) ? ( h2 - 'a' + 10 ) : ( h2 >= 'A' && h2 <= 'F' ) ? ( h2 - 'A' + 10 ) : ( h2 - '0' );
            result.push_back( static_cast<char>( ( v1 << 4 ) | v2 ) );
        }
        return result;
    }

    // Converts ASCII string ("Safian") to lowercase hex ("53616669616e")
    inline std::string stringToHex( const std::string& str )
    {
        static const char hexChars[] = "0123456789abcdef";
        std::string result;
        result.reserve( str.length() * 2 );
        for( unsigned char c : str )
        {
            result.push_back( hexChars[( c >> 4 ) & 0x0f] );
            result.push_back( hexChars[c & 0x0f] );
        }
        return result;
    }

    /**
     * Strips control/newline characters for INI config values without adding shell backslashes.
     */
    inline std::string sanitizeIniValue( const std::string& val )
    {
        std::string out;
        out.reserve( val.length() );
        for( char c : val )
        {
            if( c != '\r' && c != '\n' && c != '\0' )
                out.push_back( c );
        }
        return out;
    }

    /**
     * Splits busctl ConnMan Manager GetServices output into individual Wi-Fi service blocks.
     * Splitting at every "/net/connman/service/" boundary ensures an adjacent ethernet or
     * bluetooth service block never bleeds into a preceding wifi block.
     */
    inline std::vector<std::string> splitBusctlConnmanWifiBlocks( const std::string& text )
    {
        std::vector<std::string> blocks;
        const std::string servicePrefix = "/net/connman/service/";
        const std::string wifiPrefix    = "/net/connman/service/wifi_";
        size_t pos = 0;
        while( ( pos = text.find( servicePrefix, pos ) ) != std::string::npos )
        {
            size_t nextPos = text.find( servicePrefix, pos + servicePrefix.size() );
            std::string block = ( nextPos == std::string::npos ) ? text.substr( pos ) : text.substr( pos, nextPos - pos );
            if( block.compare( 0, wifiPrefix.size(), wifiPrefix ) == 0 )
                blocks.push_back( std::move( block ) );
            pos = ( nextPos == std::string::npos ) ? text.size() : nextPos;
        }
        return blocks;
    }

    /**
     * Extracts a string property formatted as `"Key" s "Value"` from a busctl service block.
     */
    inline std::string extractBusctlStringProperty( const std::string& block, const std::string& keyName )
    {
        const std::string key = "\"" + keyName + "\" s \"";
        size_t pos = block.find( key );
        if( pos == std::string::npos )
            return "";
        size_t start = pos + key.size();
        size_t end   = block.find( '"', start );
        if( end == std::string::npos )
            return "";
        return block.substr( start, end - start );
    }

    /**
     * Parses unique Wi-Fi SSIDs from busctl ConnMan Manager GetServices output.
     */
    inline std::vector<std::string> parseBusctlConnmanServices( const std::string& text )
    {
        std::vector<std::string> list;
        for( const auto& block : splitBusctlConnmanWifiBlocks( text ) )
        {
            std::string ssid = extractBusctlStringProperty( block, "Name" );
            if( !ssid.empty() && std::find( list.begin(), list.end(), ssid ) == list.end() )
                list.push_back( std::move( ssid ) );
        }
        return list;
    }

    /**
     * Parses currently connected Wi-Fi SSID from busctl ConnMan Manager GetServices output.
     */
    inline std::string parseBusctlConnectedService( const std::string& text )
    {
        for( const auto& block : splitBusctlConnmanWifiBlocks( text ) )
        {
            std::string state = extractBusctlStringProperty( block, "State" );
            if( state == "online" || state == "ready" )
            {
                std::string ssid = extractBusctlStringProperty( block, "Name" );
                if( !ssid.empty() )
                    return ssid;
            }
        }
        return "";
    }

    /**
     * Finds the ConnMan Wi-Fi service object path for a given SSID in busctl GetServices output.
     */
    inline std::string parseBusctlServicePathForSsid( const std::string& text, const std::string& ssid )
    {
        if( text.empty() || ssid.empty() )
            return "";

        for( const auto& block : splitBusctlConnmanWifiBlocks( text ) )
        {
            if( extractBusctlStringProperty( block, "Name" ) == ssid )
            {
                size_t endPath = block.find( '"' );
                if( endPath != std::string::npos )
                    return block.substr( 0, endPath );
            }
        }
        return "";
    }

    /**
     * Resolves the iwd Station D-Bus object path (e.g. "/net/connman/iwd/0/4") from `busctl tree --list net.connman.iwd`.
     */
    inline std::string parseIwdStationPath( const std::string& treeOutput )
    {
        const std::string prefix = "/net/connman/iwd/";
        std::istringstream iss( treeOutput );
        std::string line;
        auto isDigits = []( const std::string& s ) {
            if( s.empty() ) return false;
            for( unsigned char c : s )
                if( !std::isdigit( c ) ) return false;
            return true;
        };
        while( std::getline( iss, line ) )
        {
            size_t start = line.find( prefix );
            if( start == std::string::npos )
                continue;
            // Trim trailing CR/whitespace
            while( !line.empty() && ( line.back() == '\r' || line.back() == ' ' || line.back() == '\t' ) )
                line.pop_back();
            std::string path = line.substr( start );
            std::string tail = path.substr( prefix.size() );
            size_t slash1 = tail.find( '/' );
            if( slash1 == std::string::npos )
                continue;
            std::string phy = tail.substr( 0, slash1 );
            std::string rest = tail.substr( slash1 + 1 );
            if( isDigits( phy ) && isDigits( rest ) )
                return path;
        }
        return "";
    }

    /**
     * Formats an iwd provisioning filename under `/var/lib/iwd/`, using `=<hex>` encoding
     * whenever the SSID contains unsafe characters or path separators.
     */
    inline std::string formatIwdNetworkFilename( const std::string& ssid, bool hasPassphrase )
    {
        bool needsHex = ssid.empty() || ssid.front() == '.';
        if( !needsHex )
        {
            for( unsigned char c : ssid )
            {
                if( !std::isalnum( c ) && c != '-' && c != '_' && c != ' ' && c != '.' )
                {
                    needsHex = true;
                    break;
                }
            }
        }
        std::string stem = needsHex ? ( "=" + stringToHex( ssid ) ) : ssid;
        return "/var/lib/iwd/" + stem + ( hasPassphrase ? ".psk" : ".open" );
    }

    /**
     * Generates ConnMan provisioning `.config` file content using INI-safe raw values.
     */
    inline std::string formatConnmanConfig( const std::string& ssid, const std::string& passphrase )
    {
        std::string iniSsid = sanitizeIniValue( ssid );
        std::string iniPass = sanitizeIniValue( passphrase );
        std::string hexSsid = stringToHex( iniSsid );

        std::ostringstream oss;
        oss << "[global]\n"
            << "Name = wifi_" << hexSsid << "\n\n"
            << "[service_wifi_" << hexSsid << "]\n"
            << "Type = wifi\n"
            << "Name = " << iniSsid << "\n";
        if( !iniPass.empty() )
            oss << "Passphrase = " << iniPass << "\n";
        oss << "AutoConnect = true\n";
        return oss.str();
    }

    /**
     * Generates iwd provisioning `.psk` file content using INI-safe raw passphrase.
     */
    inline std::string formatIwdPskConfig( const std::string& passphrase )
    {
        std::ostringstream oss;
        oss << "[Security]\n"
            << "Passphrase=" << sanitizeIniValue( passphrase ) << "\n\n"
            << "[Settings]\n"
            << "AutoConnect=true\n";
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
