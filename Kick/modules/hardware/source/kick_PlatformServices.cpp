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

#include "kick_PlatformServices.h"
#include "kick_HardwareMode.h"
#include "../../common/source/kick_logger.h"
#include <fstream>
#include <sstream>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <mutex>
#include <atomic>
#include <sys/stat.h>

#if( JUCE_LINUX || __linux__ )
    #include <sys/ioctl.h>
    #include <netinet/in.h>
    #include <arpa/inet.h>
    #include <net/if.h>
    #include <unistd.h>
#endif

namespace mt
{

static std::string runPipedCommand( const std::string& cmd )
{
    std::string out;
#if( JUCE_LINUX || __linux__ || JUCE_MAC || __APPLE__ )
    FILE* pipe = popen( cmd.c_str(), "r" );
    if( !pipe )
        return "";

    char buf[256];
    while( fgets( buf, sizeof( buf ), pipe ) != nullptr )
    {
        out += buf;
    }
    pclose( pipe );
#else
    juce::ignoreUnused( cmd );
#endif
    return out;
}

static std::string trimString( const std::string& s )
{
    size_t start = 0;
    while( start < s.size() && ( s[start] == ' ' || s[start] == '\t' || s[start] == '\r' || s[start] == '\n' ) )
        ++start;
    if( start >= s.size() )
        return "";
    size_t end = s.size();
    while( end > start && ( s[end - 1] == ' ' || s[end - 1] == '\t' || s[end - 1] == '\r' || s[end - 1] == '\n' ) )
        --end;
    return s.substr( start, end - start );
}

static bool checkFileExists( const std::string& path )
{
    struct stat st;
    return ( ::stat( path.c_str(), &st ) == 0 );
}

// ----------------------------------------------------------------------------
// Base Implementation with Shared Linux Networking Helpers
// ----------------------------------------------------------------------------

class LinuxPlatformServicesBase : public PlatformServices
{
public:
    int getDaysInMonth( int month, int year ) override
    {
        if( month < 1 || month > 12 )
            return 31;

        static const int daysPerMonth[] = { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };
        int days = daysPerMonth[month - 1];

        // Leap year check for February
        if( month == 2 && ( ( year % 4 == 0 && year % 100 != 0 ) || ( year % 400 == 0 ) ) )
            days = 29;

        return days;
    }

    int getDateTime( dateInfo& date, timeInfo& time, const juce::String& timezone ) override
    {
        juce::ignoreUnused( timezone );
        std::time_t currentTime = std::time( nullptr );
        std::tm tmVal;
#if defined(_WIN32)
        localtime_s( &tmVal, &currentTime );
#else
        localtime_r( &currentTime, &tmVal );
#endif
        date.YYYY = tmVal.tm_year + 1900;
        date.MM   = tmVal.tm_mon + 1;
        date.DD   = tmVal.tm_mday;
        time.HH   = tmVal.tm_hour;
        time.MM   = tmVal.tm_min;
        return 1;
    }

    juce::String detectSystemTimezone() override
    {
        // 1. Try reading /etc/timezone
        std::ifstream tzFile( "/etc/timezone" );
        if( tzFile.is_open() )
        {
            std::string line;
            if( std::getline( tzFile, line ) )
            {
                std::string tz = trimString( line );
                if( platform_cmd::validateTimezone( tz ) )
                    return juce::String::fromUTF8( tz.c_str() );
            }
        }

        // 2. Try reading /etc/localtime symlink
#if !defined(_WIN32)
        char buf[1024] = { 0 };
        ssize_t len = ::readlink( "/etc/localtime", buf, sizeof( buf ) - 1 );
        if( len > 0 )
        {
            buf[len] = '\0';
            std::string parsed = platform_cmd::parseTimezoneFromZoneinfoLink( std::string( buf ) );
            if( platform_cmd::validateTimezone( parsed ) )
                return juce::String::fromUTF8( parsed.c_str() );
        }
#endif

        // 3. Fallback default
        return "Europe/Budapest";
    }

    void detectTimezoneAsync( std::function<void( const juce::String& )> onComplete ) override
    {
        juce::Thread::launch( [this, onComplete]() {
            // First check local system
            juce::String detected = detectSystemTimezone();

            // Try GeoIP lookup via HTTP (fast 2s timeout)
            juce::URL url( "http://ip-api.com/json/?fields=status,timezone" );
            auto opts = juce::URL::InputStreamOptions( juce::URL::ParameterHandling::inAddress )
                            .withConnectionTimeoutMs( 2000 )
                            .withNumRedirectsToFollow( 0 );
            std::unique_ptr<juce::InputStream> in = url.createInputStream( opts );
            if( in != nullptr )
            {
                juce::String json = in->readEntireStreamAsString();
                std::string parsed = platform_cmd::parseTimezoneFromGeoIpJson( json.toStdString() );
                if( platform_cmd::validateTimezone( parsed ) )
                    detected = juce::String::fromUTF8( parsed.c_str() );
            }

            juce::MessageManager::callAsync( [onComplete, detected]() {
                if( onComplete )
                    onComplete( detected );
            } );
        } );
    }

    juce::String getControlSurfaceVersion() override
    {
        std::lock_guard<std::mutex> lock( m_mutex );
        return m_surfaceVersion.isEmpty() ? "1.0" : m_surfaceVersion;
    }

    void setControlSurfaceVersion( const juce::String& ver ) override
    {
        std::lock_guard<std::mutex> lock( m_mutex );
        m_surfaceVersion = ver;
    }

protected:
    std::string querySubnetMask( const std::string& interface )
    {
        std::string subnetMask = "0.0.0.0";
#if( JUCE_LINUX || __linux__ )
        int sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
        if( sockfd < 0 )
            return subnetMask;

        struct ifreq ifr;
        std::memset( &ifr, 0, sizeof( ifr ) );
        ifr.ifr_addr.sa_family = AF_INET;
        std::strncpy( ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1 );

        if( ioctl( sockfd, SIOCGIFNETMASK, &ifr ) == 0 )
        {
            auto* netmask = reinterpret_cast<struct sockaddr_in*>( &ifr.ifr_netmask );
            const char* addrStr = inet_ntoa( netmask->sin_addr );
            if( addrStr )
                subnetMask = addrStr;
        }
        close( sockfd );
#else
        juce::ignoreUnused( interface );
#endif
        return subnetMask;
    }

    std::string queryDefaultGateway( const std::string& interface )
    {
#if( JUCE_LINUX || __linux__ )
        std::string routeOut = runPipedCommand( "ip route show" );
        std::istringstream iss( routeOut );
        std::string line;
        while( std::getline( iss, line ) )
        {
            if( line.find( "default via" ) != std::string::npos && line.find( interface ) != std::string::npos )
            {
                std::istringstream ls( line );
                std::string word;
                ls >> word; // skip "default"
                ls >> word; // skip "via"
                std::string gw;
                ls >> gw;
                return trimString( gw );
            }
        }
#else
        juce::ignoreUnused( interface );
#endif
        return "0.0.0.0";
    }

