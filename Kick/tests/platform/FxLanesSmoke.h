// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <iostream>

// FX automation lanes (Touch FX gesture, insert parameters) and the insert chain against the real engine on the message thread:
// lane points are stored per track, survive save / reopen, clear with undo; the automated playback calls on the insert interface
// (gesture, one parameter, restore from the stored tree) neither crash nor change the stored state. Run with KICK_SMOKE_FXLANES=1.
inline juce::String runKickFxLanesSmoke() {
    auto& api = mt::getAudioInterface();
    auto file = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("fxlanes/Project.kickPrj");
    if (!file.getParentDirectory().createDirectory()) return "Cannot create project directory";
    if (!api.loadProject(file, true)) return "Cannot create project";
    const auto fail = [](const juce::String& m) { std::cout << "FXLANES FAIL: " << m << "\n" << std::flush; return m; };
    const auto say  = [](const juce::String& m) { std::cout << "FXLANES: " << m << "\n" << std::flush; };
    namespace au = mt::automation;

    api.createSequence("FxSeq");
    const int seq = api.getNumSequences() - 1;
    api.setSelectedSequence(seq);
    api.createTrack(seq, "FxTrack");
    const int trk = api.getNumTracks(seq) - 1;
    const int pi  = api.createProgram(mt::MTProgramType::Drum, "FxProg");
    if (pi < 0) return fail("createProgram failed");
    api.setProgramToTrack("FxProg", seq, trk);
    mt::MTSelected sel = api.getSelected();
    sel.sequence = seq;
    sel.track    = trk;

    // ---- lanes: points, count per lane
    const auto count = [&](int param) {
        int n = 0;
        for (const auto& e : api.getAutomationEvents(sel, true))
            if (e.param == param) ++n;
        return n;
    };
    for (int p : { (int)au::TouchX, (int)au::TouchY, (int)au::TouchGate, (int)au::InsertParam0, (int)au::InsertParam3 }) {
        if (api.setAutomationEvent(sel, p, -1, 0, 100) != 0 || api.setAutomationEvent(sel, p, -1, 96, 900) != 0) return fail("setAutomationEvent refused FX lane " + juce::String(p));
        if (count(p) != 2) return fail("FX lane " + juce::String(p) + " does not hold its 2 points");
    }
    if (api.setAutomationEvent(sel, 99, -1, 0, 1) == 0) return fail("an unknown lane was accepted");
    if (api.setAutomationEvent(sel, au::TouchX, -1, 192, 5000) != 0) return fail("out of range value refused");
    bool clamped = false;
    for (const auto& e : api.getAutomationEvents(sel, true))
        if (e.param == au::TouchX && e.tick == 192) clamped = e.value == au::kNormScale;
    if (!clamped) return fail("an out of range FX lane value was not clamped to 1000");
    for (const auto& e : api.getAutomationEvents(sel, false))
        if (au::isFxLane(e.param)) return fail("List Edit's event list contains an FX lane");
    say("FX lanes store, clamp and stay out of the List Edit list");

    // ---- recording is a no-op unless the mode is Write and the track plays
    api.setAutomationMode(au::Write);
    const int before = count(au::TouchX);
    api.recordAutomationValue(sel, au::TouchX, 300);
    if (count(au::TouchX) != before) return fail("recording while the track is stopped changed the lane");
    api.setAutomationMode(au::Off);

    // ---- persistence + clear / undo
    if (!api.saveProject() || !api.closeProject() || !api.loadProject(file)) return fail("save / reopen failed");
    for (int p : { (int)au::TouchX, (int)au::TouchGate, (int)au::InsertParam0 })
        if (count(p) < 2) return fail("FX lane " + juce::String(p) + " did not survive save / reopen");
    api.clearAutomation(sel);
    for (int p = au::kFirstFxLane; p < au::Count; ++p)
        if (count(p) != 0) return fail("clearAutomation left points in lane " + juce::String(p));
    api.performUndo();
    if (count(au::TouchX) < 2 || count(au::InsertParam3) != 2) return fail("undo of the clear did not bring the FX lanes back");
    say("FX lanes survive save / reopen, clear and undo");

    // ---- automated playback calls on the program's insert chain
    auto* ins = api.getInsertsInterface(api.getProgramIndexByName("FxProg"));
    if (!ins || !ins->isAvailable()) return fail("the drum program has no insert interface");
    if (ins->addInsert("Reverb", 0) != 0) return fail("addInsert failed");
    const float stored = ins->getParameterValue(0, 0);
    ins->setTouchFxGesture(0.25f, 0.75f, true);
    ins->setTouchFxGesture(7.f, -3.f, false); // clamped, never throws
    ins->setParameterAutomated(0, 0, 0.9f);
    ins->setParameterAutomated(0, 999, 0.9f); // no such parameter: ignored
    ins->setParameterAutomated(3, 0, 0.9f);   // no such slot: ignored
    if (std::abs(ins->getParameterValue(0, 0) - 0.9f) > 1e-4f) return fail("setParameterAutomated did not reach the effect");
    ins->restoreInsertFromState(0);
    if (std::abs(ins->getParameterValue(0, 0) - stored) > 1e-3f) return fail("restoreInsertFromState did not put the stored value back (" + juce::String(ins->getParameterValue(0, 0)) + " vs " + juce::String(stored) + ")");
    ins->closeTouchFx();
    say("automated gesture / parameter playback calls ok, the stored value comes back");

    // ---- INS-23: the chain through the value tree: names are registration keys (NC-17), values and bypass survive save / reopen
    if (ins->addInsert("NCSeventeen", 1) != 0 || ins->addInsert("Console7Channel", 2) != 0) return fail("addInsert of the second / third effect failed");
    ins->setParameterValue(2, 0, 0.65f);
    ins->setInsertBypass(1, true);
    if (!api.saveProject() || !api.closeProject() || !api.loadProject(file)) return fail("save / reopen failed (inserts)");
    ins = api.getInsertsInterface(api.getProgramIndexByName("FxProg"));
    if (!ins) return fail("no insert interface after reopen");
    if (ins->getInsertName(0) != "Reverb" || ins->getInsertName(1) != "NCSeventeen" || ins->getInsertName(2) != "Console7Channel")
        return fail("insert names changed: " + ins->getInsertName(0) + ", " + ins->getInsertName(1) + ", " + ins->getInsertName(2));
    if (ins->getInsertName(3).isNotEmpty()) return fail("an empty slot got a name: " + ins->getInsertName(3));
    if (!ins->isInsertBypassed(1) || ins->isInsertBypassed(0)) return fail("insert bypass did not survive save / reopen");
    if (std::abs(ins->getParameterValue(2, 0) - 0.65f) > 1e-3f) return fail("insert parameter did not survive save / reopen");
    say("insert chain (names incl. NC-17, bypass, parameter) survives save / reopen");

    // ---- bus inserts: pre-fader by default, post-fader selectable and persistent; a program has no such choice
    if (ins->supportsPostFader()) return fail("a program chain claims a pre/post-fader choice");
    const auto returns = api.getBusManager().getReturnBusNames();
    if (returns.empty()) return fail("no return bus");
    auto* bus = api.getBusManager().getBus(juce::String(returns.front()));
    auto* busIns = bus ? bus->getInsertsInterface() : nullptr;
    if (!busIns || !busIns->supportsPostFader()) return fail("a bus does not offer the pre/post-fader choice");
    if (busIns->isPostFader()) return fail("bus inserts are not pre-fader by default");
    busIns->setPostFader(true);
    if (!api.saveProject() || !api.closeProject() || !api.loadProject(file)) return fail("save / reopen failed (bus)");
    bus = api.getBusManager().getBus(juce::String(returns.front()));
    busIns = bus ? bus->getInsertsInterface() : nullptr;
    if (!busIns || !busIns->isPostFader()) return fail("post-fader did not survive save / reopen");
    busIns->setPostFader(false);
    say("bus inserts: pre-fader default, post-fader persists");

    if (!api.saveProject() || !api.closeProject()) return fail("cannot save/close");
    std::cout << "PASS: FX lanes (lanes, persistence, clear/undo, automated playback calls)\n";
    return {};
}
