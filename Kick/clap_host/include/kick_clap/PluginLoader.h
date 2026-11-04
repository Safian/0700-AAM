// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "PluginManifest.h"
#include "TrustStore.h"
#include "Host.h"

#include <clap/clap.h>
#include <filesystem>
#include <memory>
#include <string>

namespace kick::clap {

class PluginLoader {
public:
    virtual ~PluginLoader() = default;

    virtual const PluginManifest& getManifest() const = 0;
    virtual bool isStatic() const = 0;
    virtual bool isLoaded() const = 0;
    virtual bool load() = 0;
    virtual void unload() = 0;

    virtual const clap_plugin_entry_t* getClapEntry() = 0;
    virtual std::filesystem::path getBinaryPath() const = 0;
    virtual Descriptor getDescriptor() const = 0;
    virtual TrustLevel getTrustLevel() const = 0;
};

class StaticPluginLoader : public PluginLoader {
public:
    StaticPluginLoader(PluginManifest manifest,
                       const clap_plugin_entry_t* entry,
                       TrustLevel trustLevel = TrustLevel::Official);
    ~StaticPluginLoader() override = default;

    const PluginManifest& getManifest() const override { return manifest_; }
    bool isStatic() const override { return true; }
    bool isLoaded() const override { return true; }
    bool load() override { return true; }
    void unload() override {}

    const clap_plugin_entry_t* getClapEntry() override { return entry_; }
    std::filesystem::path getBinaryPath() const override { return "internal:static"; }
    Descriptor getDescriptor() const override;
    TrustLevel getTrustLevel() const override { return trustLevel_; }

private:
    PluginManifest manifest_;
    const clap_plugin_entry_t* entry_{nullptr};
    TrustLevel trustLevel_{TrustLevel::Official};
};

class DynamicPluginLoader : public PluginLoader {
public:
    DynamicPluginLoader(PluginManifest manifest,
                        std::filesystem::path binaryPath,
                        TrustLevel trustLevel);
    ~DynamicPluginLoader() override;

    const PluginManifest& getManifest() const override { return manifest_; }
    bool isStatic() const override { return false; }
    bool isLoaded() const override { return handle_ != nullptr; }
    bool load() override;
    void unload() override;

    const clap_plugin_entry_t* getClapEntry() override;
    std::filesystem::path getBinaryPath() const override { return binaryPath_; }
    Descriptor getDescriptor() const override;
    TrustLevel getTrustLevel() const override { return trustLevel_; }

private:
    PluginManifest        manifest_;
    std::filesystem::path binaryPath_;
    TrustLevel            trustLevel_{TrustLevel::Unknown};
    void*                 handle_{nullptr};
    const clap_plugin_entry_t* entry_{nullptr};
};

} // namespace kick::clap