    std::string queryIpAddress( const std::string& interface )
    {
#if( JUCE_LINUX || __linux__ )
        int sockfd = socket( AF_INET, SOCK_DGRAM, 0 );
        if( sockfd >= 0 )
        {
            struct ifreq ifr;
            std::memset( &ifr, 0, sizeof( ifr ) );
            ifr.ifr_addr.sa_family = AF_INET;
            std::strncpy( ifr.ifr_name, interface.c_str(), IFNAMSIZ - 1 );
            if( ioctl( sockfd, SIOCGIFADDR, &ifr ) == 0 )
            {
                auto* addr = reinterpret_cast<struct sockaddr_in*>( &ifr.ifr_addr );
                const char* addrStr = inet_ntoa( addr->sin_addr );
                if( addrStr && std::string( addrStr ) != "0.0.0.0" )
                {
                    close( sockfd );
                    return addrStr;
                }
            }
            close( sockfd );
        }

        std::string cmd = "ip -4 addr show " + interface + " 2>/dev/null | grep -o 'inet [0-9.]*' | cut -d' ' -f2";
        std::string res = runPipedCommand( cmd );
        std::string ip = trimString( res );
        if( platform_cmd::validateIpAddress( ip ) )
            return ip;

        std::string ifcCmd = "ifconfig " + interface + " 2>/dev/null | grep -o 'inet [0-9.]*' | cut -d' ' -f2";
        std::string ifcRes = runPipedCommand( ifcCmd );
        std::string ifcIp = trimString( ifcRes );
        if( platform_cmd::validateIpAddress( ifcIp ) )
            return ifcIp;
#else
        juce::ignoreUnused( interface );
#endif
        return "0.0.0.0";
    }

    mutable std::mutex m_mutex;
    juce::String       m_surfaceVersion;
    int                m_brightness{ 100 };
    juce::String       m_timeZone{ "UTC" };
};

// ----------------------------------------------------------------------------
// Desktop Platform (macOS / Windows / non-hardware Linux)
// ----------------------------------------------------------------------------

class DesktopPlatformServices : public LinuxPlatformServicesBase
{
public:
    PlatformType getPlatformType() const override { return PlatformType::Desktop; }
    const char* getPlatformName() const override { return "Desktop / Simulator"; }
    bool isHardwareControlAvailable() const override { return false; }
    juce::String getHardwareUnavailableReason() const override
    {
        return "Hardware device control is disabled on Desktop / Simulator.";
    }

    int setBrightness( int percent ) override
    {
        m_brightness = std::max( 0, std::min( 100, percent ) );
        kick_log( "prefs", "[PREFS] Desktop setBrightness simulated: %d%%\n", m_brightness );
        return 1;
    }
    int getBrightness() override { return m_brightness; }

    int setDateTime( int day, int month, int year, int hour, int minute ) override
    {
        juce::ignoreUnused( day, month, year, hour, minute );
        kick_log( "prefs", "[PREFS] Desktop setDateTime no-op (host clock protected)\n" );
        return 0;
    }

    int setTimeZone( const juce::String& tz ) override
    {
        if( !platform_cmd::validateTimezone( tz.toStdString() ) )
            return 0;
        m_timeZone = tz;
        kick_log( "prefs", "[PREFS] Desktop setTimeZone stored: %s\n", tz.toRawUTF8() );
        return 1;
    }
    juce::String getTimeZone() override { return m_timeZone; }

    std::string getEthernetInterfaceName() const override { return "eth0"; }
    bool isEthernetCableConnected() override { return true; }
    bool isEthernetInterfaceUp() override { return true; }
    bool ethernetEnable( bool enable ) override
    {
        kick_log( "prefs", "[PREFS] Desktop ethernetEnable simulated: %d\n", (int)enable );
        return true;
    }

    void getEthernetIP( sysEthInfo& info ) override
    {
        info.status  = true;
        info.IP      = "127.0.0.1";
        info.subnet  = "255.0.0.0";
        info.gateway = "127.0.0.1";
    }

    int setEthernetIP( const std::string& ipAddress, const std::string& subnetMask, const std::string& gateway, const std::string& dns ) override
    {
        juce::ignoreUnused( ipAddress, subnetMask, gateway, dns );
        kick_log( "prefs", "[PREFS] Desktop setEthernetIP simulated: %s mask %s gw %s\n", ipAddress.c_str(), subnetMask.c_str(), gateway.c_str() );
        return 0;
    }

    int startDhcp() override { return 0; }
    int stopDhcp() override { return 0; }

    std::string getWiFiInterfaceName() const override { return "wlan0"; }
    bool isWiFiAvailable() const override { return false; }
    bool enableWiFi() override { return false; }
    bool disableWiFi() override { return false; }
    std::vector<juce::String> scanWiFiNetworks() override { return {}; }
    bool connectToWiFi( const juce::String& ssid, const juce::String& password ) override
    {
        juce::ignoreUnused( ssid, password );
        return false;
    }
    bool disconnectWiFi() override { return false; }
    void getWiFiIP( sysEthInfo& info ) override
    {
        info.status  = false;
        info.IP      = "0.0.0.0";
        info.subnet  = "0.0.0.0";
        info.gateway = "0.0.0.0";
    }

    int factoryReset() override
    {
        kick_log( "prefs", "[PREFS] Desktop factoryReset requested\n" );
        return 0;
    }
    // The desktop (and the tests / smoke) never power anything off: the request is only logged.
    int reboot() override
    {
        kick_log( "info", "[PREFS] Desktop reboot requested (nothing is done)\n" );
        return 0;
    }
    int powerOff() override
    {
        kick_log( "info", "[PREFS] Desktop power off requested (nothing is done)\n" );
        return 0;
    }

    DeviceInfo getDeviceInfo() override
    {
        DeviceInfo info;
        std::memset( &info, 0, sizeof( info ) );
        std::strncpy( info.serialNumber, "DESKTOP-SIM", sizeof( info.serialNumber ) - 1 );
        std::strncpy( info.firmwareVersion, "0.1.0-desktop", sizeof( info.firmwareVersion ) - 1 );
        juce::String buildStamp = juce::String( __DATE__ ) + " :: " + juce::String( __TIME__ );
        std::strncpy( info.firmwareBuildDate, buildStamp.toRawUTF8(), sizeof( info.firmwareBuildDate ) - 1 );
        return info;
    }

    juce::String getFirmwareVersion() override { return "0.1.0-desktop"; }
    juce::String getBuildStamp() override { return juce::String( __DATE__ ) + " :: " + juce::String( __TIME__ ); }
    juce::String getDevSerial() override { return "DESKTOP-SIM"; }
};

// ----------------------------------------------------------------------------
// Akai MPC Platform (MPC Key 61, etc.)
// ----------------------------------------------------------------------------

class AkaiMpcPlatformServices : public LinuxPlatformServicesBase
{
public:
    PlatformType getPlatformType() const override { return PlatformType::AkaiMpc; }
    const char* getPlatformName() const override { return "Akai MPC Key 61"; }

