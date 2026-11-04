// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginManifest.h"
#include <string>
#include <vector>
#include <map>
#include <set>
#include <cstdint>

namespace kick::clap {

enum class TrustLevel {
    Official,   // Signed by Kick / Safian official release key
    Verified,   // Signed by a registered, trusted publisher
    Unknown,    // Unsigned or signed by an unrecognized key
    Revoked,    // Signing key has been revoked
    Expired     // Signing key has expired
};

std::string trustLevelToString(TrustLevel level);

struct TrustedKey {
    std::string keyId;              // e.g. "safian-official-2026"
    std::string name;               // e.g. "Safian Official Release"
    std::string publicKeyHex;       // 64 hex characters (32 bytes Ed25519 public key)
    bool        isOfficial{false};  // True if official Kick key
    int64_t     expiresAtEpoch{0};  // Unix timestamp (0 = never expires)
};

struct TrustEvaluation {
    TrustLevel  level{TrustLevel::Unknown};
    std::string keyId;
    std::string publisherName;
    bool        signatureValid{false};
    std::string message;
};

class TrustStore {
public:
    TrustStore();
    static TrustStore& defaultStore();

    void addKey(const TrustedKey& key);
    void removeKey(const std::string& keyId);
    void revokeKey(const std::string& keyId);
    bool isRevoked(const std::string& keyId) const;

    const TrustedKey* findKey(const std::string& keyId) const;
    std::vector<TrustedKey> listKeys() const;

    // Evaluates trust level and cryptographic signature of a manifest
    TrustEvaluation evaluateManifest(const PluginManifest& manifest, int64_t currentTimestamp = 0) const;

    // Helper to sign a manifest with a given private key and keyId
    static bool signManifest(PluginManifest& manifest, const std::string& keyId, const std::string& secretKeyHex);

private:
    std::map<std::string, TrustedKey> keys_;
    std::set<std::string>             revokedKeyIds_;

    void initializeBuiltinKeys();
};

} // namespace kick::clap
