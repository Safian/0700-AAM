// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "../kick_ui_settings.h"

namespace kick_ui
{
    class SettingsCloud : public SettingsInterface
    {
    public:
        SettingsCloud();

        std::function<void( int service )> onOpenCloudAuth;
        std::function<void()>              onConfigCloudPaths;

        void refresh() override;
        void setupHeader( kick_ui::KickModeHeader& Hdr ) override;
        void handleCommand( CPktMgr::HWMsg_t& ) override;

    private:
        mt::KickSettings& settings{ mt::getSettings() };

        // Google Drive Section
        ui::KickToggleButton toggleGoogleDrive{ "GOOGLE DRIVE", "ENABLED", "DISABLED" };
        ui::KickLabel        lblGoogleUser{ "Google Account" };
        ui::KickButton       btnGoogleLogin{ "GoogleLogin", "QR Sign In" };
        ui::KickButton       btnGoogleDisconnect{ "GoogleDisconnect", "Disconnect" };
        ui::KickButton       btnGooglePath{ "GooglePath", "Folder..." };

        // Splice Section
        ui::KickToggleButton toggleSplice{ "SPLICE", "ENABLED", "DISABLED" };
        ui::KickLabel        lblSplicePath{ "Splice Path" };
        ui::KickButton       btnSpliceQr{ "SpliceQr", "Link (QR)" };
        ui::KickButton       btnSplicePath{ "SplicePath", "Folder..." };

        // WebDAV / NAS Section
        ui::KickToggleButton toggleWebDav{ "WEBDAV / NAS", "ENABLED", "DISABLED" };
        ui::KickButton       btnWebDavConfig{ "WebDavConfig", "Configure..." };

        void updateStatusDisplay();
    };
}