    // IMPORTANT: On Akai MPC, board commands work even when KICK_NO_HARDWARE=1
    // because Kick's hardware frontpanel emulation is bypassed for the MPC bridge.
    bool isHardwareControlAvailable() const override { return true; }
    juce::String getHardwareUnavailableReason() const override { return ""; }

    int setBrightness( int percent ) override
    {
        m_brightness = std::max( 0, std::min( 100, percent ) );
        int hwVal = platform_cmd::formatBrightnessPercentToValue( m_brightness, 20, 255 );

        // Real MPC MIPI backlight sysfs
        const char* paths[] = {
            "/sys/class/backlight/mipi-backlight/brightness",
            "/sys/devices/platform/mipi-backlight/backlight/mipi-backlight/brightness",
            nullptr
        };

        for( int i = 0; paths[i] != nullptr; ++i )
        {
            std::ofstream f( paths[i] );
            if( f.is_open() )
            {
                f << hwVal << std::endl;
                kick_log( "prefs", "[PREFS] MPC backlight set to %d (%d%%) on %s\n", hwVal, m_brightness, paths[i] );
                return 1;
            }
        }
        kick_log( "warn", "[PREFS] MPC backlight sysfs not found\n" );
        return 0;
    }

    int getBrightness() override { return m_brightness; }

    int setDateTime( int day, int month, int year, int hour, int minute ) override
    {
        std::string cmd = platform_cmd::formatDateTimeCommand( year, month, day, hour, minute, 0 );
        if( cmd.empty() )
            return 0;

        cmd += " && hwclock -w";
        kick_log( "prefs", "[PREFS] MPC setting system datetime: %s\n", cmd.c_str() );
        int res = runBoardCommand( cmd.c_str() );
        return ( res == 0 ) ? 1 : 0;
    }

    int setTimeZone( const juce::String& tz ) override
    {
        std::string tzStr = tz.toStdString();
        if( !platform_cmd::validateTimezone( tzStr ) )
        {
            kick_log( "warn", "[PREFS] Invalid timezone string rejected: %s\n", tz.toRawUTF8() );
            return 0;
        }

        m_timeZone = tz;
        std::string cmd = platform_cmd::formatSetTimezoneCommand( tzStr );
        kick_log( "prefs", "[PREFS] MPC setting timezone: %s\n", cmd.c_str() );
        int res = runBoardCommand( cmd.c_str() );
        return ( res == 0 ) ? 1 : 0;
    }

    juce::String getTimeZone() override { return m_timeZone; }

    // Ethernet interface on MPC is end0
    std::string getEthernetInterfaceName() const override { return "end0"; }

    bool isEthernetCableConnected() override
    {
        std::ifstream f( "/sys/class/net/end0/carrier" );
        if( f.is_open() )
        {
            std::string line;
            if( std::getline( f, line ) )
                return trimString( line ) == "1";
        }
        return false;
    }

    bool isEthernetInterfaceUp() override
    {
        std::ifstream f( "/sys/class/net/end0/operstate" );
        if( f.is_open() )
        {
            std::string line;
            if( std::getline( f, line ) )
            {
                std::string st = trimString( line );
                return ( st == "up" || st == "unknown" );
            }
        }
        return false;
    }

    bool ethernetEnable( bool enable ) override
    {
        std::string cmd = enable ? "ifconfig end0 up" : "ifconfig end0 down";
        kick_log( "prefs", "[PREFS] MPC ethernetEnable: %s\n", cmd.c_str() );
        return ( runBoardCommand( cmd.c_str() ) == 0 );
    }

    void getEthernetIP( sysEthInfo& info ) override
    {
        info.status  = isEthernetInterfaceUp();
        info.IP      = queryIpAddress( "end0" );
        info.subnet  = querySubnetMask( "end0" );
        info.gateway = queryDefaultGateway( "end0" );
    }

    int setEthernetIP( const std::string& ipAddress, const std::string& subnetMask, const std::string& gateway, const std::string& dns ) override
    {
        std::string cmd = platform_cmd::formatSetIpCommand( "end0", ipAddress, subnetMask, gateway );
        if( cmd.empty() )
            return -1;

        kick_log( "prefs", "[PREFS] MPC setEthernetIP: %s\n", cmd.c_str() );
        int ret = runBoardCommand( cmd.c_str() );

        // Optional DNS resolver update
        if( ret == 0 && !dns.empty() && platform_cmd::validateIpAddress( dns ) )
        {
            std::ofstream resolv( "/etc/resolv.conf", std::ios::trunc );
            if( resolv.is_open() )
            {
                resolv << "nameserver " << dns << "\n";
            }
        }
        return ret;
    }

    int startDhcp() override
    {
        kick_log( "prefs", "[PREFS] MPC starting DHCP on end0\n" );
        std::string cmd = "killall udhcpc 2>/dev/null; /sbin/udhcpc -i end0 -b 2>/dev/null || udhcpc -i end0 -b 2>/dev/null & dhclient end0 2>/dev/null &";
        return runBoardCommand( cmd.c_str() );
    }

    int stopDhcp() override
    {
        kick_log( "prefs", "[PREFS] MPC stopping udhcpc\n" );
        return runBoardCommand( "killall udhcpc 2>/dev/null; killall dhclient 2>/dev/null" );
    }

    // WiFi interface on MPC is wlan0
    std::string getWiFiInterfaceName() const override { return "wlan0"; }

    bool isWiFiAvailable() const override
    {
        return checkFileExists( "/sys/class/net/wlan0" );
    }

    bool enableWiFi() override
    {
        kick_log( "prefs", "[PREFS] MPC enable WiFi: unblocking radio and bringing link up\n" );
        runBoardCommand( "rfkill unblock wifi 2>/dev/null; rfkill unblock all 2>/dev/null" );
        runBoardCommand( "busctl call net.connman /net/connman/technology/wifi net.connman.Technology SetProperty sv \"Powered\" b true 2>/dev/null || connmanctl enable wifi 2>/dev/null" );
        return ( runBoardCommand( "ip link set wlan0 up 2>/dev/null || ifconfig wlan0 up 2>/dev/null" ) == 0 );
    }

    bool disableWiFi() override
    {
        kick_log( "prefs", "[PREFS] MPC disable WiFi: powering down wifi and setting link down\n" );
        runBoardCommand( "busctl call net.connman /net/connman/technology/wifi net.connman.Technology SetProperty sv \"Powered\" b false 2>/dev/null || connmanctl disable wifi 2>/dev/null" );
        runBoardCommand( "killall -9 wpa_supplicant 2>/dev/null" );
        runBoardCommand( "killall udhcpc 2>/dev/null" );
        return ( runBoardCommand( "ip link set wlan0 down 2>/dev/null || ifconfig wlan0 down 2>/dev/null" ) == 0 );
    }

