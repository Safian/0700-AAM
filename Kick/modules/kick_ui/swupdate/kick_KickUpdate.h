// SPDX-License-Identifier: AGPL-3.0-or-later
// "Kick Update" screen (Akai MPC / desktop): installed Kick, the package on the USB drive and what the pendrive bootstrap
// will do with it. Logic: hardware/source/kick_update_logic.h (host-tested).
#pragma once

#include <hardware/source/kick_sysinfo.h>
#include <hardware/source/kick_update_logic.h>

namespace kick_ui
{
    class KickUpdateScreen : public IScreen
        , public juce::Component
    {
    public:
        KickUpdateScreen();
        ~KickUpdateScreen() override;

    private:
        struct Scan
        {
            kick_update::State        state = kick_update::State::NoDrive;
            kick_update::PackageInfo  installed, package;
            std::string               reason, drive, previousVersion;
            bool                      hasPrevious = false;
        };
        class Scanner;

        bool requiresControlBar() override { return false; }
        bool requiresStatusBar() override { return false; }
        bool requiresHandleCommand() override { return true; }
        void handleCommand( CPktMgr::HWMsg_t& ) override;
        void visibilityChanged() override;
        void resized() override;
        void paint( juce::Graphics& g ) override;

        void startScan();
        void showScan( const Scan& scan );

        kick_ui::KickModeHeader header{ "Kick Update", "Icon_SettingsScreen" };
        ui::KickLabel           installedLabel{ "" };
        ui::KickLabel           driveLabel{ "" };
        ui::KickLabel           stateLabel{ "" };
        ui::KickLabel           howtoLabel{ "" };
        ui::KickButton          refresh{ "Rescan" };
        ui::KickButton          back{ "Back" };

        std::unique_ptr<Scanner> scanner;
        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( KickUpdateScreen )
    };
}
