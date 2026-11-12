// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_cloudAuthPopup.h"
#include "kick_ui/recent_project/kick_ProjectActions.h"

namespace ui
{
    CloudAuthQrPopup::CloudAuthQrPopup( Service service )
        : serviceType( service )
    {
        titleLabel.setValueColour( ui::Theme::titleColour( 1 ) );
        titleLabel.setValueFontHeight( 20.0f );
        titleLabel.setValueJustification( juce::Justification::centredLeft );
        titleLabel.showBorder( false );
        addAndMakeVisible( titleLabel );

        instructionsLabel.setValueColour( ui::Theme::textColour( 1 ) );
        instructionsLabel.setValueFontHeight( 15.0f );
        instructionsLabel.setValueJustification( juce::Justification::topLeft );
        instructionsLabel.showBorder( false );
        addAndMakeVisible( instructionsLabel );

        codeLabel.setValueColour( ui::Theme::textColour( 2 ) );
        codeLabel.setValueFontHeight( 18.0f );
        codeLabel.setValueJustification( juce::Justification::centredLeft );
        codeLabel.showBorder( false );
        addAndMakeVisible( codeLabel );

        statusLabel.setValueColour( ui::Theme::textColour( 3 ) );
        statusLabel.setValueFontHeight( 14.0f );
        statusLabel.setValueJustification( juce::Justification::centredLeft );
        statusLabel.showBorder( false );
        addAndMakeVisible( statusLabel );

        addAndMakeVisible( qrCode );

        actionBtn.setValueColour( ui::Theme::textColour( 1 ) );
        actionBtn.setValueBackgroundColour( ui::Theme::backgroundColour( 2 ) );
        actionBtn.setValueFontHeight( 15.0f );
        actionBtn.onClick = [this]() {
            stopTimer();
            mt::utils::exitDialog( this, UI::CANCEL );
        };
        addAndMakeVisible( actionBtn );

        manualBtn.setValueColour( ui::Theme::textColour( 1 ) );
        manualBtn.setValueBackgroundColour( ui::Theme::backgroundColour( 2 ) );
        manualBtn.setValueFontHeight( 14.0f );
        manualBtn.setVisible( false );
        addAndMakeVisible( manualBtn );

        folderBtn.setValueColour( ui::Theme::textColour( 1 ) );
        folderBtn.setValueBackgroundColour( ui::Theme::backgroundColour( 2 ) );
        folderBtn.setValueFontHeight( 14.0f );
        folderBtn.setVisible( false );
        addAndMakeVisible( folderBtn );

        syncBtn.setValueColour( ui::Theme::textColour( 1 ) );
        syncBtn.setValueBackgroundColour( ui::Theme::backgroundColour( 2 ) );
        syncBtn.setValueFontHeight( 14.0f );
        syncBtn.setVisible( false );
        addAndMakeVisible( syncBtn );

        reloginBtn.setValueColour( ui::Theme::textColour( 1 ) );
        reloginBtn.setValueBackgroundColour( ui::Theme::backgroundColour( 2 ) );
        reloginBtn.setValueFontHeight( 14.0f );
        reloginBtn.setVisible( false );
        addAndMakeVisible( reloginBtn );

        pairingServer = std::make_unique<PhonePairingServer>();
        juce::Component::SafePointer<CloudAuthQrPopup> safeThis( this );

        pairingServer->getStatus = []() -> PhonePairingServer::StatusInfo {
            PhonePairingServer::StatusInfo st;
            st.googleEmail = mt::getSettings().cloudSources.googleUserEmail.get();
            st.googleConnected = st.googleEmail.isNotEmpty();

            juce::File spliceToken( "/data/Settings/splice-authtoken" );
            if( spliceToken.existsAsFile() )
            {
                juce::String tok = spliceToken.loadFileAsString().trim();
                st.spliceUser = tok.upToFirstOccurrenceOf( ";", false, false );
                st.spliceConnected = true;
            }
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
                "/media/az01-internal-sd/MPC Documents/Splice",
                "/data",
#endif
            };
            juce::String curPath = mt::getSettings().cloudSources.splicePath.get();
#if JUCE_LINUX || __linux__
            std::string fallback = juce::File( "/sdcard/Splice" ).isDirectory() ? "/sdcard/Splice" : docs.getChildFile( "Splice" ).getFullPathName().toStdString();
#else
            std::string fallback = docs.getChildFile( "Splice" ).getFullPathName().toStdString();
#endif
            std::string resolved = kick_incoming::resolveSpliceDir( curPath.toStdString(), roots, fallback );
            st.spliceFolder = juce::String::fromUTF8( resolved.c_str() );
            juce::File sdir( st.spliceFolder );
            if( sdir.isDirectory() )
            {
                auto allSamples = sdir.getChildFile( "All Your Samples" );
                if( allSamples.isDirectory() )
                    st.spliceSamples = allSamples.getNumberOfChildFiles( juce::File::findFiles );
                else
                    st.spliceSamples = sdir.getNumberOfChildFiles( juce::File::findFiles | juce::File::ignoreHiddenFiles );
            }
            return st;
        };