    std::string getIwdStationPath() const
    {
        std::string treeOut = runPipedCommand( "busctl tree --list net.connman.iwd 2>/dev/null" );
        std::string resolved = platform_cmd::parseIwdStationPath( treeOut );
        return resolved.empty() ? "/net/connman/iwd/0/4" : resolved;
    }

    juce::String getConnectedWiFiSSID() override
    {
        // 1. Try ConnMan GetServices via busctl (look for service with State == "online" or "ready")
        std::string cmOut = runPipedCommand( "busctl call net.connman / net.connman.Manager GetServices 2>/dev/null" );
        if( !cmOut.empty() )
        {
            std::string connected = platform_cmd::parseBusctlConnectedService( cmOut );
            if( !connected.empty() )
                return juce::String::fromUTF8( connected.c_str() );
        }

        // 2. Try iwd Station ConnectedNetwork via busctl
        std::string stationPath = getIwdStationPath();
        std::string iwdNet = runPipedCommand( "busctl get-property net.connman.iwd \"" + stationPath + "\" net.connman.iwd.Station ConnectedNetwork 2>/dev/null" );
        if( !iwdNet.empty() )
        {
            size_t q1 = iwdNet.find( '"' );
            size_t q2 = ( q1 != std::string::npos ) ? iwdNet.find( '"', q1 + 1 ) : std::string::npos;
            if( q1 != std::string::npos && q2 != std::string::npos )
            {
                std::string path = iwdNet.substr( q1 + 1, q2 - q1 - 1 );
                if( !path.empty() && path != "/" )
                {
                    std::string nameOut = runPipedCommand( "busctl get-property net.connman.iwd \"" + path + "\" net.connman.iwd.Network Name 2>/dev/null" );
                    size_t nq1 = nameOut.find( '"' );
                    size_t nq2 = ( nq1 != std::string::npos ) ? nameOut.find( '"', nq1 + 1 ) : std::string::npos;
                    if( nq1 != std::string::npos && nq2 != std::string::npos )
                    {
                        std::string name = nameOut.substr( nq1 + 1, nq2 - nq1 - 1 );
                        if( !name.empty() )
                            return juce::String::fromUTF8( name.c_str() );
                    }
                }
            }
        }

        // 3. Fallbacks
        std::string iwgetid = runPipedCommand( "iwgetid -r 2>/dev/null" );
        std::string s = trimString( iwgetid );
        if( !s.empty() )
            return juce::String::fromUTF8( s.c_str() );

        std::string wpaStatus = runPipedCommand( "wpa_cli -i wlan0 status 2>/dev/null | grep '^ssid=' | cut -d= -f2" );
        s = trimString( wpaStatus );
        if( !s.empty() )
            return juce::String::fromUTF8( s.c_str() );

        return {};
    }

    std::vector<juce::String> scanWiFiNetworks() override
    {
        std::vector<juce::String> networks;
        kick_log( "prefs", "[PREFS] MPC scanning WiFi networks...\n" );

        // Ensure radio is unblocked and interface is up before scanning
        runBoardCommand( "rfkill unblock wifi 2>/dev/null; rfkill unblock all 2>/dev/null" );
        runBoardCommand( "busctl call net.connman /net/connman/technology/wifi net.connman.Technology SetProperty sv \"Powered\" b true 2>/dev/null" );
        runBoardCommand( "ip link set wlan0 up 2>/dev/null || ifconfig wlan0 up 2>/dev/null" );

        auto addNetwork = [&]( const std::string& name ) {
            std::string s = trimString( name );
            if( s.size() >= 2 && s.front() == '"' && s.back() == '"' )
                s = s.substr( 1, s.size() - 2 );
            s = trimString( s );
            if( s.empty() || s == ">" || s == "*" || s.find( "\\x00" ) != std::string::npos )
                return;
            juce::String jName = juce::String::fromUTF8( s.c_str() );
            if( std::find( networks.begin(), networks.end(), jName ) == networks.end() )
                networks.push_back( jName );
        };

        std::string stationPath = getIwdStationPath();

        // 1. Trigger scan via D-Bus on ConnMan / iwd
        runBoardCommand( ( "busctl call --timeout=5s net.connman /net/connman/technology/wifi net.connman.Technology Scan 2>/dev/null || busctl call --timeout=5s net.connman.iwd \"" + stationPath + "\" net.connman.iwd.Station Scan 2>/dev/null || connmanctl scan wifi 2>/dev/null" ).c_str() );

        // 2. Query ConnMan GetServices via busctl (native to Akai MPC firmware)
        std::string cmOut = runPipedCommand( "busctl call net.connman / net.connman.Manager GetServices 2>/dev/null" );
        if( !cmOut.empty() )
        {
            auto parsed = platform_cmd::parseBusctlConnmanServices( cmOut );
            for( const auto& ssid : parsed )
                addNetwork( ssid );
        }

        // 3. Query iwd GetOrderedNetworks via busctl
        if( networks.empty() )
        {
            std::string iwdOut = runPipedCommand( "busctl call net.connman.iwd \"" + stationPath + "\" net.connman.iwd.Station GetOrderedNetworks 2>/dev/null" );
            if( !iwdOut.empty() )
            {
                const std::string prefix = stationPath + "/";
                size_t pos = 0;
                while( ( pos = iwdOut.find( prefix, pos ) ) != std::string::npos )
                {
                    pos += prefix.size();
                    size_t endUnderscore = iwdOut.find( '_', pos );
                    if( endUnderscore != std::string::npos )
                    {
                        std::string hexSsid = iwdOut.substr( pos, endUnderscore - pos );
                        std::string ssid = platform_cmd::hexToString( hexSsid );
                        if( !ssid.empty() )
                            addNetwork( ssid );
                        pos = endUnderscore + 1;
                    }
                }
            }
        }

        // 4. Try connmanctl (if installed)
        if( networks.empty() )
        {
            std::string connmanServices = runPipedCommand( "connmanctl services 2>/dev/null" );
            if( !connmanServices.empty() )
            {
                std::istringstream iss( connmanServices );
                std::string line;
                while( std::getline( iss, line ) )
                {
                    size_t wifiPos = line.find( "wifi_" );
                    if( wifiPos != std::string::npos )
                    {
                        std::string namePart = line.substr( 0, wifiPos );
                        addNetwork( namePart );
                    }
                }
            }
        }

        // 5. Try iw dev wlan0 scan
        if( networks.empty() )
        {
            std::string iwOut = runPipedCommand( "iw dev wlan0 scan 2>/dev/null | grep -E '^[ \t]*SSID: '" );
            std::istringstream iss( iwOut );
            std::string line;
            while( std::getline( iss, line ) )
            {
                size_t pos = line.find( "SSID: " );
                if( pos != std::string::npos )
                {
                    addNetwork( line.substr( pos + 6 ) );
                }
            }
        }

        // 6. Try iwlist wlan0 scan
        if( networks.empty() )
        {
            std::string iwlOut = runPipedCommand( "iwlist wlan0 scan 2>/dev/null | grep 'ESSID:'" );
            std::istringstream iss( iwlOut );
            std::string line;
            while( std::getline( iss, line ) )
            {
                size_t pos = line.find( "ESSID:\"" );
                if( pos != std::string::npos )
                {
                    size_t start = pos + 7;
                    size_t end = line.find( '"', start );
                    if( end != std::string::npos )
                        addNetwork( line.substr( start, end - start ) );
                }
            }
        }

        // 7. Try wpa_cli scan
        if( networks.empty() )
        {
            runBoardCommand( "wpa_cli -i wlan0 scan 2>/dev/null; sleep 1" );
            std::string wpaOut = runPipedCommand( "wpa_cli -i wlan0 scan_results 2>/dev/null" );
            std::istringstream iss( wpaOut );
            std::string line;
            bool pastHdr = false;
            while( std::getline( iss, line ) )
            {
                if( !pastHdr )
                {
                    if( line.find( "ssid" ) != std::string::npos || line.find( "SSID" ) != std::string::npos )
                        pastHdr = true;
                    continue;
                }
                std::istringstream ls( line );
                std::string bssid, freq, sig, flags, ssid;
                if( ls >> bssid >> freq >> sig >> flags )
                {
                    std::getline( ls, ssid );
                    addNetwork( ssid );
                }
            }
        }

        // 8. Try iwctl
        if( networks.empty() )
        {
            runBoardCommand( "iwctl station wlan0 scan 2>/dev/null" );
            std::string output = runPipedCommand( "iwctl station wlan0 get-networks 2>/dev/null" );
            if( !output.empty() && output.find( "Network name" ) != std::string::npos )
            {
                std::istringstream iss( output );
                std::string line;
                bool pastHeader = false;
                while( std::getline( iss, line ) )
                {
                    if( !pastHeader )
                    {
                        if( line.find( "----" ) != std::string::npos ) pastHeader = true;
                        continue;
                    }
                    std::string clean;
                    bool inEscape = false;
                    for( char c : line )
                    {
                        if( c == '\033' ) inEscape = true;
                        else if( inEscape && c == 'm' ) inEscape = false;
                        else if( !inEscape ) clean.push_back( c );
                    }
                    std::string trimmed = trimString( clean );
                    if( trimmed.empty() ) continue;
                    std::istringstream lineStream( trimmed );
                    std::string name;
                    lineStream >> name;
                    addNetwork( name );
                }
            }
        }

        kick_log( "prefs", "[PREFS] MPC WiFi scan found %zu networks\n", networks.size() );
        return networks;
    }

