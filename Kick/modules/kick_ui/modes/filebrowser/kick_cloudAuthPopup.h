// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include <kick_ui_controls/kick_ui_controls.h>
#include "kick_phonePairingServer.h"
#include "kick_sourceConfig.h"
#include "kick_googleDriveHelper.h"
#include "kick_googleDriveSource.h"
#include "kick_incomingFiles.h"
#include "kick_secretStore.h"
#include "kick_sourceCache.h"

namespace ui
{
    class CloudAuthQrPopup : public juce::Component
        , private juce::Timer
    {
    public:
        enum class Service { GoogleDrive, Splice };

        explicit CloudAuthQrPopup( Service service );
        ~CloudAuthQrPopup() override;

        std::function<void()> onAuthSuccess;

        void resized() override;
        void paint( juce::Graphics& g ) override;

    private:
        Service     serviceType;
        KickQrCode  qrCode;
        KickLabel   titleLabel{ "TITLE" };
        KickLabel   instructionsLabel{ "INSTRUCTIONS" };
        KickLabel   codeLabel{ "CODE" };
        KickLabel   statusLabel{ "STATUS" };
        KickButton  actionBtn{ "Action", "Close" };
        KickButton  manualBtn{ "Manual", "Enter Token / Client ID..." };
        KickButton  folderBtn{ "Folder", "Set Folder..." };
        KickButton  syncBtn{ "Sync", "Sync Samples Now" };
        KickButton  reloginBtn{ "Relogin", "Re-pair Account" };

        std::unique_ptr<PhonePairingServer> pairingServer;
        std::string       deviceCode;
        std::string       clientId;
        std::string       clientSecret;
        int               pollInterval{ 5 };
        std::atomic<bool> isPolling{ false };
        int               dotCounter{ 0 };
        juce::String      spliceShortCode;
        bool              isSpliceLoggingIn{ false };

        void timerCallback() override;
        void startGoogleDeviceFlow();
        void pollGoogleToken();
        void processGoogleToken( juce::String credInput, std::function<void( bool ok, const juce::String& email, const juce::String& err )> done = nullptr );
        void setupSplice();
        void triggerSpliceSync();
        void startSpliceLogin();
        void finishSuccess( const juce::String& msg );

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( CloudAuthQrPopup )
    };
}
