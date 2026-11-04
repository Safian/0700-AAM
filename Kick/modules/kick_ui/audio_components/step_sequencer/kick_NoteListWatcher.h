/*
 * SPDX-License-Identifier: AGPL-3.0-or-later
 */
#pragma once

namespace kick_ui
{
    // "The note list of the selected track changed" signal for screens that show notes (Step Sequencer,
    // List Edit): undo/redo, live recording, edits made on another screen. It listens to the NOTELIST
    // ValueTree of the selected track, follows track/sequence selection changes and coalesces bursts
    // into one message-thread callback. ValueTree callbacks may come from any thread, so they only
    // trigger the async update. Nothing is polled.
    class NoteListWatcher : private juce::ValueTree::Listener
        , private juce::AsyncUpdater
        , private mt::MTAudioInterfaceListener
    {
    public:
        NoteListWatcher()
            : ae( mt::getAudioInterface() )
        {
            ae.addListener( this );
        }

        ~NoteListWatcher() override
        {
            cancelPendingUpdate();
            detach();
            ae.removeListener( this );
        }

        // Called on the message thread, at most once per burst, only while enabled.
        std::function<void()> onChanged;

        // Enable while the screen is shown; enabling attaches to the current track right away.
        void setEnabled( bool Enable )
        {
            if( enabled == Enable )
                return;
            enabled = Enable;
            if( enabled )
                attach();
            else
            {
                cancelPendingUpdate();
                detach();
            }
        }

        // Re-bind to the (possibly different) selected track's note list.
        void rebind()
        {
            if( enabled )
                attach();
        }

    private:
        void attach()
        {
            auto list = ae.isProjectLoaded() ? ae.getNoteList() : juce::ValueTree{};
            if( list == watched )
                return;
            detach();
            watched = list;
            if( watched.isValid() )
                watched.addListener( this );
        }

        void detach()
        {
            if( watched.isValid() )
                watched.removeListener( this );
            watched = juce::ValueTree{};
        }

        void handleAsyncUpdate() override
        {
            if( !enabled )
                return;
            attach(); // the selection may have moved to another track meanwhile
            if( onChanged )
                onChanged();
        }

        // ValueTree::Listener (any thread)
        void valueTreePropertyChanged( juce::ValueTree&, const juce::Identifier& ) override { triggerAsyncUpdate(); }
        void valueTreeChildAdded( juce::ValueTree&, juce::ValueTree& ) override { triggerAsyncUpdate(); }
        void valueTreeChildRemoved( juce::ValueTree&, juce::ValueTree&, int ) override { triggerAsyncUpdate(); }
        void valueTreeChildOrderChanged( juce::ValueTree&, int, int ) override { triggerAsyncUpdate(); }

        // MTAudioInterfaceListener: another track / sequence became the selected one.
        void onTrackSelectionChange( const mt::MTSelected& ) override { triggerAsyncUpdate(); }
        void onSequenceSelectionChange( const mt::MTSelected& ) override { triggerAsyncUpdate(); }

        mt::MTAudioInterface& ae;
        juce::ValueTree       watched;
        bool                  enabled{ false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( NoteListWatcher )
    };
}
