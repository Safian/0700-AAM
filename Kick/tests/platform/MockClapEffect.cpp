// SPDX-License-Identifier: MIT
#include "clap/clap.h"
#include <cstdio>
#include <cstring>
#include <atomic>
#include <vector>
#include <string>

namespace {

static constexpr const char* kMockPluginId = "org.antigravity.mock_clap_effect";
static constexpr const char* kMockPluginName = "Mock Delay/Gain Effect";

struct MockEffectInstance {
    clap_plugin_t plugin{};
    std::atomic<float> gain{ 1.0f };
    std::atomic<float> mix{ 1.0f };
    std::atomic<bool> isGuiCreated{ false };
    std::atomic<bool> isGuiVisible{ false };
    std::atomic<double> guiScale{ 1.0 };
    uint32_t guiWidth{ 640 };
    uint32_t guiHeight{ 480 };
    clap_window_t parentWindow{};
};

// Params extension
static uint32_t mockParamsCount(const clap_plugin_t*) {
    return 2;
}

static bool mockParamsGetInfo(const clap_plugin_t*, uint32_t index, clap_param_info_t* info) {
    if (!info) return false;
    if (index == 0) {
        info->id = 1000;
        info->flags = CLAP_PARAM_IS_AUTOMATABLE;
        info->min_value = 0.0;
        info->max_value = 2.0;
        info->default_value = 1.0;
        std::snprintf(info->name, sizeof(info->name), "Gain");
        std::snprintf(info->module, sizeof(info->module), "Main");
        return true;
    } else if (index == 1) {
        info->id = 1001;
        info->flags = CLAP_PARAM_IS_AUTOMATABLE;
        info->min_value = 0.0;
        info->max_value = 1.0;
        info->default_value = 1.0;
        std::snprintf(info->name, sizeof(info->name), "Mix");
        std::snprintf(info->module, sizeof(info->module), "Main");
        return true;
    }
    return false;
}

static bool mockParamsGetValue(const clap_plugin_t* plugin, clap_id param_id, double* out_value) {
    if (!plugin || !out_value) return false;
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    if (param_id == 1000) {
        *out_value = self->gain.load();
        return true;
    } else if (param_id == 1001) {
        *out_value = self->mix.load();
        return true;
    }
    return false;
}

static bool mockParamsValueToText(const clap_plugin_t*, clap_id param_id, double value, char* out_buffer, uint32_t out_buffer_capacity) {
    if (!out_buffer || out_buffer_capacity == 0) return false;
    if (param_id == 1000) {
        std::snprintf(out_buffer, out_buffer_capacity, "%.2f x", value);
        return true;
    } else if (param_id == 1001) {
        std::snprintf(out_buffer, out_buffer_capacity, "%.0f %%", value * 100.0);
        return true;
    }
    return false;
}

static bool mockParamsTextToValue(const clap_plugin_t*, clap_id, const char*, double*) {
    return false;
}

static void mockParamsFlush(const clap_plugin_t*, const clap_input_events_t*, const clap_output_events_t*) {}

static const clap_plugin_params_t sMockParamsExt = {
    mockParamsCount,
    mockParamsGetInfo,
    mockParamsGetValue,
    mockParamsValueToText,
    mockParamsTextToValue,
    mockParamsFlush
};

// Audio ports extension
static uint32_t mockAudioPortsCount(const clap_plugin_t*, bool) {
    return 1; // 1 stereo in, 1 stereo out
}

static bool mockAudioPortsGet(const clap_plugin_t*, uint32_t index, bool is_input, clap_audio_port_info_t* info) {
    if (index != 0 || !info) return false;
    info->id = is_input ? 0 : 1;
    std::snprintf(info->name, sizeof(info->name), "%s", is_input ? "Audio In" : "Audio Out");
    info->flags = CLAP_AUDIO_PORT_IS_MAIN;
    info->channel_count = 2;
    info->port_type = CLAP_PORT_STEREO;
    info->in_place_pair = is_input ? 1 : 0;
    return true;
}

static const clap_plugin_audio_ports_t sMockAudioPortsExt = {
    mockAudioPortsCount,
    mockAudioPortsGet
};

// Latency extension
static uint32_t mockLatencyGet(const clap_plugin_t*) {
    return 32; // 32 samples latency
}

static const clap_plugin_latency_t sMockLatencyExt = {
    mockLatencyGet
};

// Tail extension
static uint32_t mockTailGet(const clap_plugin_t*) {
    return 256; // 256 samples tail
}

static const clap_plugin_tail_t sMockTailExt = {
    mockTailGet
};

// State extension
static bool mockStateSave(const clap_plugin_t* plugin, const clap_ostream_t* stream) {
    if (!plugin || !stream) return false;
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    float vals[2] = { self->gain.load(), self->mix.load() };
    int64_t written = stream->write(stream, vals, sizeof(vals));
    return written == sizeof(vals);
}

static bool mockStateLoad(const clap_plugin_t* plugin, const clap_istream_t* stream) {
    if (!plugin || !stream) return false;
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    float vals[2] = { 1.0f, 1.0f };
    int64_t bytesRead = stream->read(stream, vals, sizeof(vals));
    if (bytesRead == sizeof(vals)) {
        self->gain.store(vals[0]);
        self->mix.store(vals[1]);
        return true;
    }
    return false;
}

static const clap_plugin_state_t sMockStateExt = {
    mockStateSave,
    mockStateLoad
};

// GUI extension
static bool mockGuiIsApiSupported(const clap_plugin_t*, const char* api, bool is_floating) {
    if (is_floating) return false;
    if (!api) return false;
    if (std::strcmp(api, CLAP_WINDOW_API_COCOA) == 0 ||
        std::strcmp(api, CLAP_WINDOW_API_X11) == 0 ||
        std::strcmp(api, CLAP_WINDOW_API_WIN32) == 0) {
        return true;
    }
    return false;
}

static bool mockGuiGetPreferredApi(const clap_plugin_t*, const char** api, bool* is_floating) {
    if (is_floating) *is_floating = false;
#if defined(__APPLE__)
    if (api) *api = CLAP_WINDOW_API_COCOA;
#elif defined(_WIN32)
    if (api) *api = CLAP_WINDOW_API_WIN32;
#else
    if (api) *api = CLAP_WINDOW_API_X11;
#endif
    return true;
}

static bool mockGuiCreate(const clap_plugin_t* plugin, const char* api, bool is_floating) {
    if (is_floating || !api) return false;
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    self->isGuiCreated.store(true);
    return true;
}

static void mockGuiDestroy(const clap_plugin_t* plugin) {
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    self->isGuiCreated.store(false);
    self->isGuiVisible.store(false);
}

static bool mockGuiSetScale(const clap_plugin_t* plugin, double scale) {
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    self->guiScale.store(scale);
    return true;
}

static bool mockGuiGetSize(const clap_plugin_t* plugin, uint32_t* width, uint32_t* height) {
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    if (width) *width = self->guiWidth;
    if (height) *height = self->guiHeight;
    return true;
}

static bool mockGuiCanResize(const clap_plugin_t*) {
    return true;
}

static bool mockGuiGetResizeHints(const clap_plugin_t*, clap_gui_resize_hints_t*) {
    return false;
}

static bool mockGuiAdjustSize(const clap_plugin_t*, uint32_t*, uint32_t*) {
    return true;
}

static bool mockGuiSetSize(const clap_plugin_t* plugin, uint32_t width, uint32_t height) {
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    self->guiWidth = width;
    self->guiHeight = height;
    return true;
}

static bool mockGuiSetParent(const clap_plugin_t* plugin, const clap_window_t* window) {
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    if (window) self->parentWindow = *window;
    return true;
}

static bool mockGuiSetTransient(const clap_plugin_t*, const clap_window_t*) {
    return true;
}

static void mockGuiSuggestTitle(const clap_plugin_t*, const char*) {}

static bool mockGuiShow(const clap_plugin_t* plugin) {
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    self->isGuiVisible.store(true);
    return true;
}

static bool mockGuiHide(const clap_plugin_t* plugin) {
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    self->isGuiVisible.store(false);
    return true;
}

static const clap_plugin_gui_t sMockGuiExt = {
    mockGuiIsApiSupported,
    mockGuiGetPreferredApi,
    mockGuiCreate,
    mockGuiDestroy,
    mockGuiSetScale,
    mockGuiGetSize,
    mockGuiCanResize,
    mockGuiGetResizeHints,
    mockGuiAdjustSize,
    mockGuiSetSize,
    mockGuiSetParent,
    mockGuiSetTransient,
    mockGuiSuggestTitle,
    mockGuiShow,
    mockGuiHide
};

// Plugin lifecycle
static bool pluginInit(const clap_plugin_t*) { return true; }
static void pluginDestroy(const clap_plugin_t* plugin) {
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);
    delete self;
}
static bool pluginActivate(const clap_plugin_t*, double, uint32_t, uint32_t) { return true; }
static void pluginDeactivate(const clap_plugin_t*) {}
static bool pluginStartProcessing(const clap_plugin_t*) { return true; }
static void pluginStopProcessing(const clap_plugin_t*) {}
static void pluginReset(const clap_plugin_t*) {}

