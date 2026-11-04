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

#include "kick_Hwpref.h"

namespace mt
{
    namespace fs = std::filesystem;

    const char* FACTORY_RESET_CMD = "/bin/rm -rf /home/Kick/projects* /home/Kick/prefs";
    const char* POWEROFF          = "/sbin/poweroff";

    HwPref::HwPref()
        : juce::Thread( "powerbutton" )
    {
        DBG( "HwPref Constructor called" );
        //startThread();
    }

    void HwPref::run()
    {
        const char* devicePath = "/dev/input/event2"; // Replace X with the appropriate event number

        while( !threadShouldExit() )
        {
#if defined( __linux__ ) || defined( __linux ) || defined( linux )
            if( !inputDevice.is_open() ) // Check if inputDevice is not valid
            {
                // Open the input device file
                inputDevice.open( devicePath, std::ios::binary );
                if( !inputDevice.is_open() )
                {
                    DBG( "Error opening Input device" );
                    // Handle the error (e.g., return, continue loop, or retry opening)
                    continue; // Continue the loop if opening failed
                }
            }

            // Read the input event
            struct input_event ev;
            if( inputDevice.read( reinterpret_cast<char*>( &ev ), sizeof( struct input_event ) ) )
            {
                // Process the input event
                if( ev.type == EV_KEY && ev.code == KEY_STOP )
                {
                    if( ev.value == 1 )
                    {
                        DBG( "KEY_STOP pressed" );
                    }
                    else if( ev.value == 0 )
                    {
                        DBG( "KEY_STOP released" );
                        if( powerButtonUI )
                            powerButtonUI();
                    }
                }
            }
            else
            {
                continue; // Continue the loop if reading failed
            }
#endif
            juce::Thread::sleep( 300 );
        }
    }

    std::string HwPref::executeCommand( const std::string& command )
    {
        if (!hardwareEnabled()) return {};
        std::string result = "";
#if( JUCE_LINUX || __linux__ )
        char buffer[128];

        FILE* pipe = popen( command.c_str(), "r" );
        if( !pipe )
        {
            return "Error";
        }

        while( !feof( pipe ) )
        {
            if( fgets( buffer, 128, pipe ) != nullptr )
            {
                result += buffer;
            }
        }

        pclose( pipe );
#endif
        return result;
    }

    int HwPref::executeSetCommand( const std::string& command )
    {
        if (!hardwareEnabled()) return -1;
        int result = 0;
#if( JUCE_LINUX || __linux__ )
        result = runBoardCommand( command.c_str() );
#endif
        return result;
    }

    int HwPref::startDhcp( void )
    {
        return PlatformServices::getInstance().startDhcp();
    }

    int HwPref::stopDhcp( void )
    {
        return PlatformServices::getInstance().stopDhcp();
    }

    int HwPref::factoryReset( void )
    {
        return PlatformServices::getInstance().factoryReset();
    }

    int HwPref::shutdown( void )
    {
        return PlatformServices::getInstance().powerOff();
    }

    // Function to compare firmware versions
    mt::VersionComparisonResult HwPref::isLinuxFirmwareVersionLess( const mt::linuxFwInfo& current, const mt::linuxFwInfo& stored )
    {
        if( current.major < stored.major )
        {
            return mt::VersionComparisonResult::Less;
        }
        else if( current.major == stored.major )
        {
            if( current.minor < stored.minor )
            {
                return mt::VersionComparisonResult::Less;
            }
            else if( current.minor == stored.minor )
            {
                if( current.patch < stored.patch )
                {
                    return mt::VersionComparisonResult::Less;
                }
                else if( current.patch == stored.patch )
                {
                    return mt::VersionComparisonResult::Equal;
                }
            }
        }
        return mt::VersionComparisonResult::Greater;
    }

