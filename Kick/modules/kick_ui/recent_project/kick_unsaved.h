// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) "unsaved changes" gate: when does leaving the open project (switching, new project, loading, quitting) ask
// Save / Don't save / Cancel, and what happens on each answer. The prompt and the engine calls are injected (Gate::Env), so the
// decision table and the order of the steps are host-tested: Kick/tests/platform/UnsavedGateTests.cpp.
#pragma once

#include <functional>

namespace kick_unsaved
{
    enum class Choice { Save, DontSave, Cancel };

    struct State
    {
        bool projectLoaded{ false };
        bool dirty{ false };        // the engine's changed-since-last-save flag (any ValueTree / UndoManager change)
        bool autoSave{ false };     // the Preferences auto save switch
        bool readOnlyLocation{ false }; // a factory demo / template: Save is impossible, only Save As
    };

    // With auto save on, leaving saves silently (that is what the switch means); a demo / template can never be auto saved, so a
    // changed one always asks.
    inline bool needsPrompt( const State& s )
    {
        return s.projectLoaded && s.dirty && ( !s.autoSave || s.readOnlyLocation );
    }

    struct Env
    {
        std::function<State()>                          state;
        std::function<void( std::function<void( Choice )> )> ask;        // shows the three-way question, calls back once
        std::function<bool()>                           save;            // plain save of the open project; false = failed (already reported)
        std::function<void( std::function<void( bool )> )> saveAs;       // read-only project: Save As flow; calls back true when saved
        std::function<void()>                           discard;         // the next close of the project must not write it
    };

    // Runs `proceed` when leaving is allowed: nothing to save, saved, or the user chose not to save. Cancel / failed save: not.
    inline void leaveProject( const Env& env, std::function<void()> proceed )
    {
        const auto s = env.state();
        if( !needsPrompt( s ) )
        {
            proceed();
            return;
        }
        env.ask( [env, s, proceed]( Choice c ) {
            switch( c )
            {
                case Choice::Cancel: return;
                case Choice::DontSave:
                    env.discard();
                    proceed();
                    return;
                case Choice::Save:
                    if( s.readOnlyLocation )
                    {
                        env.saveAs( [proceed]( bool saved ) {
                            if( saved )
                                proceed();
                        } );
                        return;
                    }
                    if( env.save() )
                        proceed();
                    return;
            }
        } );
    }
}