        pairingServer->onGoogleDriveAuth = [safeThis]( const juce::String& credInput, std::function<void( bool, const juce::String&, const juce::String& )> done ) {
            juce::MessageManager::callAsync( [safeThis, credInput, done]() {
                if( safeThis != nullptr )
                    safeThis->processGoogleToken( credInput, done );
                else if( done )
                    done( false, {}, "Dialog closed" );
            } );
        };

        pairingServer->onSpliceSync = [safeThis]( std::function<void( bool, const juce::String& )> done ) {
            juce::MessageManager::callAsync( [safeThis, done]() {
                if( safeThis != nullptr )
                    safeThis->triggerSpliceSync();
                if( done )
                    done( true, "Splice sync started on device" );
            } );
        };

        if( serviceType == Service::GoogleDrive )
        {
            titleLabel.setText( "GOOGLE DRIVE SIGN-IN", juce::dontSendNotification );
            manualBtn.setVisible( true );
            manualBtn.onClick = [safeThis]() {
                const juce::String curId = mt::getSettings().cloudSources.googleClientId.get();
                ui::project_actions::askText( "GOOGLE OAUTH CLIENT ID OR TOKEN (ya29... / 1//...)", curId, [safeThis]( juce::String credInput ) {
                    if( safeThis == nullptr )
                        return;
                    safeThis->processGoogleToken( credInput );
                } );
            };

            startGoogleDeviceFlow();
        }
        else
        {
            setupSplice();
        }
    }

    CloudAuthQrPopup::~CloudAuthQrPopup()
    {
        stopTimer();
        pairingServer.reset();
    }

    void CloudAuthQrPopup::setupSplice()
    {
        titleLabel.setText( "SPLICE SOUND SYNC", juce::dontSendNotification );

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
            "/media/az01-internal-sd/MPC Documents/Splice",
            "/data",
#endif
        };
        juce::String curPath = mt::getSettings().cloudSources.splicePath.get();
#if JUCE_LINUX || __linux__
        std::string fallback = juce::File( "/sdcard/Splice" ).isDirectory() ? "/sdcard/Splice" : docs.getChildFile( "Splice" ).getFullPathName().toStdString();
#else
        std::string fallback = docs.getChildFile( "Splice" ).getFullPathName().toStdString();
