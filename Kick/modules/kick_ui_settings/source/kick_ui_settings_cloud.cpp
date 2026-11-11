// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_ui_settings_cloud.h"
#include "../../kick_ui/recent_project/kick_ProjectActions.h"
#include "../../kick_ui/modes/filebrowser/kick_sourceConfig.h"

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
        addComponent( btnGooglePath, 1, 2 );

        addComponent( toggleSplice, 2, 0 );
        addComponent( lblSplicePath, 2, 1, 1, 3 );

        addComponent( btnSpliceQr, 3, 0 );
        addComponent( btnSplicePath, 3, 1 );

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
            juce::String initGdrive = settings.cloudSources.googleDrivePath.get();
            if( initGdrive.isEmpty() )
                initGdrive = juce::File::getSpecialLocation( juce::File::userHomeDirectory ).getChildFile( "Google Drive" ).getFullPathName();
            ui::project_actions::askText( "GOOGLE DRIVE FOLDER PATH", initGdrive, [this]( juce::String path ) {
                settings.cloudSources.googleDrivePath   = path.trim();
                settings.cloudSources.enableGoogleDrive = true;
                updateStatusDisplay();
            } );
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
            juce::String curPath = settings.cloudSources.splicePath.get();
            if( curPath.isEmpty() )
                curPath = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory ).getChildFile( "Splice" ).getFullPathName();
            ui::project_actions::askText( "SPLICE FOLDER PATH", curPath, [this]( juce::String path ) {
                path = path.trim();
                settings.cloudSources.splicePath   = path;
                settings.cloudSources.enableSplice = true;
                if( path.isNotEmpty() )
                    juce::File( path ).createDirectory();
                updateStatusDisplay();
            } );
        };

        toggleWebDav.onClick = [this]() {
            settings.cloudSources.enableWebDav = toggleWebDav.getToggleState();
        };

        btnWebDavConfig.onClick = [this]() {
            if( onConfigCloudPaths )
                onConfigCloudPaths();
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
            lblGoogleUser.setText( "Status: Not Signed In (Tap 'QR Sign In' to connect)", juce::dontSendNotification );

        toggleSplice.setToggleState( settings.cloudSources.enableSplice.get(), juce::dontSendNotification );
        juce::String sPath = settings.cloudSources.splicePath.get();
        if( sPath.isEmpty() )
        {
            juce::File docs = juce::File::getSpecialLocation( juce::File::userDocumentsDirectory );
            sPath           = docs.getChildFile( "Splice" ).getFullPathName();
        }
        lblSplicePath.setText( "Folder: " + sPath, juce::dontSendNotification );

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
