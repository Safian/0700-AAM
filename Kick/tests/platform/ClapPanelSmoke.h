// SPDX-License-Identifier: AGPL-3.0-or-later
// KICK_SMOKE_CLAP_PANEL=<path of a plug-in .clap that offers com.safian.bitmap-gui/1, e.g. the real SynthyBoy module>:
// loads it through the dynamic path into a real Plugin program, opens the plug-in panel (ClapInstrumentPanel) at 1024x600, checks that the plug-in's own GUI is shown,
// renders audio through the program, measures what the GUI costs (idle and animating), drives the Q-Link knobs and writes a screenshot of the panel
// (KICK_SMOKE_PANEL_PNG, default $KICK_DATA_ROOT/clap-panel.png). Smoke test only; asynchronous (the app has no modal loop): the steps are timer callbacks.
#pragma once
#include <audio_program/source/mt_audio_clap_program.h>
#include <kick_clap/Discovery.h>
#include <kick_clap/PluginManager.h>
#include <kick_ui/program_view/source/kick_ClapPanel.h>
#include <sys/resource.h>
#include <iostream>

namespace kick_smoke {
inline double cpuSeconds() {
    rusage usage{};
    getrusage(RUSAGE_SELF, &usage);
    return static_cast<double>(usage.ru_utime.tv_sec + usage.ru_stime.tv_sec) + static_cast<double>(usage.ru_utime.tv_usec + usage.ru_stime.tv_usec) * 1e-6;
}

class ClapPanelSmoke : public std::enable_shared_from_this<ClapPanelSmoke> {
public:
    explicit ClapPanelSmoke(std::function<void(const juce::String&)> done) : finish(std::move(done)) {}

    // The audio callback the engine would run: renders the program's session every 10 ms
    struct AudioPump : juce::Timer {
        std::shared_ptr<kick::clap::PluginSession> session;
        juce::AudioBuffer<float> block{2, 480};
        void timerCallback() override { session->render(block); }
    };

    void start(const juce::String& path) {
        using namespace kick::clap;
        const auto catalog = inspect(path.toStdString());
        if (catalog.plugins.empty()) return fail("Cannot enumerate " + path);
        auto& api = mt::getAudioInterface();
        auto file = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("smoke-panel/Project.kickPrj");
        if (!file.getParentDirectory().createDirectory() || !api.loadProject(file, true)) return fail("Cannot create the smoke project");
        const auto index = api.createProgram(mt::MTProgramType::Plugin, "SmokePanel");
        program = dynamic_cast<mt::MTClapProgram*>(api.getProgram(index));
        if (!program) return fail("Cannot create the Plugin program");
        session = program->getPluginSession();
        session->prepare(48000, 480); session->service(); session->select(catalog.plugins.front());
        if (!session->host().loaded()) return fail("Plug-in did not load: " + juce::String(session->status()));
        if (session->host().descriptor().path.rfind("internal:", 0) == 0) return fail("Loaded the built-in instead of the binary");
        if (!BitmapGuiClient::supported(session->host())) return fail("The plug-in has no bitmap GUI");
        pump.session = session;
        // baseline: what the whole app costs with the plug-in loaded and no panel open (the panel's cost is the difference)
        baseCpu = cpuSeconds(); baseWall = juce::Time::getMillisecondCounterHiRes() / 1000.0;
        later(3000, [](ClapPanelSmoke& s) { s.openPanel(); });
    }
    void openPanel() {
        baselinePercent = 100.0 * (cpuSeconds() - baseCpu) / (juce::Time::getMillisecondCounterHiRes() / 1000.0 - baseWall);
        std::cout << "baseline (whole app, plug-in loaded, panel closed): process CPU " << baselinePercent << " %\n";
        panel = std::make_unique<kick_ui::ClapInstrumentPanel>(session);
        panel->setBounds(0, 0, 1024, 600);
        later(1500, [](ClapPanelSmoke& s) { s.idleStart(); });
    }

private:
    void later(int ms, std::function<void(ClapPanelSmoke&)> step) {
        auto self = shared_from_this();
        juce::Timer::callAfterDelay(ms, [self, step] { step(*self); });
    }
    void fail(const juce::String& why) { pump.stopTimer(); panel.reset(); finish(why); }
    struct Mark { kick::clap::BitmapGuiComponent::Statistics stats; double cpu = 0, wall = 0; };
    Mark mark() { return {panel->pluginGui()->statistics(), cpuSeconds(), juce::Time::getMillisecondCounterHiRes() / 1000.0}; }

