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

    // Registers a dynamically loaded (installed package) plug-in; scanInstalledPlugins() does the same for what is on disk.
    void registerDynamicPlugin(std::shared_ptr<DynamicPluginLoader> loader);

    // ---- Precedence of a built-in plug-in against a binary of the same id (desktop and MPC) ----
    // The statically linked SynthyBoy has no UI toolkit and is the fallback of static-only targets (iOS / Android style). Wherever the real
    // module is installed, the module wins, because it is the full build (engine, bitmap GUI). The rule, in order:
    //   1. an installed package of that id whose signature is not Revoked or Expired (PluginInstaller + TrustStore, unchanged checks),
    //   2. a .clap binary found by discovery (the isolated scanner enumerated this exact id, the file still exists),
    //   3. the built-in.
    // Only a request for a built-in ("internal:...") is redirected; a request with an explicit binary path is never rewritten.
    // setDiscovered() takes the instruments of a finished scan (replaces the previous list); it is thread safe.
    void setDiscovered(const std::vector<Descriptor>& scanned);
    std::vector<Descriptor> discovered() const;
    // setDiscovered() plus a revision bump when the list changed, so sessions notice (a panel scan and the background discovery use it).
    void publishDiscovered(const std::vector<Descriptor>& scanned);
    uint64_t discoveryRevision() const;
    Descriptor resolve(const Descriptor& requested) const;

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
    std::vector<Descriptor> discovered_;
    uint64_t discoveryRevision_{0};
};

} // namespace kick::clap
