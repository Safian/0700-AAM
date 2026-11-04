// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <iostream>

// Sequencer screens against the real engine on the message thread (KICK_SMOKE_SEQUENCERS=1, see AppSmoke.h):
// undo grouping (ScopedUndoTransaction), exact note erase, Timing Correct as one undo step, song undo and missing
// sequences, pad colour defaults, the note list watcher. The seeded project stays loaded, so KICK_SMOKE_SCREENS
// screenshots (HomeSequencerView, HomeSequencerGrid, MenuListEdit, MenuNextSequence, MenuSong, MenuColorEdit)
// show real content.
inline juce::String runKickSequencersSmoke() {
    auto& api  = mt::getAudioInterface();
    auto  file = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("sequencers/Project.kickPrj");
    if (!file.getParentDirectory().createDirectory()) return "Cannot create project directory";
    if (!api.loadProject(file, true)) return "Cannot create project";
    const auto fail = [](const juce::String& m) { std::cout << "SEQUENCERS FAIL: " << m << "\n" << std::flush; return m; };
    const auto say  = [](const juce::String& m) { std::cout << "SEQUENCERS: " << m << "\n" << std::flush; };

    const int prog = api.createProgram(mt::MTProgramType::Drum, "SeqDrum");
    api.createSequence("SeqA");
    const int seq = api.getNumSequences() - 1;
    api.createSequence("SeqB");
    const int seq2 = api.getNumSequences() - 1;
    if (prog < 0 || seq < 0 || seq2 <= seq) return fail("cannot create program / sequences");
    api.setSelectedSequence(seq);
    api.createTrack(seq, "SeqTrack");
    const int trk = api.getNumTracks(seq) - 1;
    if (trk < 0) return fail("cannot create track");
    api.setProgramToTrack("SeqDrum", seq, trk);
    api.setSelectedTrack(trk);
    auto& um  = api.getUndoManager();
    const auto sel = api.getSelected();

    // Raw UndoManager undo: the property level undo that these checks are about. (MTAudioInterface::performUndo also runs
    // the engine reconcile, which in this scripted setup drops the newest track created by createTrack: an engine
    // observation, reported separately, not touched here.)
    const auto undo = [&]() { um.undo(); };
    const auto notes = [&]() {
        std::vector<mt::MTNote> n;
        api.getNotesInRange(sel, n, mt::NotesInRange(0, 127));
        return n;
    };

    // ---- undo grouping: 8 notes, then one edit pass over all of them = ONE undo step
    {
        mt::ScopedUndoTransaction scope(um, "add");
        for (int i = 0; i < 8; ++i) api.addOrModifyNote(0, i * 96, 96, 100, 100, 1);
    }
    if (notes().size() != 8) return fail("expected 8 notes, got " + juce::String((int)notes().size()));
    {
        mt::ScopedUndoTransaction scope(um, "edit");
        for (auto& n : notes()) api.editNoteVelocity(sel, n.id.get(), 50);
    }
    for (auto& n : notes()) if ((int)n.velocity.get() != 50) return fail("velocity edit did not apply");
    undo();
    for (auto& n : notes()) if ((int)n.velocity.get() != 100) return fail("one undo did not revert all 8 velocity edits (" + juce::String((int)n.velocity.get()) + ")");
    if (notes().size() != 8) return fail("after undo expected 8 notes, got " + juce::String((int)notes().size()));
    say("edit pass over 8 notes is one undo step");

    // ---- exact erase leaves a longer earlier note alone
    api.addOrModifyNote(5, 0, 384, 100, 100, 1);
    api.addOrModifyNote(5, 96, 96, 100, 100, 1);
    api.eraseNote(5, 96, 96);
    int pad5 = 0, longKept = 0;
    for (auto& n : notes()) if ((int)n.number.get() == 5) { ++pad5; if (n.length.get() == 384) ++longKept; }
    if (pad5 != 1 || longKept != 1) return fail("exact erase removed the wrong note (pad5=" + juce::String(pad5) + ", long=" + juce::String(longKept) + ")");
    api.eraseNote(5, 0, 384);
    say("exact erase ok");

    // ---- Timing Correct is one undo step
    for (int i = 0; i < 4; ++i) api.addOrModifyNote(9, i * 96 + 7 + i, 90, 100, 100, 1);
    std::vector<int> before;
    for (auto& n : notes()) if ((int)n.number.get() == 9) before.push_back(n.start.get());
    mt::track::timingCorrectInfo info; info.gridTicks = 96;
    api.timingCorrect(sel, info);
    int moved = 0;
    for (auto& n : notes()) if ((int)n.number.get() == 9 && n.start.get() % 96 == 0) ++moved;
    if (moved != 4) { juce::String st; for (auto& n : notes()) if ((int)n.number.get() == 9) st += juce::String(n.start.get()) + " "; return fail("timing correct did not snap all notes (" + juce::String(moved) + "): starts " + st); }
    undo();
    std::vector<int> after;
    for (auto& n : notes()) if ((int)n.number.get() == 9) after.push_back(n.start.get());
    if (after != before) return fail("one undo did not restore all timing-corrected notes");
    say("timing correct = one undo step");

    // ---- note list watcher (asynchronous: the result is printed after the message loop has run; this is not
    //      a modal-loop build, so it cannot be pumped here)
    {
        auto watcher = std::make_shared<kick_ui::NoteListWatcher>();
        auto hits    = std::make_shared<int>(0);
        watcher->onChanged = [hits]() { ++*hits; };
        watcher->setEnabled(true);
        api.addOrModifyNote(11, 0, 96, 100, 100, 1);
        juce::Timer::callAfterDelay(300, [watcher, hits]() {
            const int h1 = *hits;
            std::cout << (h1 >= 1 ? "SEQUENCERS: note list watcher reported the added note\n" : "SEQUENCERS FAIL: note list watcher silent on add\n") << std::flush;
            mt::getAudioInterface().addOrModifyNote(12, 0, 96, 100, 100, 1); // a second change (an undo here would revert the seeded notes)
            juce::Timer::callAfterDelay(300, [watcher, hits, h1]() {
                std::cout << (*hits > h1 ? "SEQUENCERS: note list watcher reported a second change\n" : "SEQUENCERS FAIL: note list watcher silent on a second change\n") << std::flush;
                watcher->setEnabled(false);
            });
        });
    }

    // ---- song: undo, missing sequence, rename, delete
    api.createSong("SongA");
    mt::MTSongStepInfo s0; s0.seqNo = seq;  api.addSongStep("SongA", s0, 0);
    mt::MTSongStepInfo s1; s1.seqNo = seq2; api.addSongStep("SongA", s1, 1);
    if (api.getSongInfo("SongA").steps.size() != 2) return fail("song steps not added");
    undo();
    if (api.getSongInfo("SongA").steps.size() != 1) return fail("undo of a song step did not remove it");
    um.redo();
    api.deleteAllSongSteps("SongA");
    undo();
    if (api.getSongInfo("SongA").steps.size() != 2) return fail("undo of Clear did not restore the song");
    api.renameSong("SongA", "SongB");
    if (!api.isSongExists("SongB") || api.isSongExists("SongA")) return fail("rename failed");
    undo();
    if (!api.isSongExists("SongA")) return fail("undo of rename failed");
    api.deleteSequence(seq2);
    auto info2 = api.getSongInfo("SongA");
    if (info2.steps.size() != 2 || info2.steps[1].seqNo != -1) return fail("step of a deleted sequence was not kept as missing");
    api.playSong("SongA", 0);
    if (api.isSequencePlaying()) return fail("a song with a missing sequence started");
    say("song undo / missing sequence / rename ok");

    // ---- pad colours: default tables cover all banks; reset; one undo step for all pads
    auto* pd = api.getProgramData(prog);
    if (!pd) return fail("no program data");
    const int def0 = mt::defaultDrumColours[0], def20 = mt::defaultDrumColours[20];
    if (def20 != mt::defaultDrumColours[4]) return fail("default drum colours do not repeat per bank");
    api.setProgramPadColour(prog, 3, 20);
    if (pd->padColour[20].get() != 3) return fail("pad colour not set");
    api.resetProgramPadColours(prog);
    if (pd->padColour[20].get() != def20 || pd->padColour[16].get() != def0) return fail("Make Default did not restore the program type defaults");
    {
        mt::ScopedUndoTransaction scope(um, "all");
        api.setProgramPadColour(prog, 7, -1);
    }
    undo();
    if (pd->padColour[20].get() != def20 || pd->padColour[127].get() != mt::defaultDrumColours[127]) return fail("one undo did not revert colouring all pads");
    say("pad colour defaults ok");

    api.setSelectedSequence(seq);
    api.setSelectedTrack(trk);
    for (int i = 0; i < 8; ++i) api.addOrModifyNote(0, i * 96, 96, 40 + i * 12, 100, 1); // seeded content for the screenshots
    for (int i : {1, 3, 6}) api.addOrModifyNote(2, i * 96, 96, 110, 60, 2);
    std::cout << "PASS: sequencer screens engine checks\n";
    return {};
}
