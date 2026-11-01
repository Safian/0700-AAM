/*
 * Copyright (c) 2024 MUSIC Tribe
 *
 * This file is part of Kick.
 *
 * Kick is free software: you can redistribute it and/or modify it under the
 * terms of the GNU Affero General Public License as published by the Free
 * Software Foundation, either version 3 of the License, or (at your option)
 * any later version.
 *
 * Kick is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 * FOR A PARTICULAR PURPOSE. See the GNU Affero General Public License for
 * more details.
 *
 * You should have received a copy of the GNU Affero General Public License
 * along with Kick. If not, see <https://www.gnu.org/licenses/>.
 *
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */

#include "kick_xml_manager.h"
#include "../../common/source/kick_paths.h"
#include <map>
#include <set>

namespace {
// Never hold the document lock while waiting for the message thread. Project
// loading runs on a worker, but CLAP snapshots must run on the owning UI thread.
void onPersistenceThread(std::function<void()> action) {
    auto* manager = juce::MessageManager::getInstanceWithoutCreating();
    if (!manager || manager->isThisTheMessageThread()) action();
    else manager->callFunctionOnMessageThread([](void* context) -> void* {
        (*static_cast<std::function<void()>*>(context))();
        return nullptr;
    }, &action);
}

bool isAudioFilePath(const juce::String& path) {
    if (path.isEmpty()) return false;
    return juce::File(path).hasFileExtension(".wav;bwf;aif;aiff;flac;ogg;mp3;wmv;asf;wm;wma");
}

// Live pass (renameCollisions=false): copies every foreign sample into the project (once) and records source -> destination in
// `copied`. The live property is rewritten to samples/<name> only when that is safe: the destination is the source's own file
// name AND no other external source in the tree has the same file name. Otherwise the live source stays absolute, because the
// asset slice map is keyed by the absolute path and MTAudioAssetManager's file-name fallback (first key with that name, in map
// order) would hand this layer ANOTHER source's slice once a same-named sibling was rewritten (wrong slice, or mute).
// Snapshot pass (renameCollisions=true): only applies the recorded destination to the serialised copy.
// copied: per save, absolute source path -> destination. The same source may share its destination (two layers on one external
// file); a different source is never mapped onto a file another source claimed in this save.
using CopiedMap = std::map<juce::String, juce::File>;
using NameClashSet = std::map<juce::String, std::set<juce::String>>; // file name -> distinct external absolute sources

void collectExternalSamples(const juce::ValueTree& tree, const juce::File& projectDir, NameClashSet& names) {
    for (const char* prop : {"path", "source"}) {
        if (!tree.hasProperty(prop)) continue;
        const auto val = tree.getProperty(prop).toString();
        if (isAudioFilePath(val) && juce::File::isAbsolutePath(val) && !juce::File(val).isAChildOf(projectDir))
            names[juce::File(val).getFileName()].insert(juce::File(val).getFullPathName());
    }
    for (int i = 0; i < tree.getNumChildren(); ++i) collectExternalSamples(tree.getChild(i), projectDir, names);
}

// Candidate names for a source: <name>, <parent>_<name>, <parent>_<stem>_2<ext>, <parent>_<stem>_3<ext>, ...
juce::File candidateDest(const juce::File& samplesDir, const juce::File& src, int index) {
    if (index == 0) return samplesDir.getChildFile(src.getFileName());
    const auto parent = src.getParentDirectory().getFileName() + "_";
    if (index == 1) return samplesDir.getChildFile(parent + src.getFileName());
    return samplesDir.getChildFile(parent + src.getFileNameWithoutExtension() + "_" + juce::String(index) + src.getFileExtension());
}

// First candidate that is free (copy there), or already holds this source's own copy: claimed by this very source earlier in the
// save, or an unclaimed file from an earlier save with identical bytes. A file claimed by a different source is skipped, so two
// sources never share a file (separate slice maps). Remaining case: two distinct assets with identical bytes AND identical
// parent folder name + file name (e.g. /Kit1/Kicks/kick.wav and /Kit2/Kicks/kick.wav, same bytes) get candidate names in order;
// in a later session, when the earlier copy is not claimed in that save, the first of them may adopt the other's identical file.
// A source that no longer exists cannot be compared: it only maps to an existing renamed copy (index >= 1), as before.
juce::File chooseDest(const juce::File& samplesDir, const juce::File& src, const CopiedMap& copied) {
    const bool exists = src.existsAsFile();
    for (int index = exists ? 0 : 1; index < 1000; ++index) {
        const auto dest = candidateDest(samplesDir, src, index);
        if (!dest.existsAsFile()) return exists ? dest : juce::File();
        bool claimedByOther = false;
        for (const auto& entry : copied)
            if (entry.second == dest) {
                if (entry.first == src.getFullPathName()) return dest;
                claimedByOther = true;
            }
        if (claimedByOther) continue;
        if (!exists || dest.hasIdenticalContentTo(src)) return dest;
    }
    return {};
}

void relativizeSamplePaths(juce::ValueTree& tree, const juce::File& projectDir, bool renameCollisions, CopiedMap& copied,
                           const NameClashSet& names) {
    if (!projectDir.exists() || !tree.isValid()) return;

    static const juce::Identifier idPath("path");
    static const juce::Identifier idSource("source");

    auto processProperty = [&](const juce::Identifier& prop) {
        if (!tree.hasProperty(prop)) return;
        auto val = tree.getProperty(prop).toString();
        if (isAudioFilePath(val) && juce::File::isAbsolutePath(val)) {
            juce::File f(val);
            if (f.isAChildOf(projectDir)) {
                if (!renameCollisions) tree.setProperty(prop, f.getRelativePathFrom(projectDir), nullptr);
            } else if (renameCollisions) {
                const auto done = copied.find(f.getFullPathName());
                if (done != copied.end()) tree.setProperty(prop, done->second.getRelativePathFrom(projectDir), nullptr);
            } else {
                auto samplesDir = projectDir.getChildFile("samples");
                if (!samplesDir.exists() && projectDir.getChildFile("Imported").exists())
                    samplesDir = projectDir.getChildFile("Imported");

                const auto done = copied.find(f.getFullPathName());
                auto dest = done != copied.end() ? done->second : chooseDest(samplesDir, f, copied);
                if (dest == juce::File()) return;
                if (!dest.existsAsFile()) {
                    samplesDir.createDirectory();
                    f.copyFileTo(dest);
                }
                copied[f.getFullPathName()] = dest;
                const auto clash = names.find(f.getFileName());
                const bool unique = clash == names.end() || clash->second.size() <= 1;
                if (unique && dest.getFileName() == f.getFileName())
                    tree.setProperty(prop, dest.getRelativePathFrom(projectDir), nullptr);
            }
        }
    };

    processProperty(idPath);
    processProperty(idSource);

    for (int i = 0; i < tree.getNumChildren(); ++i) {
        auto child = tree.getChild(i);
        relativizeSamplePaths(child, projectDir, renameCollisions, copied, names);
    }
}
}