    // Function to compare mcu versions
    mt::VersionComparisonResult HwPref::isMCUFirmwareVersionLess( const mt::mcuFwInfo& current, const mt::mcuFwInfo& stored )
    {
        if( current.major < stored.major )
        {
            return mt::VersionComparisonResult::Less;
        }
        else if( current.major == stored.major )
        {
            if( current.minor < stored.minor )
            {
                return mt::VersionComparisonResult::Less;
            }
            else if( current.minor == stored.minor )
            {
                return mt::VersionComparisonResult::Equal;
            }
        }
        return mt::VersionComparisonResult::Greater;
    }

    std::string HwPref::isImageFound( juce::String Path, juce::String format )
    {
        DBG( "isImageFound checking in Folder path " << Path << "Format of file " << format );
        std::vector<std::string> swuFiles = getSWUFiles( Path.toStdString(), format.toStdString() );
        if( swuFiles.empty() )
        {
            DBG( "No SWU files found in the folder." );
            return "";
        }
        else
        {
            DBG( "SWU Files in the folder:" );
            for( const auto& swuFile : swuFiles )
            {
                DBG( swuFile );
            }
        }
        fwFileName     = swuFiles.at( 0 ); // taking whichever is first
        fwFileFullpath = Path.toStdString() + "/" + swuFiles.at( 0 );
        DBG( "FWFile Name -> " << fwFileName );
        DBG( "FWFile Name -> " << fwFileFullpath );
        return fwFileFullpath;
    }

    bool HwPref::extractImage( std::string path )
    {
        if (!hardwareEnabled()) return false;
        bool        status         = true;
        std::string imageFormat    = ".swu";
        std::string creatFolderCmd = "mkdir " + path;
        DBG( "FwUpdate ->extractImage->  " << creatFolderCmd );
        runBoardCommand( creatFolderCmd.c_str() );
        std::string outFullpath = decrptionDir + decryptedFwname;
        std::string cmd         = "mount -o loop -t ext4  " + outFullpath + " " + extractDir;
        DBG( "FwUpdate ->extractImage->  " << cmd );
        runBoardCommand( cmd.c_str() );
        std::vector<std::string> swuFiles = getSWUFiles( extractDir, imageFormat );
        if( swuFiles.empty() )
        {
            DBG( "FwUpdate -> After exraction image file In " << extractDir << " Is not Found" );
            return false;
        }
        else
        {
            DBG( "FwUpdate -> After exraction image file In " << extractDir << " Is Found" );
            for( const auto& swuFile : swuFiles )
            {
                DBG( swuFile );
            }
        }
        return status;
    }
    juce::String HwPref::getDevSerial()
    {
        return PlatformServices::getInstance().getDevSerial();
    }

    juce::String HwPref::getFirmwareVersion()
    {
        return PlatformServices::getInstance().getFirmwareVersion();
    }

    bool HwPref::getLinuxVersion( std::string& path, mt::linuxFwInfo& linfo )
    {
        std::string   filePath = path + "version.txt";
        std::ifstream file( filePath );

        if( !file.is_open() )
        {
            DBG( "FwUpdate -> getFwVersion Error opening file: " << filePath );
            return false;
        }

        std::string line;

        // Read the first line for Linux firmware information
        if( std::getline( file, line ) )
        {
            // Define a regular expression for parsing Linux firmware information
            std::regex  linuxRegex( "^([a-zA-Z]+) (\\d+)\\.(\\d+)\\.(\\d+)$" );
            std::smatch match;

            // Try to match the line against the regular expression
            if( std::regex_match( line, match, linuxRegex ) )
            {
                linfo.linuxname = match[1];
                linfo.major     = std::stoi( match[2] );
                linfo.minor     = std::stoi( match[3] );
                linfo.patch     = std::stoi( match[4] );
                return true;
                // Display parsed information
            }
            else
            {
                DBG( "Error parsing Linux firmware information. Invalid format." );
                file.close();
                return false;
            }
        }
        else
        {
            DBG( "Error reading Linux firmware information from file." );
            file.close();
            return false;
        }
    }

