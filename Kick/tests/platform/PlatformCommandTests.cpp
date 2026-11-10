// SPDX-License-Identifier: AGPL-3.0-or-later
#include "../../modules/hardware/source/kick_PlatformCommands.h"
#include <cassert>
#include <iostream>

int main()
{
    using namespace mt::platform_cmd;

    // 1. Timezone validation & injection protection
    assert( validateTimezone( "Europe/Budapest" ) );
    assert( validateTimezone( "America/New_York" ) );
    assert( validateTimezone( "UTC" ) );
    assert( validateTimezone( "Etc/GMT+1" ) );
    assert( !validateTimezone( "Europe/Budapest; rm -rf /" ) );
    assert( !validateTimezone( "UTC\nreboot" ) );
    assert( !validateTimezone( "America/New`reboot`York" ) );
    assert( !validateTimezone( "tz $(whoami)" ) );
    assert( !validateTimezone( "" ) );

    // 2. IP & Netmask validation
    assert( validateIpAddress( "192.168.0.1" ) );
    assert( validateIpAddress( "10.0.0.1" ) );
    assert( validateIpAddress( "255.255.255.255" ) );
    assert( !validateIpAddress( "192.168.0.1\nifconfig" ) );
    assert( !validateIpAddress( "256.0.0.1" ) );
    assert( !validateIpAddress( "192.168.1" ) );
    assert( !validateIpAddress( "abc.def.ghi.jkl" ) );
    assert( !validateIpAddress( "" ) );

    assert( validateSubnetMask( "255.255.255.0" ) );
    assert( validateSubnetMask( "255.255.0.0" ) );
    assert( validateSubnetMask( "255.0.0.0" ) );
    assert( validateSubnetMask( "255.255.255.252" ) );
    assert( !validateSubnetMask( "255.255.255.1" ) );
    assert( !validateSubnetMask( "255.0.255.0" ) );

    // 3. Command formatting
    assert( formatDateCommand( 2026, 11, 4 ) == "date -s \"2026-11-04\"" );
    assert( formatDateCommand( 2024, 5, 9 ) == "date -s \"2024-05-09\"" );

    assert( formatTimeCommand( 2, 5, 9 ) == "date -s \"02:05:09\"" );
    assert( formatTimeCommand( 14, 30, 0 ) == "date -s \"14:30:00\"" );

    assert( formatDateTimeCommand( 2026, 11, 4, 2, 5, 9 ) == "date -s \"2026-11-04 02:05:09\"" );

    assert( formatSetTimezoneCommand( "Europe/Budapest" ) == "timedatectl set-timezone Europe/Budapest" );
    assert( formatSetTimezoneCommand( "Europe/Budapest; rm -rf /" ).empty() );

    assert( parseTimezoneFromZoneinfoLink( "/var/db/timezone/zoneinfo/Europe/Budapest" ) == "Europe/Budapest" );
    assert( parseTimezoneFromZoneinfoLink( "/usr/share/zoneinfo/America/New_York" ) == "America/New_York" );
    assert( parseTimezoneFromZoneinfoLink( "../zoneinfo/UTC" ) == "UTC" );
    assert( parseTimezoneFromZoneinfoLink( "/etc/localtime" ).empty() );

    assert( parseTimezoneFromGeoIpJson( "{\"status\":\"success\",\"timezone\":\"Europe/Budapest\"}" ) == "Europe/Budapest" );
    assert( parseTimezoneFromGeoIpJson( "{\"status\": \"success\", \"timezone\": \"America/Chicago\"}" ) == "America/Chicago" );
    assert( parseTimezoneFromGeoIpJson( "{\"status\":\"fail\",\"message\":\"invalid query\"}" ).empty() );

    assert( formatSetIpCommand( "end0", "192.168.0.50", "255.255.255.0", "192.168.0.1" )
            == "ifconfig end0 192.168.0.50 netmask 255.255.255.0 && ip route replace default via 192.168.0.1 dev end0" );

    // 4. Wi-Fi Sanitization & wpa_supplicant / iwd formatting
    assert( sanitizeWifiSsid( "MySSID\"withQuote\nandNewline" ) == "MySSID\\\"withQuoteandNewline" );
    assert( sanitizeWifiPassword( "pass`$(whoami)\"word" ) == "pass\\`\\$\\(whoami\\)\\\"word" );

    std::string wpaConf = formatWifiWpaSupplicantConfig( "MySSID", "secret123" );
    assert( wpaConf.find( "ssid=\"MySSID\"" ) != std::string::npos );
    assert( wpaConf.find( "psk=\"secret123\"" ) != std::string::npos );

    std::string iwdCmd = formatIwdConnectCommand( "wlan0", "Test Net", "pass123" );
    assert( iwdCmd == "iwctl --passphrase \"pass123\" station wlan0 connect \"Test Net\"" );

    // 4b. Hex / D-Bus ConnMan & iwd parsing helpers
    assert( hexToString( "53616669616e" ) == "Safian" );
    assert( hexToString( "537a61626957696669" ) == "SzabiWifi" );
    assert( hexToString( "invalid" ).empty() );
    assert( hexToString( "123" ).empty() );
    assert( stringToHex( "Safian" ) == "53616669616e" );
    assert( stringToHex( "SzabiWifi" ) == "537a61626957696669" );
    assert( hexToString( stringToHex( "Hello World! 123" ) ) == "Hello World! 123" );

    std::string sampleBusctl =
        "a(oa{sv}) 2 \"/net/connman/service/wifi_08e9f686864c_54656c656b6f6d2d396238636534_managed_psk\" 24 "
        "\"Type\" s \"wifi\" \"Security\" as 1 \"psk\" \"State\" s \"online\" \"Name\" s \"Telekom-9b8ce4\" "
        "\"/net/connman/service/wifi_08e9f686864c_53616669616e_managed_psk\" 24 "
        "\"Type\" s \"wifi\" \"Security\" as 1 \"psk\" \"State\" s \"idle\" \"Name\" s \"Safian\" ";

    auto services = parseBusctlConnmanServices( sampleBusctl );
    assert( services.size() == 2 );
    assert( services[0] == "Telekom-9b8ce4" );
    assert( services[1] == "Safian" );

    assert( parseBusctlConnectedService( sampleBusctl ) == "Telekom-9b8ce4" );
    assert( parseBusctlServicePathForSsid( sampleBusctl, "Safian" ) == "/net/connman/service/wifi_08e9f686864c_53616669616e_managed_psk" );
    assert( parseBusctlServicePathForSsid( sampleBusctl, "Telekom-9b8ce4" ) == "/net/connman/service/wifi_08e9f686864c_54656c656b6f6d2d396238636534_managed_psk" );
    assert( parseBusctlServicePathForSsid( sampleBusctl, "NonExistent" ).empty() );

    // Adjacent ethernet service ("online", "Wired") must not pollute Wi-Fi scan or trigger false connected state on idle wifi
    std::string mixedBusctl =
        "a(oa{sv}) 2 \"/net/connman/service/wifi_08e9f686864c_53616669616e_managed_psk\" 24 "
        "\"Type\" s \"wifi\" \"State\" s \"idle\" \"Name\" s \"Safian\" "
        "\"/net/connman/service/ethernet_001122334455_cable\" 18 "
        "\"Type\" s \"ethernet\" \"State\" s \"online\" \"Name\" s \"Wired\" ";
    auto wifiOnly = parseBusctlConnmanServices( mixedBusctl );
    assert( wifiOnly.size() == 1 && wifiOnly[0] == "Safian" );
    assert( parseBusctlConnectedService( mixedBusctl ).empty() );

    std::string cmCfg = formatConnmanConfig( "MyWifi", "Pass$123;!\n" );
    assert( cmCfg.find( "Name = MyWifi\n" ) != std::string::npos );
    assert( cmCfg.find( "Passphrase = Pass$123;!\n" ) != std::string::npos );
    assert( cmCfg.find( "\\$" ) == std::string::npos );

    std::string iwdPsk = formatIwdPskConfig( "Pass$123;!\n" );
    assert( iwdPsk.find( "Passphrase=Pass$123;!\n" ) != std::string::npos );
    assert( iwdPsk.find( "\\$" ) == std::string::npos );
    assert( iwdPsk.find( "AutoConnect=true" ) != std::string::npos );

    std::string iwdTree =
        "/\n/net\n/net/connman\n/net/connman/iwd\n/net/connman/iwd/0\n/net/connman/iwd/0/7\n/net/connman/iwd/0/7/53616669616e_psk\n";
    assert( parseIwdStationPath( iwdTree ) == "/net/connman/iwd/0/7" );
    assert( parseIwdStationPath( "/\n/net\n" ).empty() );

    assert( formatIwdNetworkFilename( "Safian", true ) == "/var/lib/iwd/Safian.psk" );
    assert( formatIwdNetworkFilename( "Guest Wifi", false ) == "/var/lib/iwd/Guest Wifi.open" );
    assert( formatIwdNetworkFilename( "../bad/ssid", true ) == "/var/lib/iwd/=" + stringToHex( "../bad/ssid" ) + ".psk" );
    assert( formatIwdNetworkFilename( ".hidden", true ) == "/var/lib/iwd/=" + stringToHex( ".hidden" ) + ".psk" );

    // 5. Brightness scaling
    assert( formatBrightnessPercentToValue( 0 ) == 20 );
    assert( formatBrightnessPercentToValue( 100 ) == 255 );
    assert( formatBrightnessValueToPercent( 20 ) == 0 );
    assert( formatBrightnessValueToPercent( 255 ) == 100 );

    // 6. Factory Reset Commands
    assert( formatMpcFactoryResetCommand().find( "/data/prefs" ) != std::string::npos );
    assert( formatMpcFactoryResetCommand().find( "sync" ) != std::string::npos );
    assert( formatBehringerFactoryResetCommand().find( "/home/Kick/prefs" ) != std::string::npos );

    // 7. VERSION file parser
    std::string sampleVer =
        "VERSION=0.1.0-alpha\n"
        "GIT_COMMIT=bdd6e744\n"
        "BUILD_DATE=2026-11-04 02:00:00Z\n"
        "KICK_SHA256=1234567890abcdef\n";
    auto parsed = parseVersionContent( sampleVer );
    assert( parsed.isValid );
    assert( parsed.version == "0.1.0-alpha" );
    assert( parsed.commit == "bdd6e744" );
    assert( parsed.buildDate == "2026-11-04 02:00:00Z" );
    assert( parsed.sha256 == "1234567890abcdef" );

    // 7. CV Mapping & Calibration
    assert( mapCvPortToDacChannel( 1 ) == 4 );
    assert( mapCvPortToDacChannel( 2 ) == 5 );
    assert( mapCvPortToDacChannel( 3 ) == 6 );
    assert( mapCvPortToDacChannel( 4 ) == 7 );
    assert( mapCvPortToDacChannel( 5 ) == 0 );
    assert( mapCvPortToDacChannel( 6 ) == 1 );
    assert( mapCvPortToDacChannel( 7 ) == 2 );
    assert( mapCvPortToDacChannel( 8 ) == 3 ); // 8th port supported!

    // 0-based mapping compatibility
    assert( mapCvPortToDacChannel( 0 ) == 4 );
    assert( mapCvPortToDacChannel( 7 ) == 2 );

    std::array<uint16_t, 61> dacTable;
    computeCvCalibrationTable( kDefaultCvCalibration, dacTable );
    assert( dacTable[0] == 0 );
    assert( dacTable[12] == 820 );
    assert( dacTable[24] == 1640 );
    assert( dacTable[36] == 2460 );
    assert( dacTable[48] == 3280 );
    assert( dacTable[60] == 4095 );

    std::cout << "PlatformCommandTests OK" << std::endl;
    return 0;
}
