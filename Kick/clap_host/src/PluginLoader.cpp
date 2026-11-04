// SPDX-License-Identifier: AGPL-3.0-or-later
#include "kick_clap/PluginLoader.h"

#if defined(_WIN32)
#include <windows.h>
#else
#include <dlfcn.h>
#endif

namespace kick::clap {

// --- StaticPluginLoader ---

StaticPluginLoader::StaticPluginLoader(PluginManifest manifest,
                                       const clap_plugin_entry_t* entry,
                                       TrustLevel trustLevel)
    : manifest_(std::move(manifest)), entry_(entry), trustLevel_(trustLevel) {
}

Descriptor StaticPluginLoader::getDescriptor() const {
    Descriptor d;
    d.id = manifest_.id;
    d.name = manifest_.name;
    d.vendor = manifest_.author;
    d.version = manifest_.version;
    d.description = manifest_.description;
    d.path = "internal:static";
    d.features = (manifest_.kind == PluginKind::Instrument)
                     ? std::vector<std::string>{ "instrument" }
                     : std::vector<std::string>{ "audio-effect" };
    return d;
}

// --- DynamicPluginLoader ---

DynamicPluginLoader::DynamicPluginLoader(PluginManifest manifest,
                                         std::filesystem::path binaryPath,
                                         TrustLevel trustLevel)
    : manifest_(std::move(manifest)), binaryPath_(std::move(binaryPath)), trustLevel_(trustLevel) {
}

DynamicPluginLoader::~DynamicPluginLoader() {
    unload();
}

bool DynamicPluginLoader::load() {
    if (handle_) return true;

    std::string pathStr = binaryPath_.string();
#if defined(_WIN32)
    handle_ = static_cast<void*>(LoadLibraryA(pathStr.c_str()));
    if (!handle_) return false;
    entry_ = reinterpret_cast<const clap_plugin_entry_t*>(GetProcAddress(static_cast<HMODULE>(handle_), "clap_entry"));
#else
    handle_ = dlopen(pathStr.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (!handle_) return false;
    entry_ = static_cast<const clap_plugin_entry_t*>(dlsym(handle_, "clap_entry"));
#endif

    if (!entry_) {
        unload();
        return false;
    }

    if (entry_->init && !entry_->init(pathStr.c_str())) {
        unload();
        return false;
    }

    return true;
}

void DynamicPluginLoader::unload() {
    if (entry_ && entry_->deinit) {
        entry_->deinit();
    }
    entry_ = nullptr;

    if (handle_) {
#if defined(_WIN32)
        FreeLibrary(static_cast<HMODULE>(handle_));
#else
        dlclose(handle_);
#endif
        handle_ = nullptr;
    }
}

const clap_plugin_entry_t* DynamicPluginLoader::getClapEntry() {
    if (!handle_) {
        if (!load()) return nullptr;
    }
    return entry_;
}

Descriptor DynamicPluginLoader::getDescriptor() const {
    Descriptor d;
    d.id = manifest_.id;
    d.name = manifest_.name;
    d.vendor = manifest_.author;
    d.version = manifest_.version;
    d.description = manifest_.description;
    d.path = binaryPath_.string();
    d.features = (manifest_.kind == PluginKind::Instrument)
                     ? std::vector<std::string>{ "instrument" }
                     : std::vector<std::string>{ "audio-effect" };
    return d;
}

} // namespace kick::clap