#endif
        std::string resolved = kick_incoming::resolveSpliceDir( curPath.toStdString(), roots, fallback );
        juce::File spliceDir( juce::String::fromUTF8( resolved.c_str() ) );
        if( !spliceDir.exists() )
            spliceDir.createDirectory();

        mt::getSettings().cloudSources.splicePath = spliceDir.getFullPathName();
        mt::getSettings().cloudSources.enableSplice = true;

        juce::File spliceBinary( "/usr/share/Splice/splicesync" );
        juce::File spliceToken( "/data/Settings/splice-authtoken" );

        if( spliceBinary.existsAsFile() && spliceToken.existsAsFile() )
        {
            const juce::String tokContent = spliceToken.loadFileAsString().trim();
            const juce::String user = tokContent.upToFirstOccurrenceOf( ";", false, false );

            int numSamples = 0;
            auto allSamples = spliceDir.getChildFile( "All Your Samples" );
            if( allSamples.isDirectory() )
                numSamples = allSamples.getNumberOfChildFiles( juce::File::findFiles );

            qrCode.setText( "https://splice.com/link" );
            instructionsLabel.setText( "Connected Splice Account: " + user + "\nFolder: " + spliceDir.getFullPathName() + ( numSamples > 0 ? " (" + juce::String( numSamples ) + " samples)" : "" ) + "\n\nAll loops and samples appear in File Browser under Places.", juce::dontSendNotification );
            codeLabel.setText( "Account: " + user + "  (Status: Active)", juce::dontSendNotification );
            statusLabel.setText( "Tap 'Sync Samples Now' to sync newly added samples.", juce::dontSendNotification );

            syncBtn.setVisible( true );
            syncBtn.onClick = [this]() { triggerSpliceSync(); };

            reloginBtn.setVisible( true );
            reloginBtn.onClick = [this]() { startSpliceLogin(); };
        }
        else if( spliceBinary.existsAsFile() )
        {
            startSpliceLogin();
        }
        else
        {
            qrCode.setText( "https://splice.com/link" );
            instructionsLabel.setText( "1. Scan the QR code with your phone camera, or visit:\n   https://splice.com/link\n2. Sign in to your Splice account.", juce::dontSendNotification );
            codeLabel.setText( "Splice Link: https://splice.com/link", juce::dontSendNotification );
            statusLabel.setText( "Splice folder: " + spliceDir.getFullPathName(), juce::dontSendNotification );

            folderBtn.setVisible( true );
            juce::Component::SafePointer<CloudAuthQrPopup> safeThis( this );
            folderBtn.onClick = [safeThis, spliceDir]() {
                if( safeThis == nullptr )
                    return;
                ui::project_actions::askText( "SPLICE FOLDER PATH", spliceDir.getFullPathName(), [safeThis]( juce::String newPath ) {
                    if( safeThis == nullptr )
                        return;
                    newPath = newPath.trim();
                    if( newPath.isNotEmpty() )
                    {
                        juce::File f( newPath );
                        f.createDirectory();
                        mt::getSettings().cloudSources.splicePath   = newPath;
                        mt::getSettings().cloudSources.enableSplice = true;
                        safeThis->statusLabel.setText( "Splice folder: " + newPath, juce::dontSendNotification );
                    }
                } );
            };
        }

        actionBtn.setButtonText( "Done" );
        actionBtn.onClick = [this]() {
            mt::getSettings().cloudSources.enableSplice = true;
            if( onAuthSuccess )
                onAuthSuccess();
            mt::utils::exitDialog( this, UI::APPLY );
        };
    }

    void CloudAuthQrPopup::triggerSpliceSync()
    {
#if JUCE_LINUX || __linux__
        statusLabel.setText( "Syncing samples with Splice cloud...", juce::dontSendNotification );
        syncBtn.setEnabled( false );
        juce::Component::SafePointer<CloudAuthQrPopup> safeThis( this );

        juce::Thread::launch( [safeThis]() {
            const juce::String cmd = "/usr/share/Splice/splicesync -authTokenPath /data/Settings/splice-authtoken -path /sdcard/Splice -logPath /tmp/splicesync.log";
            int ret = std::system( cmd.toRawUTF8() );

            juce::MessageManager::callAsync( [safeThis, ret]() {
                if( safeThis == nullptr )
                    return;
                safeThis->syncBtn.setEnabled( true );
                if( ret == 0 )
                {
                    safeThis->statusLabel.setText( "Sync complete! Samples refreshed in File Browser.", juce::dontSendNotification );
                    safeThis->statusLabel.setValueColour( juce::Colours::lightgreen );
                    if( safeThis->onAuthSuccess )
                        safeThis->onAuthSuccess();
                }
                else
                {
                    safeThis->statusLabel.setText( "Sync finished (code " + juce::String( ret ) + ").", juce::dontSendNotification );
                }
            } );
        } );
#else
        statusLabel.setText( "Splice sync is handled automatically on this platform.", juce::dontSendNotification );
#endif
    }

    void CloudAuthQrPopup::startSpliceLogin()
    {
#if JUCE_LINUX || __linux__
        isSpliceLoggingIn = true;
        reloginBtn.setEnabled( false );
        statusLabel.setText( "Requesting Splice pairing code...", juce::dontSendNotification );

        juce::Thread::launch( []() {
            std::system( "killall splicesync 2>/dev/null" );
            std::system( "/usr/share/Splice/splicesync -authTokenPath /data/Settings/splice-authtoken -logPath /tmp/splicelogin.log -login &" );
        } );

        startTimer( 1000 );
#endif
    }

    void CloudAuthQrPopup::processGoogleToken( juce::String credInput, std::function<void( bool ok, const juce::String& email, const juce::String& err )> done )
    {
        credInput = credInput.trim();
        if( credInput.isEmpty() )
        {
            if( done )
                done( false, {}, "Empty token" );
            return;
        }

        if( credInput.startsWith( "ya29." ) || credInput.startsWith( "1//" ) )
        {
            fileSource::gdrive::Credentials creds;
            if( credInput.startsWith( "ya29." ) )
            {
                creds.mode        = "bearer";
                creds.accessToken = credInput.toStdString();
            }
            else
            {
                creds.mode         = "oauth";
                creds.refreshToken = credInput.toStdString();
                creds.clientId     = clientId;
            }
            statusLabel.setText( "Verifying Google Drive token...", juce::dontSendNotification );
            juce::Component::SafePointer<CloudAuthQrPopup> safeThis( this );
            juce::Thread::launch( [safeThis, creds, done]() mutable {
                std::string em, nm;
                if( !creds.accessToken.empty() )
                    fileSource::GoogleDriveSource::fetchUserAbout( creds.accessToken, em, nm );
                if( !em.empty() )
                    creds.email = em;

                const auto prefs     = getAppDirs( MTPrefs );
                const auto secretDir = prefs.getChildFile( "file_sources" );
                secretDir.createDirectory();
                const auto userKey  = !creds.email.empty() ? creds.email : std::string( "token" );
                const auto srcId    = "gdrive-" + fileSource::SourceCache::makeKey( "gd", userKey ).substr( 0, 8 );
                fileSource::writeSecretFile( secretDir.getChildFile( srcId + ".secret" ).getFullPathName().toStdString(),
                                             fileSource::gdrive::serializeCredentials( creds ) );

                auto sources = fileSource::loadSources( prefs.getChildFile( "file_sources.txt" ).getFullPathName().toStdString() );
                fileSource::SourceConfig cfg;
                cfg.type     = "gdrive";
                cfg.id       = srcId;
                cfg.name     = !creds.email.empty() ? ( "Google Drive (" + creds.email.substr( 0, 20 ) + ")" ) : "Google Drive";
                cfg.location = "gdrive://root";
                cfg.user     = creds.email;
                cfg.auth     = creds.mode;

                bool found = false;
                for( auto& x : sources )
                    if( x.id == cfg.id ) { x = cfg; found = true; }
                if( !found )
                    sources.push_back( cfg );
                fileSource::saveSources( prefs.getChildFile( "file_sources.txt" ).getFullPathName().toStdString(), sources );

                juce::MessageManager::callAsync( [safeThis, cfg, creds, done]() {
                    mt::getSettings().cloudSources.enableGoogleDrive = true;
                    if( !creds.email.empty() )
                        mt::getSettings().cloudSources.googleUserEmail = juce::String::fromUTF8( creds.email.c_str() );
                    if( done )
                        done( true, juce::String::fromUTF8( creds.email.c_str() ), {} );
                    if( safeThis != nullptr )
                        safeThis->finishSuccess( "Connected: " + juce::String::fromUTF8( cfg.name.c_str() ) );
                } );
            } );
        }
        else
        {
            mt::getSettings().cloudSources.googleClientId = credInput;
            clientId = credInput.toStdString();
            if( done )
                done( true, {}, "Client ID set" );
            startGoogleDeviceFlow();
        }
    }

    void CloudAuthQrPopup::startGoogleDeviceFlow()
    {
        clientId = mt::getSettings().cloudSources.googleClientId.get().trim().toStdString();
        if( clientId.empty() )
        {
#if defined( KICK_GOOGLE_CLIENT_ID )
            clientId = KICK_GOOGLE_CLIENT_ID;
#endif
            if( clientId.empty() )
                clientId = juce::SystemStats::getEnvironmentVariable( "KICK_GOOGLE_CLIENT_ID", {} ).toStdString();
        }

        if( clientId.empty() )
        {
            if( pairingServer && pairingServer->isRunning() )
            {
                const auto phoneUrl = pairingServer->getUrl();
                qrCode.setText( phoneUrl );
                instructionsLabel.setText( "1. Scan the QR code with your phone camera, or visit:\n   " + phoneUrl + "\n2. Authorize Google Drive on the web page.", juce::dontSendNotification );
                codeLabel.setText( "Phone Connect URL:  " + phoneUrl, juce::dontSendNotification );
                statusLabel.setText( "Waiting for phone connection...", juce::dontSendNotification );
                startTimer( 1000 );
                return;
            }

            qrCode.setText( "https://console.cloud.google.com/apis/credentials" );
            instructionsLabel.setText( "No Google OAuth Client ID configured.\nScan or enter an OAuth Client ID or Token below:", juce::dontSendNotification );
            codeLabel.setText( "Device Flow requires a Google Client ID", juce::dontSendNotification );
            statusLabel.setText( "Tap 'Enter Token / Client ID...' to configure.", juce::dontSendNotification );
            return;
        }

        instructionsLabel.setText( "Requesting Google authorization code...", juce::dontSendNotification );
        statusLabel.setText( "Connecting to Google OAuth...", juce::dontSendNotification );

        juce::Component::SafePointer<CloudAuthQrPopup> safeThis( this );
        juce::Thread::launch( [safeThis, cid = clientId]() {
            juce::URL url( "https://oauth2.googleapis.com/device/code" );
            std::string post = "client_id=" + fileSource::gdrive::encodeQueryComponent( cid )
                             + "&scope=" + fileSource::gdrive::encodeQueryComponent( "https://www.googleapis.com/auth/drive.readonly https://www.googleapis.com/auth/userinfo.email" );
            url = url.withPOSTData( juce::String::fromUTF8( post.c_str() ) );

            int statusCode = 0;
            auto in = url.createInputStream( juce::URL::InputStreamOptions( juce::URL::ParameterHandling::inPostData )
                                                 .withConnectionTimeoutMs( 12000 )
                                                 .withHttpRequestCmd( "POST" )
                                                 .withExtraHeaders( "Content-Type: application/x-www-form-urlencoded\r\n" )
                                                 .withStatusCode( &statusCode )
                                                 .withNumRedirectsToFollow( 2 ) );
            std::string body;
            if( in )
                body = in->readEntireStreamAsString().toStdString();

            juce::MessageManager::callAsync( [safeThis, statusCode, body, cid]() {
                if( safeThis == nullptr )
                    return;

                const auto parsed = juce::JSON::parse( juce::String::fromUTF8( body.c_str() ) );
                auto*      obj    = parsed.getDynamicObject();
                if( obj != nullptr && obj->hasProperty( "user_code" ) && obj->hasProperty( "device_code" ) )
                {
                    safeThis->deviceCode = obj->getProperty( "device_code" ).toString().toStdString();
                    const auto userCode  = obj->getProperty( "user_code" ).toString();
                    const auto vUrl      = obj->hasProperty( "verification_url" ) ? obj->getProperty( "verification_url" ).toString() : "https://www.google.com/device";
                    const auto vUriComp  = obj->hasProperty( "verification_uri_complete" )
                                              ? obj->getProperty( "verification_uri_complete" ).toString()
                                              : ( vUrl + "?user_code=" + userCode );
                    safeThis->pollInterval = juce::jmax( 5, static_cast<int>( obj->getProperty( "interval" ) ) );

                    safeThis->qrCode.setText( vUriComp );
                    safeThis->instructionsLabel.setText( "1. Scan the QR code with your phone camera, or visit:\n   " + vUrl, juce::dontSendNotification );
                    safeThis->codeLabel.setText( "2. Confirmation Code:  " + userCode, juce::dontSendNotification );
                    safeThis->statusLabel.setText( "Waiting for authorization on your device...", juce::dontSendNotification );
                    safeThis->startTimer( safeThis->pollInterval * 1000 );
                }
                else
                {
                    if( safeThis->pairingServer && safeThis->pairingServer->isRunning() )
                    {
                        const auto phoneUrl = safeThis->pairingServer->getUrl();
                        safeThis->qrCode.setText( phoneUrl );
                        safeThis->instructionsLabel.setText( "1. Scan the QR code with your phone camera, or visit:\n   " + phoneUrl + "\n2. Authorize Google Drive on the web page.", juce::dontSendNotification );
                        safeThis->codeLabel.setText( "Phone Connect URL:  " + phoneUrl, juce::dontSendNotification );
                        safeThis->statusLabel.setText( "Waiting for phone connection...", juce::dontSendNotification );
                        safeThis->startTimer( 1000 );
                        return;
                    }

                    juce::String err = "Google Device Code error (" + juce::String( statusCode ) + ")";
                    if( obj != nullptr && obj->hasProperty( "error_description" ) )
                        err += ": " + obj->getProperty( "error_description" ).toString();
                    else if( obj != nullptr && obj->hasProperty( "error" ) )
                        err += ": " + obj->getProperty( "error" ).toString();

                    safeThis->qrCode.setText( "https://myaccount.google.com" );
                    safeThis->instructionsLabel.setText( "Device Authorization returned:\n" + err, juce::dontSendNotification );
                    safeThis->codeLabel.setText( "Client ID: " + juce::String::fromUTF8( cid.c_str() ).substring( 0, 24 ) + "...", juce::dontSendNotification );
                    safeThis->statusLabel.setText( "Tap 'Enter Token / Client ID...' to enter an OAuth Token directly.", juce::dontSendNotification );
                }
            } );
        } );
    }

    void CloudAuthQrPopup::timerCallback()
    {
        if( isSpliceLoggingIn )
        {
            juce::File logFile( "/tmp/splicelogin.log" );
            if( logFile.existsAsFile() )
            {
                const juce::String log = logFile.loadFileAsString();
                const int scIdx = log.indexOf( "msg: shortcode: " );
                if( scIdx >= 0 && spliceShortCode.isEmpty() )
                {
                    spliceShortCode = log.substring( scIdx + 16, scIdx + 22 ).trim();
                    if( spliceShortCode.isNotEmpty() )
                    {
                        qrCode.setText( "https://splice.com/link?code=" + spliceShortCode );
                        instructionsLabel.setText( "1. Scan QR code with phone, or visit:\n   https://splice.com/link\n2. Enter code: " + spliceShortCode, juce::dontSendNotification );
                        codeLabel.setText( "Confirmation Code:  " + spliceShortCode, juce::dontSendNotification );
                        statusLabel.setText( "Waiting for confirmation on splice.com...", juce::dontSendNotification );
                    }
                }
                if( log.contains( "msg: done:" ) || log.contains( "msg: user:" ) )
                {
                    stopTimer();
                    isSpliceLoggingIn = false;
                    mt::getSettings().cloudSources.enableSplice = true;
                    finishSuccess( "Splice Connected Successfully!" );
                    return;
                }
            }
            return;
        }

        if( isPolling.load() || deviceCode.empty() )
        {
            if( pairingServer && pairingServer->isRunning() && serviceType == Service::GoogleDrive )
            {
                dotCounter = ( dotCounter + 1 ) % 4;
                juce::String dots;
                for( int i = 0; i < dotCounter; ++i ) dots += ".";
                statusLabel.setText( "Waiting for phone connection at " + pairingServer->getUrl() + dots, juce::dontSendNotification );
            }
            return;
        }

        dotCounter = ( dotCounter + 1 ) % 4;
        juce::String dots;
        for( int i = 0; i < dotCounter; ++i ) dots += ".";
        statusLabel.setText( "Waiting for authorization on your device" + dots, juce::dontSendNotification );

        pollGoogleToken();
    }

    void CloudAuthQrPopup::pollGoogleToken()
    {
        isPolling = true;
        juce::Component::SafePointer<CloudAuthQrPopup> safeThis( this );
        juce::Thread::launch( [safeThis, cid = clientId, sec = clientSecret, dcode = deviceCode]() {
            juce::URL url( "https://oauth2.googleapis.com/token" );
            std::string post = "client_id=" + fileSource::gdrive::encodeQueryComponent( cid )
                             + "&device_code=" + fileSource::gdrive::encodeQueryComponent( dcode )
                             + "&grant_type=urn:ietf:params:oauth:grant-type:device_code";
            if( !sec.empty() )
                post += "&client_secret=" + fileSource::gdrive::encodeQueryComponent( sec );
            url = url.withPOSTData( juce::String::fromUTF8( post.c_str() ) );

            int statusCode = 0;
            auto in = url.createInputStream( juce::URL::InputStreamOptions( juce::URL::ParameterHandling::inPostData )
                                                 .withConnectionTimeoutMs( 10000 )
                                                 .withHttpRequestCmd( "POST" )
                                                 .withExtraHeaders( "Content-Type: application/x-www-form-urlencoded\r\n" )
                                                 .withStatusCode( &statusCode )
                                                 .withNumRedirectsToFollow( 2 ) );
            std::string body;
            if( in )
                body = in->readEntireStreamAsString().toStdString();

            juce::MessageManager::callAsync( [safeThis, body]() {
                if( safeThis == nullptr )
                    return;
                safeThis->isPolling = false;

                const auto parsed = juce::JSON::parse( juce::String::fromUTF8( body.c_str() ) );
                auto*      obj    = parsed.getDynamicObject();
                if( obj == nullptr )
                    return;

                if( obj->hasProperty( "error" ) )
                {
                    const auto err = obj->getProperty( "error" ).toString().toStdString();
                    if( err == "authorization_pending" )
                        return;
                    if( err == "slow_down" )
                    {
                        safeThis->pollInterval += 5;
                        safeThis->startTimer( safeThis->pollInterval * 1000 );
                        return;
                    }
                    if( err == "expired_token" )
                    {
                        safeThis->stopTimer();
                        safeThis->statusLabel.setText( "Authorization code expired. Please close and try again.", juce::dontSendNotification );
                        return;
                    }
                    if( err == "access_denied" )
                    {
                        safeThis->stopTimer();
                        safeThis->statusLabel.setText( "Sign-in was denied by user.", juce::dontSendNotification );
                        return;
                    }
                    safeThis->stopTimer();
                    safeThis->statusLabel.setText( "Authorization error: " + juce::String::fromUTF8( err.c_str() ), juce::dontSendNotification );
                    return;
                }

                if( obj->hasProperty( "access_token" ) )
                {
                    safeThis->stopTimer();
                    const auto accessToken  = obj->getProperty( "access_token" ).toString().toStdString();
                    const auto refreshToken = obj->getProperty( "refresh_token" ).toString().toStdString();

                    safeThis->statusLabel.setText( "Authorization successful! Connecting...", juce::dontSendNotification );

                    juce::Thread::launch( [safeThis, accessToken, refreshToken, authCid = safeThis->clientId, authSec = safeThis->clientSecret]() {
                        std::string email, displayName;
                        fileSource::GoogleDriveSource::fetchUserAbout( accessToken, email, displayName );

                        fileSource::gdrive::Credentials creds;
                        creds.mode         = "oauth";
                        creds.clientId     = authCid;
                        creds.clientSecret = authSec;
                        creds.accessToken  = accessToken;
                        creds.refreshToken = refreshToken;
                        creds.email        = email;
                        creds.displayName  = displayName;
                        creds.expiresAtMs  = juce::Time::currentTimeMillis() + 3500ll * 1000;

                        const auto prefs     = getAppDirs( MTPrefs );
                        const auto secretDir = prefs.getChildFile( "file_sources" );
                        secretDir.createDirectory();
                        const auto userKey  = !email.empty() ? email : std::string( "oauth" );
                        const auto sourceId = "gdrive-" + fileSource::SourceCache::makeKey( "gd", userKey ).substr( 0, 8 );
                        fileSource::writeSecretFile( secretDir.getChildFile( sourceId + ".secret" ).getFullPathName().toStdString(),
                                                     fileSource::gdrive::serializeCredentials( creds ) );

                        auto sources = fileSource::loadSources( prefs.getChildFile( "file_sources.txt" ).getFullPathName().toStdString() );
                        fileSource::SourceConfig cfg;
                        cfg.type     = "gdrive";
                        cfg.id       = sourceId;
                        cfg.name     = !email.empty() ? ( "Google Drive (" + email.substr( 0, 20 ) + ")" ) : "Google Drive";
                        cfg.location = "gdrive://root";
                        cfg.user     = email;
                        cfg.auth     = "oauth";

                        bool found = false;
                        for( auto& x : sources )
                            if( x.id == cfg.id ) { x = cfg; found = true; }
                        if( !found )
                            sources.push_back( cfg );
                        fileSource::saveSources( prefs.getChildFile( "file_sources.txt" ).getFullPathName().toStdString(), sources );

                        juce::MessageManager::callAsync( [safeThis, cfg, email]() {
                            if( safeThis == nullptr )
                                return;
                            mt::getSettings().cloudSources.enableGoogleDrive = true;
                            if( !email.empty() )
                                mt::getSettings().cloudSources.googleUserEmail = juce::String::fromUTF8( email.c_str() );
                            safeThis->finishSuccess( "Connected: " + juce::String::fromUTF8( cfg.name.c_str() ) );
                        } );
                    } );
                }
            } );
        } );
    }

    void CloudAuthQrPopup::finishSuccess( const juce::String& msg )
    {
        statusLabel.setText( msg, juce::dontSendNotification );
        statusLabel.setValueColour( juce::Colours::lightgreen );
        if( onAuthSuccess )
            onAuthSuccess();

        juce::Timer::callAfterDelay( 1200, [this]() {
            mt::utils::exitDialog( this, UI::APPLY );
        } );
    }

    void CloudAuthQrPopup::resized()
    {
        auto bounds = getLocalBounds().reduced( 20, 16 );
        titleLabel.setBounds( bounds.removeFromTop( 32 ) );
        bounds.removeFromTop( 10 );

        auto bottomRow = bounds.removeFromBottom( 38 );
        actionBtn.setBounds( bottomRow.removeFromRight( 100 ) );

        if( manualBtn.isVisible() )
        {
            bottomRow.removeFromRight( 10 );
            manualBtn.setBounds( bottomRow.removeFromRight( 200 ) );
        }
        if( syncBtn.isVisible() )
        {
            bottomRow.removeFromRight( 10 );
            syncBtn.setBounds( bottomRow.removeFromRight( 160 ) );
        }
        if( reloginBtn.isVisible() )
        {
            bottomRow.removeFromRight( 10 );
            reloginBtn.setBounds( bottomRow.removeFromRight( 150 ) );
        }
        if( folderBtn.isVisible() )
        {
            bottomRow.removeFromRight( 10 );
            folderBtn.setBounds( bottomRow.removeFromRight( 140 ) );
        }

        bounds.removeFromBottom( 10 );

        const int qrWidth = 200;
        qrCode.setBounds( bounds.removeFromLeft( qrWidth ).withHeight( qrWidth ) );
        bounds.removeFromLeft( 20 );

        instructionsLabel.setBounds( bounds.removeFromTop( 72 ) );
        bounds.removeFromTop( 8 );
        codeLabel.setBounds( bounds.removeFromTop( 36 ) );
        bounds.removeFromTop( 8 );
        statusLabel.setBounds( bounds.removeFromTop( 54 ) );
    }

    void CloudAuthQrPopup::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 1 ) );
        g.setColour( ui::Theme::outlineColour( 2 ) );
        g.drawRect( getLocalBounds(), 1.0f );
    }
}