    bool HwPref::getFwVersion( std::string& path, mt::linuxFwInfo& linfo, mt::mcuFwInfo& minfo )
    {
        std::string   filePath = path + "version.txt";
        std::ifstream file( filePath );

        if( !file.is_open() )
        {
            DBG( "FwUpdate -> getFwVersion Error opening file: " << filePath );
            return false;
        }

        std::string line;

        // Read the first line for Linux firmware information
        if( std::getline( file, line ) )
        {
            // Define a regular expression for parsing Linux firmware information
            std::regex  linuxRegex( "^([a-zA-Z]+) (\\d+)\\.(\\d+)\\.(\\d+)$" );
            std::smatch match;

            // Try to match the line against the regular expression
            if( std::regex_match( line, match, linuxRegex ) )
            {
                linfo.linuxname = match[1];
                linfo.major     = std::stoi( match[2] );
                linfo.minor     = std::stoi( match[3] );
                linfo.patch     = std::stoi( match[4] );

                // Display parsed information
            }
            else
            {
                DBG( "Error parsing Linux firmware information. Invalid format." );
                file.close();
                return false;
            }
        }
        else
        {
            DBG( "Error reading Linux firmware information from file." );
            file.close();
            return false;
        }

        // Read the second line for MCU firmware information
        if( std::getline( file, line ) )
        {
            // Define a regular expression for parsing MCU firmware information
            std::regex  mcuRegex( "^([a-zA-Z]+) (\\d+)\\.(\\d+)$" );
            std::smatch mcuMatch;

            // Try to match the line against the regular expression
            if( std::regex_match( line, mcuMatch, mcuRegex ) )
            {
                minfo.mcuname = mcuMatch[1];
                minfo.major   = std::stoi( mcuMatch[2] );
                minfo.minor   = std::stoi( mcuMatch[3] );

                // Display parsed information
            }
            else
            {
                DBG( "Error parsing MCU firmware information. Invalid format." );
                file.close();
                return false;
            }
        }
        else
        {
            DBG( "Error reading MCU firmware information from file." );
            file.close();
            return false;
        }

        file.close();
        return true;
    }

    bool HwPref::addVersionNotoFile( std::string filename )
    {
        return true;
    }

    bool HwPref::getVersionfromFile( std::string filename, linuxFwInfo& info )
    {
        fw_ver_index = -1;
        // Open the encrypted file
        std::ifstream encrypted_file( filename, std::ios::binary );
        if( !encrypted_file )
        {
            DBG( "Error opening encrypted filei" );
            return false;
        }

        // Define buffer to store the version information
        char version_buffer[20]; // 20 characters for version, 1 for null terminator

        // Read the version information from the file
        if( !encrypted_file.read( version_buffer, 20 ) )
        {
            DBG( "Error reading version information." );
            return false;
        }

        // Close the file
        encrypted_file.close();

        for( auto i = 0; i < 20; i++ )
            if( ( version_buffer[i] == 'E' ) || ( version_buffer[i] == 'R' ) )
                fw_ver_index = ( i + 2 );

        if( fw_ver_index < 0 )
        {
            DBG( "Version is not correct" );
            return false;
        }
        version_buffer[fw_ver_index] = '\0';

        // Parse the version buffer and fill the linuxFwInfo structure
        std::istringstream version_stream( std::string( version_buffer, fw_ver_index ) );

        if( !( version_stream >> info.major ) )
        {
            DBG( "Error parsing major version." );
            return false;
        }

        if( version_stream.get() != '.' || !( version_stream >> info.minor ) )
        {
            DBG( "Error parsing minor version." );
            return false;
        }

        if( version_stream.get() != '.' || !( version_stream >> info.patch ) )
        {
            DBG( "Error parsing patch version." );
            return false;
        }

        // Output the version information
        DBG( "Version from future Update File is=" << filename << ":" << info.major << "." << info.minor << "." << info.patch );
        return true;
    }

