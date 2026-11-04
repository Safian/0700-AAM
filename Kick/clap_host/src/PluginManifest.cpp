// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_clap/PluginManifest.h"
#include "kick_clap/Json.h"

#include <algorithm>
#include <cctype>
#include <sstream>

namespace kick::clap {

const char* const PLATFORM_LINUX_ARMV7     = "linux-armv7";
const char* const PLATFORM_LINUX_AARCH64   = "linux-aarch64";
const char* const PLATFORM_LINUX_X86_64    = "linux-x86_64";
const char* const PLATFORM_MACOS_UNIVERSAL = "macos-universal";
const char* const PLATFORM_MACOS_ARM64     = "macos-arm64";
const char* const PLATFORM_MACOS_X86_64    = "macos-x86_64";
const char* const PLATFORM_WINDOWS_X64     = "windows-x64";

std::string pluginKindToString(PluginKind kind) {
    switch (kind) {
        case PluginKind::Instrument: return "instrument";
        case PluginKind::Effect:     return "effect";
    }
    return "instrument";
}

bool stringToPluginKind(const std::string& str, PluginKind& outKind) {
    if (str == "instrument" || str == "Instrument" || str == "synth") {
        outKind = PluginKind::Instrument;
        return true;
    }
    if (str == "effect" || str == "Effect" || str == "fx" || str == "audio-effect") {
        outKind = PluginKind::Effect;
        return true;
    }
    return false;
}

std::string currentPlatform() {
#if defined(__APPLE__)
    return PLATFORM_MACOS_UNIVERSAL;
#elif defined(__arm__) || defined(_M_ARM)
    return PLATFORM_LINUX_ARMV7;
#elif defined(__aarch64__)
    return PLATFORM_LINUX_AARCH64;
#elif defined(_WIN32)
    return PLATFORM_WINDOWS_X64;
#elif defined(__x86_64__)
    return PLATFORM_LINUX_X86_64;
#else
    return "unknown";
#endif
}

bool isPlatformCompatible(const std::string& buildPlatform, const std::string& hostPlatformIn) {
    std::string host = hostPlatformIn.empty() ? currentPlatform() : hostPlatformIn;
    if (buildPlatform == host) return true;

    // macOS universal binary runs on both macos-arm64 and macos-x86_64
    if (buildPlatform == PLATFORM_MACOS_UNIVERSAL &&
        (host == PLATFORM_MACOS_UNIVERSAL || host == PLATFORM_MACOS_ARM64 || host == PLATFORM_MACOS_X86_64)) {
        return true;
    }
    if (host == PLATFORM_MACOS_UNIVERSAL &&
        (buildPlatform == PLATFORM_MACOS_ARM64 || buildPlatform == PLATFORM_MACOS_X86_64)) {
        return true;
    }

    return false;
}

static void parseSemVerParts(const std::string& v, int& major, int& minor, int& patch, std::string& prerelease) {
    major = minor = patch = 0;
    prerelease.clear();
    if (v.empty()) return;

    size_t dash = v.find('-');
    std::string core = (dash == std::string::npos) ? v : v.substr(0, dash);
    if (dash != std::string::npos) {
        prerelease = v.substr(dash + 1);
    }

    std::stringstream ss(core);
    char dot1, dot2;
    if (ss >> major) {
        if (ss >> dot1 && dot1 == '.') {
            if (ss >> minor) {
                if (ss >> dot2 && dot2 == '.') {
                    ss >> patch;
                }
            }
        }
    }
}

int compareSemVer(const std::string& v1, const std::string& v2) {
    if (v1 == v2) return 0;
    int maj1, min1, pat1;
    int maj2, min2, pat2;
    std::string pre1, pre2;
    parseSemVerParts(v1, maj1, min1, pat1, pre1);
    parseSemVerParts(v2, maj2, min2, pat2, pre2);

    if (maj1 != maj2) return (maj1 < maj2) ? -1 : 1;
    if (min1 != min2) return (min1 < min2) ? -1 : 1;
    if (pat1 != pat2) return (pat1 < pat2) ? -1 : 1;

    // Versions without prerelease have higher precedence than those with prerelease
    if (pre1.empty() && !pre2.empty()) return 1;
    if (!pre1.empty() && pre2.empty()) return -1;
    if (pre1 < pre2) return -1;
    if (pre1 > pre2) return 1;
    return 0;
}

bool isValidSemVer(const std::string& v) {
    if (v.empty()) return false;
    int maj, min, pat;
    std::string pre;
    parseSemVerParts(v, maj, min, pat, pre);
    if (maj < 0 || min < 0 || pat < 0) return false;
    // Core numbers check
    size_t dot1 = v.find('.');
    if (dot1 == std::string::npos) return false;
    size_t dot2 = v.find('.', dot1 + 1);
    if (dot2 == std::string::npos) return false;
    return true;
}

bool isValidPluginId(const std::string& id) {
    if (id.empty() || id.size() > 128) return false;
    // Expect reverse-DNS style: letters, digits, '.', '-', '_'
    for (char c : id) {
        if (!std::isalnum(static_cast<unsigned char>(c)) && c != '.' && c != '-' && c != '_') {
            return false;
        }
    }
    return (id.find('.') != std::string::npos);
}

bool PluginManifest::parseJson(const std::string& jsonString, std::string& outError) {
    json::Value root;
    if (!json::Value::parse(jsonString, root, outError)) {
        return false;
    }
    if (!root.is_object()) {
        outError = "Root of manifest JSON must be an object";
        return false;
    }

    id = root.get("id").as_string();
    name = root.get("name").as_string();
    version = root.get("version").as_string();
    author = root.get("author").as_string();
    license = root.get("license").as_string();
    homepage = root.get("homepage").as_string();
    description = root.get("description").as_string();
    minHostVersion = root.get("min_host_version").as_string();

    std::string kindStr = root.get("kind").as_string();
    if (!stringToPluginKind(kindStr, kind)) {
        kind = PluginKind::Instrument;
    }

    // Builds
    builds.clear();
    const auto& buildsObj = root.get("builds").as_object();
    for (const auto& pair : buildsObj) {
        const auto& bVal = pair.second;
        if (bVal.is_object()) {
            PluginBuild b;
            b.platform = pair.first;
            b.url = bVal.get("url").as_string();
            b.sha256 = bVal.get("sha256").as_string();
            b.size = static_cast<uint64_t>(bVal.get("size").as_int());
            b.entryPoint = bVal.get("entry_point").as_string();
            builds[pair.first] = b;
        }
    }

    // Assets
    assets.clear();
    const auto& assetsArr = root.get("assets").as_array();
    for (const auto& aVal : assetsArr) {
        if (aVal.is_object()) {
            PluginAssetPack a;
            a.id = aVal.get("id").as_string();
            a.name = aVal.get("name").as_string();
            a.version = aVal.get("version").as_string();
            a.url = aVal.get("url").as_string();
            a.sha256 = aVal.get("sha256").as_string();
            a.size = static_cast<uint64_t>(aVal.get("size").as_int());
            a.required = aVal.get("required").as_bool(true);
            a.type = aVal.get("type").as_string();
            a.license = aVal.get("license").as_string();
            assets.push_back(a);
        }
    }

    // Permissions
    if (root.has("permissions")) {
        const auto& permObj = root.get("permissions");
        permissions.network = permObj.get("network").as_bool(false);
        permissions.filesystem = permObj.get("filesystem").get_string_or("isolated");
    }

    // Signature and Key
    signature = root.get("signature").as_string();
    keyId = root.get("key_id").as_string();

    return true;
}

std::string PluginManifest::toJson(bool includeSignature, bool pretty) const {
    json::Value root(std::map<std::string, json::Value>{});
    root.set("id", json::Value(id));
    root.set("name", json::Value(name));
    root.set("version", json::Value(version));
    root.set("author", json::Value(author));
    root.set("license", json::Value(license));
    if (!homepage.empty()) root.set("homepage", json::Value(homepage));
    if (!description.empty()) root.set("description", json::Value(description));
    root.set("kind", json::Value(pluginKindToString(kind)));
    if (!minHostVersion.empty()) root.set("min_host_version", json::Value(minHostVersion));

    // Builds
    json::Value buildsObj(std::map<std::string, json::Value>{});
    for (const auto& pair : builds) {
        json::Value b(std::map<std::string, json::Value>{});
        if (!pair.second.url.empty()) b.set("url", json::Value(pair.second.url));
        b.set("sha256", json::Value(pair.second.sha256));
        b.set("size", json::Value(pair.second.size));
        if (!pair.second.entryPoint.empty()) b.set("entry_point", json::Value(pair.second.entryPoint));
        buildsObj.set(pair.first, b);
    }
    root.set("builds", buildsObj);

    // Assets
    json::Value assetsArr(std::vector<json::Value>{});
    for (const auto& a : assets) {
        json::Value aObj(std::map<std::string, json::Value>{});
        aObj.set("id", json::Value(a.id));
        aObj.set("name", json::Value(a.name));
        if (!a.version.empty()) aObj.set("version", json::Value(a.version));
        if (!a.url.empty()) aObj.set("url", json::Value(a.url));
        aObj.set("sha256", json::Value(a.sha256));
        aObj.set("size", json::Value(a.size));
        aObj.set("required", json::Value(a.required));
        if (!a.type.empty()) aObj.set("type", json::Value(a.type));
        if (!a.license.empty()) aObj.set("license", json::Value(a.license));
        assetsArr.push_back(aObj);
    }
    root.set("assets", assetsArr);

    // Permissions
    json::Value permObj(std::map<std::string, json::Value>{});
    permObj.set("network", json::Value(permissions.network));
    permObj.set("filesystem", json::Value(permissions.filesystem));
    root.set("permissions", permObj);

    // Key and signature
    if (!keyId.empty()) root.set("key_id", json::Value(keyId));
    if (includeSignature && !signature.empty()) {
        root.set("signature", json::Value(signature));
    }

    return root.serialize(true, pretty ? 2 : 0);
}

std::string PluginManifest::canonicalSigningPayload() const {
    // Exact canonical JSON without the "signature" field
    return toJson(false, false);
}

static bool isValidSha256(const std::string& hash) {
    if (hash.size() != 64) return false;
    for (char c : hash) {
        if (!std::isxdigit(static_cast<unsigned char>(c))) return false;
    }
    return true;
}

ManifestValidationResult PluginManifest::validate() const {
    ManifestValidationResult res;
    res.valid = true;

    if (id.empty()) {
        res.valid = false;
        res.errors.push_back("Manifest 'id' is required");
    } else if (!isValidPluginId(id)) {
        res.valid = false;
        res.errors.push_back("Manifest 'id' format invalid (expecting reverse-DNS style, e.g. com.example.plugin)");
    }

    if (name.empty()) {
        res.valid = false;
        res.errors.push_back("Manifest 'name' is required");
    }

    if (version.empty()) {
        res.valid = false;
        res.errors.push_back("Manifest 'version' is required");
    } else if (!isValidSemVer(version)) {
        res.valid = false;
        res.errors.push_back("Manifest 'version' must follow semantic versioning (X.Y.Z)");
    }

    // License is strictly mandatory (SPDX identifier)
    if (license.empty()) {
        res.valid = false;
        res.errors.push_back("Manifest 'license' is mandatory (SPDX identifier, e.g. AGPL-3.0-or-later)");
    }

    if (!minHostVersion.empty() && !isValidSemVer(minHostVersion)) {
        res.valid = false;
        res.errors.push_back("Manifest 'min_host_version' must follow semantic versioning");
    }

    // Builds
    if (builds.empty()) {
        res.valid = false;
        res.errors.push_back("Manifest must specify at least one platform build");
    } else {
        for (const auto& pair : builds) {
            if (pair.second.sha256.empty()) {
                res.valid = false;
                res.errors.push_back("Build for platform '" + pair.first + "' is missing sha256 checksum");
            } else if (!isValidSha256(pair.second.sha256)) {
                res.valid = false;
                res.errors.push_back("Build for platform '" + pair.first + "' has invalid sha256 hex string");
            }
        }
    }

    // Assets
    for (const auto& a : assets) {
        if (a.id.empty()) {
            res.valid = false;
            res.errors.push_back("Asset pack missing id");
        }
        if (!a.sha256.empty() && !isValidSha256(a.sha256)) {
            res.valid = false;
            res.errors.push_back("Asset pack '" + a.id + "' has invalid sha256 checksum");
        }
    }

    return res;
}

const PluginBuild* PluginManifest::findBuildForPlatform(const std::string& targetPlatform) const {
    auto it = builds.find(targetPlatform);
    if (it != builds.end()) return &it->second;

    for (const auto& pair : builds) {
        if (isPlatformCompatible(pair.first, targetPlatform)) {
            return &pair.second;
        }
    }
    return nullptr;
}

const PluginBuild* PluginManifest::findBuildForCurrentPlatform() const {
    return findBuildForPlatform(currentPlatform());
}

const PluginAssetPack* PluginManifest::findAssetPack(const std::string& assetId) const {
    for (const auto& a : assets) {
        if (a.id == assetId) return &a;
    }
    return nullptr;
}

} // namespace kick::clap
