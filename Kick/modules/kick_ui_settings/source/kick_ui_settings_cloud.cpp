// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_ui_settings_cloud.h"
#include "../../kick_ui/recent_project/kick_ProjectActions.h"
#include "../../kick_ui/modes/filebrowser/kick_sourceConfig.h"
#include "../../kick_ui/modes/filebrowser/kick_incomingFiles.h"

namespace kick_ui
{
    SettingsCloud::SettingsCloud()
        : SettingsInterface( 5, 4, true )
    {
        ui::KickGrid::setBackgroundColour( juce::Colours::transparentBlack );
        setRowTitles( { "Google Drive", "Google Actions", "Splice Sound Sync", "Splice Actions", "Network Storage" } );

        lblGoogleUser.setValueColour( ui::Theme::textColour( 1 ) );
        lblGoogleUser.setValueFontHeight( 15.0f );
        lblGoogleUser.setValueJustification( juce::Justification::centredLeft );
        lblGoogleUser.showBorder( false );

        lblSplicePath.setValueColour( ui::Theme::textColour( 1 ) );
        lblSplicePath.setValueFontHeight( 15.0f );
        lblSplicePath.setValueJustification( juce::Justification::centredLeft );
        lblSplicePath.showBorder( false );

        addComponent( toggleGoogleDrive, 0, 0 );
        addComponent( lblGoogleUser, 0, 1, 1, 3 );

        addComponent( btnGoogleLogin, 1, 0 );
        addComponent( btnGoogleDisconnect, 1, 1 );
#if !JUCE_IOS
        addComponent( btnGooglePath, 1, 2 );
#endif

        addComponent( toggleSplice, 2, 0 );
        addComponent( lblSplicePath, 2, 1, 1, 3 );

        addComponent( btnSpliceQr, 3, 0 );
#if !JUCE_IOS
        addComponent( btnSplicePath, 3, 1 );
#endif

#if JUCE_IOS
        btnGoogleLogin.setButtonText( "Sign In..." );
        btnSpliceQr.setButtonText( "Splice Info..." );
#endif

        addComponent( toggleWebDav, 4, 0 );
        addComponent( btnWebDavConfig, 4, 1 );

        toggleGoogleDrive.onClick = [this]() {
            settings.cloudSources.enableGoogleDrive = toggleGoogleDrive.getToggleState();
            updateStatusDisplay();
        };

        btnGoogleLogin.onClick = [this]() {
            if( onOpenCloudAuth )
                onOpenCloudAuth( 0 ); // 0 = Google Drive
        };

        btnGoogleDisconnect.onClick = [this]() {
            settings.cloudSources.googleUserEmail = "";
            const auto prefs     = getAppDirs( MTPrefs );
            const auto secretDir = prefs.getChildFile( "file_sources" );
            if( secretDir.isDirectory() )
            {
                for( auto iter : juce::RangedDirectoryIterator( secretDir, false, "gdrive-*.secret" ) )
                    iter.getFile().deleteFile();
            }
            auto sources = ui::fileSource::loadSources( prefs.getChildFile( "file_sources.txt" ).getFullPathName().toStdString() );
            sources.erase( std::remove_if( sources.begin(), sources.end(), []( const ui::fileSource::SourceConfig& c ) {
                return c.type == "gdrive";
            } ), sources.end() );
            ui::fileSource::saveSources( prefs.getChildFile( "file_sources.txt" ).getFullPathName().toStdString(), sources );
            updateStatusDisplay();
        };

        btnGooglePath.onClick = [this]() {
#if !JUCE_IOS
            juce::String initGdrive = settings.cloudSources.googleDrivePath.get();
            if( initGdrive.isEmpty() || initGdrive.contains( "/root" ) )
            {
#if JUCE_LINUX || __linux__
                initGdrive = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory ).getChildFile( "Google Drive" ).getFullPathName();
#else
                initGdrive = juce::File::getSpecialLocation( juce::File::userHomeDirectory ).getChildFile( "Google Drive" ).getFullPathName();
#endif
            }
            ui::project_actions::askText( "GOOGLE DRIVE FOLDER PATH", initGdrive, [this]( juce::String path ) {
                settings.cloudSources.googleDrivePath   = path.trim();
                settings.cloudSources.enableGoogleDrive = true;
                updateStatusDisplay();
            } );
#endif
        };

        toggleSplice.onClick = [this]() {
            settings.cloudSources.enableSplice = toggleSplice.getToggleState();
            updateStatusDisplay();
        };

        btnSpliceQr.onClick = [this]() {
            if( onOpenCloudAuth )
                onOpenCloudAuth( 1 ); // 1 = Splice
        };

        btnSplicePath.onClick = [this]() {
#if JUCE_IOS
            auto spliceDir = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory ).getChildFile( "Splice" );
            spliceDir.createDirectory();
            settings.cloudSources.splicePath   = spliceDir.getFullPathName();
            settings.cloudSources.enableSplice = true;
            updateStatusDisplay();
#else
            juce::String curPath = settings.cloudSources.splicePath.get();
            if( curPath.isEmpty() || curPath.contains( "/root" ) )
            {
#if JUCE_LINUX || __linux__
                curPath = juce::File( "/sdcard/Splice" ).isDirectory() ? "/sdcard/Splice" : juce::File::getSpecialLocation( juce::File::userDocumentsDirectory ).getChildFile( "Splice" ).getFullPathName();
#else
                curPath = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory ).getChildFile( "Splice" ).getFullPathName();
#endif
            }
            ui::project_actions::askText( "SPLICE FOLDER PATH", curPath, [this]( juce::String path ) {
                path = path.trim();
                settings.cloudSources.splicePath   = path;
                settings.cloudSources.enableSplice = true;
                if( path.isNotEmpty() )
                    juce::File( path ).createDirectory();
                updateStatusDisplay();
            } );
