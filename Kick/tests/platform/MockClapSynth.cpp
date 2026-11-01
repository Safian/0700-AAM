// SPDX-License-Identifier: MIT
#include "clap/clap.h"
#include <cstdio>
#include <cstring>
#include <atomic>
#include <vector>

namespace {

static constexpr uint32_t kMockParamCount = 8;
static constexpr const char* kMockPluginId = "org.antigravity.mock_clap_synth";

struct MockSynthInstance {
    clap_plugin_t plugin{};
    std::atomic<bool> isNoteOn{false};
    std::atomic<float> macro0Value{0.0f};
    std::atomic<float> macro1Value{0.0f};
};

static uint32_t mockParamsCount(const clap_plugin_t*) {
    return kMockParamCount;
}

static bool mockParamsGetInfo(const clap_plugin_t*, uint32_t index, clap_param_info_t* info) {
    if (index >= kMockParamCount || !info) return false;
    info->id = 1000 + index;
    info->flags = CLAP_PARAM_IS_AUTOMATABLE;
    info->min_value = 0.0;
    info->max_value = 1.0;
    info->default_value = 0.5;
    snprintf(info->name, sizeof(info->name), "Macro %u", index + 1);
    snprintf(info->module, sizeof(info->module), "Main");
    return true;
}

static bool mockParamsGetValue(const clap_plugin_t* plugin, clap_id param_id, double* out_value) {
    auto* self = static_cast<MockSynthInstance*>(plugin->plugin_data);
    if (param_id == 1000) {
        *out_value = self->macro0Value.load();
        return true;
    }
    *out_value = 0.5;
    return true;
}

static bool mockParamsValueToText(const clap_plugin_t*, clap_id, double, char*, uint32_t) { return false; }
static bool mockParamsTextToValue(const clap_plugin_t*, clap_id, const char*, double*) { return false; }
static void mockParamsFlush(const clap_plugin_t*, const clap_input_events_t*, const clap_output_events_t*) {}

static const clap_plugin_params_t sMockParamsExt = {
    mockParamsCount,
    mockParamsGetInfo,
    mockParamsGetValue,
    mockParamsValueToText,
    mockParamsTextToValue,
    mockParamsFlush
};

static bool pluginInit(const clap_plugin_t*) { return true; }
static void pluginDestroy(const clap_plugin_t* plugin) {
    auto* self = static_cast<MockSynthInstance*>(plugin->plugin_data);
    delete self;
}
static bool pluginActivate(const clap_plugin_t*, double, uint32_t, uint32_t) { return true; }
static void pluginDeactivate(const clap_plugin_t*) {}
static bool pluginStartProcessing(const clap_plugin_t*) { return true; }
static void pluginStopProcessing(const clap_plugin_t*) {}
static void pluginReset(const clap_plugin_t*) {}

static clap_process_status pluginProcess(const clap_plugin_t* plugin, const clap_process_t* process) {
    auto* self = static_cast<MockSynthInstance*>(plugin->plugin_data);

    // Drain events
    if (process->in_events) {
        const uint32_t numEvents = process->in_events->size(process->in_events);
        for (uint32_t i = 0; i < numEvents; ++i) {
            const auto* hdr = process->in_events->get(process->in_events, i);
            if (hdr->space_id == CLAP_CORE_EVENT_SPACE_ID) {
                if (hdr->type == CLAP_EVENT_NOTE_ON) {
                    self->isNoteOn.store(true);
                } else if (hdr->type == CLAP_EVENT_NOTE_OFF) {
                    self->isNoteOn.store(false);
                } else if (hdr->type == CLAP_EVENT_PARAM_VALUE) {
                    const auto* pv = reinterpret_cast<const clap_event_param_value_t*>(hdr);
                    if (pv->param_id == 1000) {
                        self->macro0Value.store(static_cast<float>(pv->value));
                    } else if (pv->param_id == 1001) {
                        self->macro1Value.store(static_cast<float>(pv->value));
                    }
                }
            }
        }
    }

    // Render audio: if note is on, output 0.75f, otherwise output 0.05f
    const float val = self->isNoteOn.load() ? 0.75f : 0.05f;
    if (process->audio_outputs_count > 0 && process->audio_outputs) {
        auto& out = process->audio_outputs[0];
        for (uint32_t ch = 0; ch < out.channel_count; ++ch) {
            for (uint32_t f = 0; f < process->frames_count; ++f) {
                out.data32[ch][f] = val;
            }
        }
    }

    return CLAP_PROCESS_CONTINUE;
}

static const void* pluginGetExtension(const clap_plugin_t*, const char* id) {
    if (strcmp(id, CLAP_EXT_PARAMS) == 0) {
        return &sMockParamsExt;
    }
    return nullptr;
}

static void pluginOnMainThread(const clap_plugin_t*) {}

static const clap_plugin_descriptor_t sMockDescriptor = {
    CLAP_VERSION_INIT,
    kMockPluginId,
    "Mock Synth",
    "MusicTribe Tests",
    "https://example.com",
    "",
    "",
    "1.0.0",
    "A minimal mock CLAP synth fixture",
    nullptr
};

static uint32_t factoryGetPluginCount(const clap_plugin_factory_t*) { return 1; }
static const clap_plugin_descriptor_t* factoryGetPluginDescriptor(const clap_plugin_factory_t*, uint32_t) {
    return &sMockDescriptor;
}

static const clap_plugin_t* factoryCreatePlugin(const clap_plugin_factory_t*, const clap_host_t* host, const char* plugin_id) {
    if (strcmp(plugin_id, kMockPluginId) != 0) return nullptr;

    auto* instance = new MockSynthInstance();
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
    if (strcmp(factory_id, CLAP_PLUGIN_FACTORY_ID) == 0) {
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
