// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_ui_settings_plugins.h"

namespace kick_ui
{
    SettingsPlugins::SettingsPlugins()
        : SettingsInterface( 1, 1 )
    {
        pluginList.setModel( this );
        pluginList.setRowHeight( 44 );
        pluginList.setColour( juce::ListBox::backgroundColourId, juce::Colour( 0xff15191e ) );

        mt::utils::addAndMakeVisible( this,
                                      pluginList,
                                      pluginIdValue,
                                      pluginNameValue,
                                      pluginVersionValue,
                                      pluginAuthorValue,
                                      pluginLicenseValue,
                                      pluginTrustValue,
                                      pluginKindValue,
                                      rollbackBtn,
                                      uninstallBtn,
                                      rescanBtn,
                                      removeContentBtn,
                                      restoreContentBtn,
                                      statusLabel );

        for (auto* tv : { &pluginIdValue, &pluginNameValue, &pluginVersionValue, &pluginAuthorValue, &pluginLicenseValue, &pluginTrustValue, &pluginKindValue })
        {
            tv->setTitleRatio( 0.28f );
            tv->value.setValueJustification( juce::Justification::centredLeft );
            tv->title.setFontHeight( 16.0f );
            tv->value.setFontHeight( 16.0f );
        }

        rescanBtn.onClick = [this]() {
            kick::clap::PluginManager::getInstance().scanInstalledPlugins();
            populateItems();
            statusLabel.setText( "Rescanned plugins.", juce::dontSendNotification );
        };

        rollbackBtn.onClick = [this]() {
            if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(items_.size())) {
                const auto& item = items_[selectedIndex_];
                if (item.canRollback) {
                    std::string newVer;
                    if (kick::clap::PluginManager::getInstance().getInstaller().rollback(item.id, newVer)) {
                        kick::clap::PluginManager::getInstance().scanInstalledPlugins();
                        populateItems();
                        statusLabel.setText( "Rolled back " + juce::String(item.name) + " to " + juce::String(newVer), juce::dontSendNotification );
                    } else {
                        statusLabel.setText( "Rollback failed.", juce::dontSendNotification );
                    }
                }
            }
        };

