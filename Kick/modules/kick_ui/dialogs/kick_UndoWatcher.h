// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include "kick_undo_watch.h"

namespace kick_ui
{
    // One watcher for every undo / redo, whichever way it happened (toolbar, the hardware UNDO button, a screen): it tells the user
    // what was undone ("Undid: Create track", a notification) and lets the screens refresh through undoRedoDone.
    class UndoWatcher : private juce::ChangeListener
    {
    public:
        static inline sigslot::signal<> undoRedoDone;

        UndoWatcher() : um( mt::getAudioInterface().getUndoManager() )
        {
            last = snapshot();
            um.addChangeListener( this );
        }
        ~UndoWatcher() override { um.removeChangeListener( this ); }

    private:
        juce::UndoManager&       um;
        undo_watch::Snapshot     last;

        undo_watch::Snapshot snapshot() const
        {
            undo_watch::Snapshot s;
            const auto           undoList = um.getUndoDescriptions();
            const auto           redoList = um.getRedoDescriptions();
            s.undoCount = undoList.size();
            s.redoCount = redoList.size();
            s.nextUndo  = um.getUndoDescription().toStdString();
            s.nextRedo  = um.getRedoDescription().toStdString();
            return s;
        }

        void changeListenerCallback( juce::ChangeBroadcaster* ) override
        {
            const auto now   = snapshot();
            const auto event = undo_watch::classify( last, now );
            last             = now;
            if( event.kind == undo_watch::Kind::Undid || event.kind == undo_watch::Kind::Redid )
            {
                mt::utils::showNotification( juce::String( undo_watch::message( event ) ) );
                undoRedoDone();
            }
        }
    };
}
