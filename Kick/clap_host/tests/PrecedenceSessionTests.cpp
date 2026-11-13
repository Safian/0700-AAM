// SPDX-License-Identifier: AGPL-3.0-or-later
// A session that holds the built-in plug-in is swapped for the binary of the same id once discovery finds it (state kept), and a project that
// saved the built-in opens with the binary when it is already known. The built-in is simulated with the entry point of the fixture binary.
#include "PluginSession.h"
#include <kick_clap/Discovery.h>
#include <kick_clap/PluginManager.h>
#include <cstdlib>
#include <dlfcn.h>
#include <filesystem>
#include <iostream>
using namespace kick::clap;
static void check(bool value, const char* message) { if (!value) { std::cerr << "FAIL: " << message << '\n'; std::exit(1); } }
int main(int argc, char** argv) {
    check(argc >= 2, "plugin path");
    juce::ScopedJuceInitialiser_GUI juce;
    const auto catalog = inspect(argv[1]);
    check(!catalog.plugins.empty(), "discover instrument");
    const auto binary = catalog.plugins.front();   // org.kick.test.synth, path = the fixture binary
    void* handle = dlopen(argv[1], RTLD_NOW | RTLD_LOCAL);
    check(handle != nullptr, "dlopen the fixture for its entry point");
    const auto* entry = static_cast<const clap_plugin_entry_t*>(dlsym(handle, "clap_entry"));
    check(entry != nullptr, "clap_entry");
    PluginManifest manifest; manifest.id = binary.id; manifest.name = "builtin"; manifest.kind = PluginKind::Instrument;
    auto& manager = PluginManager::getInstance();
    manager.registerStaticPlugin(std::make_shared<StaticPluginLoader>(manifest, entry));
    manager.setDiscovered({});
    Descriptor builtin; builtin.path = "internal:static"; builtin.id = binary.id;

    {   // 1. loaded built-in, discovery finishes later
        juce::ValueTree tree("PROGRAM");
        auto session = PluginSession::create(tree);
        session->prepare(48000, 128); session->service(); session->select(builtin);
        check(session->host().loaded() && session->host().descriptor().path.rfind("internal:", 0) == 0, "the built-in is used while nothing is discovered");
        check(session->host().setParameter(7, .3), "parameter edit on the built-in");
        manager.publishDiscovered(catalog.plugins);
        session->service();
        check(session->host().loaded(), "still loaded after the swap");
        check(std::filesystem::path(session->host().descriptor().path) == std::filesystem::canonical(argv[1]), "the binary replaced the built-in");
        check(std::abs(session->host().parameterValue(7) - .3) < 1e-9, "the state survived the swap");
        check(tree.getChildWithName("CLAP").getProperty("path").toString().toStdString() == session->host().descriptor().path, "the project now names the binary");
    }
    {   // 2. a project saved with the built-in, binary already known: it opens with the binary
        juce::ValueTree tree("PROGRAM");
        auto clap = tree.getOrCreateChildWithName("CLAP", nullptr);
        clap.setProperty("path", "internal:static", nullptr); clap.setProperty("pluginId", juce::String(binary.id), nullptr);
        auto session = PluginSession::create(tree);
        session->prepare(48000, 128); session->service();
        check(session->host().loaded() && std::filesystem::path(session->host().descriptor().path) == std::filesystem::canonical(argv[1]), "a saved built-in opens as the binary");
    }
    {   // 3. nothing discovered (binary removed): the built-in is the fallback
        manager.publishDiscovered({});
        juce::ValueTree tree("PROGRAM");
        auto clap = tree.getOrCreateChildWithName("CLAP", nullptr);
        clap.setProperty("path", "internal:static", nullptr); clap.setProperty("pluginId", juce::String(binary.id), nullptr);
        auto session = PluginSession::create(tree);
        session->prepare(48000, 128); session->service();
        check(session->host().loaded() && session->host().descriptor().path.rfind("internal:", 0) == 0, "without a binary the built-in loads");
    }
    std::cout << "PASS: built-in vs binary precedence in the session (swap with state, saved project, fallback)\n";
}