    bool connectToWiFi( const juce::String& ssid, const juce::String& password ) override
    {
        kick_log( "prefs", "[PREFS] MPC connecting to WiFi SSID: '%s'\n", ssid.toRawUTF8() );
        std::string sSsid = ssid.toStdString();
        std::string sPass = password.toStdString();
        if( sSsid.empty() )
            return false;

        // 1. Unblock radio and bring link up
        runBoardCommand( "rfkill unblock wifi 2>/dev/null; rfkill unblock all 2>/dev/null" );
        runBoardCommand( "busctl call net.connman /net/connman/technology/wifi net.connman.Technology SetProperty sv \"Powered\" b true 2>/dev/null" );
        runBoardCommand( "ip link set wlan0 up 2>/dev/null || ifconfig wlan0 up 2>/dev/null" );

        // 2. Provision iwd network file (/var/lib/iwd/<SSID>.psk or .open)
        {
            std::string iwdPath = platform_cmd::formatIwdNetworkFilename( sSsid, !sPass.empty() );
            std::ofstream iwdFile( iwdPath );
            if( iwdFile.is_open() )
            {
                if( !sPass.empty() )
                    iwdFile << platform_cmd::formatIwdPskConfig( sPass );
                else
                    iwdFile << "[Settings]\nAutoConnect=true\n";
                iwdFile.close();
                ::chmod( iwdPath.c_str(), 0600 );
            }
        }

        // 3. Provision ConnMan .config file (/var/lib/connman/wifi_<hex>.config)
        std::string hexSsid = platform_cmd::stringToHex( sSsid );
        {
            std::string cmCfgPath = "/var/lib/connman/wifi_" + hexSsid + ".config";
            std::ofstream cmFile( cmCfgPath );
            if( cmFile.is_open() )
            {
                cmFile << platform_cmd::formatConnmanConfig( sSsid, sPass );
                cmFile.close();
                ::chmod( cmCfgPath.c_str(), 0600 );
            }
        }

        // 4. Try ConnMan D-Bus Connect
        std::string cmOut = runPipedCommand( "busctl call net.connman / net.connman.Manager GetServices 2>/dev/null" );
        std::string currentConnected = platform_cmd::parseBusctlConnectedService( cmOut );
        if( !currentConnected.empty() && currentConnected != sSsid )
        {
            std::string oldPath = platform_cmd::parseBusctlServicePathForSsid( cmOut, currentConnected );
            if( !oldPath.empty() )
                runBoardCommand( ( "busctl call net.connman \"" + oldPath + "\" net.connman.Service Disconnect 2>/dev/null" ).c_str() );
        }

        std::string servicePath = platform_cmd::parseBusctlServicePathForSsid( cmOut, sSsid );
        if( !servicePath.empty() )
        {
            kick_log( "prefs", "[PREFS] MPC ConnMan service path found: %s\n", servicePath.c_str() );
            runBoardCommand( ( "busctl call net.connman \"" + servicePath + "\" net.connman.Service Connect 2>/dev/null" ).c_str() );
        }

        // 5. Try iwd D-Bus Connect
        std::string stationPath = getIwdStationPath();
        runBoardCommand( ( "busctl call net.connman.iwd \"" + stationPath + "/" + hexSsid + "_psk\" net.connman.iwd.Network Connect 2>/dev/null || busctl call net.connman.iwd \"" + stationPath + "/" + hexSsid + "_open\" net.connman.iwd.Network Connect 2>/dev/null" ).c_str() );

        // 6. Fallback: standard wpa_supplicant + DHCP
        std::string wpaConf = platform_cmd::formatWifiWpaSupplicantConfig( sSsid, sPass );
        juce::File confDir( "/data/prefs" );
        if( !confDir.exists() )
            confDir.createDirectory();

        std::ofstream conf( "/data/prefs/wpa_supplicant.conf" );
        if( conf.is_open() )
        {
            conf << wpaConf;
            conf.close();
            ::chmod( "/data/prefs/wpa_supplicant.conf", 0600 );
        }

        // 7. Poll for connection to the requested SSID up to 10 seconds
        for( int i = 0; i < 10; ++i )
        {
            juce::Thread::sleep( 1000 );
            std::string ip = queryIpAddress( "wlan0" );
            juce::String connSsid = getConnectedWiFiSSID();
            bool hasIp = ( !ip.empty() && ip != "0.0.0.0" );
            bool targetSsidMatched = ( connSsid == ssid );
            bool otherSsidActive = ( connSsid.isNotEmpty() && !targetSsidMatched );

            if( !otherSsidActive && ( ( targetSsidMatched && ( hasIp || i >= 3 ) ) || ( connSsid.isEmpty() && hasIp ) ) )
            {
                kick_log( "prefs", "[PREFS] MPC WiFi connected to %s (IP: %s)\n", sSsid.c_str(), ip.c_str() );

                std::string credFile = "/data/prefs/wifi_credentials.txt";
                std::ofstream cf( credFile, std::ios::app );
                if( cf.is_open() )
                {
                    cf << platform_cmd::sanitizeWifiSsid( sSsid ) << ":"
                       << platform_cmd::sanitizeWifiPassword( sPass ) << "\n";
                    cf.close();
                    ::chmod( credFile.c_str(), 0600 );
                }
                return true;
            }
        }

        kick_log( "warn", "[PREFS] MPC WiFi connection timed out: no IP acquired on wlan0\n" );
        return false;
    }

