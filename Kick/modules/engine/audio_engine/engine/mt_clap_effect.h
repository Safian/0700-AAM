// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <AudIO/Includes/AudioObject.h>
#include <kick_clap/Host.h>
#include <kick_clap/Discovery.h>
#include <kick_clap/PluginManager.h>
#include <juce_core/juce_core.h>
#include <string>
#include <vector>
#include <memory>
#include <mutex>

namespace mt {

class ClapEffect : public AudIO::Object {
public:
    ClapEffect();
    explicit ClapEffect(const kick::clap::Descriptor& desc);
    ~ClapEffect() override;

    bool loadPlugin(const kick::clap::Descriptor& desc, double rate, uint32_t maxBlockSize, const std::vector<uint8_t>& state = {});
    bool loadPluginById(const std::string& id, const std::string& path, double rate, uint32_t maxBlockSize, const std::vector<uint8_t>& state = {});
    void unload();

    // AudIO::Object implementation
    void prepareToPlay(uint32_t samplerate, uint32_t blocksize) override;
    void process(AudIO::Buffer<float>& buffer) override;
    int getNumParameters() override;
    float getParameter(int index) override;
    void setParameter(int index, float value) override;
    const std::vector<AudIO::ParamInfo>& getParameters() override;
    void getParameterName(int index, char* text) override;
    void getParameterDisplay(int index, char* text) override;

    // CLAP-specific metadata and state
    kick::clap::Host& getHost() noexcept { return host; }
    const kick::clap::Host& getHost() const noexcept { return host; }
    const kick::clap::Descriptor& getDescriptor() const noexcept { return descriptor; }
    const std::string& getPluginId() const noexcept { return pluginId; }
    const std::string& getPluginVersion() const noexcept { return version; }
    const std::string& getPluginPath() const noexcept { return path; }

    bool isLoaded() const noexcept { return host.loaded(); }
    bool isMissing() const noexcept { return missing; }
    void markMissing(const std::string& id, const std::string& name, const std::string& ver, const std::string& pth, const std::vector<uint8_t>& state);
    const std::string& getStatusMessage() const noexcept { return statusMessage; }

    uint32_t getLatency() const noexcept { return missing ? 0 : host.latency(); }
    uint32_t getTail() const noexcept { return missing ? 0 : host.tail(); }

    bool saveState(std::vector<uint8_t>& outState);
    bool loadState(const std::vector<uint8_t>& inState);

    // Automation gesture helpers
    void beginGesture(int index);
    void endGesture(int index);

private:
    kick::clap::Host host;
    kick::clap::Descriptor descriptor;
    std::string pluginId;
    std::string version;
    std::string path;
    bool missing{ false };
    std::string statusMessage{ "Ready" };
    std::vector<uint8_t> savedStateBlob;
};

// Singleton / Global Registry for CLAP Effects
class ClapEffectRegistry {
public:
    static ClapEffectRegistry& getInstance();

    // Scans default and custom search paths for CLAP effects
    void scanEffects(bool force = false);

    const std::vector<kick::clap::Descriptor>& getEffectDescriptors() const;
    const kick::clap::Descriptor* findById(const std::string& id) const;
    const kick::clap::Descriptor* findByName(const std::string& name) const;
    void registerDescriptor(const kick::clap::Descriptor& desc);

    std::vector<juce::String> getEffectNames() const;

private:
    ClapEffectRegistry() = default;
    mutable std::mutex mutex;
    std::vector<kick::clap::Descriptor> descriptors;
    bool scanned{ false };
};

} // namespace mt
