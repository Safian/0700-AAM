// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for PluginManifest, Ed25519 signatures, TrustStore, PluginInstaller, and PluginLoader
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -I../../clap_host/include -I../../clap_host/third_party/clap/include -I../../modules/3rdParty/tweetnacl PluginManagerTests.cpp ../../clap_host/src/PluginManifest.cpp ../../clap_host/src/TrustStore.cpp ../../clap_host/src/PluginInstaller.cpp ../../clap_host/src/PluginLoader.cpp ../../clap_host/src/PluginManager.cpp ../../modules/3rdParty/tweetnacl/tweetnacl.c ../../modules/3rdParty/tweetnacl/sha256.c ../../modules/3rdParty/tweetnacl/crypto_utils.cpp -ldl -o /tmp/pm_test && /tmp/pm_test

#include <iostream>
#include <vector>
#include <string>
#include <cassert>
#include <fstream>
#include <filesystem>
#include <chrono>

#include <kick_clap/PluginManifest.h>
#include <kick_clap/TrustStore.h>
#include <kick_clap/PluginInstaller.h>
#include <kick_clap/PluginLoader.h>
#include <kick_clap/PluginManager.h>
#include "../../modules/3rdParty/tweetnacl/crypto_utils.h"

namespace fs = std::filesystem;
using namespace kick::clap;

static int g_fail = 0;

#define CHECK(cond, msg) \
    do { \
        if (!(cond)) { \
            std::cerr << "FAIL [" << __FILE__ << ":" << __LINE__ << "]: " << (msg) << '\n'; \
            ++g_fail; \
        } \
    } while (0)

static fs::path createTempDir(const std::string& prefix) {
    auto now = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    fs::path p = fs::temp_directory_path() / (prefix + "_" + std::to_string(now));
    fs::create_directories(p);
    return p;
}

static void testSemVerAndPlatform() {
    std::cout << "[TEST] testSemVerAndPlatform...\n";

    CHECK(compareSemVer("1.0.0", "1.0.0") == 0, "equal versions");
    CHECK(compareSemVer("1.0.0", "1.0.1") < 0, "1.0.0 < 1.0.1");
    CHECK(compareSemVer("1.1.0", "1.0.9") > 0, "1.1.0 > 1.0.9");
    CHECK(compareSemVer("2.0.0", "1.99.99") > 0, "2.0.0 > 1.99.99");
    CHECK(compareSemVer("1.0.0-beta", "1.0.0") < 0, "prerelease is lower precedence than release");

    CHECK(isValidSemVer("0.1.0"), "0.1.0 is valid semver");
    CHECK(isValidSemVer("1.2.3-rc1"), "1.2.3-rc1 is valid semver");
    CHECK(!isValidSemVer(""), "empty version invalid");
    CHECK(!isValidSemVer("1"), "single number invalid");
    CHECK(!isValidSemVer("1.2"), "two numbers invalid");

    CHECK(isValidPluginId("com.safian.synthyboy"), "valid reverse-dns id");
    CHECK(isValidPluginId("org.kick.eq-plugin"), "valid id with hyphen");
    CHECK(!isValidPluginId(""), "empty id invalid");
    CHECK(!isValidPluginId("synthyboy"), "id without dot invalid");

    // Platform matching
    CHECK(isPlatformCompatible("linux-armv7", "linux-armv7"), "linux-armv7 matches linux-armv7");
    CHECK(isPlatformCompatible("macos-universal", "macos-arm64"), "macos-universal compatible with macos-arm64");
    CHECK(isPlatformCompatible("macos-universal", "macos-x86_64"), "macos-universal compatible with macos-x86_64");
    CHECK(isPlatformCompatible("windows-x64", "windows-x64"), "windows-x64 matches windows-x64");
    CHECK(!isPlatformCompatible("linux-armv7", "macos-arm64"), "linux-armv7 incompatible with macos-arm64");
}