    bool disconnectWiFi() override
    {
        kick_log( "prefs", "[PREFS] MPC disconnecting WiFi\n" );
        // 1. ConnMan service disconnect
        std::string cmOut = runPipedCommand( "busctl call net.connman / net.connman.Manager GetServices 2>/dev/null" );
        if( !cmOut.empty() )
        {
            std::string connectedSsid = platform_cmd::parseBusctlConnectedService( cmOut );
            if( !connectedSsid.empty() )
            {
                std::string sPath = platform_cmd::parseBusctlServicePathForSsid( cmOut, connectedSsid );
                if( !sPath.empty() )
                    runBoardCommand( ( "busctl call net.connman \"" + sPath + "\" net.connman.Service Disconnect 2>/dev/null" ).c_str() );
            }
        }
        // 2. iwd disconnect
        std::string stationPath = getIwdStationPath();
        runBoardCommand( ( "busctl call net.connman.iwd \"" + stationPath + "\" net.connman.iwd.Station Disconnect 2>/dev/null" ).c_str() );
        // 3. Fallbacks
        runBoardCommand( "connmanctl disable wifi 2>/dev/null" );
        runBoardCommand( "iwctl station wlan0 disconnect 2>/dev/null" );
        runBoardCommand( "killall -9 wpa_supplicant 2>/dev/null" );
        runBoardCommand( "killall udhcpc 2>/dev/null" );
        return true;
    }

    void getWiFiIP( sysEthInfo& info ) override
    {
        info.status  = checkFileExists( "/sys/class/net/wlan0/operstate" );
        info.IP      = queryIpAddress( "wlan0" );
        info.subnet  = querySubnetMask( "wlan0" );
        info.gateway = queryDefaultGateway( "wlan0" );
    }

    int factoryReset() override
    {
        kick_log( "info", "[PREFS] MPC Factory Reset executed: purging /data/prefs and /data/projects\n" );
        runBoardCommand( platform_cmd::formatMpcFactoryResetCommand().c_str() );
        return reboot();
    }

    int reboot() override
    {
        kick_log( "info", "[PREFS] MPC Rebooting system...\n" );
        return runBoardCommand( "/sbin/reboot -f &" );
    }

    int powerOff() override
    {
        kick_log( "info", "[PREFS] MPC Powering off system...\n" );
        return runBoardCommand( "/sbin/poweroff -f &" );
    }

    DeviceInfo getDeviceInfo() override
    {
        DeviceInfo info;
        std::memset( &info, 0, sizeof( info ) );
        juce::String serial = getDevSerial();
        juce::String fwVer = getFirmwareVersion();
        juce::String stamp = getBuildStamp();

        std::strncpy( info.serialNumber, serial.toRawUTF8(), sizeof( info.serialNumber ) - 1 );
        std::strncpy( info.firmwareVersion, fwVer.toRawUTF8(), sizeof( info.firmwareVersion ) - 1 );
        std::strncpy( info.firmwareBuildDate, stamp.toRawUTF8(), sizeof( info.firmwareBuildDate ) - 1 );

        mt::sysRamInfo rInfo;
        mt::SystemResource::getResourceInterface().ramInfo( rInfo );
        std::strncpy( info.memoryStatus.available, juce::String( rInfo.freeRam ).toRawUTF8(), sizeof( info.memoryStatus.available ) - 1 );
        std::strncpy( info.memoryStatus.total, juce::String( rInfo.totalRam ).toRawUTF8(), sizeof( info.memoryStatus.total ) - 1 );
        std::strncpy( info.memoryStatus.used, juce::String( rInfo.usedRam ).toRawUTF8(), sizeof( info.memoryStatus.used ) - 1 );

        mt::sysSizeInfo sInfo;
        std::string internalPath = "/data";
        mt::SystemResource::getResourceInterface().sizeInfo( internalPath, sInfo );
        std::strncpy( info.internalStorageStatus.availableSpace, juce::String( sInfo.avaSize ).toRawUTF8(), sizeof( info.internalStorageStatus.availableSpace ) - 1 );
        std::strncpy( info.internalStorageStatus.totalSpace, juce::String( sInfo.totalSize ).toRawUTF8(), sizeof( info.internalStorageStatus.totalSpace ) - 1 );
        std::strncpy( info.internalStorageStatus.usedSpace, juce::String( sInfo.usedSize ).toRawUTF8(), sizeof( info.internalStorageStatus.usedSpace ) - 1 );

        return info;
    }

    juce::String getFirmwareVersion() override
    {
        loadVersionFileIfNeeded();
        if( m_versionInfo.isValid )
            return juce::String( m_versionInfo.version );
#if defined( KICK_APP_VERSION )
        return KICK_APP_VERSION; // no /data/Kick.version (desktop, dev build): the version this binary was built as
#else
        return "0.1.0-alpha";
#endif
    }

    juce::String getBuildStamp() override
    {
        loadVersionFileIfNeeded();
        if( m_versionInfo.isValid && !m_versionInfo.commit.empty() )
        {
            return juce::String( m_versionInfo.buildDate ) + " :: " + juce::String( m_versionInfo.commit );
        }
#if defined( TIMESTAMP ) && ( TIMESTAMP > 0 )
        return juce::String( BUILD_TIMESTAMP_STR ) + " :: " + juce::String( GIT_SHORT_HASH_STR );
#else
        return juce::String( __DATE__ ) + " :: " + juce::String( __TIME__ );
#endif
    }

