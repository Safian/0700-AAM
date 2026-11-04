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

        // Also report changes of the track's automation lanes (List Edit shows their breakpoints). Set before enabling.
        void setWatchAutomation( bool Watch ) { watchAutomation = Watch; }

        // Re-bind to the (possibly different) selected track's note list.
        void rebind()
        {
            if( enabled )
                attach();
        }

    private:
        // With automation the listener sits on the track's tree (it also hears the note list below it and the AUTOLANE
        // children); otherwise on the note list only, as before.
        juce::ValueTree listenTree() const { return watchAutomation && watched.isValid() && watched.getParent().isValid() ? watched.getParent() : watched; }

        void attach()
        {
            auto list = ae.isProjectLoaded() ? ae.getNoteList() : juce::ValueTree{};
            if( list == watched && ( !watchAutomation || listening == listenTree() ) )
                return;
            detach();
            watched = list;
            listening = listenTree();
            if( listening.isValid() )
                listening.addListener( this );
        }

        void detach()
        {
            if( listening.isValid() )
                listening.removeListener( this );
            listening = juce::ValueTree{};
            watched   = juce::ValueTree{};
        }

        // A change in the note list, or (when asked for) in an automation lane of the track.
        bool relevant( const juce::ValueTree& T, const juce::Identifier* Property ) const
        {
            if( !watchAutomation )
                return true;
            if( watched.isValid() && ( T == watched || T.isAChildOf( watched ) ) )
                return true;
            return T.hasType( mt::IDs::AUTOLANE ) && ( Property == nullptr || *Property == mt::IDs::autoPoints || *Property == mt::IDs::autoProgram );
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
        void valueTreePropertyChanged( juce::ValueTree& T, const juce::Identifier& P ) override
        {
            if( relevant( T, &P ) )
                triggerAsyncUpdate();
        }
        void valueTreeChildAdded( juce::ValueTree& Parent, juce::ValueTree& Child ) override
        {
            if( relevant( Parent, nullptr ) || ( watchAutomation && Child.hasType( mt::IDs::AUTOLANE ) ) )
                triggerAsyncUpdate();
        }
        void valueTreeChildRemoved( juce::ValueTree& Parent, juce::ValueTree& Child, int ) override
        {
            if( relevant( Parent, nullptr ) || ( watchAutomation && Child.hasType( mt::IDs::AUTOLANE ) ) )
                triggerAsyncUpdate();
        }
        void valueTreeChildOrderChanged( juce::ValueTree& Parent, int, int ) override
        {
            if( relevant( Parent, nullptr ) )
                triggerAsyncUpdate();
        }

        // MTAudioInterfaceListener: another track / sequence became the selected one.
        void onTrackSelectionChange( const mt::MTSelected& ) override { triggerAsyncUpdate(); }
        void onSequenceSelectionChange( const mt::MTSelected& ) override { triggerAsyncUpdate(); }

        mt::MTAudioInterface& ae;
        juce::ValueTree       watched;   // the note list
        juce::ValueTree       listening; // the tree the listener is registered on
        bool                  enabled{ false };
        bool                  watchAutomation{ false };

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR( NoteListWatcher )
    };
}