static void testManifestValidation() {
    std::cout << "[TEST] testManifestValidation...\n";

    PluginManifest m;
    m.id = "com.safian.synthyboy";
    m.name = "SynthyBoy";
    m.version = "1.0.0";
    m.author = "Safian";
    m.license = "AGPL-3.0-or-later";
    m.kind = PluginKind::Instrument;

    PluginBuild bArm;
    bArm.platform = PLATFORM_LINUX_ARMV7;
    bArm.sha256 = "1111222233334444555566667777888899990000aaaabbbbccccddddeeeeffff";
    bArm.size = 1048576;
    m.builds[bArm.platform] = bArm;

    PluginBuild bMac;
    bMac.platform = PLATFORM_MACOS_UNIVERSAL;
    bMac.sha256 = "aaaabbbbccccddddeeeeffff1111222233334444555566667777888899990000";
    bMac.size = 2097152;
    m.builds[bMac.platform] = bMac;

    auto valRes = m.validate();
    CHECK(valRes.valid, "Valid manifest passes validation");

    // License is strictly mandatory
    PluginManifest mNoLicense = m;
    mNoLicense.license = "";
    auto resNoLicense = mNoLicense.validate();
    CHECK(!resNoLicense.valid, "Missing license must fail validation");

    // Missing id
    PluginManifest mNoId = m;
    mNoId.id = "";
    CHECK(!mNoId.validate().valid, "Missing id must fail validation");

    // Invalid semver
    PluginManifest mBadVer = m;
    mBadVer.version = "invalid-version";
    CHECK(!mBadVer.validate().valid, "Invalid version must fail validation");

    // Empty builds
    PluginManifest mNoBuilds = m;
    mNoBuilds.builds.clear();
    CHECK(!mNoBuilds.validate().valid, "Empty builds must fail validation");

    // Bad sha256
    PluginManifest mBadSha = m;
    mBadSha.builds[PLATFORM_LINUX_ARMV7].sha256 = "not-a-sha256";
    CHECK(!mBadSha.validate().valid, "Bad sha256 must fail validation");

    // JSON roundtrip and canonical payload
    std::string jsonStr = m.toJson(true, false);
    PluginManifest mParsed;
    std::string parseErr;
    CHECK(mParsed.parseJson(jsonStr, parseErr), "Manifest JSON parses successfully");
    CHECK(mParsed.id == m.id, "ID preserved across parse");
    CHECK(mParsed.license == "AGPL-3.0-or-later", "License preserved across parse");
    CHECK(mParsed.builds.size() == 2, "Builds count preserved");

    std::string payload1 = m.canonicalSigningPayload();
    m.signature = "abcdef";
    std::string payload2 = m.canonicalSigningPayload();
    CHECK(payload1 == payload2, "canonicalSigningPayload is deterministic and independent of signature");
}

static void testEd25519SignatureAndTrustStore() {
    std::cout << "[TEST] testEd25519SignatureAndTrustStore...\n";

    TrustStore store;

    // Generate Official keypair
    uint8_t offPk[32], offSk[64];
    CHECK(kick::crypto::ed25519_create_keypair(offPk, offSk), "Created official keypair");
    std::string offPkHex = kick::crypto::to_hex(offPk, 32);
    std::string offSkHex = kick::crypto::to_hex(offSk, 64);

    TrustedKey offKey;
    offKey.keyId = "safian-official-test";
    offKey.name = "Safian Official Test Key";
    offKey.publicKeyHex = offPkHex;
    offKey.isOfficial = true;
    store.addKey(offKey);

    // Generate Verified Publisher keypair
    uint8_t verPk[32], verSk[64];
    CHECK(kick::crypto::ed25519_create_keypair(verPk, verSk), "Created verified keypair");
    std::string verPkHex = kick::crypto::to_hex(verPk, 32);
    std::string verSkHex = kick::crypto::to_hex(verSk, 64);

    TrustedKey verKey;
    verKey.keyId = "verified-partner-2026";
    verKey.name = "Partner Audio Labs";
    verKey.publicKeyHex = verPkHex;
    verKey.isOfficial = false;
    store.addKey(verKey);

    // Base manifest
    PluginManifest m;
    m.id = "com.safian.synthyboy";
    m.name = "SynthyBoy";
    m.version = "1.0.0";
    m.author = "Safian";
    m.license = "AGPL-3.0-or-later";
    m.builds["macos-universal"].sha256 = "1111222233334444555566667777888899990000aaaabbbbccccddddeeeeffff";

    // 1. Unsigned manifest -> Unknown
    auto evalUnsigned = store.evaluateManifest(m);
    CHECK(evalUnsigned.level == TrustLevel::Unknown, "Unsigned manifest is Unknown");
    CHECK(!evalUnsigned.signatureValid, "Unsigned signatureValid is false");

    // 2. Official signature -> Official
    CHECK(TrustStore::signManifest(m, "safian-official-test", offSkHex), "Signed with official key");
    auto evalOfficial = store.evaluateManifest(m);
    CHECK(evalOfficial.level == TrustLevel::Official, "Evaluates to TrustLevel::Official");
    CHECK(evalOfficial.signatureValid, "Official signature is valid");

    // 3. Verified signature -> Verified
    CHECK(TrustStore::signManifest(m, "verified-partner-2026", verSkHex), "Signed with verified key");
    auto evalVerified = store.evaluateManifest(m);
    CHECK(evalVerified.level == TrustLevel::Verified, "Evaluates to TrustLevel::Verified");
    CHECK(evalVerified.signatureValid, "Verified signature is valid");

    // 4. Unknown keypair
    uint8_t unkPk[32], unkSk[64];
    CHECK(kick::crypto::ed25519_create_keypair(unkPk, unkSk), "Created unknown keypair");
    std::string unkSkHex = kick::crypto::to_hex(unkSk, 64);

    CHECK(TrustStore::signManifest(m, "unknown-key-999", unkSkHex), "Signed with unknown key");
    auto evalUnknown = store.evaluateManifest(m);
    CHECK(evalUnknown.level == TrustLevel::Unknown, "Unregistered key evaluates to Unknown");

    // 5. Tampered manifest -> Signature mismatch
    CHECK(TrustStore::signManifest(m, "safian-official-test", offSkHex), "Re-signed with official key");
    m.description = "Tampered description injected after signing!";
    auto evalTampered = store.evaluateManifest(m);
    CHECK(!evalTampered.signatureValid, "Tampered manifest signature fails verification");
    CHECK(evalTampered.level == TrustLevel::Unknown, "Tampered manifest is rejected as Unknown");

    // 6. Revoked key
    CHECK(TrustStore::signManifest(m, "verified-partner-2026", verSkHex), "Signed with verified key");
    store.revokeKey("verified-partner-2026");
    auto evalRevoked = store.evaluateManifest(m);
    CHECK(evalRevoked.level == TrustLevel::Revoked, "Revoked key evaluates to TrustLevel::Revoked");

    // 7. Expired key
    TrustedKey expKey;
    expKey.keyId = "expired-key-1";
    expKey.name = "Old Key";
    expKey.publicKeyHex = verPkHex;
    expKey.expiresAtEpoch = 1000; // Far in past
    store.addKey(expKey);

    m.keyId = "expired-key-1";
    auto evalExpired = store.evaluateManifest(m, 2000);
    CHECK(evalExpired.level == TrustLevel::Expired, "Expired key evaluates to TrustLevel::Expired");
}