static clap_process_status pluginProcess(const clap_plugin_t* plugin, const clap_process_t* process) {
    auto* self = static_cast<MockEffectInstance*>(plugin->plugin_data);

    // Drain events
    if (process->in_events) {
        const uint32_t numEvents = process->in_events->size(process->in_events);
        for (uint32_t i = 0; i < numEvents; ++i) {
            const auto* hdr = process->in_events->get(process->in_events, i);
            if (hdr->space_id == CLAP_CORE_EVENT_SPACE_ID && hdr->type == CLAP_EVENT_PARAM_VALUE) {
                const auto* pv = reinterpret_cast<const clap_event_param_value_t*>(hdr);
                if (pv->param_id == 1000) {
                    self->gain.store(static_cast<float>(pv->value));
                } else if (pv->param_id == 1001) {
                    self->mix.store(static_cast<float>(pv->value));
                }
            }
        }
    }

    const float curGain = self->gain.load();
    const float curMix = self->mix.load();

    if (process->audio_outputs_count > 0 && process->audio_outputs) {
        auto& out = process->audio_outputs[0];
        const bool hasInput = (process->audio_inputs_count > 0 && process->audio_inputs && process->audio_inputs[0].data32);
        for (uint32_t ch = 0; ch < out.channel_count; ++ch) {
            const float* inData = hasInput ? process->audio_inputs[0].data32[ch] : nullptr;
            float* outData = out.data32[ch];
            for (uint32_t f = 0; f < process->frames_count; ++f) {
                const float dry = inData ? inData[f] : 0.0f;
                outData[f] = dry * (1.0f - curMix) + (dry * curGain) * curMix;
            }
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

static const void* pluginGetExtension(const clap_plugin_t*, const char* id) {
    if (std::strcmp(id, CLAP_EXT_AUDIO_PORTS) == 0) return &sMockAudioPortsExt;
    if (std::strcmp(id, CLAP_EXT_PARAMS) == 0) return &sMockParamsExt;
    if (std::strcmp(id, CLAP_EXT_LATENCY) == 0) return &sMockLatencyExt;
    if (std::strcmp(id, CLAP_EXT_TAIL) == 0) return &sMockTailExt;
    if (std::strcmp(id, CLAP_EXT_STATE) == 0) return &sMockStateExt;
    if (std::strcmp(id, CLAP_EXT_GUI) == 0) return &sMockGuiExt;
    return nullptr;
}

static void pluginOnMainThread(const clap_plugin_t*) {}

static const char* const sMockFeatures[] = {
    CLAP_PLUGIN_FEATURE_AUDIO_EFFECT,
    "utility",
    nullptr
};

static const clap_plugin_descriptor_t sMockDescriptor = {
    CLAP_VERSION_INIT,
    kMockPluginId,
    kMockPluginName,
    "MusicTribe Tests",
    "https://example.com",
    "",
    "",
    "1.2.3",
    "A minimal mock CLAP effect fixture with latency, tail, state and GUI",
    sMockFeatures
};

static uint32_t factoryGetPluginCount(const clap_plugin_factory_t*) { return 1; }
static const clap_plugin_descriptor_t* factoryGetPluginDescriptor(const clap_plugin_factory_t*, uint32_t) {
    return &sMockDescriptor;
}

static const clap_plugin_t* factoryCreatePlugin(const clap_plugin_factory_t*, const clap_host_t*, const char* plugin_id) {
    if (std::strcmp(plugin_id, kMockPluginId) != 0) return nullptr;

    auto* instance = new MockEffectInstance();
    instance->plugin.desc = &sMockDescriptor;
    instance->plugin.plugin_data = instance;
    instance->plugin.init = pluginInit;
    instance->plugin.destroy = pluginDestroy;
    instance->plugin.activate = pluginActivate;
    instance->plugin.deactivate = pluginDeactivate;
    instance->plugin.start_processing = pluginStartProcessing;
    instance->plugin.stop_processing = pluginStopProcessing;
    instance->plugin.reset = pluginReset;
    instance->plugin.process = pluginProcess;
    instance->plugin.get_extension = pluginGetExtension;
    instance->plugin.on_main_thread = pluginOnMainThread;

    return &instance->plugin;
}

static const clap_plugin_factory_t sMockFactory = {
    factoryGetPluginCount,
    factoryGetPluginDescriptor,
    factoryCreatePlugin
};

static bool entryInit(const char*) { return true; }
static void entryDeinit() {}
static const void* entryGetFactory(const char* factory_id) {
    if (std::strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
        return &sMockFactory;
    }
    return nullptr;
}

} // namespace

extern "C" CLAP_EXPORT const clap_plugin_entry_t clap_entry = {
    CLAP_VERSION_INIT,
    entryInit,
    entryDeinit,
    entryGetFactory
};
