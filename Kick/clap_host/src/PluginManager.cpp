// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_clap/PluginManager.h"

#include <cstdlib>
#include <set>

namespace kick::clap {

namespace fs = std::filesystem;

PluginManager& PluginManager::getInstance() {
    static PluginManager instance;
    return instance;
}

static fs::path defaultPluginsRoot() {
    if (const char* kickData = std::getenv("KICK_DATA_ROOT")) {
        return fs::path(kickData) / "plugins";
    }
#if defined(__APPLE__)
    if (const char* home = std::getenv("HOME")) {
        return fs::path(home) / "Library/Application Support/KICK/plugins";
    }
    return "/Library/Application Support/KICK/plugins";
#elif defined(_WIN32)
    if (const char* appData = std::getenv("APPDATA")) {
        return fs::path(appData) / "KICK/plugins";
    }
    return "C:\\KICK\\plugins";
#else
    if (fs::exists("/opt/kick/plugins")) {
        return "/opt/kick/plugins";
    }
    if (const char* home = std::getenv("HOME")) {
        return fs::path(home) / ".local/share/kick/plugins";
    }
    return "/tmp/kick_plugins";
#endif
}

extern "C" const clap_plugin_entry_t* synthyboy_clap_get_entry(void);

PluginManager::PluginManager()
    : pluginsRoot_(defaultPluginsRoot()),
      installer_(std::make_unique<PluginInstaller>(pluginsRoot_, &trustStore_)) {
    scanInstalledPlugins();
    registerBuiltinPlugins();
}

void PluginManager::initialize(const fs::path& customRoot) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (!customRoot.empty()) {
        pluginsRoot_ = customRoot;
    } else {
        pluginsRoot_ = defaultPluginsRoot();
    }
    installer_ = std::make_unique<PluginInstaller>(pluginsRoot_, &trustStore_);
    scanInstalledPlugins();
    registerBuiltinPlugins();
}

void PluginManager::registerBuiltinPlugins() {
    if (const auto* entry = synthyboy_clap_get_entry()) {
        PluginManifest synthyManifest;
        synthyManifest.id = "com.safian.synthyboy";
        synthyManifest.name = "SynthyBoy";
        synthyManifest.version = "1.0.0";
        synthyManifest.author = "Safian";
        synthyManifest.license = "AGPL-3.0-or-later";
        synthyManifest.kind = PluginKind::Instrument;
        synthyManifest.description = "Synthesizer plugin (VST3 + CLAP)";

        auto loader = std::make_shared<StaticPluginLoader>(synthyManifest, entry, TrustLevel::Official);
        registerStaticPlugin(loader);
    }
}

void PluginManager::registerStaticPlugin(std::shared_ptr<StaticPluginLoader> loader) {
    if (!loader) return;
    std::lock_guard<std::mutex> lock(mutex_);
    staticLoaders_[loader->getManifest().id] = std::move(loader);
}

void PluginManager::scanInstalledPlugins() {
    std::lock_guard<std::mutex> lock(mutex_);
    dynamicLoaders_.clear();

    if (!installer_) return;

    auto pluginIds = installer_->listInstalledPluginIds();
    for (const auto& id : pluginIds) {
        auto manifestOpt = installer_->getManifest(id);
        if (!manifestOpt) continue;

        fs::path binPath = installer_->getBinaryPath(id);
        if (binPath.empty() || !fs::exists(binPath)) continue;

        auto trustEval = trustStore_.evaluateManifest(*manifestOpt);
        auto loader = std::make_shared<DynamicPluginLoader>(*manifestOpt, binPath, trustEval.level);
        dynamicLoaders_[id] = loader;
    }
}

std::vector<std::shared_ptr<PluginLoader>> PluginManager::getAllLoaders() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::vector<std::shared_ptr<PluginLoader>> loaders;
    for (const auto& pair : staticLoaders_) {
        loaders.push_back(pair.second);
    }
    for (const auto& pair : dynamicLoaders_) {
        // If an ID exists in both, static or dynamic?
        // If installed dynamically as update to static, dynamic overrides
        if (staticLoaders_.find(pair.first) == staticLoaders_.end()) {
            loaders.push_back(pair.second);
        } else {
            loaders.push_back(pair.second);
        }
    }
    return loaders;
}

std::vector<std::shared_ptr<PluginLoader>> PluginManager::getInstrumentLoaders() const {
    auto all = getAllLoaders();
    std::vector<std::shared_ptr<PluginLoader>> res;
    for (const auto& l : all) {
        if (l->getManifest().kind == PluginKind::Instrument) {
            res.push_back(l);
        }
    }
    return res;
}

std::vector<std::shared_ptr<PluginLoader>> PluginManager::getEffectLoaders() const {
    auto all = getAllLoaders();
    std::vector<std::shared_ptr<PluginLoader>> res;
    for (const auto& l : all) {
        if (l->getManifest().kind == PluginKind::Effect) {
            res.push_back(l);
        }
    }
    return res;
}

std::shared_ptr<PluginLoader> PluginManager::findLoaderById(const std::string& pluginId) const {
    std::lock_guard<std::mutex> lock(mutex_);
    auto itDyn = dynamicLoaders_.find(pluginId);
    if (itDyn != dynamicLoaders_.end()) return itDyn->second;

    auto itStat = staticLoaders_.find(pluginId);
    if (itStat != staticLoaders_.end()) return itStat->second;

    return nullptr;
}

std::vector<Descriptor> PluginManager::getAllDescriptors() const {
    auto loaders = getAllLoaders();
    std::vector<Descriptor> descs;
    for (const auto& l : loaders) {
        descs.push_back(l->getDescriptor());
    }
    return descs;
}

std::vector<Descriptor> PluginManager::getInstrumentDescriptors() const {
    auto loaders = getInstrumentLoaders();
    std::vector<Descriptor> descs;
    for (const auto& l : loaders) {
        descs.push_back(l->getDescriptor());
    }
    return descs;
}

std::vector<Descriptor> PluginManager::getEffectDescriptors() const {
    auto loaders = getEffectLoaders();
    std::vector<Descriptor> descs;
    for (const auto& l : loaders) {
        descs.push_back(l->getDescriptor());
    }
    return descs;
}

std::vector<fs::path> PluginManager::getPluginSearchPaths() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::set<fs::path> uniquePaths;

    if (installer_) {
        for (const auto& id : installer_->listInstalledPluginIds()) {
            fs::path vDir = installer_->getVersionDirectory(id);
            if (!vDir.empty() && fs::exists(vDir)) {
                uniquePaths.insert(vDir);
            }
        }
    }

    std::vector<fs::path> result(uniquePaths.begin(), uniquePaths.end());
    return result;
}

void PluginManager::registerRemotePlugin(const RemotePluginDescriptor& desc) {
    std::lock_guard<std::mutex> lock(mutex_);
    remoteCatalog_.push_back(desc);
}

std::vector<RemotePluginDescriptor> PluginManager::getAvailableRemotePlugins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return remoteCatalog_;
}

} // namespace kick::clap