static void testAtomicInstallationAndRollback() {
    std::cout << "[TEST] testAtomicInstallationAndRollback...\n";

    fs::path tempRoot = createTempDir("kick_test_plugins");
    TrustStore store;

    // Set up test publisher
    uint8_t offPk[32], offSk[64];
    kick::crypto::ed25519_create_keypair(offPk, offSk);
    std::string offPkHex = kick::crypto::to_hex(offPk, 32);
    std::string offSkHex = kick::crypto::to_hex(offSk, 64);

    TrustedKey offKey;
    offKey.keyId = "safian-test";
    offKey.name = "Safian Test";
    offKey.publicKeyHex = offPkHex;
    offKey.isOfficial = true;
    store.addKey(offKey);

    PluginInstaller installer(tempRoot, &store);

    // Create a mock .clap binary file
    fs::path mockBinPath = tempRoot / "mock_plugin.clap";
    {
        std::ofstream ofs(mockBinPath, std::ios::binary);
        ofs << "MOCK CLAP BINARY DATA VERSION 1.0.0";
    }
    std::string mockSha256_v1 = kick::crypto::sha256_file(mockBinPath);

    // Create Manifest v1.0.0
    PluginManifest m1;
    m1.id = "com.safian.synthyboy";
    m1.name = "SynthyBoy";
    m1.version = "1.0.0";
    m1.author = "Safian";
    m1.license = "AGPL-3.0-or-later";
    m1.kind = PluginKind::Instrument;

    PluginBuild b1;
    b1.platform = currentPlatform();
    b1.sha256 = mockSha256_v1;
    b1.size = fs::file_size(mockBinPath);
    m1.builds[b1.platform] = b1;

    // 1. Unknown publisher without approval must fail
    PluginManifest mUnapproved = m1;
    mUnapproved.keyId = "unregistered-key";
    InstallOptions optNoApprove;
    optNoApprove.explicitUserApproval = false;
    auto resNoApprove = installer.installFromFiles(mUnapproved, mockBinPath, {}, optNoApprove);
    CHECK(!resNoApprove.success, "Installation without approval from unknown publisher fails");
    CHECK(installer.listInstalledPluginIds().empty(), "No plugins installed after rejected attempt");

    // 2. Corrupted SHA256 must fail
    PluginManifest mBadHash = m1;
    mBadHash.builds[b1.platform].sha256 = "0000000000000000000000000000000000000000000000000000000000000000";
    TrustStore::signManifest(mBadHash, "safian-test", offSkHex);
    auto resBadHash = installer.installFromFiles(mBadHash, mockBinPath);
    CHECK(!resBadHash.success, "Installation with mismatched SHA256 fails");
    CHECK(installer.listInstalledPluginIds().empty(), "No plugins installed after bad hash failure");

    // 3. Successful Install v1.0.0
    TrustStore::signManifest(m1, "safian-test", offSkHex);
    auto resV1 = installer.installFromFiles(m1, mockBinPath);
    CHECK(resV1.success, "v1.0.0 installed successfully");
    CHECK(installer.getActiveVersion(m1.id) == "1.0.0", "Active version is 1.0.0");
    CHECK(fs::exists(resV1.installedPath / "manifest.json"), "manifest.json exists in installed dir");
    CHECK(fs::exists(resV1.binaryPath), "binary exists in installed dir");

    // 4. Install update v1.1.0 (with older version retention)
    fs::path mockBinPathV2 = tempRoot / "mock_plugin_v2.clap";
    {
        std::ofstream ofs(mockBinPathV2, std::ios::binary);
        ofs << "MOCK CLAP BINARY DATA VERSION 1.1.0 ENHANCED";
    }
    std::string mockSha256_v2 = kick::crypto::sha256_file(mockBinPathV2);

    PluginManifest m2 = m1;
    m2.version = "1.1.0";
    m2.builds[b1.platform].sha256 = mockSha256_v2;
    m2.builds[b1.platform].size = fs::file_size(mockBinPathV2);
    TrustStore::signManifest(m2, "safian-test", offSkHex);

    auto resV2 = installer.installFromFiles(m2, mockBinPathV2);
    CHECK(resV2.success, "v1.1.0 installed successfully");
    CHECK(installer.getActiveVersion(m1.id) == "1.1.0", "Active version is updated to 1.1.0");

    // Check version retention: BOTH 1.0.0 and 1.1.0 exist on disk
    auto installedVersions = installer.listInstalledVersions(m1.id);
    CHECK(installedVersions.size() == 2, "Both 1.0.0 and 1.1.0 are retained");
    CHECK(installedVersions[0] == "1.0.0" && installedVersions[1] == "1.1.0", "Versions sorted correctly");

    // 5. Test Rollback to 1.0.0
    CHECK(installer.canRollback(m1.id), "Rollback is available");
    CHECK(installer.getPreviousVersion(m1.id) == "1.0.0", "Previous version is 1.0.0");

    std::string rolledBackVer;
    CHECK(installer.rollback(m1.id, rolledBackVer), "Rollback execution succeeded");
    CHECK(rolledBackVer == "1.0.0", "Rolled back version returned is 1.0.0");
    CHECK(installer.getActiveVersion(m1.id) == "1.0.0", "Active version is now 1.0.0");
    auto activeManifest = installer.getManifest(m1.id);
    CHECK(activeManifest.has_value() && activeManifest->version == "1.0.0", "Active manifest is v1.0.0");

    // Clean up temp dir
    std::error_code ec;
    fs::remove_all(tempRoot, ec);
}

