// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <iostream>
#include <set>

// Mixer / mute engine behaviour against the real engine on the message thread (KICK_SMOKE_MIXERS=1, see AppSmoke.h):
// Clear Solos really clears the track solos (MU-H3), mute groups propagate through the engine API (MU-H7),
// read-only pad getters do not create samples (PM-H10).
inline juce::String runKickMixersSmoke() {
    auto& api = mt::getAudioInterface();
    auto file = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("mixers/Project.kickPrj");
    if (!file.getParentDirectory().createDirectory()) return "Cannot create project directory";
    if (!api.loadProject(file)) return "Cannot create project";
    const auto fail = [](const juce::String& m) { std::cout << "MIXERS FAIL: " << m << "\n" << std::flush; return m; };
    const auto say  = [](const juce::String& m) { std::cout << "MIXERS: " << m << "\n" << std::flush; };

    api.createSequence("MixSeq");
    const int seq = api.getNumSequences() - 1;
    api.setSelectedSequence(seq);
    for (int i = 0; i < 3; ++i) api.createTrack(seq, "T" + juce::String(i));

    // Distinct tracks only: a direct createTrack() call lists every track twice on this tree (same value tree, same id;
    // the MAINTABS smoke trips over the same thing), so work on the first entry of each id.
    std::vector<int> t;
    {
        std::set<juce::String> ids;
        for (int i = 0; i < api.getNumTracks(seq); ++i)
            if (auto* d = api.getTrackData(seq, i); d && ids.insert(d->id.get()).second) t.push_back(i);
    }
    if (t.size() < 3) return fail("tracks not created");
    const auto soloCount = [&] { int n = 0; for (int i : t) n += api.getTrackSolo(seq, i) ? 1 : 0; return n; };

    // ---- Clear Solos (MU-H3): the solo flags of the tracks are gone, a later solo is a single solo.
    api.setTrackSolo(seq, t[0], true);
    if (!api.getTrackSolo(seq, t[0]) || soloCount() != 1) return fail("solo not set");
    if (api.resetTrackSolos() != 0) return fail("resetTrackSolos reported nothing to clear");
    if (soloCount() != 0 || api.getNumSoloTracks(seq) != 0) return fail("Clear Solos left a solo in the bit set");
    if (api.getTrackData(seq, t[0])->solo.get()) return fail("Clear Solos left the solo flag on the track (the old solo came back with the next one)");
    api.setTrackSolo(seq, t[1], true);
    if (soloCount() != 1 || api.getTrackSolo(seq, t[0]) || !api.getTrackSolo(seq, t[1])) return fail("after Clear Solos a new solo is not single");
    api.resetTrackSolos();
    if (soloCount() != 0) return fail("second Clear Solos failed");
    say("Clear Solos ok");

    // ---- mute group through the engine API (the Mixer now uses the same call): the group follows one mute.
    api.seqAddTrackToGroup(seq, t[0], 3);
    api.seqAddTrackToGroup(seq, t[1], 3);
    api.setTrackMute(seq, t[0], true);
    if (!api.getTrackData(seq, t[0])->mute.get() || !api.getTrackData(seq, t[1])->mute.get()) return fail("mute did not propagate to the mute group");
    if (api.getTrackData(seq, t[2])->mute.get()) return fail("mute reached a track outside the group");
    api.setTrackMute(seq, t[0], false);
    if (api.getTrackData(seq, t[1])->mute.get()) return fail("unmute did not propagate to the mute group");
    api.seqRemoveTrackFromGroup(seq, t[1]);
    if (api.getTrackData(seq, t[1])->muteGroup.get() > 0) return fail("seqRemoveTrackFromGroup left the group");
    say("mute group ok");

    // ---- read-only pad getters do not create samples (PM-H10).
    const int pi = api.createProgram(mt::MTProgramType::Drum, "MixDrum");
    auto*     pp = api.getProgram(pi);
    if (!pp) return fail("no drum program");
    std::vector<mt::MTISample*> before, after;
    pp->getSamples(before);
    (void)pp->getNoteGain(5); (void)pp->getNotePan(5); (void)pp->isNoteMute(5); (void)pp->getNoteMuteGroup(5);
    pp->getSamples(after);
    if (before.size() != after.size()) return fail("a pad getter created a sample (" + juce::String((int)before.size()) + " -> " + juce::String((int)after.size()) + ")");
    pp->setNoteGain(7, 0.5);
    pp->getSamples(after);
    if (after.size() != before.size() + 1 || std::abs(pp->getNoteGain(7) - 0.5) > 1e-6) return fail("setNoteGain did not create / store the pad");
    pp->setNoteMute(7, true);
    if (!pp->isNoteMute(7)) return fail("pad mute not stored");
    if (pp->toggleNoteMute(7) != false || pp->isNoteMute(7)) return fail("toggleNoteMute returned the wrong state");
    say("pad getters ok");

    std::cout << "PASS: mixers smoke\n" << std::flush;
    return {};
}