#endif
        };

        toggleWebDav.onClick = [this]() {
            settings.cloudSources.enableWebDav = toggleWebDav.getToggleState();
        };

        btnWebDavConfig.onClick = [this]() {
            if( onConfigWebDav )
                onConfigWebDav();
        };

        updateStatusDisplay();
    }

    void SettingsCloud::updateStatusDisplay()
    {
        toggleGoogleDrive.setToggleState( settings.cloudSources.enableGoogleDrive.get(), juce::dontSendNotification );
        const juce::String gEmail = settings.cloudSources.googleUserEmail.get();
        if( gEmail.isNotEmpty() )
            lblGoogleUser.setText( "Status: Connected (" + gEmail + ")", juce::dontSendNotification );
        else
        {
            // Check for mounted Google Drive
            juce::File home = juce::File::getSpecialLocation( juce::File::userHomeDirectory );
            juce::File cloudStorage = home.getChildFile( "Library/CloudStorage" );
            juce::String localUser;
            if( cloudStorage.isDirectory() )
            {
                for( auto iter : juce::RangedDirectoryIterator( cloudStorage, false, "*GoogleDrive*" ) )
                {
                    if( iter.getFile().isDirectory() )
                    {
                        juce::String fn = iter.getFile().getFileName();
                        localUser = fn.startsWith( "GoogleDrive-" ) ? fn.substring( 12 ) : fn;
                        break;
                    }
                }
            }
            if( localUser.isNotEmpty() )
                lblGoogleUser.setText( "Status: Mounted Locally (" + localUser + ")", juce::dontSendNotification );
            else
            {
#if JUCE_IOS
                lblGoogleUser.setText( "Status: Not Signed In (Tap 'Sign In...' to connect)", juce::dontSendNotification );
#else
                lblGoogleUser.setText( "Status: Not Signed In (Tap 'QR Sign In' to connect)", juce::dontSendNotification );
#endif
            }
        }

        toggleSplice.setToggleState( settings.cloudSources.enableSplice.get(), juce::dontSendNotification );

        juce::File docs = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory );
        std::vector<std::string> roots = {
            docs.getFullPathName().toStdString(),
            docs.getChildFile( "Kick" ).getFullPathName().toStdString(),
#if !JUCE_LINUX && !__linux__
            juce::File::getSpecialLocation( juce::File::userHomeDirectory ).getFullPathName().toStdString(),
#else
            "/sdcard",
            "/sdcard/Splice",
            "/media/az01-internal-sd",
            "/media/az01-internal-sd/Splice",
            "/data",
#endif
        };
        juce::String curPath = settings.cloudSources.splicePath.get();
#if JUCE_LINUX || __linux__
        std::string fallback = juce::File( "/sdcard/Splice" ).isDirectory() ? "/sdcard/Splice" : docs.getChildFile( "Splice" ).getFullPathName().toStdString();
#else
        std::string fallback = docs.getChildFile( "Splice" ).getFullPathName().toStdString();
#endif
        std::string resolved = kick_incoming::resolveSpliceDir( curPath.toStdString(), roots, fallback );
        juce::File sdir( juce::String::fromUTF8( resolved.c_str() ) );
        if( curPath != sdir.getFullPathName() )
            settings.cloudSources.splicePath = sdir.getFullPathName();

        juce::File spliceToken( "/data/Settings/splice-authtoken" );
        if( spliceToken.existsAsFile() )
        {
            juce::String user = spliceToken.loadFileAsString().trim().upToFirstOccurrenceOf( ";", false, false );
            int numSamples = 0;
            if( sdir.isDirectory() )
            {
                auto all = sdir.getChildFile( "All Your Samples" );
                if( all.isDirectory() )
                    numSamples = all.getNumberOfChildFiles( juce::File::findFiles );
                else
                    numSamples = sdir.getNumberOfChildFiles( juce::File::findFiles | juce::File::ignoreHiddenFiles );
            }
            lblSplicePath.setText( "Status: Connected (" + user + ")  |  Folder: " + sdir.getFullPathName() + ( numSamples > 0 ? " (" + juce::String( numSamples ) + " samples)" : "" ), juce::dontSendNotification );
        }
        else
        {
#if JUCE_IOS
            int numSamples = 0;
            if( sdir.isDirectory() )
                numSamples = sdir.getNumberOfChildFiles( juce::File::findFiles | juce::File::ignoreHiddenFiles );
            lblSplicePath.setText( "Location: Files app > On My iPhone > Kick > Splice" + ( numSamples > 0 ? " (" + juce::String( numSamples ) + " samples)" : "" ), juce::dontSendNotification );
#else
            lblSplicePath.setText( "Folder: " + sdir.getFullPathName(), juce::dontSendNotification );
#endif
        }

        toggleWebDav.setToggleState( settings.cloudSources.enableWebDav.get(), juce::dontSendNotification );
    }

    void SettingsCloud::refresh()
    {
        updateStatusDisplay();
    }

    void SettingsCloud::setupHeader( kick_ui::KickModeHeader& Hdr )
    {
        Hdr.setTitle( "Preferences - Cloud & Accounts" );
    }

    void SettingsCloud::handleCommand( CPktMgr::HWMsg_t& ) {}
}