static void testContentPacks() {
    std::cout << "[TEST] testContentPacks...\n";

    fs::path tempRoot = createTempDir("kick_test_assets");
    PluginInstaller installer(tempRoot);

    // Create mock plugin with asset pack
    fs::path binPath = tempRoot / "synth.clap";
    { std::ofstream(binPath) << "SYNTH"; }

    fs::path samplePackDir = tempRoot / "sample_source";
    fs::create_directories(samplePackDir);
    { std::ofstream(samplePackDir / "kick.wav") << "WAV"; }

    PluginManifest m;
    m.id = "com.safian.sampler";
    m.name = "KickSampler";
    m.version = "1.0.0";
    m.author = "Safian";
    m.license = "AGPL-3.0-or-later";
    m.builds[currentPlatform()].sha256 = kick::crypto::sha256_file(binPath);

    PluginAssetPack pack;
    pack.id = "factory-samples";
    pack.name = "Factory Samples";
    pack.version = "1.0.0";
    pack.type = "samples";
    pack.license = "CC-BY-4.0";
    m.assets.push_back(pack);

    InstallOptions opt;
    opt.explicitUserApproval = true;
    std::map<std::string, fs::path> assets;
    assets["factory-samples"] = samplePackDir;

    auto res = installer.installFromFiles(m, binPath, assets, opt);
    CHECK(res.success, "Installed with content pack");

    CHECK(installer.isAssetPackInstalled(m.id, m.version, "factory-samples"), "Asset pack is installed");

    // Uninstall asset pack (free storage)
    CHECK(installer.uninstallAssetPack(m.id, m.version, "factory-samples"), "Asset pack uninstalled");
    CHECK(!installer.isAssetPackInstalled(m.id, m.version, "factory-samples"), "Asset pack marked uninstalled");

    // Restore asset pack
    CHECK(installer.restoreAssetPack(m.id, m.version, "factory-samples", samplePackDir), "Asset pack restored");
    CHECK(installer.isAssetPackInstalled(m.id, m.version, "factory-samples"), "Asset pack marked installed after restore");

    std::error_code ec;
    fs::remove_all(tempRoot, ec);
}