    bool HwPref::isImgFile( std::string filepath )
    {
        // Check if the file extension is ".img"
        return fs::path( filepath ).extension() == ".img";
    }

    bool HwPref::removeHeaderFromImage( std::string fwfilepath )
    {
        if (!hardwareEnabled()) return false;
        std::string cmd = "tail -c +" + std::to_string( fw_ver_index ) + " " + fwfilepath + " > " + decrptionDir + fwHeaderFile;
        DBG( "removing Header from image" << cmd );
        int result = runBoardCommand( cmd.c_str() ); // Execute the command using system call

	if ( result != 0 )
    {
        DBG( "Failed to remove header from image." );
        return false;
    }

    // Now remove the original file
    if ( std::remove( fwfilepath.c_str() ) != 0 )
    {
        DBG( "Error deleting original file: " << fwfilepath );
        return false;
    }
    else
    {
        DBG( "Successfully deleted original file: " << fwfilepath );
    }
    	return true;

    }

    bool HwPref::decryptopenSSLImage( std::string fwfilepath )
    {
        if (!hardwareEnabled()) return false;
        bool        status        = false;
        std::string outFwfilepath = decrptionDir + decryptedFwname;
        std::string inFwfilepath  = decrptionDir + fwHeaderFile;
        std::string cmd           = "mkdir -p " + decrptionDir; // Create decryption directory if not exists
#if( JUCE_LINUX || __linux__ )
        runBoardCommand( cmd.c_str() ); // Execute the command using system call to create directory

        if( isImgFile( fwfilepath ) )
        { // Check if it's an img file
            if( removeHeaderFromImage( fwfilepath ) )
            { // Remove header from image
                std::string command = "openssl enc -aes-256-cbc -pbkdf2 -d -salt -in " + inFwfilepath + " -out " + outFwfilepath + " -k '" + decrptKey + "'";

                std::cout << "Decrypting image: " << command << std::endl;
                int result = runBoardCommand( command.c_str() ); // Execute the command using system call

                if( result == 0 )
                {
                    std::cout << "Decryption successful" << std::endl;
                    return true;
                }
                else
                {
                    std::cout << "Incorrect password or decryption failed" << std::endl;
                    return false;
                }
            }
            else
            {
                std::cout << "Header removal failed" << std::endl;
                return false;
            }
        }
        else
        {
            std::cout << "Not an IMG file" << std::endl;
            return false;
        }
#endif
        return true;
    }
    std::vector<std::pair<std::string, linuxFwInfo>> HwPref::getSWUFileswithVersion( const std::string& folderPath, const std::string& imageFormat )
    {
        std::vector<std::pair<std::string, linuxFwInfo>> swuFiles;
        if( fs::exists( folderPath ) && fs::is_directory( folderPath ) )
        {
            try
            {
                bool foundSWU = false;
                for( const auto& entry : fs::directory_iterator( folderPath ) )
                {
                    if( entry.path().extension() == imageFormat )
                    {
                        std::string fullpath = folderPath + "/" + entry.path().filename().string();
                        linuxFwInfo info;
                        if( getVersionfromFile( fullpath, info ) )
                        {
                            swuFiles.push_back( std::make_pair( fullpath, info ) );
                            DBG( "FwUpdate file -> " << fullpath << " Version: " << info.major << "." << info.minor << "." << info.patch );
                            foundSWU = true;
                        }
                        else
                        {
                            DBG( "Error extracting version information from file: " << entry.path().string() );
                        }
                    }
                }
                if( !foundSWU )
                {
                    DBG( "No SWU files found in the specified directory." );
                }
            } catch( const std::filesystem::filesystem_error& e )
            {
                DBG( "Error accessing directory: " << e.what() );
            }
        }
        else
        {
            DBG( "The path is not valid or does not exist." );
        }
        return swuFiles;
    }

