// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <juce_core/juce_core.h>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <vector>

namespace ui
{
    class PhonePairingServer : public juce::Thread
    {
    public:
        PhonePairingServer( int preferredPort = 8080 );
        ~PhonePairingServer() override;

        bool isRunning() const { return running.load(); }
        juce::String getUrl() const;
        juce::String getLanIp() const { return lanIp; }
        int getPort() const { return activePort; }

        struct StatusInfo
        {
            juce::String googleEmail;
            bool         googleConnected{ false };
            juce::String spliceUser;
            juce::String spliceFolder;
            int          spliceSamples{ 0 };
            bool         spliceConnected{ false };
        };

        std::function<StatusInfo()> getStatus;
        std::function<void( const juce::String& credInput, std::function<void( bool ok, const juce::String& email, const juce::String& err )> done )> onGoogleDriveAuth;
        std::function<void( std::function<void( bool ok, const juce::String& msg )> done )> onSpliceSync;
        std::function<void( const juce::String& filename, const juce::MemoryBlock& data )> onSampleUpload;

        void run() override;

    private:
        std::unique_ptr<juce::StreamingSocket> listenerSocket;
        std::atomic<bool>                      running{ false };
        juce::String                           lanIp;
        int                                    activePort{ 8080 };

        static juce::String discoverLanIp();
        void handleClient( std::unique_ptr<juce::StreamingSocket> client );
        juce::String buildHtmlPage() const;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( PhonePairingServer )
    };
}