    juce::String getDevSerial() override
    {
        const char* serialFiles[] = {
            "/sys/firmware/devicetree/base/serial-number",
            "/sys/devices/soc0/serial_number",
            nullptr
        };

        for( int i = 0; serialFiles[i] != nullptr; ++i )
        {
            std::ifstream f( serialFiles[i] );
            if( f.is_open() )
            {
                std::string s;
                if( std::getline( f, s ) )
                {
                    std::string trimmed = trimString( s );
                    if( !trimmed.empty() )
                        return juce::String( trimmed );
                }
            }
        }
        return "AKAI-KEY61-001";
    }

private:
    void loadVersionFileIfNeeded()
    {
        if( m_versionInfoLoaded )
            return;
        m_versionInfoLoaded = true;

        const char* candidatePaths[] = {
            "/data/Kick.version",
            "/data/Kick/VERSION",
            "/data/VERSION",
            "VERSION",
            nullptr
        };

        for( int i = 0; candidatePaths[i] != nullptr; ++i )
        {
            std::ifstream f( candidatePaths[i] );
            if( f.is_open() )
            {
                std::string content( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
                auto parsed = platform_cmd::parseVersionContent( content );
                if( parsed.isValid )
                {
                    m_versionInfo = parsed;
                    kick_log( "prefs", "[PREFS] Loaded MPC VERSION file from %s (v%s, commit %s)\n",
                              candidatePaths[i], parsed.version.c_str(), parsed.commit.c_str() );
                    return;
                }
            }
        }
    }

    bool m_versionInfoLoaded = false;
    platform_cmd::ParsedVersionInfo m_versionInfo;
};

// ----------------------------------------------------------------------------
// Behringer Hardware Platform
// ----------------------------------------------------------------------------

class BehringerPlatformServices : public LinuxPlatformServicesBase
{
public:
    PlatformType getPlatformType() const override { return PlatformType::Behringer; }
    const char* getPlatformName() const override { return "Behringer Kick"; }

    bool isHardwareControlAvailable() const override { return hardwareEnabled(); }
    juce::String getHardwareUnavailableReason() const override
    {
        return hardwareEnabled() ? "" : "Behringer hardware board controls disabled (KICK_NO_HARDWARE=1).";
    }

    int setBrightness( int percent ) override
    {
        if( !hardwareEnabled() ) return 0;
        m_brightness = std::max( 0, std::min( 100, percent ) );
        int hwVal = platform_cmd::formatBrightnessPercentToValue( m_brightness, 20, 100 );
        std::string cmd = "echo " + std::to_string( hwVal ) + " > /sys/devices/platform/backlight/backlight/backlight/brightness";
        return ( runBoardCommand( cmd.c_str() ) == 0 );
    }

    int getBrightness() override { return m_brightness; }

    int setDateTime( int day, int month, int year, int hour, int minute ) override
    {
        if( !hardwareEnabled() ) return 0;
        std::string cmd = platform_cmd::formatDateTimeCommand( year, month, day, hour, minute, 0 );
        if( cmd.empty() ) return 0;
        return ( runBoardCommand( cmd.c_str() ) == 0 );
    }

    int setTimeZone( const juce::String& tz ) override
    {
        if( !hardwareEnabled() ) return 0;
        std::string tzStr = tz.toStdString();
        if( !platform_cmd::validateTimezone( tzStr ) ) return 0;
        m_timeZone = tz;
        std::string cmd = platform_cmd::formatSetTimezoneCommand( tzStr );
        return ( runBoardCommand( cmd.c_str() ) == 0 );
    }

    juce::String getTimeZone() override { return m_timeZone; }

    std::string getEthernetInterfaceName() const override { return "eth0"; }
    bool isEthernetCableConnected() override
    {
        std::string out = runPipedCommand( "ethtool eth0 2>/dev/null | grep 'Link detected'" );
        return ( out.find( "yes" ) != std::string::npos );
    }

    bool isEthernetInterfaceUp() override
    {
        std::ifstream f( "/sys/class/net/eth0/operstate" );
        if( f.is_open() )
        {
            std::string line;
            if( std::getline( f, line ) )
                return trimString( line ) == "up";
        }
        return false;
    }

    bool ethernetEnable( bool enable ) override
    {
        if( !hardwareEnabled() ) return false;
        std::string cmd = enable ? "ifconfig eth0 up" : "ifconfig eth0 down";
        return ( runBoardCommand( cmd.c_str() ) == 0 );
    }

    void getEthernetIP( sysEthInfo& info ) override
    {
        info.status  = isEthernetInterfaceUp();
        info.IP      = queryIpAddress( "eth0" );
        info.subnet  = querySubnetMask( "eth0" );
        info.gateway = queryDefaultGateway( "eth0" );
    }

    int setEthernetIP( const std::string& ipAddress, const std::string& subnetMask, const std::string& gateway, const std::string& dns ) override
    {
        juce::ignoreUnused( dns );
        if( !hardwareEnabled() ) return -1;
        std::string cmd = platform_cmd::formatSetIpCommand( "eth0", ipAddress, subnetMask, gateway );
        return runBoardCommand( cmd.c_str() );
    }

    int startDhcp() override
    {
        if( !hardwareEnabled() ) return -1;
        return runBoardCommand( "/usr/sbin/udhcpc -i eth0 &" );
    }

    int stopDhcp() override
    {
        if( !hardwareEnabled() ) return -1;
        return runBoardCommand( "killall udhcpc" );
    }

    std::string getWiFiInterfaceName() const override { return "wlan0"; }
    bool isWiFiAvailable() const override { return hardwareEnabled() && checkFileExists( "/sys/class/net/wlan0" ); }
    bool enableWiFi() override { return hardwareEnabled() && ( runBoardCommand( "ip link set wlan0 up" ) == 0 ); }
    bool disableWiFi() override { return hardwareEnabled() && ( runBoardCommand( "ip link set wlan0 down" ) == 0 ); }

    std::vector<juce::String> scanWiFiNetworks() override
    {
        if( !hardwareEnabled() ) return {};
        std::vector<juce::String> networks;
        std::string out = runPipedCommand( "iwlist wlan0 scan 2>/dev/null" );
        std::istringstream iss( out );
        std::string line;
        while( std::getline( iss, line ) )
        {
            size_t pos = line.find( "ESSID:\"" );
            if( pos != std::string::npos )
            {
                size_t start = pos + 7;
                size_t end = line.find( '"', start );
                if( end != std::string::npos )
                {
                    juce::String name = juce::String::fromUTF8( line.substr( start, end - start ).c_str() ).trim();
                    if( !name.isEmpty() && std::find( networks.begin(), networks.end(), name ) == networks.end() )
                        networks.push_back( name );
                }
            }
        }
        return networks;
    }

    bool connectToWiFi( const juce::String& ssid, const juce::String& password ) override
    {
        if( !hardwareEnabled() ) return false;
        std::string wpaConf = platform_cmd::formatWifiWpaSupplicantConfig( ssid.toStdString(), password.toStdString() );
        std::ofstream conf( "/home/Kick/wpa_supplicant.conf" );
        if( conf.is_open() )
        {
            conf << wpaConf;
            conf.close();
            ::chmod( "/home/Kick/wpa_supplicant.conf", 0600 );
        }
        runBoardCommand( "killall -9 wpa_supplicant dhclient 2>/dev/null" );
        runBoardCommand( "wpa_supplicant -B -i wlan0 -c /home/Kick/wpa_supplicant.conf" );
        return ( runBoardCommand( "dhclient wlan0" ) == 0 );
    }