    std::vector<std::string> HwPref::getSWUFiles( const std::string& folderPath, const std::string& imageFormat )
    {
        std::vector<std::string> swuFiles;
        if( fs::exists( folderPath ) && fs::is_directory( folderPath ) )
        {
            try
            {
                for( const auto& entry : fs::directory_iterator( folderPath ) )
                {
                    if( entry.path().extension() == imageFormat )
                    {
                        swuFiles.push_back( entry.path().filename().string() );
                        DBG( "FwUpate files -> " << entry.path().filename().string() );
                    }
                }
            } catch( const std::filesystem::filesystem_error& e )
            {
                DBG( "Error accessing directory: " << e.what() );
            }
        }
        else
        {
            DBG( "The path is not valid or does not exist." );
        }
        return swuFiles;
    }

    bool HwPref::isEthernetCableConnected()
    {
        return PlatformServices::getInstance().isEthernetCableConnected();
    }

    bool HwPref::ethernetEnable( bool status )
    {
        return PlatformServices::getInstance().ethernetEnable( status );
    }

    bool HwPref::isInterfaceUp()
    {
        return PlatformServices::getInstance().isEthernetInterfaceUp();
    }

    int HwPref::setIP( const std::string& interface, const std::string& ipAddress, const std::string& subnetMask, const std::string& gateway, const std::string& dns )
    {
        juce::ignoreUnused( interface );
        return PlatformServices::getInstance().setEthernetIP( ipAddress, subnetMask, gateway, dns );
    }

    std::string HwPref::getSubnetMask( const std::string& interface )
    {
        sysEthInfo info;
        getIP( interface, info );
        return info.subnet;
    }

    std::string HwPref::getGateway( const std::string& interface )
    {
        sysEthInfo info;
        getIP( interface, info );
        return info.gateway;
    }

    void HwPref::getIP( const std::string interface, sysEthInfo& info )
    {
        if( interface == "wlan0" )
            PlatformServices::getInstance().getWiFiIP( info );
        else
            PlatformServices::getInstance().getEthernetIP( info );
    }

    int HwPref::getDateTime( dateInfo& date, timeInfo& time, const juce::String& timezone )
    {
        return PlatformServices::getInstance().getDateTime( date, time, timezone );
    }

    int HwPref::setTimeZone( juce::String& str )
    {
        return PlatformServices::getInstance().setTimeZone( str );
    }

    int HwPref::setBrightness( int value )
    {
        return PlatformServices::getInstance().setBrightness( value );
    }

    DeviceInfo HwPref::getDeviceInfo()
    {
        return PlatformServices::getInstance().getDeviceInfo();
    }

    bool HwPref::isStorageconnected( std::string id ) // flash-> usb , sdcard->SDCARD
    {
#if defined( __linux__ ) || defined( __linux ) || defined( linux )
        std::fstream versionFile;
        versionFile.open( "/proc/mounts", std::ios::in );

        if( versionFile.is_open() )
        {
            std::string str;
            while( getline( versionFile, str ) )
            {
                if( str.find( id ) != std::string::npos )
                {
                    DBG( "USB Storage is connected" );
                    return true;
                }
            }
        }
#endif
        DBG( " " << id << " Storage is not connected" );
        return false;
    }

    void HwPref::setDate( int day, int month, int year )
    {
        dateInfo d;
        timeInfo t;
        PlatformServices::getInstance().getDateTime( d, t, "" );
        PlatformServices::getInstance().setDateTime( day, month, year, t.HH, t.MM );
    }

    void HwPref::setTime( int hour, int minute )
    {
        dateInfo d;
        timeInfo t;
        PlatformServices::getInstance().getDateTime( d, t, "" );
        PlatformServices::getInstance().setDateTime( d.DD, d.MM, d.YYYY, hour, minute );
    }

    // Get the number of days in a month, considering leap years
    int HwPref::getDaysInMonth( int month, int year )
    {
        return PlatformServices::getInstance().getDaysInMonth( month, year );
    }
}
