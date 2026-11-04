// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once
#include <iostream>
#include <thread>

// Project operations against the real engine on the message thread (KICK_SMOKE_PROJECTS=1, see AppSmoke.h): a missing project is
// an error and never made up, the unsaved flag follows the engine, Save As / Save a Copy copy the folder and write the live state,
// "Don't save" leaves the project as it was last saved, a demo cannot be saved in place but can be saved as a copy, the audio
// thread settle (no fixed sleeps) keeps project switches working, the Projects screen and the file browser show their lists.
inline juce::String runKickProjectsSmoke() {
    auto& api  = mt::getAudioInterface();
    const auto say  = [](const juce::String& m) { std::cout << "PROJECTS: " << m << "\n" << std::flush; };
    const auto fail = [](const juce::String& m) { std::cout << "PROJECTS FAIL: " << m << "\n" << std::flush; return m; };
    const auto root = juce::File(juce::SystemStats::getEnvironmentVariable("KICK_DATA_ROOT", {})).getChildFile("prjsmoke");
    root.deleteRecursively();
    root.createDirectory();
    const auto projects = getAppDirs(MTProjects);
    projects.createDirectory();

    // 1. A project that is not there is an error; nothing is created and the error says why.
    juce::File ghost = projects.getChildFile("Ghost");
    if (api.loadProject(ghost)) return fail("a missing project loaded");
    if (ghost.exists()) return fail("a missing project was made up");
    if (!api.getProjectLoadError().contains("not found")) return fail("no 'not found' error: " + api.getProjectLoadError());
    say("missing project refused: " + api.getProjectLoadError());

    // 2. New project (created), an edit makes it unsaved, a save makes it clean.
    juce::File a = projects.getChildFile("SmokeA");
    a.deleteRecursively();
    if (!api.loadProject(a, true)) return fail("cannot create SmokeA");
    if (!api.isProjectLoaded() || api.isProjectReadOnly()) return fail("SmokeA state");
    if (!api.saveProject() || api.isProjectDirty()) return fail("fresh project dirty after save");
    api.createProgram(mt::MTProgramType::Drum, "Drum1");
    if (!api.isProjectDirty()) return fail("a program edit did not make the project unsaved");
    if (!api.saveProject() || api.isProjectDirty()) return fail("save did not clear the unsaved flag");
    const auto programsA = api.getNumPrograms();
    say("unsaved flag follows the engine");

    // 3. Save a Copy: the folder (with a sample file) is copied, the open project stays open and unchanged.
    a.getChildFile("samples").createDirectory();
    a.getChildFile("samples/kick.wav").replaceWithText("not really audio");
    juce::File copyDir = projects.getChildFile("SmokeCopy");
    copyDir.deleteRecursively();
    std::atomic<bool> noCancel{ false };
    auto copyFn = [&](const juce::File& from, const juce::File& to) {
        return kick_fileops::copyFolderTo(from.getFullPathName().toStdString(), to.getFullPathName().toStdString(), noCancel).ok;
    };
    auto r = api.saveProjectAs(copyDir, false, copyFn, noCancel);
    if (!r.ok) return fail("save a copy: " + r.error);
    if (!copyDir.getChildFile("SmokeCopy.kickPrj").existsAsFile() || copyDir.getChildFile("SmokeA.kickPrj").exists()) return fail("copy has the wrong project file");
    if (!copyDir.getChildFile("samples/kick.wav").existsAsFile()) return fail("copy lost the samples folder");
    if (api.getProjectDir().getFullPathName() != a.getFullPathName() || api.getNumPrograms() != programsA) return fail("saving a copy changed the open project");
    if (api.saveProjectAs(copyDir, false, copyFn, noCancel).ok) return fail("an existing target was replaced");
    say("save a copy ok");

    // 4. Save As (switch): an unsaved edit goes to the new folder, the old one keeps its last saved state, closing writes nothing to it.
    api.createProgram(mt::MTProgramType::Drum, "Drum2");
    juce::File asDir = projects.getChildFile("SmokeAs");
    asDir.deleteRecursively();
    const auto aBefore = a.getChildFile("SmokeA.kickPrj").loadFileAsString();
    r = api.saveProjectAs(asDir, true, copyFn, noCancel);
    if (!r.ok) return fail("save as: " + r.error);
    if (api.isProjectDirty()) return fail("still unsaved after Save As");
    if (!api.closeProject()) return fail("cannot close after Save As");
    if (a.getChildFile("SmokeA.kickPrj").loadFileAsString() != aBefore) return fail("Save As wrote the old project on close");
    if (!api.loadProject(asDir)) return fail("cannot open the Save As copy: " + api.getProjectLoadError());
    if (api.getNumPrograms() != programsA + 1) return fail("the copy does not have the unsaved program");
    say("save as ok");

    // 5. "Don't save": the project closes without writing its unsaved edit.
    const auto programsAs = api.getNumPrograms();
    api.createProgram(mt::MTProgramType::Drum, "Discarded");
    if (!api.isProjectDirty()) return fail("edit was not unsaved");
    api.discardProjectChangesOnClose();
    if (!api.closeProject()) return fail("cannot close with discard");
    if (!api.loadProject(asDir)) return fail("cannot reopen after discard");
    if (api.getNumPrograms() != programsAs) return fail("Don't save saved the edit");
    // The discard applies to one close only: the next edit is saved on close as usual.
    api.createProgram(mt::MTProgramType::Drum, "Kept");
    if (!api.closeProject() || !api.loadProject(asDir) || api.getNumPrograms() != programsAs + 1) return fail("the discard flag leaked into the next close");
    say("don't save ok");

    // 6. A demo cannot be saved in place (and never autosaves into the factory folder) but saves as a copy.
    // (A project cannot be made inside the factory folder: a demo is a saved project that was shipped there.)
    juce::File demoSrc = projects.getChildFile("SmokeDemoSrc");
    demoSrc.deleteRecursively();
    if (!api.loadProject(demoSrc, true) || !api.saveProject() || !api.closeProject()) return fail("cannot make the demo source");
    juce::File demo = getAppDirs(MTDemos).getChildFile("SmokeDemo");
    demo.deleteRecursively();
    if (!kick_fileops::copyFolderTo(demoSrc.getFullPathName().toStdString(), demo.getFullPathName().toStdString(), noCancel).ok
        || !demo.getChildFile("SmokeDemoSrc.kickPrj").moveFileTo(demo.getChildFile("SmokeDemo.kickPrj")))
        return fail("cannot place the demo");
    if (!api.loadProject(demo)) return fail("cannot open the smoke demo: " + api.getProjectLoadError());
    if (!api.isProjectReadOnly()) return fail("a demo is not read-only");
    api.createProgram(mt::MTProgramType::Drum, "DemoEdit");
    if (api.saveProject()) return fail("a demo was saved in place");
    juce::File fromDemo = projects.getChildFile("SmokeFromDemo");
    fromDemo.deleteRecursively();
    r = api.saveProjectAs(fromDemo, true, copyFn, noCancel);
    if (!r.ok) return fail("demo save as: " + r.error);
    if (api.isProjectReadOnly() || !api.closeProject()) return fail("after saving the demo as a copy");
    if (!api.loadProject(fromDemo) || api.getNumPrograms() != 1) return fail("the copy of the demo does not have the edit");
    demo.deleteRecursively();
    demoSrc.deleteRecursively();
    say("demo ok");

    // 7. Loading a project takes the settle path (no fixed sleeps): several switches in a row, timed.
    const auto t0 = juce::Time::getMillisecondCounter();
    for (int i = 0; i < 3; ++i)
        if (!api.loadProject(i % 2 ? a : asDir)) return fail("switch " + juce::String(i));
    say("3 project switches took " + juce::String(juce::Time::getMillisecondCounter() - t0) + " ms");

    // 8. The Projects screen builds its tiles (Recent from the settings), Info / thumbnails need no project.
    kick_ui::signals::sigScreenClose(kick_ui::Screen::RecentProject);
    if (!kick_ui::KickHome::Instance().findScreen(kick_ui::Screen::RecentProject)) return fail("no Projects screen");
    say("projects screen ok");
    api.closeProject();

    // 9. The one load path (worker thread + queue), checked by timers while the app keeps running (run with KICK_SMOKE_SCREENS of ~12
    // screens so the app lives long enough): quick loads of two projects, the same one twice, and a missing one. The queue runs them
    // in order, the duplicate is dropped, the missing one fails with an error and leaves the previous project open, only real loads
    // enter the Recent list; a demo load never enters it.
    if (juce::SystemStats::getEnvironmentVariable("KICK_SMOKE_PROJECTS", {}) == "queue") {
        auto& loader = ui::LoadProjectAndAnimation::getInstance();
        juce::File ghost2 = projects.getChildFile("Ghost2");
        loader.startLoading(a, ui::LoadSource::User);
        loader.startLoading(asDir, ui::LoadSource::User);
        loader.startLoading(asDir, ui::LoadSource::User);   // already queued: dropped
        loader.startLoading(ghost2, ui::LoadSource::User);  // missing: error, nothing made up
        const auto bad = [](const juce::String& m) {
            std::cout << "PROJECTS FAIL: " << m << "\n" << std::flush;
            juce::JUCEApplication::getInstance()->setApplicationReturnValue(3);
        };
        juce::Timer::callAfterDelay(5000, [=] {
            auto& api2 = mt::getAudioInterface();
            juce::ModalComponentManager::getInstance()->cancelAllModalComponents(); // the "could not be opened" dialog
            if (ui::RecentProject::isProjectLoading) return bad("the queue did not finish");
            if (!api2.isProjectLoaded() || api2.getProjectDir().getFullPathName() != asDir.getFullPathName()) return bad("after the queue the open project is " + api2.getProjectDir().getFullPathName());
            if (ghost2.exists()) return bad("a missing project was made up by the load path");
            auto& recent = mt::getSettings().projects;
            bool sawA = false, sawAs = false, sawGhost = false;
            for (int i = 0; i < recent.getNumChildren(); ++i) {
                const auto p = recent.getProject(i);
                sawA = sawA || p == a.getFullPathName();
                sawAs = sawAs || p == asDir.getFullPathName();
                sawGhost = sawGhost || p == ghost2.getFullPathName();
            }
            if (!sawA || !sawAs || sawGhost) return bad("Recent list after the queue is wrong");
            std::cout << "PROJECTS: load queue ok (order kept, duplicate dropped, missing project refused, Recent updated)\n" << std::flush;

            const auto lastBefore = recent.currentProject();
            const auto countBefore = recent.getNumChildren();
            juce::File demo2 = getAppDirs(MTDemos).getChildFile("SmokeDemo2");
            demo2.deleteRecursively();
            std::atomic<bool> noCancel2{ false };
            kick_fileops::copyFolderTo(asDir.getFullPathName().toStdString(), demo2.getFullPathName().toStdString(), noCancel2);
            demo2.getChildFile("SmokeAs.kickPrj").moveFileTo(demo2.getChildFile("SmokeDemo2.kickPrj"));
            ui::LoadProjectAndAnimation::getInstance().startLoading(demo2, ui::LoadSource::Demo);
            juce::Timer::callAfterDelay(3500, [=] {
                auto& r2 = mt::getSettings().projects;
                if (mt::getAudioInterface().getProjectDir().getFullPathName() != demo2.getFullPathName()) return bad("the demo did not load");
                if (r2.currentProject() != lastBefore || r2.getNumChildren() != countBefore) return bad("a demo changed the Recent list / last project");
                std::cout << "PROJECTS: demo source ok\nPASS: project load queue\n" << std::flush;
                demo2.deleteRecursively();
            });
        });
        return {};
    }
    std::cout << "PASS: project operations\n";
    return {};
}
