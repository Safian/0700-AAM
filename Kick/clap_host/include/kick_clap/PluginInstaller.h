// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginManifest.h"
#include "TrustStore.h"

#include <filesystem>
#include <string>
#include <vector>
#include <map>
#include <optional>

namespace kick::clap {

struct InstallOptions {
    bool        explicitUserApproval{false}; // Required if trust level is Unknown
    bool        overwriteExisting{false};
    std::string targetPlatform;              // If empty, defaults to currentPlatform()
};

struct InstallResult {
    bool                  success{false};
    std::string           error;
    TrustEvaluation       trust;
    std::string           installedVersion;
    std::filesystem::path installedPath;     // Directory where version is installed
    std::filesystem::path binaryPath;        // Path to the installed .clap binary
};

class PluginInstaller {
public:
    explicit PluginInstaller(std::filesystem::path pluginsRoot, TrustStore* trustStore = nullptr);

    const std::filesystem::path& pluginsRoot() const { return pluginsRoot_; }

    // Core installation entry points
    InstallResult installFromDirectory(const std::filesystem::path& packageDir, const InstallOptions& options = {});
    InstallResult installFromFiles(const PluginManifest& manifest,
                                  const std::filesystem::path& binaryPath,
                                  const std::map<std::string, std::filesystem::path>& assetFiles = {},
                                  const InstallOptions& options = {});

    // Version management & query
    std::vector<std::string> listInstalledPluginIds() const;
    std::vector<std::string> listInstalledVersions(const std::string& pluginId) const;
    std::string getActiveVersion(const std::string& pluginId) const;
    bool setActiveVersion(const std::string& pluginId, const std::string& version);

    std::optional<PluginManifest> getManifest(const std::string& pluginId, const std::string& version = "") const;
    std::filesystem::path getBinaryPath(const std::string& pluginId, const std::string& version = "") const;
    std::filesystem::path getVersionDirectory(const std::string& pluginId, const std::string& version = "") const;

    // Rollback
    bool canRollback(const std::string& pluginId) const;
    std::string getPreviousVersion(const std::string& pluginId) const;
    bool rollback(const std::string& pluginId, std::string& outNewActiveVersion);

    // Uninstall
    bool uninstallVersion(const std::string& pluginId, const std::string& version);
    bool uninstallPlugin(const std::string& pluginId);

    // Asset / Content Pack Management
    bool isAssetPackInstalled(const std::string& pluginId, const std::string& version, const std::string& assetId) const;
    bool uninstallAssetPack(const std::string& pluginId, const std::string& version, const std::string& assetId);
    bool restoreAssetPack(const std::string& pluginId, const std::string& version, const std::string& assetId,
                          const std::filesystem::path& assetSource);
    std::filesystem::path getAssetPackPath(const std::string& pluginId, const std::string& version, const std::string& assetId) const;

private:
    std::filesystem::path pluginsRoot_;
    TrustStore*           trustStore_{nullptr};

    std::filesystem::path getPluginDir(const std::string& pluginId) const;
    std::filesystem::path getCurrentVersionFile(const std::string& pluginId) const;
};

} // namespace kick::clap