        uninstallBtn.onClick = [this]() {
            if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(items_.size())) {
                const auto& item = items_[selectedIndex_];
                if (!item.isStatic) {
                    if (kick::clap::PluginManager::getInstance().getInstaller().uninstallPlugin(item.id)) {
                        kick::clap::PluginManager::getInstance().scanInstalledPlugins();
                        populateItems();
                        statusLabel.setText( "Uninstalled " + juce::String(item.name), juce::dontSendNotification );
                    } else {
                        statusLabel.setText( "Uninstall failed.", juce::dontSendNotification );
                    }
                }
            }
        };

        removeContentBtn.onClick = [this]() {
            if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(items_.size())) {
                const auto& item = items_[selectedIndex_];
                bool anyRemoved = false;
                for (const auto& packId : item.assetPacks) {
                    if (kick::clap::PluginManager::getInstance().getInstaller().uninstallAssetPack(item.id, item.version, packId)) {
                        anyRemoved = true;
                    }
                }
                if (anyRemoved) {
                    statusLabel.setText( "Removed content packs for " + juce::String(item.name), juce::dontSendNotification );
                } else {
                    statusLabel.setText( "No content packs to remove.", juce::dontSendNotification );
                }
            }
        };

        restoreContentBtn.onClick = [this]() {
            if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(items_.size())) {
                const auto& item = items_[selectedIndex_];
                statusLabel.setText( "Content restore triggered for " + juce::String(item.name), juce::dontSendNotification );
            }
        };

        populateItems();
    }

    void SettingsPlugins::populateItems()
    {
        items_.clear();
        auto& pm = kick::clap::PluginManager::getInstance();
        auto loaders = pm.getAllLoaders();

        for (const auto& loader : loaders) {
            const auto& m = loader->getManifest();
            ItemInfo item;
            item.id = m.id;
            item.name = m.name.empty() ? m.id : m.name;
            item.version = m.version;
            item.author = m.author;
            item.license = m.license;
            item.kind = (m.kind == kick::clap::PluginKind::Instrument) ? "Instrument" : "Effect";
            item.trust = kick::clap::trustLevelToString(loader->getTrustLevel());
            item.path = loader->getBinaryPath().string();
            item.isStatic = loader->isStatic();
            item.canRollback = pm.getInstaller().canRollback(m.id);
            item.previousVersion = pm.getInstaller().getPreviousVersion(m.id);
            for (const auto& a : m.assets) {
                item.assetPacks.push_back(a.id);
            }
            items_.push_back(item);
        }

        pluginList.updateContent();
        if (!items_.empty() && selectedIndex_ < 0) {
            pluginList.selectRow( 0 );
        } else if (selectedIndex_ >= static_cast<int>(items_.size())) {
            pluginList.selectRow( std::max( 0, static_cast<int>(items_.size()) - 1 ) );
        } else {
            updateSelectionDetails();
        }
    }

    void SettingsPlugins::refresh()
    {
        populateItems();
    }

    void SettingsPlugins::setupHeader( kick_ui::KickModeHeader& Hdr )
    {
        Hdr.setTitle( "Preferences - Plugin Manager" );
    }

    int SettingsPlugins::getNumRows()
    {
        return static_cast<int>(items_.size());
    }

    void SettingsPlugins::paintListBoxItem( int rowNumber, juce::Graphics& g, int width, int height, bool rowIsSelected )
    {
        if (rowNumber < 0 || rowNumber >= static_cast<int>(items_.size())) return;
        const auto& item = items_[rowNumber];

        if (rowIsSelected) {
            g.fillAll( ui::Theme::selectionColour( 1 ) );
        } else {
            g.fillAll( (rowNumber % 2 == 0) ? juce::Colour( 0xff1a1e24 ) : juce::Colour( 0xff15191e ) );
        }

        g.setFont( 14.0f );
        g.setColour( rowIsSelected ? juce::Colours::black : juce::Colours::white );
        g.drawText( item.name, 10, 2, width - 180, 20, juce::Justification::centredLeft, true );

        g.setFont( 11.0f );
        g.setColour( rowIsSelected ? juce::Colour( 0xff16202a ) : juce::Colours::grey );
        g.drawText( item.id + " (v" + item.version + ")", 10, 22, width - 180, 18, juce::Justification::centredLeft, true );

        // Badge on right: Trust level & Static/Installed
        juce::Colour badgeCol = juce::Colours::grey;
        if (item.trust == "Official") {
            badgeCol = juce::Colour( 0xffffb608 ); // Kick amber
        } else if (item.trust == "Verified") {
            badgeCol = juce::Colour( 0xff2ecc71 ); // Green
        } else {
            badgeCol = juce::Colour( 0xffe74c3c ); // Red
        }

        juce::String badgeText = (item.isStatic ? "[Built-in] " : "[Installed] ") + item.trust;
        g.setColour( badgeCol );
        g.drawText( badgeText, width - 170, 0, 160, height, juce::Justification::centredRight, true );
    }

    void SettingsPlugins::selectedRowsChanged( int lastRowSelected )
    {
        selectedIndex_ = lastRowSelected;
        updateSelectionDetails();
    }

    void SettingsPlugins::updateSelectionDetails()
    {
        if (selectedIndex_ >= 0 && selectedIndex_ < static_cast<int>(items_.size())) {
            const auto& item = items_[selectedIndex_];
            pluginIdValue.value.setText( item.id, juce::dontSendNotification );
            pluginNameValue.value.setText( item.name, juce::dontSendNotification );
            pluginVersionValue.value.setText( item.version, juce::dontSendNotification );
            pluginAuthorValue.value.setText( item.author, juce::dontSendNotification );
            pluginLicenseValue.value.setText( item.license, juce::dontSendNotification );
            pluginTrustValue.value.setText( item.trust + (item.isStatic ? " (Built-in)" : ""), juce::dontSendNotification );
            pluginKindValue.value.setText( item.kind, juce::dontSendNotification );

            rollbackBtn.setEnabled( item.canRollback );
            if (item.canRollback) {
                rollbackBtn.setButtonText( "Rollback to " + item.previousVersion );
            } else {
                rollbackBtn.setButtonText( "Rollback" );
            }

            uninstallBtn.setEnabled( !item.isStatic );
            removeContentBtn.setEnabled( !item.assetPacks.empty() );
            restoreContentBtn.setEnabled( !item.assetPacks.empty() );

            statusLabel.setText( item.isStatic ? "Built-in core plugin." : "Installed dynamic plugin.", juce::dontSendNotification );
        } else {
            pluginIdValue.value.setText( "", juce::dontSendNotification );
            pluginNameValue.value.setText( "", juce::dontSendNotification );
            pluginVersionValue.value.setText( "", juce::dontSendNotification );
            pluginAuthorValue.value.setText( "", juce::dontSendNotification );
            pluginLicenseValue.value.setText( "", juce::dontSendNotification );
            pluginTrustValue.value.setText( "", juce::dontSendNotification );
            pluginKindValue.value.setText( "", juce::dontSendNotification );

            rollbackBtn.setEnabled( false );
            uninstallBtn.setEnabled( false );
            removeContentBtn.setEnabled( false );
            restoreContentBtn.setEnabled( false );
            statusLabel.setText( "No plugin selected.", juce::dontSendNotification );
        }
    }

    void SettingsPlugins::paint( juce::Graphics& g )
    {
        g.fillAll( ui::Theme::backgroundColour( 1 ) );
    }

    void SettingsPlugins::resized()
    {
        auto bounds = getLocalBounds().reduced( 12 );

        // Left side: Plugin list (width ~400px)
        auto leftPane = bounds.removeFromLeft( 420 );
        rescanBtn.setBounds( leftPane.removeFromTop( 36 ).reduced( 2 ) );
        leftPane.removeFromTop( 6 );
        pluginList.setBounds( leftPane );

        bounds.removeFromLeft( 16 );

        // Right side: Plugin Details and actions
        auto rightPane = bounds;
        statusLabel.setBounds( rightPane.removeFromBottom( 28 ) );
        rightPane.removeFromBottom( 6 );

        // Buttons at the bottom of right pane
        auto buttonRow = rightPane.removeFromBottom( 40 );
        auto btnW = buttonRow.getWidth() / 4;
        rollbackBtn.setBounds( buttonRow.removeFromLeft( btnW ).reduced( 2 ) );
        uninstallBtn.setBounds( buttonRow.removeFromLeft( btnW ).reduced( 2 ) );
        removeContentBtn.setBounds( buttonRow.removeFromLeft( btnW ).reduced( 2 ) );
        restoreContentBtn.setBounds( buttonRow.reduced( 2 ) );

        rightPane.removeFromBottom( 10 );

        // Property rows
        int rowH = 34;
        pluginIdValue.setBounds( rightPane.removeFromTop( rowH ).reduced( 0, 2 ) );
        pluginNameValue.setBounds( rightPane.removeFromTop( rowH ).reduced( 0, 2 ) );
        pluginVersionValue.setBounds( rightPane.removeFromTop( rowH ).reduced( 0, 2 ) );
        pluginAuthorValue.setBounds( rightPane.removeFromTop( rowH ).reduced( 0, 2 ) );
        pluginLicenseValue.setBounds( rightPane.removeFromTop( rowH ).reduced( 0, 2 ) );
        pluginTrustValue.setBounds( rightPane.removeFromTop( rowH ).reduced( 0, 2 ) );
        pluginKindValue.setBounds( rightPane.removeFromTop( rowH ).reduced( 0, 2 ) );
    }
} // namespace kick_ui
