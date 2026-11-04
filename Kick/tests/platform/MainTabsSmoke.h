// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <iostream>

// Main screen tabs (Sequence / Track / Program) against the real engine on the message thread:
// undo / redo of create and delete follows into the engine objects, the time signature change re-maps the sequence,
// the track Monitor setting survives save / reopen. Run with KICK_SMOKE_MAINTABS=1 (see AppSmoke.h).
inline juce::String runKickMainTabsSmoke() {
    auto& api = mt::getAudioInterface();
    auto file = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("maintabs/Project.kickPrj");
    if (!file.getParentDirectory().createDirectory()) return "Cannot create project directory";
    if (!api.loadProject(file, true)) return "Cannot create project";
    const auto fail = [](const juce::String& m) { std::cout << "MAINTABS FAIL: " << m << "\n" << std::flush; return m; };
    const auto say  = [](const juce::String& m) { std::cout << "MAINTABS: " << m << "\n" << std::flush; };

    // ---- sequence create / undo / redo
    const int seq0 = api.getNumSequences();
    api.createSequence("TabSeq");
    if (api.getNumSequences() != seq0 + 1) return fail("createSequence did not add a sequence");
    api.performUndo();
    if (api.getNumSequences() != seq0) return fail("undo of createSequence left the engine sequence (" + juce::String(api.getNumSequences()) + " vs " + juce::String(seq0) + ")");
    if (api.getSelectedSequence() >= api.getNumSequences()) return fail("selected sequence out of range after undo");
    api.performRedo();
    if (api.getNumSequences() != seq0 + 1) return fail("redo of createSequence did not bring the sequence back");
    say("sequence create/undo/redo ok");

    // work on the new sequence from here
    const int seq = api.getNumSequences() - 1;
    api.setSelectedSequence(seq);

    // ---- track create / undo / redo
    const int trk0 = api.getNumTracks(seq);
    api.createTrack(seq, "TabTrack");
    if (api.getNumTracks(seq) != trk0 + 1) return fail("createTrack did not add a track");
    api.performUndo();
    if (api.getNumTracks(seq) != trk0) return fail("undo of createTrack left the engine track");
    api.performRedo();
    if (api.getNumTracks(seq) != trk0 + 1) return fail("redo of createTrack did not bring the track back");
    if (!api.getTrackData(seq, trk0) || api.getTrackData(seq, trk0)->name.get() != "TabTrack") return fail("redone track lost its name");
    api.deleteTrack(seq, trk0);
    if (api.getNumTracks(seq) != trk0) return fail("deleteTrack failed");
    api.performUndo();
    if (api.getNumTracks(seq) != trk0 + 1) return fail("undo of deleteTrack did not bring the track back");
    say("track create/delete/undo/redo ok");

    // ---- program create / undo / redo, delete with a track on it
    const int prog0 = api.getNumPrograms();
    const int pi    = api.createProgram(mt::MTProgramType::Drum, "TabProg");
    if (pi < 0 || api.getNumPrograms() != prog0 + 1) return fail("createProgram failed");
    api.performUndo();
    if (api.getNumPrograms() != prog0 || api.getProgram("TabProg") != nullptr) return fail("undo of createProgram left the engine program");
    api.performRedo();
    if (api.getNumPrograms() != prog0 + 1 || api.getProgram("TabProg") == nullptr) return fail("redo of createProgram did not bring the program back");
    api.setProgramToTrack("TabProg", seq, trk0);
    if (!api.getProgramDataForTrack(seq, trk0) || api.getProgramDataForTrack(seq, trk0)->name.get() != "TabProg") return fail("track did not take the program");
    api.deleteProgram(api.getNumPrograms() - 1);
    if (api.getProgram("TabProg") != nullptr) return fail("deleteProgram failed");
    api.performUndo();
    if (api.getProgram("TabProg") == nullptr) return fail("undo of deleteProgram did not bring the program back");
    if (!api.getProgramDataForTrack(seq, trk0) || api.getProgramDataForTrack(seq, trk0)->name.get() != "TabProg") return fail("track did not get its program back after undo of the delete");
    say("program create/delete/undo/redo ok, track program restored");

    // ---- time signature
    const int len0 = api.getSequenceLength(seq); // 4 bars of 4/4
    int num = 0, den = 0;
    if (!api.getTimeSignature(seq, num, den) || num != 4 || den != 4) return fail("default time signature is not 4/4: " + juce::String(num) + "/" + juce::String(den));
    if (api.setTimeSignature(seq, 0, 4) != -1 || api.setTimeSignature(seq, 4, 6) != -1) return fail("invalid time signature accepted");
    if (api.setTimeSignature(seq, 3, 4) != 0) return fail("setTimeSignature 3/4 failed");
    api.getTimeSignature(seq, num, den);
    const int len34 = api.getSequenceLength(seq);
    if (num != 3 || den != 4) return fail("time signature not applied");
    if (len34 != (len0 * 3840 + 2879) / 2880) return fail("3/4 length " + juce::String(len34) + " does not cover the old " + juce::String(len0) + " bars of 4/4");
    api.performUndo();
    api.getTimeSignature(seq, num, den);
    if (num != 4 || den != 4 || api.getSequenceLength(seq) != len0) return fail("undo of the time signature did not restore 4/4 and the length");
    api.performRedo();
    api.getTimeSignature(seq, num, den);
    if (num != 3 || api.getSequenceLength(seq) != len34) return fail("redo of the time signature failed");
    if (api.setTimeSignature(seq, 7, 8) != 0) return fail("setTimeSignature 7/8 failed");
    say("time signature 4/4 -> 3/4 (" + juce::String(len0) + " -> " + juce::String(len34) + " bars) -> 7/8 (" + juce::String(api.getSequenceLength(seq)) + " bars), undo/redo ok");


    // ---- seek, loop and events follow the time signature (HDR-43): 4/4, 3/4, 6/8, 7/8
    {
        api.setSelectedSequence(seq);
        api.setSelectedTrack(trk0);
        if (api.addOrModifyNote(36, 1000, 96) < 0) return fail("cannot add the test note");
        const auto noteStart = [&]() -> int {
            auto list = api.getNoteList(seq, trk0);
            for (int i = 0; i < list.getNumChildren(); ++i) { mt::MTNoteRO n(list.getChild(i)); if (n.number() == 36) return n.start(); }
            return -1;
        };
        if (noteStart() != 1000) return fail("test note start " + juce::String(noteStart()));
        const struct { int num, den; } cases[] = { { 4, 4 }, { 3, 4 }, { 6, 8 }, { 7, 8 } };
        for (const auto& cc : cases) {
            // the ticks of one bar from the engine's own resolution (96 per quarter note): 4/4 = 384, 3/4 = 6/8 = 288, 7/8 = 336
            struct { int num, den; long long ticksPerBar; } c{ cc.num, cc.den, mt::timesig::ticksPerBar(cc.num, cc.den, mt::MTTimeSignature::defaultPPQN) };
            const auto tag = juce::String(c.num) + "/" + juce::String(c.den);
            if (api.setTimeSignature(seq, c.num, c.den) != 0) return fail("setTimeSignature " + tag);
            if (api.seekToBar(seq, 2) < 0 || api.getSequenceSeekTick(seq) != (int)(2 * c.ticksPerBar)) return fail("seekToBar(2) in " + tag + " is tick " + juce::String(api.getSequenceSeekTick(seq)) + " (follows global tempo: " + juce::String(api.getSequenceData(seq) && api.getSequenceData(seq)->isMasterBPM.get() ? "yes" : "no") + ")" + ", expected " + juce::String((int)(2 * c.ticksPerBar)));
            const int len = api.getSequenceLength(seq);
            if (len < 3) return fail("sequence too short in " + tag);
            if (api.setSequenceLoopPoints(seq, 1, len) < 0) return fail("loop points in " + tag); // returns the loop end
            if (api.getSequenceLoopEnd(seq) > len || api.getSequenceLoopEnd(seq) < 2) return fail("loop end " + juce::String(api.getSequenceLoopEnd(seq)) + " out of range in " + tag);
            if (noteStart() != 1000) return fail("the event moved to tick " + juce::String(noteStart()) + " in " + tag + " (events keep their ticks)");
        }
        say("seek / loop / events ok in 4/4, 3/4, 6/8, 7/8");

        // A sequence that follows the global tempo has the global grid: also a NEW one, and one that switches back to the global tempo.
        api.setTimeSignature(seq, 6, 8); // global
        const long long tpb68 = mt::timesig::ticksPerBar(6, 8, mt::MTTimeSignature::defaultPPQN);
        api.createSequence("GridSeq");
        const int seq2 = api.getNumSequences() - 1;
        int n2 = 0, d2 = 0;
        if (!api.getTimeSignature(seq2, n2, d2) || n2 != 6 || d2 != 8) return fail("a new sequence does not follow the global 6/8: " + juce::String(n2) + "/" + juce::String(d2));
        if (api.seekToBar(seq2, 2) < 0 || api.getSequenceSeekTick(seq2) != (int)(2 * tpb68)) return fail("new sequence: seekToBar(2) is tick " + juce::String(api.getSequenceSeekTick(seq2)));
        api.setSequenceBPMToMaster(seq2, false); // own tempo map from here on
        if (api.setTimeSignature(seq2, 3, 4) != 0) return fail("own time signature");
        if (api.seekToBar(seq2, 2) < 0 || api.getSequenceSeekTick(seq2) != (int)(2 * mt::timesig::ticksPerBar(3, 4, mt::MTTimeSignature::defaultPPQN))) return fail("own 3/4: seekToBar(2) is tick " + juce::String(api.getSequenceSeekTick(seq2)));
        api.setSequenceBPMToMaster(seq2, true);   // back to global: 6/8 again, the grid follows
        api.getTimeSignature(seq2, n2, d2);
        if (n2 != 6 || d2 != 8 || api.seekToBar(seq2, 2) < 0 || api.getSequenceSeekTick(seq2) != (int)(2 * tpb68)) return fail("back on the global tempo: " + juce::String(n2) + "/" + juce::String(d2) + ", bar 2 at tick " + juce::String(api.getSequenceSeekTick(seq2)));
        say("global-following sequences (new, switched back) have the global time signature grid");
        api.setTimeSignature(seq, 4, 4);
    }

    // ---- monitor persists
    auto* td = api.getTrackData(seq, trk0);
    if (!td || td->monitor.get() != 1) return fail("default Monitor is not In");
    td->monitor = 2;
    if (!api.saveProject() || !api.closeProject() || !api.loadProject(file)) return fail("save / reopen failed");
    td = api.getTrackData(seq, trk0);
    if (!td || td->monitor.get() != 2) return fail("Monitor did not survive save / reopen");
    say("track Monitor survives save/reopen");

    if (!api.saveProject() || !api.closeProject()) return fail("cannot save/close");
    std::cout << "PASS: main tabs (undo/redo of sequence/track/program, time signature, monitor)\n";
    return {};
}
