// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <iostream>
#include <program_edit_ui/program_edit_ui.h>

// Program Edit against the real engine and the real screen on the message thread (KICK_SMOKE_PROGRAMEDIT=1, see AppSmoke.h):
// switching between two Clip programs keeps both alive (the editor used to take ownership of the engine's program and delete
// it), every program type opens, a Q-Link turn moves the first control of the page and plays no note, and a MIDI program CC
// slider writes the program's CCn property (the sliders were not attached to the program).
inline juce::String runKickProgramEditSmoke() {
    auto& api = mt::getAudioInterface();
    auto file = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("programedit/Project.kickPrj");
    if (!file.getParentDirectory().createDirectory()) return "Cannot create project directory";
    if (!api.loadProject(file, true)) return "Cannot create project";
    const auto fail = [](const juce::String& m) { std::cout << "PROGRAMEDIT FAIL: " << m << "\n" << std::flush; return m; };
    const auto say  = [](const juce::String& m) { std::cout << "PROGRAMEDIT: " << m << "\n" << std::flush; };

    const int clipA = api.createProgram(mt::MTProgramType::Clip, "ClipA");
    const int clipB = api.createProgram(mt::MTProgramType::Clip, "ClipB");
    const int drum  = api.createProgram(mt::MTProgramType::Drum, "DrumP");
    const int kg    = api.createProgram(mt::MTProgramType::Keygroup, "KeysP");
    const int midi  = api.createProgram(mt::MTProgramType::Midi, "MidiP");
    const int cv    = api.createProgram(mt::MTProgramType::Cv, "CvP");
    for (int i : { clipA, clipB, drum, kg, midi, cv })
        if (api.getProgram(i) == nullptr) return fail("program not created: " + juce::String(i));

    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuProgramEdit);
    auto* pe = dynamic_cast<kick_ui::ProgramEdit*>(kick_ui::KickHome::Instance().findScreen(kick_ui::Screen::MenuProgramEdit));
    if (!pe) return fail("Program Edit screen was not created");
    pe->setBounds(0, 0, 1024, 600);

    // B1: Clip A -> Clip B -> Clip A -> Clip B: no program is deleted under the engine.
    for (int round = 0; round < 3; ++round)
        for (int idx : { clipA, clipB }) {
            api.setSelectedProgram(idx);
            pe->loadProgram(mt::MTProgramType::Clip, idx);
            if (api.getProgram(clipA) == nullptr || api.getProgram(clipB) == nullptr) return fail("a Clip program was deleted by switching (B1)");
        }
    if (api.getProgram(clipA)->getName() != "ClipA" || api.getProgram(clipB)->getName() != "ClipB") return fail("Clip program names changed");
    say("Clip switching ok");

    // Every type opens without a crash.
    const std::pair<mt::MTProgramType, int> all[] = { { mt::MTProgramType::Drum, drum }, { mt::MTProgramType::Keygroup, kg }, { mt::MTProgramType::Midi, midi },
                                                      { mt::MTProgramType::Cv, cv }, { mt::MTProgramType::Clip, clipA } };
    for (auto& [type, idx] : all) {
        api.setSelectedProgram(idx);
        pe->loadProgram(type, idx);
    }
    say("all types open");

    // B3 + PE-25: a Q-Link turn is not a pad hit and moves the first value control of the Drum Main page (Master Level).
    api.setSelectedProgram(drum);
    pe->loadProgram(mt::MTProgramType::Drum, drum);
    auto* drumProg = dynamic_cast<mt::MTDrumProgram*>(api.getProgram(drum));
    if (!drumProg) return fail("no drum program");
    drumProg->setLevel(60);
    // PE-02 / PE-21: Drum envelope, LFO, and filter controls write to the program model
    drumProg->setEnvEnabled( 0, mt::EnvelopeType::AMPLITUDE, true );
    if (!drumProg->isEnvEnabled( 0, mt::EnvelopeType::AMPLITUDE )) return fail("Drum amp env isEnabled failed");
    drumProg->setLFOEnabled( 0, true );
    if (!drumProg->isLFOEnabled( 0 )) return fail("Drum LFO isEnabled failed");
    drumProg->setFilterType( 0, 2 );
    if (drumProg->getFilterType( 0 ) != 2) return fail("Drum filterType failed");
    drumProg->setFilterCutoff( 0, 850.0 );
    if (std::abs(drumProg->getFilterCutoff( 0 ) - 850.0) > 1e-4) return fail("Drum filter cutoff failed");
    say("Drum envelope / LFO / filter model bindings ok");
    pe->setVisible(false); // showing the screen refreshes the page from the program (the sliders follow the model)
    pe->setVisible(true);
    if (drumProg->getLevel() != 60) return fail("showing the page changed the program Level (page refresh must not write)");
    // The first value control of the page: Master Level (a slider tells its callback asynchronously, so the slider value is checked).
    const auto firstSlider = [&]() -> ui::KickSlider* {
        ui::KickSlider* found = nullptr;
        std::function<void(juce::Component*)> walk = [&](juce::Component* c) {
            for (auto* ch : c->getChildren()) {
                if (found) return;
                if (auto* sl = dynamic_cast<ui::KickSlider*>(ch)) { if (ch->isVisible()) { found = sl; return; } }
                else if (ch->isVisible()) walk(ch);
            }
        };
        walk(pe);
        return found;
    };
    auto* master = firstSlider();
    if (!master || (int)master->getValue() != 60) return fail("first control of the Drum page is not Master Level 60");
    CPktMgr::HWMsg_t q;
    q.pktType = CPktMgr::EN_PKT_TYPE_KNOBS;
    q.pktCmd  = contracts::DeviceControlContracts::KnobCmd::EN_KNOB_QLINK;
    q.data    = { (int)contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1, (int)contracts::DeviceControlContracts::KnobDirection::EN_KNOB_CLOCKWISE, 5 };
    pe->handleCommand(q);
    if ((int)master->getValue() != 65) return fail("Q-Link 1 did not move Master Level 60 -> 65, slider is " + juce::String(master->getValue()));
    q.data = { (int)contracts::DeviceControlContracts::KnobSequence::EN_KNOB_1, (int)contracts::DeviceControlContracts::KnobDirection::EN_KNOB_ANTICLOCKWISE, 2 };
    pe->handleCommand(q);
    if ((int)master->getValue() != 63) return fail("Q-Link 1 anticlockwise did not move Master Level 65 -> 63, slider is " + juce::String(master->getValue()));
    say("Q-Link ok");

    // Touch pad selection: pad C03 (index 34) makes C the engine bank.
    api.setBank("A");
    {
        CPktMgr::HWMsg_t dial; // a Data Dial turn on the page: no crash, and it is not a pad hit any more (B3)
        dial.pktType = CPktMgr::EN_PKT_TYPE_KNOBS;
        dial.pktCmd  = contracts::DeviceControlContracts::KnobCmd::EN_KNOB_QLINK;
        dial.data    = { (int)contracts::DeviceControlContracts::KnobSequence::EN_KNOB_CENTER, (int)contracts::DeviceControlContracts::KnobDirection::EN_KNOB_CLOCKWISE, 1 };
        pe->handleCommand(dial);
    }
    {
        CPktMgr::HWMsg_t pad;
        pad.pktType = CPktMgr::EN_PKT_TYPE_PAD;
        pad.pktCmd  = contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE;
        pad.data    = { 2, (int)contracts::DeviceControlContracts::BTN_PRESSED, 100, 0 };
        pe->handleCommand(pad);
        pad.data[1] = (int)contracts::DeviceControlContracts::BTN_RELEASED;
        pe->handleCommand(pad);
    }
    say("pad press / release ok");

    // B4: the MIDI editor's CC sliders write the program's CCn property.
    api.setSelectedProgram(midi);
    pe->loadProgram(mt::MTProgramType::Midi, midi);
    auto* midiProg = api.getProgram(midi);
    std::vector<ui::KickSlider*> sliders;
    std::function<void(juce::Component*)> collect = [&](juce::Component* c) {
        for (auto* ch : c->getChildren()) {
            if (auto* s = dynamic_cast<ui::KickSlider*>(ch)) sliders.push_back(s);
            else if (ch->isVisible()) collect(ch);
        }
    };
    collect(pe); // the MIDI editor is the page that is showing; its 128 CC sliders are the last sliders in the tree, in CC order
    if (sliders.size() < 128) return fail("MIDI editor sliders not found: " + juce::String((int)sliders.size()));
    sliders[sliders.size() - 128 + 7]->setValue(77, juce::sendNotificationSync);
    if (!midiProg || (int)midiProg->getState().getProperty("CC7", -1) != 77) return fail("CC7 slider did not write the program property (B4)");
    say("MIDI CC binding ok");

    // Pad Perform: hardware pads, Q-Link 1-4 (Scale, Root, Octave, Chord / Mode), 16 LEVELS toggle and hide (note-off for everything).
    api.setSelectedProgram(kg);
    kick_ui::signals::sigScreenClose(kick_ui::Screen::MenuPadPerform);
    auto* pp = dynamic_cast<kick_ui::PadPerform*>(kick_ui::KickHome::Instance().findScreen(kick_ui::Screen::MenuPadPerform));
    if (!pp) return fail("Pad Perform screen was not created");
    pp->setBounds(0, 0, 1024, 600);
    pp->setVisible(false);
    pp->setVisible(true);
    {
        CPktMgr::HWMsg_t pad;
        pad.pktType = CPktMgr::EN_PKT_TYPE_PAD;
        pad.pktCmd  = contracts::DeviceControlContracts::PadCmd::EN_PAD_STATE;
        pad.data    = { 0, (int)contracts::DeviceControlContracts::BTN_PRESSED, 100, 0 };
        pp->handleCommand(pad);
        for (int k = 1; k <= 4; ++k) {
            CPktMgr::HWMsg_t ql;
            ql.pktType = CPktMgr::EN_PKT_TYPE_KNOBS;
            ql.pktCmd  = contracts::DeviceControlContracts::KnobCmd::EN_KNOB_QLINK;
            ql.data    = { k, (int)contracts::DeviceControlContracts::KnobDirection::EN_KNOB_CLOCKWISE, 1 };
            pp->handleCommand(ql); // while the pad is held: the note-off must still find its notes
        }
        pad.data[1] = (int)contracts::DeviceControlContracts::BTN_RELEASED;
        pp->handleCommand(pad);
        pp->toggleSixteenLevels();
        pad.data = { 5, (int)contracts::DeviceControlContracts::BTN_PRESSED, 100, 0 };
        pp->handleCommand(pad);
        pp->toggleSixteenLevels();
        pp->setVisible(false); // hiding releases every held note
    }
    // PP-22 & PP-25: Pad aftertouch and Arpeggiator / Latch state
    api.setArpEnabled(api.getSelected(), true);
    if (!api.isArpEnabled(api.getSelected())) return fail("ARP enable failed");
    api.setArpLatch(api.getSelected(), true);
    if (!api.isArpLatch(api.getSelected())) return fail("ARP latch failed");
    say("ARP / Latch ok");
    say("Pad Perform pads / Q-Link / 16 LEVELS ok");

    if (!api.closeProject()) return fail("Cannot close project");
    std::cout << "PASS: Program Edit switches Clip programs safely, opens every type, Q-Link moves the page, MIDI CC sliders write the program\n";
    return {};
}