namespace mt {
KickXML::KickXML(juce::Identifier ID, const juce::String& FileExtension, const juce::String& Version)
    : id(ID), extn(FileExtension), version(Version) {}
KickXML::~KickXML() {
    // Owners with live-state hooks must close explicitly while their programs
    // still exist. Preferences retain their original destructor-save behavior.
    timer.stopTimer(); cancelPendingUpdate();
    if (!prepareSave) save();
    vt.removeListener(this);
}
bool KickXML::autoSave{true};
int KickXML::autoSaveTimeOutInMS{1500};
void KickXML::valueTreeChanged() {
    dirty.store(true);
    if (!saving.load()) triggerAsyncUpdate();
}
void KickXML::handleAsyncUpdate() {
    if (autosaveDue.exchange(false)) {
        save(SaveReason::Autosave);
        return;
    }
    if (onDirtyChanged) onDirtyChanged(dirty.load());
    if (!dirty.load() || saving.load()) return;
    const bool preferences = vt.hasType("PREFERENCES");
    if ((preferences || autoSave) && !timer.isTimerRunning())
        timer.startTimer(preferences ? prefAutoSaveTimeOutInMS : juce::jmax(1, autoSaveTimeOutInMS));
}
juce::ValueTree KickXML::loadOrCreate(const juce::File& requested) {
    juce::ValueTree result;
    onPersistenceThread([&] {
        if (!close()) return;
        const juce::ScopedLock guard(lock);
        const bool isFile = requested.existsAsFile() || requested.hasFileExtension(extn);
        const auto directory = isFile ? requested.getParentDirectory() : requested;
        const auto file = isFile ? requested : directory.getChildFile(directory.getFileName() + extn);
        juce::ValueTree loaded;
        const bool existing = file.existsAsFile() && file.getSize() != 0;
        if (existing) {
            loaded = juce::ValueTree::fromXml(file.loadFileAsString());
            if (!loaded.isValid()) { failSave("Project XML could not be read", SaveReason::Explicit); return; }
        } else {
            if (directory.createDirectory().failed()) { failSave("Cannot create project directory", SaveReason::Explicit); return; }
            loaded = juce::ValueTree{id};
            loaded.setProperty("manufacturer", __manufacturer, nullptr);
            loaded.setProperty("version", version, nullptr);
        }
        vt.removeListener(this);
        vt = loaded;
        xmlFileDir = directory; xmlFile = file; directoryText = directory.getFullPathName();
        dirty.store(!existing);
        vt.addListener(this);
        if (dirty.load() && !saveOnMessageThread(SaveReason::Explicit)) return;
        onLoad(); result = vt;
    });
    return result;
}
bool KickXML::failSave(const juce::String& message, SaveReason reason) {
    const bool changed = lastSaveError != message;
    lastSaveError = message; dirty.store(true);
    if (onDirtyChanged) onDirtyChanged(true);
    // Do not repeatedly open the same error dialog on every autosave retry.
    if (onSaveFailure && (changed || reason != SaveReason::Autosave)) onSaveFailure(message);
    // saveOnMessageThread stops the timer before attempting the transaction.
    // Keep retrying a failed autosave even if no further edit changes the tree.
    if (reason == SaveReason::Autosave) triggerAsyncUpdate();
    return false;
}
bool KickXML::saveOnMessageThread(SaveReason reason) {
    if (xmlFile == juce::File{}) return true;
    if (saving.exchange(true)) return failSave("Project save is already in progress", reason);
    struct Transaction { std::atomic<bool>& flag; ~Transaction() { flag.store(false); } } transaction{saving};
    timer.stopTimer(); cancelPendingUpdate(); autosaveDue.store(false);
    // Read-only projects (such as factory demos and templates shipped in the read-only application bundle)
    // cannot and should not be saved over. Autosave and Close must succeed cleanly without writing or failing.
    bool isReadOnlyLocation = false;
#if defined( JUCE_IOS )
    const auto bundleDir = juce::File::getSpecialLocation( juce::File::currentApplicationFile );
    if (bundleDir.exists() && xmlFile.isAChildOf( bundleDir ))
        isReadOnlyLocation = true;
#endif
    if (!isReadOnlyLocation && (!xmlFile.hasWriteAccess() || (xmlFile.getParentDirectory().exists() && !xmlFile.getParentDirectory().hasWriteAccess())))
        isReadOnlyLocation = true;

    const auto demosDir = getAppDirs( MTDemos );
    if (!isReadOnlyLocation && demosDir.exists() && xmlFile.isAChildOf( demosDir ))
        isReadOnlyLocation = true;

    const auto tempDir = getAppDirs( MTTemp );
    if (!isReadOnlyLocation && tempDir.exists() && xmlFile.isAChildOf( tempDir ))
        isReadOnlyLocation = true;

    // Nothing is written there, so Autosave/Close skip the live-state barrier entirely.
    if (isReadOnlyLocation && (reason == SaveReason::Autosave || reason == SaveReason::Close)) {
        const juce::ScopedLock guard(lock);
        if (!vt.isValid()) return failSave("No valid project document to save", reason);
        lastSaveError.clear();
        dirty.store(false);
        if (onDirtyChanged) onDirtyChanged(false);
        return true;
    }

    // Snapshot first; neither a failed snapshot nor a failed disk write may
    // replace the previous recoverable project document.
    if (prepareSave) {
        const auto prepared = prepareSave();
        if (prepared.failed()) return failSave(prepared.getErrorMessage(), reason);
    }
    const juce::ScopedLock guard(lock);
    if (!vt.isValid()) return failSave("No valid project document to save", reason);

    if (isReadOnlyLocation)
        return failSave("Cannot save over factory demo/template. Please use Save As to a project folder.", reason);

    // Sample paths stay rewritten in the live document as before: relativizeSamplePaths copies each foreign sample into the
    // project once and is only idempotent because the live path becomes relative (a copy would duplicate files every save).
    CopiedMap copied;
    NameClashSet names;
    collectExternalSamples(vt, xmlFile.getParentDirectory(), names);
    relativizeSamplePaths(vt, xmlFile.getParentDirectory(), false, copied, names);
    // Sources renamed (destination differs from their own file name) or sharing a file name with another external source are
    // relativised on the serialised copy only; the live source stays absolute.
    // patchSnapshot only ever touches this serialised copy, never the live document (the audio thread reads it).
    auto snapshot = vt.createCopy();
    relativizeSamplePaths(snapshot, xmlFile.getParentDirectory(), true, copied, names);
    if (patchSnapshot) patchSnapshot(snapshot);

    const auto xml = snapshot.toXmlString();
    if (xmlFile.loadFileAsString() != xml) {
        juce::TemporaryFile temporary(xmlFile);
        bool written = false;
        {
            // JUCE's buffered flush can discard a short write without setting
            // its status. Write directly so writeText reports incomplete data.
            auto output = temporary.getFile().createOutputStream(0);
            if (output && output->openedOk()) {
                written = output->writeText(xml, false, false, nullptr);
                output->flush();
                written = written && output->getStatus().wasOk();
            }
        }
        if (!written || !temporary.overwriteTargetFileWithTemporary())
            return failSave("Cannot write project: " + xmlFile.getFullPathName(), reason);
    }
    lastSaveError.clear(); dirty.store(false);
    if (onDirtyChanged) onDirtyChanged(false);
    return true;
}
bool KickXML::save(SaveReason reason) {
    bool result = false;
    onPersistenceThread([&] { result = saveOnMessageThread(reason); });
    return result;
}
bool KickXML::close() {
    bool result = false;
    onPersistenceThread([&] {
        if (!saveOnMessageThread(SaveReason::Close)) return;
        detachWithoutSaving(); result = true;
    });
    return result;
}
void KickXML::detachWithoutSaving() {
    timer.stopTimer(); cancelPendingUpdate(); autosaveDue.store(false);
    const juce::ScopedLock guard(lock);
    xmlFile = juce::File{}; xmlFileDir = juce::File{}; directoryText.clear();
}
} // namespace mt
