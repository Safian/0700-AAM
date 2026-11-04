// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) classification of what happened to the undo stacks between two snapshots, so ONE watcher can announce
// "Undid: <step>" / "Redid: <step>" for every way an undo happens (toolbar, hardware UNDO button, a screen), and the screens can
// refresh. Host-tested in Kick/tests/platform/UndoWatchTests.cpp.
#pragma once

#include <string>

namespace kick_ui::undo_watch
{
    struct Snapshot
    {
        int         undoCount{ 0 }, redoCount{ 0 };
        std::string nextUndo, nextRedo; // descriptions of the step an undo / redo would take (may be empty)
        bool operator==( const Snapshot& o ) const { return undoCount == o.undoCount && redoCount == o.redoCount && nextUndo == o.nextUndo && nextRedo == o.nextRedo; }
    };

    enum class Kind { None, Undid, Redid, Changed };

    struct Event
    {
        Kind        kind{ Kind::None };
        std::string name; // the step that was undone / redone
    };

    // Undid: one step moved from the undo to the redo stack (and is its next redo). Redid: the other way round. Anything else that
    // differs (a new edit, several steps at once, a cleared history) is Changed.
    inline Event classify( const Snapshot& before, const Snapshot& after )
    {
        if( before == after )
            return {};
        if( after.undoCount == before.undoCount - 1 && after.redoCount == before.redoCount + 1 && after.nextRedo == before.nextUndo )
            return { Kind::Undid, before.nextUndo };
        if( after.undoCount == before.undoCount + 1 && after.redoCount == before.redoCount - 1 && after.nextUndo == before.nextRedo )
            return { Kind::Redid, before.nextRedo };
        return { Kind::Changed, {} };
    }

    // The text of the notification.
    inline std::string message( const Event& e )
    {
        if( e.kind != Kind::Undid && e.kind != Kind::Redid )
            return {};
        const std::string verb = e.kind == Kind::Undid ? "Undid" : "Redid";
        return e.name.empty() ? verb : verb + ": " + e.name;
    }
}