    void idleStart() {
        if (!panel->showsPluginGui()) return fail("The plug-in GUI is not shown in the panel");
        const auto b = panel->pluginGui()->getBounds();
        std::cout << "panel: plug-in GUI " << b.getWidth() << "x" << b.getHeight() << " at " << b.getX() << "," << b.getY() << "\n";
        start0 = mark();
        later(juce::jmax(1000, juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_IDLE_MS", "3000").getIntValue()), [](ClapPanelSmoke& s) { s.idleEnd(); });
    }
    void idleEnd() {
        const auto m = mark();
        std::cout << "idle: " << (m.stats.ticks - start0.stats.ticks) << " ticks, " << (m.stats.renders - start0.stats.renders) << " renders in " << (m.wall - start0.wall)
                  << " s (plug-in tick() + is_dirty(): " << ((m.stats.tickMicroseconds - start0.stats.tickMicroseconds) / static_cast<double>(std::max<uint64_t>(1, m.stats.ticks - start0.stats.ticks))) << " us each), process CPU " << (100.0 * (m.cpu - start0.cpu) / (m.wall - start0.wall)) << " % (baseline " << baselinePercent << " %)\n";
        program->noteOn(60, 110);
        pump.startTimer(10);
        later(1000, [](ClapPanelSmoke& s) { s.animStart(); });
    }
    void animStart() { start0 = mark(); later(3000, [](ClapPanelSmoke& s) { s.animEnd(); }); }
    void animEnd() {
        const auto m = mark();
        const double wall = m.wall - start0.wall;
        const auto renders = m.stats.renders - start0.stats.renders;
        std::cout << "animating (held note, audio running): " << renders << " renders in " << wall << " s = " << (renders / wall) << " fps, "
                  << (renders ? (m.stats.renderMicroseconds - start0.stats.renderMicroseconds) / static_cast<double>(renders) / 1000.0 : 0.0)
                  << " ms per frame (plug-in render + copy), " << ((m.stats.pixelsCopied - start0.stats.pixelsCopied) / std::max<uint64_t>(1, renders))
                  << " pixels copied per frame, process CPU incl. the plug-in's audio " << (100.0 * (m.cpu - start0.cpu) / wall) << " %\n";
        juce::AudioBuffer<float> block(2, 480);
        pump.stopTimer();
        program->noteOn(64, 100);
        session->render(block);
        if (block.getMagnitude(0, 480) <= 0.0f) return fail("The plug-in produced no audio for a held note");
        program->noteOff(60); program->noteOff(64);
        qlink();
    }
    void qlink() {
        const auto& pages = session->host().remotePages();
        if (pages.size() < 10) return fail("The plug-in's remote-controls pages were not read");
        panel->setRemoteFocus(0, true);
        const clap_id first = pages[0].parameters[0];
        const double before = session->host().parameterValue(first);
        if (!panel->adjustKnob(0, 0.05)) return fail("Knob 1 did not drive the focused page");
        const double after = session->host().parameterValue(first);
        if (after == before) return fail("Knob 1 did not change the first parameter of the page");
        std::cout << "q-link: page '" << pages[0].name << "' knob 1 " << before << " -> " << after << ", title \"" << panel->remoteTitle() << "\"\n";
        if (panel->adjustKnob(16, 0.05)) return fail("Knob 17 does not exist");
        // page 5 knobs 9..16 drive the page after the focused one
        panel->setRemoteFocus(1, true);
        const clap_id next = pages[2].parameters[0];
        const double n0 = session->host().parameterValue(next);
        if (!panel->adjustKnob(8, 0.05) || session->host().parameterValue(next) == n0) return fail("Knob 9 did not drive the page after the focused one");
        panel->setRemoteFocus(0, true);
        // the Parameters toggle: generic list and back (the GUI keeps running)
        if (panel->showsGenericList()) return fail("The generic list is visible next to the plug-in GUI");
        panel->toggleGenericView();
        if (panel->showsPluginGui() || !panel->showsGenericList()) return fail("The Parameters toggle did not show the generic list");
        panel->toggleGenericView();
        if (!panel->showsPluginGui() || panel->showsGenericList()) return fail("The toggle did not return to the plug-in GUI");
        later(400, [](ClapPanelSmoke& s) { s.shot(); });
    }
    void shot() {
        auto png = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_PANEL_PNG", {}));
        if (png == juce::File()) png = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("clap-panel.png");
        auto image = panel->createComponentSnapshot(panel->getLocalBounds(), false, 1.0f);
        png.deleteFile();
        juce::PNGImageFormat format;
        if (auto out = png.createOutputStream()) format.writeImageToStream(image, *out); else return fail("Cannot write " + png.getFullPathName());
        std::cout << "panel screenshot: " << png.getFullPathName() << " (" << image.getWidth() << "x" << image.getHeight() << ")\n";
        panel.reset();
        std::cout << "PASS: the plug-in's own GUI is shown in the Kick plug-in panel, audio renders, Q-Link follows the plug-in's pages\n";
        finish({});
    }

    std::function<void(const juce::String&)> finish;
    mt::MTClapProgram* program = nullptr;
    std::shared_ptr<kick::clap::PluginSession> session;
    std::unique_ptr<kick_ui::ClapInstrumentPanel> panel;
    AudioPump pump;
    Mark start0;
    double baseCpu = 0, baseWall = 0, baselinePercent = 0;
};
} // namespace kick_smoke

inline void startKickClapPanelSmoke(const juce::String& path, std::function<void(const juce::String&)> done) {
    auto smoke = std::make_shared<kick_smoke::ClapPanelSmoke>(std::move(done));
    smoke->start(path);
}
