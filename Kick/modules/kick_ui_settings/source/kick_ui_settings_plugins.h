// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <kick_clap/PluginManager.h>
#include <juce_gui_basics/juce_gui_basics.h>

namespace kick_ui
{
    class SettingsPlugins : public SettingsInterface
        , private juce::ListBoxModel
    {
    public:
        SettingsPlugins();
        ~SettingsPlugins() override = default;

        void refresh() override;
        void setupHeader(kick_ui::KickModeHeader& Hdr) override;
        void handleCommand(CPktMgr::HWMsg_t&) override {}

        // ListBoxModel
        int getNumRows() override;
        void paintListBoxItem(int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected) override;
        void selectedRowsChanged(int lastRowSelected) override;

        void resized() override;
        void paint(juce::Graphics& g) override;

    private:
        struct ItemInfo {
            std::string id;
            std::string name;
            std::string version;
            std::string author;
            std::string license;
            std::string kind;
            std::string trust;
            std::string path;
            bool isStatic{false};
            bool canRollback{false};
            std::string previousVersion;
            std::vector<std::string> assetPacks;
        };

        std::vector<ItemInfo> items_;
        int selectedIndex_{-1};

        juce::ListBox pluginList;

        TextValue pluginIdValue{ "ID", "" };
        TextValue pluginNameValue{ "Name", "" };
        TextValue pluginVersionValue{ "Version", "" };
        TextValue pluginAuthorValue{ "Author", "" };
        TextValue pluginLicenseValue{ "License", "" };
        TextValue pluginTrustValue{ "Trust Level", "" };
        TextValue pluginKindValue{ "Kind", "" };

        ui::KickButton rollbackBtn{ "Rollback" };
        ui::KickButton uninstallBtn{ "Uninstall" };
        ui::KickButton rescanBtn{ "Rescan" };
        ui::KickButton removeContentBtn{ "Remove Content" };
        ui::KickButton restoreContentBtn{ "Restore Content" };

        ui::KickLabel statusLabel{ "No plugin selected" };

        void updateSelectionDetails();
        void populateItems();
    };
} // namespace kick_ui