static void testStaticAndDynamicPluginLoader() {
    std::cout << "[TEST] testStaticAndDynamicPluginLoader...\n";

    // Static loader
    PluginManifest staticM;
    staticM.id = "com.safian.synthyboy.builtin";
    staticM.name = "SynthyBoy Built-in";
    staticM.version = "1.0.0";
    staticM.author = "Safian";
    staticM.license = "AGPL-3.0-or-later";
    staticM.kind = PluginKind::Instrument;

    static clap_plugin_entry_t mockEntry;
    mockEntry.clap_version = CLAP_VERSION;
    mockEntry.init = [](const char*) { return true; };
    mockEntry.deinit = []() {};
    mockEntry.get_factory = [](const char*) -> const void* { return nullptr; };

    auto staticLoader = std::make_shared<StaticPluginLoader>(staticM, &mockEntry, TrustLevel::Official);
    CHECK(staticLoader->isStatic(), "StaticPluginLoader isStatic() is true");
    CHECK(staticLoader->isLoaded(), "StaticPluginLoader isLoaded() is true");
    CHECK(staticLoader->getClapEntry() == &mockEntry, "StaticPluginLoader returns entry point");
    CHECK(staticLoader->getTrustLevel() == TrustLevel::Official, "StaticPluginLoader has Official trust");

    // Polymorphism via PluginLoader interface
    PluginLoader* loaderPtr = staticLoader.get();
    CHECK(loaderPtr->getManifest().id == "com.safian.synthyboy.builtin", "Manifest accessed via interface");
    auto desc = loaderPtr->getDescriptor();
    CHECK(desc.id == "com.safian.synthyboy.builtin", "Descriptor matches manifest");

    // Test PluginManager integration
    auto& pm = PluginManager::getInstance();
    pm.registerStaticPlugin(staticLoader);

    auto found = pm.findLoaderById("com.safian.synthyboy.builtin");
    CHECK(found != nullptr, "Found static loader by ID");
    CHECK(found->isStatic(), "Found loader is static");

    auto instLoaders = pm.getInstrumentLoaders();
    bool foundInList = false;
    for (const auto& l : instLoaders) {
        if (l->getManifest().id == "com.safian.synthyboy.builtin") {
            foundInList = true;
            break;
        }
    }
    CHECK(foundInList, "Static loader found in instrument loaders list");

    // Remote downloadable catalog readiness
    RemotePluginDescriptor remote;
    remote.manifestUrl = "https://kick.safian.com/plugins/com.safian.kickeq/manifest.json";
    remote.id = "com.safian.kickeq";
    remote.name = "Kick EQ";
    remote.version = "1.0.0";
    remote.kind = PluginKind::Effect;
    pm.registerRemotePlugin(remote);

    auto remoteCatalog = pm.getAvailableRemotePlugins();
    CHECK(!remoteCatalog.empty(), "Remote catalog contains registered plugin");
    CHECK(remoteCatalog[0].manifestUrl == remote.manifestUrl, "Manifest URL matches");
}

int main() {
    std::cout << "========================================\n";
    std::cout << "Running Kick Plugin Ecosystem Host Tests\n";
    std::cout << "========================================\n";

    testSemVerAndPlatform();
    testManifestValidation();
    testEd25519SignatureAndTrustStore();
    testAtomicInstallationAndRollback();
    testContentPacks();
    testStaticAndDynamicPluginLoader();

    std::cout << "========================================\n";
    if (g_fail == 0) {
        std::cout << "ALL PLUGIN ECOSYSTEM HOST TESTS PASSED!\n";
        std::cout << "========================================\n";
        return 0;
    } else {
        std::cerr << g_fail << " TEST(S) FAILED!\n";
        std::cout << "========================================\n";
        return 1;
    }
}
