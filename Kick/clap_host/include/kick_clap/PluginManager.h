// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginLoader.h"
#include "PluginInstaller.h"
#include "TrustStore.h"

#include <memory>
#include <vector>
#include <map>
#include <mutex>
#include <filesystem>

namespace kick::clap {

struct RemotePluginDescriptor {
    std::string manifestUrl;    // URL pointing to remote manifest.json
    std::string id;             // Plugin unique ID
    std::string name;           // Human readable name
    std::string version;        // Latest version
    std::string description;
    PluginKind  kind{PluginKind::Instrument};
};

class PluginManager {
public:
    static PluginManager& getInstance();

    // Initializes plugin directory (default: user app data / plugins or /opt/kick/plugins)
    void initialize(const std::filesystem::path& pluginsRoot = "");

    PluginInstaller& getInstaller() { return *installer_; }
    TrustStore& getTrustStore() { return trustStore_; }
    const std::filesystem::path& getPluginsRoot() const { return pluginsRoot_; }

    // Register built-in / static plugin
    void registerStaticPlugin(std::shared_ptr<StaticPluginLoader> loader);
    void registerBuiltinPlugins();

    // Rescans installed plugins from disk and updates dynamic loaders
    void scanInstalledPlugins();

    // Query active plugin loaders
    std::vector<std::shared_ptr<PluginLoader>> getAllLoaders() const;
    std::vector<std::shared_ptr<PluginLoader>> getInstrumentLoaders() const;
    std::vector<std::shared_ptr<PluginLoader>> getEffectLoaders() const;
    std::shared_ptr<PluginLoader> findLoaderById(const std::string& pluginId) const;

    // Descriptors for Kick integration
    std::vector<Descriptor> getAllDescriptors() const;
    std::vector<Descriptor> getInstrumentDescriptors() const;
    std::vector<Descriptor> getEffectDescriptors() const;

    // Returns filesystem paths to all installed active plugin binaries (.clap)
    std::vector<std::filesystem::path> getPluginSearchPaths() const;

    // Downloadable-ready architecture (Phase 3 readiness)
    void registerRemotePlugin(const RemotePluginDescriptor& desc);
    std::vector<RemotePluginDescriptor> getAvailableRemotePlugins() const;

private:
    PluginManager();
    ~PluginManager() = default;
    PluginManager(const PluginManager&) = delete;
    PluginManager& operator=(const PluginManager&) = delete;

    std::filesystem::path pluginsRoot_;
    TrustStore trustStore_;
    std::unique_ptr<PluginInstaller> installer_;

    mutable std::mutex mutex_;
    std::map<std::string, std::shared_ptr<StaticPluginLoader>>  staticLoaders_;
    std::map<std::string, std::shared_ptr<DynamicPluginLoader>> dynamicLoaders_;
    std::vector<RemotePluginDescriptor> remoteCatalog_;
};

} // namespace kick::clap
