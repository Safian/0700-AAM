// SPDX-License-Identifier: AGPL-3.0-or-later
#include "mt_clap_effect.h"
#include <AudIO/Includes/ObjectFactory.h>
#include <common/source/kick_logger.h>
#include <clap/ext/params.h>
#include <algorithm>

namespace mt {

ClapEffect::ClapEffect()
    : AudIO::Object(2, 2, 0) {
    setName("CLAP Effect");
}

ClapEffect::ClapEffect(const kick::clap::Descriptor& desc)
    : AudIO::Object(2, 2, 0)
    , descriptor(desc)
    , pluginId(desc.id)
    , version(desc.version)
    , path(desc.path) {
    setName(desc.name.empty() ? desc.id : desc.name);
}

ClapEffect::~ClapEffect() {
    unload();
}

bool ClapEffect::loadPlugin(const kick::clap::Descriptor& desc, double rate, uint32_t maxBlockSize, const std::vector<uint8_t>& state) {
    descriptor = desc;
    pluginId = desc.id;
    version = desc.version;
    path = desc.path;
    setName(desc.name.empty() ? desc.id : desc.name);
    missing = false;

    if (rate <= 0) rate = 48000.0;
    if (maxBlockSize == 0) maxBlockSize = 512;

    kick_log("clap", "Loading CLAP effect: %s (id: %s, path: %s)", desc.name.c_str(), desc.id.c_str(), desc.path.c_str());
    if (!host.load(desc, rate, maxBlockSize, state)) {
        statusMessage = "Failed to load CLAP effect: " + host.error();
        kick_log("error", "%s", statusMessage.c_str());
        return false;
    }

    _numParams = static_cast<uint32_t>(host.parameters().size());
    _parameters.resize(_numParams);
    statusMessage = host.supportsState() ? "Ready" : "Ready (state saving not supported)";
    return true;
}

bool ClapEffect::loadPluginById(const std::string& id, const std::string& pth, double rate, uint32_t maxBlockSize, const std::vector<uint8_t>& state) {
    const auto* desc = ClapEffectRegistry::getInstance().findById(id);
    if (!desc && !pth.empty()) {
        try {
            auto res = kick::clap::inspect(pth);
            for (const auto& p : res.plugins) {
                if (p.id == id) {
                    ClapEffectRegistry::getInstance().registerDescriptor(p);
                    desc = ClapEffectRegistry::getInstance().findById(id);
                    break;
                }
            }
        } catch (...) {}
    }

    if (desc) {
        return loadPlugin(*desc, rate, maxBlockSize, state);
    }

    // Missing plugin fallback
    markMissing(id, id, "", pth, state);
    return false;
}

void ClapEffect::unload() {
    if (host.loaded()) {
        host.unload();
    }
}

void ClapEffect::prepareToPlay(uint32_t samplerate, uint32_t blocksize) {
    AudIO::Object::prepareToPlay(samplerate, blocksize);
    if (!missing && host.loaded()) {
        host.reconfigure(static_cast<double>(samplerate), blocksize);
    } else if (!missing && !pluginId.empty() && !host.loaded()) {
        if (!descriptor.path.empty() && !descriptor.id.empty()) {
            loadPlugin(descriptor, static_cast<double>(samplerate), blocksize, savedStateBlob);
        } else {
            loadPluginById(pluginId, path, static_cast<double>(samplerate), blocksize, savedStateBlob);
        }
    }
}

void ClapEffect::process(AudIO::Buffer<float>& buffer) {
    if (missing || !host.loaded() || buffer.getBlockSize() == 0) return;
    auto* outputs = buffer.getOutputs();
    uint32_t channels = buffer.getNumOutputChannels();
    host.process(outputs, channels, outputs, channels, buffer.getBlockSize());
}

int ClapEffect::getNumParameters() {
    if (missing || !host.loaded()) return 0;
    return static_cast<int>(host.parameters().size());
}

float ClapEffect::getParameter(int index) {
    if (missing || !host.loaded()) return 0.0f;
    const auto& params = host.parameters();
    if (index >= 0 && static_cast<size_t>(index) < params.size()) {
        return static_cast<float>(host.parameterValue(params[static_cast<size_t>(index)].id));
    }
    return 0.0f;
}

void ClapEffect::setParameter(int index, float value) {
    if (missing || !host.loaded()) return;
    const auto& params = host.parameters();
    if (index >= 0 && static_cast<size_t>(index) < params.size()) {
        host.setParameter(params[static_cast<size_t>(index)].id, static_cast<double>(value));
    }
}

const std::vector<AudIO::ParamInfo>& ClapEffect::getParameters() {
    if (missing || !host.loaded()) {
        _parameters.clear();
        return _parameters;
    }
    const auto& params = host.parameters();
    _parameters.resize(params.size());
    for (size_t i = 0; i < params.size(); ++i) {
        const auto& p = params[i];
        _parameters[i].name = p.name;
        _parameters[i].unit = p.module;
        const double val = host.parameterValue(p.id);
        _parameters[i].value = static_cast<float>(val);
        _parameters[i].displayValue = host.parameterText(p.id, val);
        const bool stepped = ( p.flags & CLAP_PARAM_IS_STEPPED ) != 0;
        _parameters[i].isStepped = stepped;
        if( stepped && p.maximum > p.minimum )
        {
            _parameters[i].step = static_cast<float>( 1.0 / ( p.maximum - p.minimum ) );
        }
        else
        {
            _parameters[i].step = 0.001f;
        }
    }
    return _parameters;
}

void ClapEffect::getParameterName(int index, char* text) {
    if (!text) return;
    text[0] = '\0';
    if (missing || !host.loaded()) return;
    const auto& params = host.parameters();
    if (index >= 0 && static_cast<size_t>(index) < params.size()) {
        std::snprintf(text, 64, "%s", params[static_cast<size_t>(index)].name.c_str());
    }
}

void ClapEffect::getParameterDisplay(int index, char* text) {
    if (!text) return;
    text[0] = '\0';
    if (missing || !host.loaded()) return;
    const auto& params = host.parameters();
    if (index >= 0 && static_cast<size_t>(index) < params.size()) {
        const auto val = host.parameterValue(params[static_cast<size_t>(index)].id);
        const auto str = host.parameterText(params[static_cast<size_t>(index)].id, val);
        std::snprintf(text, 64, "%s", str.c_str());
    }
}

void ClapEffect::markMissing(const std::string& id, const std::string& name, const std::string& ver, const std::string& pth, const std::vector<uint8_t>& state) {
    missing = true;
    pluginId = id;
    version = ver;
    path = pth;
    savedStateBlob = state;
    setName(name.empty() ? id : name);
    statusMessage = "Missing CLAP plugin: " + id;
    kick_log("warn", "Marked CLAP effect as missing: %s", id.c_str());
}

bool ClapEffect::saveState(std::vector<uint8_t>& outState) {
    if (missing) {
        outState = savedStateBlob;
        return true;
    }
    if (!host.loaded()) {
        outState = savedStateBlob;
        return true;
    }
    if (host.supportsState()) {
        if (host.saveState(outState)) {
            savedStateBlob = outState;
            return true;
        }
    }
    return false;
}

bool ClapEffect::loadState(const std::vector<uint8_t>& inState) {
    savedStateBlob = inState;
    if (missing || !host.loaded()) return true;
    if (host.supportsState() && !inState.empty()) {
        return host.loadState(inState);
    }
    return true;
}

void ClapEffect::beginGesture(int index) {
    if (missing || !host.loaded()) return;
    const auto& params = host.parameters();
    if (index >= 0 && static_cast<size_t>(index) < params.size()) {
        host.parameterGesture(params[static_cast<size_t>(index)].id, true);
    }
}

void ClapEffect::endGesture(int index) {
    if (missing || !host.loaded()) return;
    const auto& params = host.parameters();
    if (index >= 0 && static_cast<size_t>(index) < params.size()) {
        host.parameterGesture(params[static_cast<size_t>(index)].id, false);
    }
}

// ClapEffectRegistry implementation
ClapEffectRegistry& ClapEffectRegistry::getInstance() {
    static ClapEffectRegistry instance;
    return instance;
}

void ClapEffectRegistry::registerDescriptor(const kick::clap::Descriptor& desc) {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = std::find_if(descriptors.begin(), descriptors.end(),
                           [&](const kick::clap::Descriptor& d) { return d.id == desc.id; });
    if (it == descriptors.end()) {
        descriptors.push_back(desc);
    } else {
        *it = desc;
    }

    // Register with AudIO ObjectFactory so it shows up in MTAudioInterface
    AudIO::ObjectFactory::Get().Register(desc.name, [desc]() -> std::unique_ptr<AudIO::Object> {
        return std::make_unique<ClapEffect>(desc);
    });
    AudIO::ObjectFactory::Get().AddInfo(desc.name, {
        desc.vendor.empty() ? "CLAP" : desc.vendor,
        "CLAP",
        "Plugin",
        desc.name
    });
}

void ClapEffectRegistry::scanEffects(bool force) {
    std::lock_guard<std::mutex> lock(mutex);
    if (scanned && !force) return;
    scanned = true;

    auto roots = kick::clap::defaultSearchPaths();
    auto customFile = juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory).getChildFile("KICK/clap-paths.txt");
    if (customFile.existsAsFile()) {
        auto lines = juce::StringArray::fromLines(customFile.loadFileAsString());
        for (const auto& line : lines) {
            auto trimmed = line.trim();
            if (trimmed.isNotEmpty()) roots.emplace_back(trimmed.toStdString());
        }
    }