    bool disconnectWiFi() override
    {
        if( !hardwareEnabled() ) return false;
        return ( runBoardCommand( "killall -9 wpa_supplicant dhclient 2>/dev/null" ) == 0 );
    }

    void getWiFiIP( sysEthInfo& info ) override
    {
        info.status  = checkFileExists( "/sys/class/net/wlan0/operstate" );
        info.IP      = queryIpAddress( "wlan0" );
        info.subnet  = querySubnetMask( "wlan0" );
        info.gateway = queryDefaultGateway( "wlan0" );
    }

    int factoryReset() override
    {
        if( !hardwareEnabled() ) return -1;
        kick_log( "info", "[PREFS] Behringer factory reset executing\n" );
        return runBoardCommand( platform_cmd::formatBehringerFactoryResetCommand().c_str() );
    }

    int reboot() override
    {
        if( !hardwareEnabled() ) return -1;
        return runBoardCommand( "reboot -nf" );
    }

    int powerOff() override
    {
        if( !hardwareEnabled() ) return -1;
        return runBoardCommand( "/sbin/poweroff" );
    }

    DeviceInfo getDeviceInfo() override
    {
        DeviceInfo info;
        std::memset( &info, 0, sizeof( info ) );
        juce::String serial = getDevSerial();
        juce::String fwVer = getFirmwareVersion();
        juce::String stamp = getBuildStamp();

        std::strncpy( info.serialNumber, serial.toRawUTF8(), sizeof( info.serialNumber ) - 1 );
        std::strncpy( info.firmwareVersion, fwVer.toRawUTF8(), sizeof( info.firmwareVersion ) - 1 );
        std::strncpy( info.firmwareBuildDate, stamp.toRawUTF8(), sizeof( info.firmwareBuildDate ) - 1 );
        return info;
    }

    juce::String getFirmwareVersion() override
    {
        std::ifstream f( "/etc/version.txt" );
        if( f.is_open() )
        {
            std::string line;
            if( std::getline( f, line ) )
            {
                return juce::String( trimString( line ) );
            }
        }
        return "0.0.0";
    }

    juce::String getBuildStamp() override
    {
#if defined( TIMESTAMP ) && ( TIMESTAMP > 0 )
        return juce::String( BUILD_TIMESTAMP_STR ) + " :: " + juce::String( GIT_SHORT_HASH_STR );
#else
        return juce::String( __DATE__ ) + " :: " + juce::String( __TIME__ );
#endif
    }

    juce::String getDevSerial() override
    {
        std::ifstream f( "/sys/devices/soc0/serial_number" );
        if( f.is_open() )
        {
            std::string line;
            if( std::getline( f, line ) )
            {
                return juce::String( trimString( line ) );
            }
        }
        return "000000000";
    }
};

// ----------------------------------------------------------------------------
// Factory & Detection
// ----------------------------------------------------------------------------

static std::atomic<int> s_platformOverride{ -1 };

void PlatformServices::setPlatformOverride( PlatformType type )
{
    s_platformOverride.store( static_cast<int>( type ) );
}

PlatformType PlatformServices::detectPlatform()
{
    int overrideVal = s_platformOverride.load();
    if( overrideVal >= 0 )
        return static_cast<PlatformType>( overrideVal );

    // 1. Check environment variable override
    const char* envPlat = std::getenv( "KICK_PLATFORM" );
    if( envPlat && *envPlat )
    {
        std::string p( envPlat );
        std::transform( p.begin(), p.end(), p.begin(), []( unsigned char c ) { return std::tolower( c ); } );
        if( p == "mpc" || p == "akai" || p == "key61" )
            return PlatformType::AkaiMpc;
        if( p == "behringer" )
            return PlatformType::Behringer;
        if( p == "desktop" || p == "sim" || p == "simulator" )
            return PlatformType::Desktop;
    }

#if( JUCE_LINUX || __linux__ )
    // 2. Akai MPC indicators:
    //  - Backlight sysfs
    if( checkFileExists( "/sys/class/backlight/mipi-backlight" ) ||
        checkFileExists( "/sys/devices/platform/mipi-backlight/backlight/mipi-backlight" ) )
    {
        return PlatformType::AkaiMpc;
    }

    //  - Local version / flash file
    if( checkFileExists( "/data/Kick.version" ) || checkFileExists( "/data/Kick/VERSION" ) )
    {
        return PlatformType::AkaiMpc;
    }

    //  - Devicetree model or ALSA cards
    std::ifstream dtModel( "/sys/firmware/devicetree/base/model" );
    if( dtModel.is_open() )
    {
        std::string model;
        if( std::getline( dtModel, model ) )
        {
            if( model.find( "Key 61" ) != std::string::npos ||
                model.find( "MPC" ) != std::string::npos ||
                model.find( "Akai" ) != std::string::npos )
            {
                return PlatformType::AkaiMpc;
            }
        }
    }

    std::ifstream asound( "/proc/asound/cards" );
    if( asound.is_open() )
    {
        std::string line;
        while( std::getline( asound, line ) )
        {
            if( line.find( "ACVM" ) != std::string::npos ||
                line.find( "MPC Key 61" ) != std::string::npos )
            {
                return PlatformType::AkaiMpc;
            }
        }
    }

    // 3. Behringer indicators:
    if( checkFileExists( "/sys/devices/soc0/serial_number" ) ||
        checkFileExists( "/sys/devices/platform/backlight/backlight/backlight/brightness" ) ||
        checkFileExists( "/etc/version.txt" ) )
    {
        return PlatformType::Behringer;
    }
#endif

    // Default to Desktop
    return PlatformType::Desktop;
}

PlatformServices& PlatformServices::getInstance()
{
    static std::unique_ptr<PlatformServices> s_instance;
    static std::once_flag s_once;
    std::call_once( s_once, []() {
        PlatformType type = detectPlatform();
        switch( type )
        {
            case PlatformType::AkaiMpc:
                s_instance = std::make_unique<AkaiMpcPlatformServices>();
                kick_log( "info", "[PLATFORM] Initialized Akai MPC Platform Services\n" );
                break;
            case PlatformType::Behringer:
                s_instance = std::make_unique<BehringerPlatformServices>();
                kick_log( "info", "[PLATFORM] Initialized Behringer Platform Services\n" );
                break;
            case PlatformType::Desktop:
            default:
                s_instance = std::make_unique<DesktopPlatformServices>();
                kick_log( "info", "[PLATFORM] Initialized Desktop Platform Services\n" );
                break;
        }
    } );
    return *s_instance;
}

} // namespace mt
