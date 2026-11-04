// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <string>
#include <vector>
#include <map>
#include <optional>
#include <cstdint>

namespace kick::clap {

enum class PluginKind {
    Instrument,
    Effect
};

std::string pluginKindToString(PluginKind kind);
bool stringToPluginKind(const std::string& str, PluginKind& outKind);

// Supported platforms
extern const char* const PLATFORM_LINUX_ARMV7;
extern const char* const PLATFORM_LINUX_AARCH64;
extern const char* const PLATFORM_LINUX_X86_64;
extern const char* const PLATFORM_MACOS_UNIVERSAL;
extern const char* const PLATFORM_MACOS_ARM64;
extern const char* const PLATFORM_MACOS_X86_64;
extern const char* const PLATFORM_WINDOWS_X64;

std::string currentPlatform();
bool isPlatformCompatible(const std::string& buildPlatform, const std::string& hostPlatform = "");

int compareSemVer(const std::string& v1, const std::string& v2);
bool isValidSemVer(const std::string& v);
bool isValidPluginId(const std::string& id);

struct PluginBuild {
    std::string platform;   // e.g. "linux-armv7", "macos-universal", etc.
    std::string url;        // Download URL or relative path to .clap binary
    std::string sha256;     // Hex SHA256 of the binary (64 chars)
    uint64_t    size{0};    // File size in bytes
    std::string entryPoint; // Optional descriptor ID or binary subpath
};

struct PluginAssetPack {
    std::string id;         // e.g. "factory-presets", "sample-pack"
    std::string name;       // Human readable name
    std::string version;    // Asset pack version
    std::string url;        // Download URL or relative archive path
    std::string sha256;     // Hex SHA256 of the asset pack (64 chars)
    uint64_t    size{0};    // File size in bytes
    bool        required{true};
    std::string type;       // "presets", "samples", "wavetables"
    std::string license;    // SPDX license for content
};

struct PluginPermissions {
    bool        network{false};
    std::string filesystem{"isolated"}; // "isolated", "readonly", "none"
};

struct ManifestValidationResult {
    bool valid{false};
    std::vector<std::string> errors;

    std::string errorSummary() const {
        std::string s;
        for (const auto& err : errors) {
            if (!s.empty()) s += "; ";
            s += err;
        }
        return s;
    }
};

struct PluginManifest {
    std::string id;                 // Unique reverse-DNS identifier (e.g. "com.safian.synthyboy")
    std::string name;               // Display name (e.g. "SynthyBoy")
    std::string version;            // Semantic version (e.g. "1.0.0")
    std::string author;             // Author / organization (e.g. "Safian")
    std::string license;            // Mandatory SPDX license identifier (e.g. "AGPL-3.0-or-later")
    std::string homepage;           // Project URL
    std::string description;        // Summary / description
    PluginKind  kind{PluginKind::Instrument};
    std::string minHostVersion;     // Minimum Kick host version required (e.g. "0.3.0")

    std::map<std::string, PluginBuild> builds;
    std::vector<PluginAssetPack>       assets;
    PluginPermissions                  permissions;

    std::string signature;          // 128 hex chars (64 bytes Ed25519 detached signature)
    std::string keyId;              // Identifier of public key used to sign

    // Serialization & Parsing
    bool parseJson(const std::string& jsonString, std::string& outError);
    std::string toJson(bool includeSignature = true, bool pretty = false) const;
    std::string canonicalSigningPayload() const;

    // Schema Validation
    ManifestValidationResult validate() const;

    // Platform Matching
    const PluginBuild* findBuildForPlatform(const std::string& platform) const;
    const PluginBuild* findBuildForCurrentPlatform() const;

    // Asset Lookup
    const PluginAssetPack* findAssetPack(const std::string& assetId) const;
};

} // namespace kick::clap
