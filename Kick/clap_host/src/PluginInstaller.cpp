// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_clap/PluginInstaller.h"
#include "../../modules/3rdParty/tweetnacl/crypto_utils.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <chrono>
#include <random>

namespace kick::clap {

namespace fs = std::filesystem;

PluginInstaller::PluginInstaller(fs::path pluginsRoot, TrustStore* trustStore)
    : pluginsRoot_(std::move(pluginsRoot)), trustStore_(trustStore) {
    if (!trustStore_) {
        trustStore_ = &TrustStore::defaultStore();
    }
}

fs::path PluginInstaller::getPluginDir(const std::string& pluginId) const {
    return pluginsRoot_ / "plugins" / pluginId;
}

fs::path PluginInstaller::getCurrentVersionFile(const std::string& pluginId) const {
    return getPluginDir(pluginId) / "current_version.txt";
}

InstallResult PluginInstaller::installFromDirectory(const fs::path& packageDir, const InstallOptions& options) {
    InstallResult res;
    if (!fs::exists(packageDir) || !fs::is_directory(packageDir)) {
        res.error = "Package directory does not exist: " + packageDir.string();
        return res;
    }

    fs::path manifestFile = packageDir / "manifest.json";
    if (!fs::exists(manifestFile)) {
        res.error = "manifest.json not found in package directory";
        return res;
    }

    std::ifstream ifs(manifestFile);
    if (!ifs.is_open()) {
        res.error = "Failed to open manifest.json";
        return res;
    }
    std::string jsonStr((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    PluginManifest manifest;
    std::string parseErr;
    if (!manifest.parseJson(jsonStr, parseErr)) {
        res.error = "Failed to parse manifest.json: " + parseErr;
        return res;
    }

    // Locate binary in packageDir
    std::string targetPlatform = options.targetPlatform.empty() ? currentPlatform() : options.targetPlatform;
    const auto* build = manifest.findBuildForPlatform(targetPlatform);
    if (!build) {
        res.error = "Manifest has no compatible build for platform: " + targetPlatform;
        return res;
    }

    fs::path binaryPath;
    if (!build->entryPoint.empty() && fs::exists(packageDir / build->entryPoint)) {
        binaryPath = packageDir / build->entryPoint;
    } else {
        // Look for .clap file or directory matching manifest.id
        for (const auto& entry : fs::directory_iterator(packageDir)) {
            if (entry.path().extension() == ".clap") {
                binaryPath = entry.path();
                break;
            }
        }
    }

    if (binaryPath.empty() || !fs::exists(binaryPath)) {
        res.error = "Plugin binary (.clap) not found in package directory";
        return res;
    }

    // Collect asset files if assets/ folder exists
    std::map<std::string, fs::path> assetFiles;
    fs::path assetsDir = packageDir / "assets";
    if (fs::exists(assetsDir) && fs::is_directory(assetsDir)) {
        for (const auto& a : manifest.assets) {
            fs::path p = assetsDir / a.id;
            if (fs::exists(p)) {
                assetFiles[a.id] = p;
            }
        }
    }

    return installFromFiles(manifest, binaryPath, assetFiles, options);
}

static std::string generateRandomId() {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    std::mt19937_64 rng(static_cast<uint64_t>(now));
    return std::to_string(rng());
}

InstallResult PluginInstaller::installFromFiles(const PluginManifest& manifest,
                                                const fs::path& binaryPath,
                                                const std::map<std::string, fs::path>& assetFiles,
                                                const InstallOptions& options) {
    InstallResult res;

    // 1. Schema Validation
    auto valRes = manifest.validate();
    if (!valRes.valid) {
        res.error = "Manifest schema validation failed: " + valRes.errorSummary();
        return res;
    }

    // 2. Signature and Trust Store Check
    res.trust = trustStore_->evaluateManifest(manifest);
    if (res.trust.level == TrustLevel::Revoked) {
        res.error = "Installation blocked: signing key is revoked";
        return res;
    }
    if (res.trust.level == TrustLevel::Expired) {
        res.error = "Installation blocked: signing key has expired";
        return res;
    }
    if (!manifest.signature.empty() && !res.trust.signatureValid) {
        res.error = "Installation blocked: cryptographic signature is invalid or corrupted";
        return res;
    }
    if (res.trust.level == TrustLevel::Unknown && !options.explicitUserApproval) {
        res.error = "Plugin publisher is unknown/unverified. Explicit user approval is required to install.";
        return res;
    }

    // 3. Platform build check
    std::string targetPlatform = options.targetPlatform.empty() ? currentPlatform() : options.targetPlatform;
    const auto* build = manifest.findBuildForPlatform(targetPlatform);
    if (!build) {
        res.error = "No compatible build found for platform: " + targetPlatform;
        return res;
    }

    // 4. SHA256 verification of binary
    if (!fs::exists(binaryPath)) {
        res.error = "Binary file does not exist: " + binaryPath.string();
        return res;
    }
    std::string computedSha256 = crypto::sha256_file(binaryPath);
    if (computedSha256 != build->sha256) {
        res.error = "Binary SHA256 checksum mismatch (expected: " + build->sha256 + ", got: " + computedSha256 + ")";
        return res;
    }

    // 5. Check if version already exists
    fs::path pluginDir = getPluginDir(manifest.id);
    fs::path versionDir = pluginDir / manifest.version;
    if (fs::exists(versionDir) && !options.overwriteExisting) {
        res.error = "Version " + manifest.version + " of plugin " + manifest.id + " is already installed";
        return res;
    }

    // 6. Atomic Installation using Staging Directory
    fs::path stagingDir = pluginDir / (".staging_" + manifest.version + "_" + generateRandomId());
    try {
        fs::create_directories(stagingDir);

        // Copy binary
        std::string binFileName = binaryPath.filename().string();
        fs::path destBinary = stagingDir / binFileName;
        if (fs::is_directory(binaryPath)) {
            fs::copy(binaryPath, destBinary, fs::copy_options::recursive);
        } else {
            fs::copy_file(binaryPath, destBinary, fs::copy_options::overwrite_existing);
        }

        // Write manifest.json
        fs::path destManifest = stagingDir / "manifest.json";
        std::ofstream ofs(destManifest);
        if (!ofs.is_open()) {
            fs::remove_all(stagingDir);
            res.error = "Failed to write manifest.json to staging directory";
            return res;
        }
        ofs << manifest.toJson(true, true);
        ofs.close();

        // Install asset files
        if (!assetFiles.empty()) {
            fs::path stagingAssets = stagingDir / "assets";
            fs::create_directories(stagingAssets);
            for (const auto& pair : assetFiles) {
                const std::string& assetId = pair.first;
                const fs::path& srcPath = pair.second;
                if (fs::exists(srcPath)) {
                    // Check asset SHA256 if defined in manifest
                    const auto* assetPack = manifest.findAssetPack(assetId);
                    if (assetPack && !assetPack->sha256.empty()) {
                        std::string assetHash = fs::is_directory(srcPath) ? "" : crypto::sha256_file(srcPath);
                        if (!assetHash.empty() && assetHash != assetPack->sha256) {
                            fs::remove_all(stagingDir);
                            res.error = "Asset '" + assetId + "' SHA256 mismatch (expected: " + assetPack->sha256 + ", got: " + assetHash + ")";
                            return res;
                        }
                    }

                    fs::path destAssetDir = stagingAssets / assetId;
                    if (fs::is_directory(srcPath)) {
                        fs::copy(srcPath, destAssetDir, fs::copy_options::recursive);
                    } else {
                        fs::create_directories(destAssetDir);
                        fs::copy_file(srcPath, destAssetDir / srcPath.filename(), fs::copy_options::overwrite_existing);
                    }
                }
            }
        }

        // Atomic Rename from Staging to Version directory
        if (fs::exists(versionDir)) {
            fs::remove_all(versionDir);
        }
        fs::rename(stagingDir, versionDir);

        // Update active version
        setActiveVersion(manifest.id, manifest.version);

        res.success = true;
        res.installedVersion = manifest.version;
        res.installedPath = versionDir;
        res.binaryPath = versionDir / binFileName;
        return res;

    } catch (const std::exception& e) {
        if (fs::exists(stagingDir)) {
            std::error_code ec;
            fs::remove_all(stagingDir, ec);
        }
        res.error = std::string("Installation exception: ") + e.what();
        return res;
    }
}

std::vector<std::string> PluginInstaller::listInstalledPluginIds() const {
    std::vector<std::string> ids;
    fs::path base = pluginsRoot_ / "plugins";
    if (!fs::exists(base) || !fs::is_directory(base)) return ids;

    for (const auto& entry : fs::directory_iterator(base)) {
        if (entry.is_directory()) {
            std::string id = entry.path().filename().string();
            // Must contain at least one version subdirectory
            if (!listInstalledVersions(id).empty()) {
                ids.push_back(id);
            }
        }
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> PluginInstaller::listInstalledVersions(const std::string& pluginId) const {
    std::vector<std::string> versions;
    fs::path dir = getPluginDir(pluginId);
    if (!fs::exists(dir) || !fs::is_directory(dir)) return versions;

    for (const auto& entry : fs::directory_iterator(dir)) {
        if (entry.is_directory()) {
            std::string name = entry.path().filename().string();
            if (!name.empty() && name[0] != '.' && isValidSemVer(name)) {
                // Must have a manifest.json
                if (fs::exists(entry.path() / "manifest.json")) {
                    versions.push_back(name);
                }
            }
        }
    }

    std::sort(versions.begin(), versions.end(), [](const std::string& a, const std::string& b) {
        return compareSemVer(a, b) < 0;
    });
    return versions;
}

std::string PluginInstaller::getActiveVersion(const std::string& pluginId) const {
    fs::path curFile = getCurrentVersionFile(pluginId);
    if (fs::exists(curFile)) {
        std::ifstream ifs(curFile);
        if (ifs.is_open()) {
            std::string ver;
            ifs >> ver;
            if (!ver.empty() && fs::exists(getPluginDir(pluginId) / ver)) {
                return ver;
            }
        }
    }

    auto versions = listInstalledVersions(pluginId);
    if (!versions.empty()) {
        return versions.back(); // Highest version
    }
    return "";
}

bool PluginInstaller::setActiveVersion(const std::string& pluginId, const std::string& version) {
    fs::path versionDir = getPluginDir(pluginId) / version;
    if (!fs::exists(versionDir) || !fs::is_directory(versionDir)) {
        return false;
    }

    fs::path curFile = getCurrentVersionFile(pluginId);
    std::ofstream ofs(curFile);
    if (!ofs.is_open()) return false;
    ofs << version << "\n";
    return true;
}

std::optional<PluginManifest> PluginInstaller::getManifest(const std::string& pluginId, const std::string& versionIn) const {
    std::string ver = versionIn.empty() ? getActiveVersion(pluginId) : versionIn;
    if (ver.empty()) return std::nullopt;

    fs::path manifestFile = getPluginDir(pluginId) / ver / "manifest.json";
    if (!fs::exists(manifestFile)) return std::nullopt;

    std::ifstream ifs(manifestFile);
    if (!ifs.is_open()) return std::nullopt;
    std::string content((std::istreambuf_iterator<char>(ifs)), std::istreambuf_iterator<char>());

    PluginManifest m;
    std::string err;
    if (!m.parseJson(content, err)) return std::nullopt;
    return m;
}

fs::path PluginInstaller::getVersionDirectory(const std::string& pluginId, const std::string& versionIn) const {
    std::string ver = versionIn.empty() ? getActiveVersion(pluginId) : versionIn;
    if (ver.empty()) return {};
    return getPluginDir(pluginId) / ver;
}

fs::path PluginInstaller::getBinaryPath(const std::string& pluginId, const std::string& versionIn) const {
    fs::path vDir = getVersionDirectory(pluginId, versionIn);
    if (vDir.empty() || !fs::exists(vDir)) return {};

    for (const auto& entry : fs::directory_iterator(vDir)) {
        if (entry.path().extension() == ".clap") {
            return entry.path();
        }
    }
    return {};
}

bool PluginInstaller::canRollback(const std::string& pluginId) const {
    return !getPreviousVersion(pluginId).empty();
}

std::string PluginInstaller::getPreviousVersion(const std::string& pluginId) const {
    std::string active = getActiveVersion(pluginId);
    if (active.empty()) return "";

    auto versions = listInstalledVersions(pluginId);
    std::string previous;
    for (const auto& v : versions) {
        if (compareSemVer(v, active) < 0) {
            if (previous.empty() || compareSemVer(v, previous) > 0) {
                previous = v;
            }
        }
    }
    return previous;
}

bool PluginInstaller::rollback(const std::string& pluginId, std::string& outNewActiveVersion) {
    std::string prev = getPreviousVersion(pluginId);
    if (prev.empty()) return false;

    if (setActiveVersion(pluginId, prev)) {
        outNewActiveVersion = prev;
        return true;
    }
    return false;
}

bool PluginInstaller::uninstallVersion(const std::string& pluginId, const std::string& version) {
    fs::path vDir = getPluginDir(pluginId) / version;
    if (!fs::exists(vDir)) return false;

    std::error_code ec;
    fs::remove_all(vDir, ec);

    // If that was the active version, switch to highest remaining or clean up
    std::string active = getActiveVersion(pluginId);
    if (active == version) {
        auto remaining = listInstalledVersions(pluginId);
        if (!remaining.empty()) {
            setActiveVersion(pluginId, remaining.back());
        } else {
            fs::remove_all(getPluginDir(pluginId), ec);
        }
    }
    return true;
}

bool PluginInstaller::uninstallPlugin(const std::string& pluginId) {
    fs::path dir = getPluginDir(pluginId);
    if (!fs::exists(dir)) return false;
    std::error_code ec;
    fs::remove_all(dir, ec);
    return true;
}

fs::path PluginInstaller::getAssetPackPath(const std::string& pluginId, const std::string& version, const std::string& assetId) const {
    fs::path vDir = getVersionDirectory(pluginId, version);
    if (vDir.empty()) return {};
    return vDir / "assets" / assetId;
}

bool PluginInstaller::isAssetPackInstalled(const std::string& pluginId, const std::string& version, const std::string& assetId) const {
    fs::path p = getAssetPackPath(pluginId, version, assetId);
    return fs::exists(p) && !fs::is_empty(p);
}

bool PluginInstaller::uninstallAssetPack(const std::string& pluginId, const std::string& version, const std::string& assetId) {
    fs::path p = getAssetPackPath(pluginId, version, assetId);
    if (!fs::exists(p)) return false;
    std::error_code ec;
    fs::remove_all(p, ec);
    return !ec;
}

bool PluginInstaller::restoreAssetPack(const std::string& pluginId, const std::string& version, const std::string& assetId,
                                      const fs::path& assetSource) {
    if (!fs::exists(assetSource)) return false;

    fs::path p = getAssetPackPath(pluginId, version, assetId);
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);

    if (fs::exists(p)) {
        fs::remove_all(p, ec);
    }

    if (fs::is_directory(assetSource)) {
        fs::copy(assetSource, p, fs::copy_options::recursive, ec);
    } else {
        fs::create_directories(p, ec);
        fs::copy_file(assetSource, p / assetSource.filename(), fs::copy_options::overwrite_existing, ec);
    }
    return !ec;
}

} // namespace kick::clap