    auto binaries = kick::clap::findBinaries(roots);
    for (const auto& bin : binaries) {
        try {
            auto res = kick::clap::inspect(bin);
            for (const auto& p : res.plugins) {
                // Check if audio effect
                bool isEffect = false;
                for (const auto& feat : p.features) {
                    if (feat == CLAP_PLUGIN_FEATURE_AUDIO_EFFECT || feat == "audio-effect") {
                        isEffect = true;
                        break;
                    }
                }
                // If features don't specify, check if not an instrument
                if (!isEffect) {
                    bool isInst = false;
                    for (const auto& feat : p.features) {
                        if (feat == CLAP_PLUGIN_FEATURE_INSTRUMENT || feat == "instrument") {
                            isInst = true;
                            break;
                        }
                    }
                    if (!isInst) isEffect = true;
                }

                if (isEffect) {
                    auto it = std::find_if(descriptors.begin(), descriptors.end(),
                                           [&](const kick::clap::Descriptor& d) { return d.id == p.id; });
                    if (it == descriptors.end()) {
                        descriptors.push_back(p);
                        AudIO::ObjectFactory::Get().Register(p.name, [p]() -> std::unique_ptr<AudIO::Object> {
                            return std::make_unique<ClapEffect>(p);
                        });
                        AudIO::ObjectFactory::Get().AddInfo(p.name, {
                            p.vendor.empty() ? "CLAP" : p.vendor,
                            "CLAP",
                            "Plugin",
                            p.name
                        });
                        kick_log("clap", "Registered CLAP effect: %s (%s)", p.name.c_str(), p.id.c_str());
                    }
                }
            }
        } catch (const std::exception& e) {
            kick_log("warn", "Failed to inspect CLAP binary %s: %s", bin.string().c_str(), e.what());
        } catch (...) {
            kick_log("warn", "Failed to inspect CLAP binary %s: unknown exception", bin.string().c_str());
        }
    }

