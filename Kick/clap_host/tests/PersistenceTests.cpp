// SPDX-License-Identifier: AGPL-3.0-or-later
#include "PluginSession.h"
#include "../../modules/engine/project/kick_xml_manager.h"
#include <kick_clap/Discovery.h>
#include <atomic>
#include <thread>
#include <iostream>
#if defined(__linux__) || defined(__APPLE__)
 #include <csignal>
 #include <sys/resource.h>
#endif
#include "../../modules/common/source/kick_paths.h"
// kick_xml_manager.cpp asks the application for its data directories; this standalone test
// does not link the Kick common module, so provide directories that never exist (read-only logic stays inert).
static juce::File gTestDemosDir; // set by the read-only-location case; empty keeps every directory nonexistent
juce::File getAppDirs(DirType type) {
    if (type == MTDemos && gTestDemosDir != juce::File()) return gTestDemosDir;
    return juce::File::getSpecialLocation(juce::File::tempDirectory).getChildFile("kick-clap-persistence-nonexistent");
}
using namespace kick::clap;
static void check(bool ok, const char* why) { if (!ok) { std::cerr << "FAIL: " << why << '\n'; std::exit(1); } }
static void pump(int ms) { juce::MessageManager::getInstance()->runDispatchLoopUntil(ms); }
int main(int argc, char** argv) {
    check(argc == 2, "fixture path");
    juce::ScopedJuceInitialiser_GUI gui;
    const auto catalog = inspect(argv[1]); check(catalog.plugins.size() == 12, "fixtures");
    const auto directory = juce::File::getSpecialLocation(juce::File::tempDirectory).getNonexistentChildFile("kick-persistence", "", false);
    check(directory.createDirectory().wasOk(), "temporary project directory");
    const auto file = directory.getChildFile("project.xml");
    mt::KickXML document("ROOT", ".xml", "1");
    document.setAutoSave(true); document.setAutoSaveTime(20);
    std::shared_ptr<PluginSession> session;
    bool preparedOnMain = false; int preparationCalls = 0; int errors = 0; bool dirtyNotice = false;
    document.prepareSave = [&] {
        ++preparationCalls; preparedOnMain = juce::MessageManager::getInstance()->isThisTheMessageThread();
        return !session || session->save() ? juce::Result::ok() : juce::Result::fail("Program 'Lead' (fixture): " + session->status());
    };
    document.onSaveFailure = [&](const juce::String& reason) { ++errors; check(reason.contains("Lead"), "save error identifies program"); };
    document.onDirtyChanged = [&](bool dirty) { dirtyNotice = dirty; };
    auto tree = document.loadOrCreate(file); check(tree.isValid(), "create actual KickXML document");
    auto attach = [&](const Descriptor& plugin) {
        session = PluginSession::create(document.getState().getOrCreateChildWithName("PROGRAM", nullptr));
        session->prepare(48000, 128); session->service(); session->select(plugin);
        check(session->host().loaded(), "attach plugin");
    };
    auto checkDiskGain = [&](double expected) {
        const auto diskTree = juce::ValueTree::fromXml(file.loadFileAsString());
        auto diskSession = PluginSession::create(diskTree.getChildWithName("PROGRAM"));
        diskSession->prepare(48000, 128); diskSession->service();
        check(diskSession->host().loaded() && std::abs(diskSession->host().parameterValue(7) - expected) < 1e-9, "disk already contains current opaque state before any close/save/reopen");
    };
    auto reopen = [&] {
        session.reset();
        check(document.loadOrCreate(file).isValid(), "reopen saved project");
        session = PluginSession::create(document.getState().getChildWithName("PROGRAM"));
        session->prepare(48000, 128); session->service();
        check(session->host().loaded(), "restore plugin from disk");
    };
    juce::AudioBuffer<float> block(2, 128);
    attach(catalog.plugins[0]); check(document.save(), "initial save");
    check(session->host().setParameter(7, .7), "edit live sound before autosave"); session->render(block); session->service();
    // No explicit session/document save. Service's dirty marker starts KickXML's
    // real timer, and its shared barrier must snapshot the current opaque sound.
    pump(180); check(!document.isDirty() && preparedOnMain, "autosave completes on owning thread");
    checkDiskGain(.7);
    reopen(); check(std::abs(session->host().parameterValue(7) - .7) < 1e-9, "autosave then reopen retains live sound");
    session->host().setParameter(7, .6); session->render(block);
    check(document.close(), "close snapshots before programs are destroyed");
    checkDiskGain(.6);
    reopen(); check(std::abs(session->host().parameterValue(7) - .6) < 1e-9, "edit then close then reopen retains live sound");
    document.setAutoSave(false);
    session->host().setParameter(7, .55); session->render(block);
    pump(400);
    auto snapshotTree = juce::ValueTree::fromXml(document.getState().toXmlString());
    auto snapshot = PluginSession::create(snapshotTree.getChildWithName("PROGRAM"));
    snapshot->prepare(48000, 128); snapshot->service();
    check(std::abs(snapshot->host().parameterValue(7) - .55) < 1e-9, "bounded background snapshot updates opaque document state"); snapshot.reset();
    std::atomic<bool> done{false}; bool workerResult = false;
    std::thread worker([&] { workerResult = document.save(); done.store(true); });
    while (!done.load()) pump(10);
    worker.join(); check(workerResult && preparedOnMain, "worker save marshals preparation to message thread");
    session->remove(); session->select(catalog.plugins[10]); check(document.save(), "baseline state for failing fixture");
    const auto baselineDisk = file.loadFileAsString();
    const auto baselineOpaque = session->stateTree().getProperty("state");
    session->host().setParameter(7, .7); session->render(block);
    check(!document.save() && document.isDirty() && dirtyNotice && errors > 0, "failed explicit save stays visibly dirty");
    check(file.loadFileAsString() == baselineDisk && session->stateTree().getProperty("state") == baselineOpaque, "partial failed state save preserves previous disk and opaque snapshot");
    const auto beforeAutosave = preparationCalls;
    document.setAutoSave(true); session->service(); pump(100);
    check(preparationCalls > beforeAutosave, "failed autosave actually crossed the live-state barrier");
    check(document.isDirty() && file.loadFileAsString() == baselineDisk, "failed autosave preserves previous file");
    check(!document.close() && document.getDirectory().isNotEmpty(), "failed close retains active project");
    check(!document.loadOrCreate(directory.getChildFile("other.xml")).isValid() && !directory.getChildFile("other.xml").exists(), "failed close prevents project switch");
    session->host().setParameter(7, .6); session->render(block);
    check(document.save() && !document.isDirty() && document.getLastSaveError().isEmpty(), "retry can recover and clear dirty/error");
    checkDiskGain(.6);
    reopen(); check(std::abs(session->host().parameterValue(7) - .6) < 1e-9, "successful retry persisted live state");
    session->remove(); session->select(catalog.plugins[9]);
    const auto previousFile = file.loadFileAsString();
    check(!document.save() && document.getLastSaveError().contains("does not support"), "unsupported opaque state is explicit failure");
    check(file.loadFileAsString() == previousFile && document.isDirty(), "unsupported state preserves recoverable project");
    session->remove(); check(document.close(), "close after removing unsupported plugin"); session.reset();
    document.setAutoSave(false);
    // Force an I/O failure without relying on user/root permission behavior:
    // keep the previous document in a moved directory and block its old parent
    // path with a regular file. The failed transaction must leave it intact.
    document.onSaveFailure = [&](const juce::String&) { ++errors; };
    const auto ioDirectory = directory.getChildFile("io");
    const auto ioFile = ioDirectory.getChildFile("project.xml");
    check(document.loadOrCreate(ioFile).isValid(), "create document for disk failure");
    const auto originalXml = ioFile.loadFileAsString();
    document.getState().setProperty("edited", true, nullptr);
    const auto heldDirectory = directory.getChildFile("held");
    check(ioDirectory.moveFileTo(heldDirectory) && ioDirectory.replaceWithText("blocked parent"), "simulate unavailable project directory");
    check(!document.save() && document.isDirty() && document.getLastSaveError().contains("Cannot write"), "disk failure propagates and stays dirty");
    check(heldDirectory.getChildFile("project.xml").loadFileAsString() == originalXml, "failed write preserves previous document bytes");
    check(ioDirectory.deleteFile() && heldDirectory.moveFileTo(ioDirectory), "restore directory");
    check(document.save() && document.close(), "disk failure retry succeeds");
#if defined(__linux__) || defined(__APPLE__)
    check(document.loadOrCreate(ioFile).isValid(), "reopen document for incomplete write");
    const auto beforeShortWrite = ioFile.loadFileAsString();
    document.getState().setProperty("large", juce::String::repeatedString("x", 8192), nullptr);
    rlimit previousLimit{};
    check(getrlimit(RLIMIT_FSIZE, &previousLimit) == 0, "read file-size limit");
    auto limited = previousLimit; limited.rlim_cur = 1024;
    const auto previousSignal = std::signal(SIGXFSZ, SIG_IGN);
    check(setrlimit(RLIMIT_FSIZE, &limited) == 0, "force a partial project write");
    const auto savedWithLimit = document.save();
    check(setrlimit(RLIMIT_FSIZE, &previousLimit) == 0, "restore file-size limit");
    std::signal(SIGXFSZ, previousSignal);
    check(!savedWithLimit && document.isDirty() && ioFile.loadFileAsString() == beforeShortWrite,
          "partial write fails and preserves the previous project bytes");
    check(document.save() && document.close(), "save succeeds after write limit is removed");
#endif
    document.setAutoSave(true);
    check(document.loadOrCreate(ioFile).isValid(), "reopen document for automatic disk retry");
    document.getState().setProperty("retried", true, nullptr);
    check(ioDirectory.moveFileTo(heldDirectory) && ioDirectory.replaceWithText("blocked parent"), "interrupt autosave storage");
    const auto beforeFailure = errors;
    pump(100);
    check(errors > beforeFailure && document.isDirty(), "automatic write failure is reported");
    check(ioDirectory.deleteFile() && heldDirectory.moveFileTo(ioDirectory), "restore autosave storage");
    pump(150);
    check(!document.isDirty() && document.getLastSaveError().isEmpty() &&
          static_cast<bool>(juce::ValueTree::fromXml(ioFile.loadFileAsString()).getProperty("retried")),
          "failed autosave retries after storage recovers without another edit");
    check(document.close(), "close after automatic recovery");
    {   // patchSnapshot (G10): the file stores the patched ("base") value while the LIVE tree never leaves "auto" -- not before,
        // not during prepareSave/patchSnapshot, not after. (The audio thread reads the live tree during a save.)
        const auto pairFile = directory.getChildFile("pair/project.xml");
        mt::KickXML pair("ROOT", ".xml", "1");
        int patchCalls = 0; bool panSet = false; // panSet: false during loadOrCreate's initial save, which runs before "pan" exists
        pair.prepareSave = [&] { check(!panSet || pair.getState().getProperty("pan").toString() == "auto", "live tree is auto inside prepareSave"); return juce::Result::ok(); };
        pair.patchSnapshot = [&](juce::ValueTree& copy) {
            ++patchCalls;
            check(!panSet || pair.getState().getProperty("pan").toString() == "auto", "live tree is auto inside patchSnapshot");
            check(copy != pair.getState(), "patchSnapshot receives a copy, not the live tree");
            copy.setProperty("pan", "base", nullptr);
            check(!panSet || pair.getState().getProperty("pan").toString() == "auto", "patching the copy leaves the live tree alone");
        };
        check(pair.loadOrCreate(pairFile).isValid(), "create patchSnapshot document");
        pair.getState().setProperty("pan", "auto", nullptr); panSet = true;
        check(pair.save(), "save with patchSnapshot");
        check(juce::ValueTree::fromXml(pairFile.loadFileAsString()).getProperty("pan").toString() == "base", "file holds the patched value");
        check(pair.getState().getProperty("pan").toString() == "auto", "live tree untouched after save");
        check(!pair.isDirty(), "save does not leave the document dirty");
        pair.setAutoSave(true); pair.setAutoSaveTime(20);
        pump(150);
        check(juce::ValueTree::fromXml(pairFile.loadFileAsString()).getProperty("pan").toString() == "base", "no spurious autosave overwrote the file");
        pair.getState().setProperty("other", 1, nullptr);
        pump(150);
        const auto autosaved = juce::ValueTree::fromXml(pairFile.loadFileAsString());
        check(static_cast<int>(autosaved.getProperty("other")) == 1 && autosaved.getProperty("pan").toString() == "base" &&
              pair.getState().getProperty("pan").toString() == "auto", "autosave patches the copy too");
        const int patchCallsBeforeClose = patchCalls;
        pair.getState().setProperty("other", 2, nullptr);
        check(pair.close(), "close patchSnapshot document");
        check(juce::ValueTree::fromXml(pairFile.loadFileAsString()).getProperty("pan").toString() == "base", "closing save keeps the patched value");
        check(patchCalls == patchCallsBeforeClose + 1, "closing save ran patchSnapshot exactly once");
        check(static_cast<int>(juce::ValueTree::fromXml(pairFile.loadFileAsString()).getProperty("other")) == 2, "closing save wrote the latest state");
        // Read-only location (demo dir; chmod is unreliable as root): Autosave/Close succeed without writing, without running
        // the live-state barrier or the patch, and leave the document clean.
        gTestDemosDir = directory.getChildFile("demos");
        const auto roFile = gTestDemosDir.getChildFile("ro/project.xml");
        check(roFile.create().wasOk() && roFile.replaceWithText("<ROOT version=\"1\"/>"), "seed read-only-location project");
        mt::KickXML ro("ROOT", ".xml", "1");
        int roCalls = 0;
        ro.prepareSave = [&] { ++roCalls; return juce::Result::ok(); };
        ro.patchSnapshot = [&](juce::ValueTree&) { ++roCalls; };
        check(ro.loadOrCreate(roFile).isValid(), "create read-only-location document");
        ro.getState().setProperty("pan", "auto", nullptr);
        const auto roBefore = roFile.loadFileAsString();
        check(ro.close() && !ro.isDirty() && roFile.loadFileAsString() == roBefore, "read-only close writes nothing and is clean");
        check(roCalls == 0 && ro.getState().getProperty("pan").toString() == "auto", "read-only close runs no prepare/patch and keeps the live tree");
        gTestDemosDir = juce::File();
    }
    {   // Sample-path collision: samples/kick.wav exists and an external /Pack/kick.wav is loaded. The LIVE source must stay
        // absolute (the asset slice map is keyed by it), the saved file gets samples/Pack_kick.wav, and re-saving never duplicates.
        const auto projFile = directory.getChildFile("coll/project.xml");
        const auto samples = projFile.getParentDirectory().getChildFile("samples");
        const auto external = directory.getChildFile("Pack/kick.wav");
        check(samples.getChildFile("kick.wav").create().wasOk() && samples.getChildFile("kick.wav").replaceWithText("own"), "seed colliding sample");
        check(external.create().wasOk() && external.replaceWithText("external"), "seed external sample");
        mt::KickXML coll("ROOT", ".xml", "1");
        check(coll.loadOrCreate(projFile).isValid(), "create collision document");
        juce::ValueTree layer("LAYER"); layer.setProperty("source", external.getFullPathName(), nullptr);
        coll.getState().appendChild(layer, nullptr);
        auto savedSource = [&] { return juce::ValueTree::fromXml(projFile.loadFileAsString()).getChild(0).getProperty("source").toString(); };
        check(coll.save(), "save collision document");
        check(layer.getProperty("source").toString() == external.getFullPathName(), "live source stays absolute on collision");
        check(savedSource() == "samples/Pack_kick.wav", "saved file holds samples/<parent>_<name>");
        check(samples.getChildFile("Pack_kick.wav").loadFileAsString() == "external" && samples.getChildFile("kick.wav").loadFileAsString() == "own", "collision copy made, original untouched");
        coll.getState().setProperty("touch", 1, nullptr);
        check(coll.save(), "second save");
        check(savedSource() == "samples/Pack_kick.wav" && samples.findChildFiles(juce::File::findFiles, false, "*.wav").size() == 2, "second save does not duplicate");
        check(external.deleteFile(), "external source disappears");
        coll.getState().setProperty("touch", 2, nullptr);
        check(coll.save() && savedSource() == "samples/Pack_kick.wav", "vanished source still maps to the existing renamed copy");
        check(layer.getProperty("source").toString() == external.getFullPathName(), "live source still absolute");
        check(coll.close(), "close collision document");
    }
    {   // Two layers on the same external file, samples/ still empty: one shared copy, both saved sources identical and relative.
        const auto projFile = directory.getChildFile("dup/project.xml");
        const auto samples = projFile.getParentDirectory().getChildFile("samples");
        const auto external = directory.getChildFile("DupPack/kick.wav");
        check(external.create().wasOk() && external.replaceWithText("dupsample"), "seed shared external sample");
        mt::KickXML dup("ROOT", ".xml", "1");
        check(dup.loadOrCreate(projFile).isValid(), "create shared-source document");
        for (int i = 0; i < 2; ++i) {
            juce::ValueTree layer("LAYER"); layer.setProperty("source", external.getFullPathName(), nullptr);
            dup.getState().appendChild(layer, nullptr);
        }
        check(dup.save(), "save shared-source document");
        const auto saved = juce::ValueTree::fromXml(projFile.loadFileAsString());
        check(saved.getChild(0).getProperty("source").toString() == "samples/kick.wav" &&
              saved.getChild(1).getProperty("source").toString() == "samples/kick.wav", "both layers saved with the same relative source");
        check(samples.findChildFiles(juce::File::findFiles, false, "*").size() == 1 && samples.getChildFile("kick.wav").loadFileAsString() == "dupsample", "exactly one sample file created");
        check(dup.close(), "close shared-source document");
    }
    {   // Two different external files with identical bytes (separate FILE nodes): different assets, so never merged into one samples/ file.
        const auto projFile = directory.getChildFile("same/project.xml");
        const auto samples = projFile.getParentDirectory().getChildFile("samples");
        const auto a = directory.getChildFile("A/kick.wav"), b = directory.getChildFile("B/kick.wav");
        check(a.create().wasOk() && a.replaceWithText("identical") && b.create().wasOk() && b.replaceWithText("identical"), "seed identical-content sources");
        mt::KickXML same("ROOT", ".xml", "1");
        check(same.loadOrCreate(projFile).isValid(), "create identical-content document");
        juce::ValueTree la("LAYER"), lb("LAYER");
        la.setProperty("source", a.getFullPathName(), nullptr); lb.setProperty("source", b.getFullPathName(), nullptr);
        same.getState().appendChild(la, nullptr); same.getState().appendChild(lb, nullptr);
        auto savedAt = [&](int i) { return juce::ValueTree::fromXml(projFile.loadFileAsString()).getChild(i).getProperty("source").toString(); };
        for (int round = 0; round < 2; ++round) {
            check(same.save(), "save identical-content document");
            check(savedAt(0) == "samples/kick.wav" && savedAt(1) == "samples/B_kick.wav", "identical bytes, different sources -> two saved sources");
            check(samples.findChildFiles(juce::File::findFiles, false, "*.wav").size() == 2, "two separate sample files, no duplicates on re-save");
            check(lb.getProperty("source").toString() == b.getFullPathName(), "live colliding source stays absolute");
            same.getState().setProperty("touch", round, nullptr);
        }
        check(same.close(), "close identical-content document");
    }
    {   // N2: samples/kick.wav is taken, /Kit1/Kicks/kick.wav and /Kit2/Kicks/kick.wav both map to Kicks_kick.wav: three separate
        // files (Kit2 gets Kicks_kick_2.wav), stable on re-save. A second project does the same with identical bytes.
        for (int variant = 0; variant < 2; ++variant) {
            const auto root = directory.getChildFile("n2v" + juce::String(variant));
            const auto projFile = root.getChildFile("proj/project.xml");
            const auto samples = projFile.getParentDirectory().getChildFile("samples");
            const auto k1 = root.getChildFile("Kit1/Kicks/kick.wav"), k2 = root.getChildFile("Kit2/Kicks/kick.wav");
            check(samples.getChildFile("kick.wav").create().wasOk() && samples.getChildFile("kick.wav").replaceWithText("own"), "seed taken sample");
            check(k1.create().wasOk() && k1.replaceWithText("kit1") && k2.create().wasOk() && k2.replaceWithText(variant ? "kit1" : "kit2"), "seed kit sources");
            mt::KickXML doc("ROOT", ".xml", "1");
            check(doc.loadOrCreate(projFile).isValid(), "create N2 document");
            juce::ValueTree l1("LAYER"), l2("LAYER");
            l1.setProperty("source", k1.getFullPathName(), nullptr); l2.setProperty("source", k2.getFullPathName(), nullptr);
            doc.getState().appendChild(l1, nullptr); doc.getState().appendChild(l2, nullptr);
            auto savedAt = [&](int i) { return juce::ValueTree::fromXml(projFile.loadFileAsString()).getChild(i).getProperty("source").toString(); };
            for (int round = 0; round < 3; ++round) {
                check(doc.save(), "save N2 document");
                check(savedAt(0) == "samples/Kicks_kick.wav" && savedAt(1) == "samples/Kicks_kick_2.wav", "N2: two kits -> two distinct saved sources");
                check(samples.getChildFile("Kicks_kick.wav").loadFileAsString() == "kit1" &&
                      samples.getChildFile("Kicks_kick_2.wav").loadFileAsString() == (variant ? "kit1" : "kit2") &&
                      samples.getChildFile("kick.wav").loadFileAsString() == "own", "N2: contents belong to their sources");
                check(samples.findChildFiles(juce::File::findFiles, false, "*.wav").size() == 3, "N2: exactly three sample files, stable on re-save");
                doc.getState().setProperty("touch", round, nullptr);
            }
            check(doc.close(), "close N2 document");
        }
    }
    {   // N1: /Packs/Zeta/kick.wav and /Packs/Alpha/kick.wav, empty samples/. Rewriting only Zeta to the relative samples/kick.wav
        // would make the asset manager's file-name fallback (first sliceMap key named kick.wav = Alpha) give Zeta's layer Alpha's
        // slice. Both live sources must therefore stay absolute (exact slice-map keys); the saved file maps them to distinct copies.
        const auto projFile = directory.getChildFile("n1/project.xml");
        const auto samples = projFile.getParentDirectory().getChildFile("samples");
        const auto zeta = directory.getChildFile("Packs/Zeta/kick.wav"), alpha = directory.getChildFile("Packs/Alpha/kick.wav");
        check(zeta.create().wasOk() && zeta.replaceWithText("zeta") && alpha.create().wasOk() && alpha.replaceWithText("alpha"), "seed N1 sources");
        mt::KickXML doc("ROOT", ".xml", "1");
        check(doc.loadOrCreate(projFile).isValid(), "create N1 document");
        juce::ValueTree lz("LAYER"), la("LAYER");
        lz.setProperty("source", zeta.getFullPathName(), nullptr); la.setProperty("source", alpha.getFullPathName(), nullptr);
        doc.getState().appendChild(lz, nullptr); doc.getState().appendChild(la, nullptr);
        auto savedAt = [&](int i) { return juce::ValueTree::fromXml(projFile.loadFileAsString()).getChild(i).getProperty("source").toString(); };
        for (int round = 0; round < 2; ++round) {
            check(doc.save(), "save N1 document");
            check(lz.getProperty("source").toString() == zeta.getFullPathName() && la.getProperty("source").toString() == alpha.getFullPathName(),
                  "N1: same-named live sources both stay absolute");
            check(savedAt(0) == "samples/kick.wav" && savedAt(1) == "samples/Alpha_kick.wav", "N1: saved sources are distinct relative copies");
            check(samples.getChildFile("kick.wav").loadFileAsString() == "zeta" && samples.getChildFile("Alpha_kick.wav").loadFileAsString() == "alpha" &&
                  samples.findChildFiles(juce::File::findFiles, false, "*.wav").size() == 2, "N1: contents match, no duplicates");
            doc.getState().setProperty("touch", round, nullptr);
        }
        check(doc.close(), "close N1 document");
    }
    {   // P1: N1 layout, then the drive of A disappears. A has no live-relative path (name clash), so its saved reference must come
        // from the session's source -> copy map, not be rewritten to the absolute path (silent mute layer on reopen).
        const auto projFile = directory.getChildFile("p1/project.xml");
        const auto samples = projFile.getParentDirectory().getChildFile("samples");
        const auto zeta = directory.getChildFile("UsbP1/Zeta/kick.wav"), alpha = directory.getChildFile("UsbP1/Alpha/kick.wav");
        check(zeta.create().wasOk() && zeta.replaceWithText("zeta") && alpha.create().wasOk() && alpha.replaceWithText("alpha"), "seed P1 sources");
        mt::KickXML doc("ROOT", ".xml", "1");
        check(doc.loadOrCreate(projFile).isValid(), "create P1 document");
        juce::ValueTree lz("LAYER"), la("LAYER");
        lz.setProperty("source", zeta.getFullPathName(), nullptr); la.setProperty("source", alpha.getFullPathName(), nullptr);
        doc.getState().appendChild(lz, nullptr); doc.getState().appendChild(la, nullptr);
        auto savedAt = [&](int i) { return juce::ValueTree::fromXml(projFile.loadFileAsString()).getChild(i).getProperty("source").toString(); };
        check(doc.save() && savedAt(0) == "samples/kick.wav" && savedAt(1) == "samples/Alpha_kick.wav", "P1: first save maps both");
        check(zeta.deleteFile(), "P1: zeta source disappears");
        doc.getState().setProperty("touch", 1, nullptr);
        check(doc.save(), "P1: save with a missing source succeeds");
        check(savedAt(0) == "samples/kick.wav" && savedAt(1) == "samples/Alpha_kick.wav", "P1: missing source keeps its saved relative copy");
        check(lz.getProperty("source").toString() == zeta.getFullPathName(), "P1: live tree untouched");
        check(samples.getChildFile("kick.wav").loadFileAsString() == "zeta" && samples.findChildFiles(juce::File::findFiles, false, "*.wav").size() == 2, "P1: copies intact");
        check(doc.close(), "close P1 document");
    }
    {   // P1b: two Kicks_kick sources vanish together and the layer order changes: each source keeps its own copy.
        const auto projFile = directory.getChildFile("p1b/project.xml");
        const auto k1 = directory.getChildFile("UsbP1b/Kit1/Kicks/kick.wav"), k2 = directory.getChildFile("UsbP1b/Kit2/Kicks/kick.wav");
        const auto taken = projFile.getParentDirectory().getChildFile("samples/kick.wav");
        check(taken.create().wasOk() && taken.replaceWithText("own"), "seed P1b taken sample");
        check(k1.create().wasOk() && k1.replaceWithText("kit1") && k2.create().wasOk() && k2.replaceWithText("kit2"), "seed P1b sources");
        mt::KickXML doc("ROOT", ".xml", "1");
        check(doc.loadOrCreate(projFile).isValid(), "create P1b document");
        juce::ValueTree l1("LAYER"), l2("LAYER");
        l1.setProperty("source", k1.getFullPathName(), nullptr); l2.setProperty("source", k2.getFullPathName(), nullptr);
        doc.getState().appendChild(l1, nullptr); doc.getState().appendChild(l2, nullptr);
        auto savedAt = [&](int i) { return juce::ValueTree::fromXml(projFile.loadFileAsString()).getChild(i).getProperty("source").toString(); };
        check(doc.save() && savedAt(0) == "samples/Kicks_kick.wav" && savedAt(1) == "samples/Kicks_kick_2.wav", "P1b: first save maps both");
        check(k1.deleteFile() && k2.deleteFile(), "P1b: both sources disappear");
        doc.getState().moveChild(1, 0, nullptr);
        check(doc.save(), "P1b: save succeeds");
        check(savedAt(0) == "samples/Kicks_kick_2.wav" && savedAt(1) == "samples/Kicks_kick.wav", "P1b: reordered layers keep their own copies");
        check(doc.close(), "close P1b document");
    }
    {   // Q1: A's source AND its own saved copy vanish: A must not adopt B's copy (Kicks_kick.wav), B's bytes stay with B.
        const auto projFile = directory.getChildFile("q1/project.xml");
        const auto a = directory.getChildFile("UsbQ1/x/Kicks/kick.wav"), b = directory.getChildFile("UsbQ1/y/Kicks/kick.wav");
        const auto samples = projFile.getParentDirectory().getChildFile("samples");
        check(a.create().wasOk() && a.replaceWithText("aaa") && b.create().wasOk() && b.replaceWithText("bbb"), "seed Q1 sources");
        mt::KickXML doc("ROOT", ".xml", "1");
        check(doc.loadOrCreate(projFile).isValid(), "create Q1 document");
        juce::ValueTree l1("LAYER"), l2("LAYER");
        l1.setProperty("source", a.getFullPathName(), nullptr); l2.setProperty("source", b.getFullPathName(), nullptr);
        doc.getState().appendChild(l1, nullptr); doc.getState().appendChild(l2, nullptr);
        auto savedAt = [&](int i) { return juce::ValueTree::fromXml(projFile.loadFileAsString()).getChild(i).getProperty("source").toString(); };
        check(doc.save() && savedAt(0) == "samples/kick.wav" && savedAt(1) == "samples/Kicks_kick.wav", "Q1: first save maps both");
        check(a.deleteFile() && samples.getChildFile("kick.wav").deleteFile(), "Q1: A's source and its copy vanish");
        doc.getState().setProperty("touch", 1, nullptr);
        check(doc.save(), "Q1: save does not block on a permanently missing sample");
        check(savedAt(0) != "samples/Kicks_kick.wav", "Q1: A does not point at B's copy");
        check(savedAt(1) == "samples/Kicks_kick.wav" && samples.getChildFile("Kicks_kick.wav").loadFileAsString() == "bbb", "Q1: B keeps its own copy");
        check(l1.getProperty("source").toString() == a.getFullPathName() && l2.getProperty("source").toString() == b.getFullPathName(), "Q1: live tree untouched");
        check(doc.close(), "close Q1 document");
    }
    {   // O1: after a save A is live-relative (samples/kick.wav). A new external B with identical bytes, or A added again as a new
        // node, must not adopt that file: the live tree already claims it. Each gets its own copy and stays absolute live.
        const auto projFile = directory.getChildFile("o1/project.xml");
        const auto samples = projFile.getParentDirectory().getChildFile("samples");
        const auto a = directory.getChildFile("Packs/A/kick.wav"), b = directory.getChildFile("Packs/B/kick.wav");
        check(a.create().wasOk() && a.replaceWithText("same") && b.create().wasOk() && b.replaceWithText("same"), "seed O1 sources");
        mt::KickXML doc("ROOT", ".xml", "1");
        check(doc.loadOrCreate(projFile).isValid(), "create O1 document");
        juce::ValueTree l1("LAYER"), l2("LAYER"), l3("LAYER");
        l1.setProperty("source", a.getFullPathName(), nullptr);
        doc.getState().appendChild(l1, nullptr);
        auto savedAt = [&](int i) { return juce::ValueTree::fromXml(projFile.loadFileAsString()).getChild(i).getProperty("source").toString(); };
        check(doc.save() && l1.getProperty("source").toString() == "samples/kick.wav" && savedAt(0) == "samples/kick.wav", "O1: A becomes relative");
        l2.setProperty("source", b.getFullPathName(), nullptr);
        doc.getState().appendChild(l2, nullptr);
        l3.setProperty("source", a.getFullPathName(), nullptr);
        for (int round = 0; round < 2; ++round) {
            if (round == 1) doc.getState().appendChild(l3, nullptr);
            check(doc.save(), "save O1 document");
            check(l1.getProperty("source").toString() == "samples/kick.wav", "O1: A keeps its file");
            check(l2.getProperty("source").toString() == b.getFullPathName(), "O1: B stays absolute live");
            check(savedAt(0) == "samples/kick.wav" && savedAt(1) != "samples/kick.wav" && savedAt(1).startsWith("samples/"), "O1: B has its own saved file");
            if (round == 1) check(l3.getProperty("source").toString() == a.getFullPathName() && savedAt(2) != "samples/kick.wav" && savedAt(2) != savedAt(1) && savedAt(2).startsWith("samples/"), "O1: re-added A gets its own file");
            check(samples.findChildFiles(juce::File::findFiles, false, "*.wav").size() == (round == 0 ? 2 : 3), "O1: file count stable");
            doc.getState().setProperty("touch", round, nullptr);
        }
        check(doc.close(), "close O1 document");
    }
    {   // O2: a failed sample copy must fail the save (dirty, error), leave the source absolute and the file untouched.
        const auto projFile = directory.getChildFile("o2/project.xml");
        const auto src = directory.getChildFile("Packs/C/snare.wav");
        check(src.create().wasOk() && src.replaceWithText("snare"), "seed O2 source");
        mt::KickXML doc("ROOT", ".xml", "1");
        check(doc.loadOrCreate(projFile).isValid(), "create O2 document");
        const auto samples = projFile.getParentDirectory().getChildFile("samples");
        check(samples.create().wasOk(), "block samples dir with a file");
        juce::ValueTree l("LAYER");
        l.setProperty("source", src.getFullPathName(), nullptr);
        doc.getState().appendChild(l, nullptr);
        check(!doc.save() && doc.isDirty() && doc.getLastSaveError().contains("Cannot copy sample"), "O2: failed copy fails the save and keeps dirty");
        check(l.getProperty("source").toString() == src.getFullPathName() && !projFile.loadFileAsString().contains("samples/"), "O2: no relative path for a missing file");
        check(samples.deleteFile() && doc.save() && !doc.isDirty() && l.getProperty("source").toString() == "samples/snare.wav" &&
              samples.getChildFile("snare.wav").loadFileAsString() == "snare", "O2: retry succeeds once the copy works");
        check(doc.close(), "close O2 document");
    }
    directory.deleteRecursively();
    std::cout << "PASS: actual KickXML autosave, close, worker save, bounded snapshots, failed/unsupported state, dirty/error reporting and retry\n";
}