    // Also register any static or installed effect descriptors from PluginManager
    for (const auto& p : kick::clap::PluginManager::getInstance().getEffectDescriptors()) {
        auto it = std::find_if(descriptors.begin(), descriptors.end(),
                               [&](const kick::clap::Descriptor& d) { return d.id == p.id; });
        if (it == descriptors.end()) {
            descriptors.push_back(p);
            AudIO::ObjectFactory::Get().Register(p.name, [p]() -> std::unique_ptr<AudIO::Object> {
                return std::make_unique<ClapEffect>(p);
            });
            AudIO::ObjectFactory::Get().AddInfo(p.name, {
                p.vendor.empty() ? "CLAP" : p.vendor,
                "CLAP",
                "Plugin",
                p.name
            });
            kick_log("clap", "Registered PluginManager effect: %s (%s)", p.name.c_str(), p.id.c_str());
        }
    }
}

const std::vector<kick::clap::Descriptor>& ClapEffectRegistry::getEffectDescriptors() const {
    return descriptors;
}

const kick::clap::Descriptor* ClapEffectRegistry::findById(const std::string& id) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = std::find_if(descriptors.begin(), descriptors.end(),
                           [&](const kick::clap::Descriptor& d) { return d.id == id; });
    return it != descriptors.end() ? &(*it) : nullptr;
}

const kick::clap::Descriptor* ClapEffectRegistry::findByName(const std::string& name) const {
    std::lock_guard<std::mutex> lock(mutex);
    auto it = std::find_if(descriptors.begin(), descriptors.end(),
                           [&](const kick::clap::Descriptor& d) { return d.name == name; });
    return it != descriptors.end() ? &(*it) : nullptr;
}

std::vector<juce::String> ClapEffectRegistry::getEffectNames() const {
    std::lock_guard<std::mutex> lock(mutex);
    std::vector<juce::String> names;
    for (const auto& d : descriptors) {
        names.emplace_back(juce::String(d.name));
    }
    return names;
}

} // namespace mt
