// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free) shutdown / restart sequence of the device, the equivalent of the factory MPC "Power" window:
//   recording question -> transport / recording / looper stop -> unsaved-changes question (Save / Don't save / Cancel)
//   -> project close (saves) -> log flush -> sync -> poweroff / reboot.
// Every step is injected (Env), so the order, the questions and what Cancel / a failure leaves behind are host-tested:
// Kick/tests/platform/PowerLogicTests.cpp. "Restart Kick" (application only) is deliberately not an action: nothing supervises
// Kick on the device, an exit would drop to the stock MPC / a dead screen (see the R-12 report).
#pragma once

#include <functional>
#include <memory>
#include <string>

namespace kick_power
{
    enum class Action { ShutDown, Restart };

    inline const char* actionName( Action a ) { return a == Action::ShutDown ? "shut down" : "restart"; }

    struct Status
    {
        bool projectLoaded{ false };
        bool recording{ false }; // sequence / overdub take, looper, or sampler recorder running
        bool transportRunning{ false }; // sequence / song playing (informational: it is stopped without a question)
    };

    enum class Result
    {
        Started,   // the platform command was issued (the device goes down); nothing else must touch the project
        Cancelled, // the user said no at a question: nothing but the transport stop has happened
        Failed,    // the project could not be saved / the platform command failed: the user was told, the device stays up
        Busy       // another request is running
    };

    struct Env
    {
        std::function<Status()> status;
        // "Recording is running: stop it and <action>?" answered once; true = go on.
        std::function<void( Action, std::function<void( bool )> )> confirmRecording;
        // Stops sequence / song / looper / sampler recorder (a take that is running ends and is kept as any STOP does).
        std::function<void()> stopTransport;
        // The unsaved-changes gate of the open project (kick_unsaved::leaveProject): proceed = saved / nothing to save / "Don't save"
        // chosen (the discard is already armed), cancelled = Cancel or a failed save (already reported).
        std::function<void( std::function<void()> proceed, std::function<void()> cancelled )> leaveProject;
        std::function<bool()>                      closeProject; // false: could not persist, project stays open
        std::function<void( const std::string& )>  report;       // error dialog text
        std::function<void()>                      flushLogs;
        std::function<void()>                      syncDisk;
        std::function<int()>                       powerOff; // 0 = issued
        std::function<int()>                       reboot;
        std::function<void( const char* )>         log;
    };

    // One request at a time. `finished` is called exactly once with the result (possibly synchronously). The Runner must outlive
    // the request (static in the UI).
    class Runner
    {
    public:
        bool busy() const { return state_->busy; }

        void run( const Env& env, Action action, std::function<void( Result )> finished = {} )
        {
            if( state_->busy )
            {
                if( finished )
                    finished( Result::Busy );
                return;
            }
            state_->busy = true;
            auto state   = state_;
            auto done    = [state, finished]( Result r ) {
                if( r != Result::Started )
                    state->busy = false; // Started stays busy: the device is going down, a second request must not interfere
                if( finished )
                    finished( r );
            };
            const auto say = [env]( const std::string& m ) {
                if( env.log )
                    env.log( m.c_str() );
            };
            const Status st = env.status ? env.status() : Status{};
            say( std::string( "power: " ) + actionName( action ) + " requested" + ( st.recording ? ", recording running" : "" ) );

            auto afterQuestions = [env, action, done, say]() {
                // Close = save (unless "Don't save" armed the discard). A failure keeps the project and the device up.
                if( env.status && env.status().projectLoaded )
                {
                    if( env.closeProject && !env.closeProject() )
                    {
                        say( "power: the project could not be saved, staying up" );
                        if( env.report )
                            env.report( "The project could not be saved: the device stays on." );
                        done( Result::Failed );
                        return;
                    }
                }
                if( env.flushLogs )
                    env.flushLogs();
                say( std::string( "power: project closed, sync, " ) + actionName( action ) );
                if( env.syncDisk )
                    env.syncDisk();
                const int rc = action == Action::ShutDown ? ( env.powerOff ? env.powerOff() : -1 ) : ( env.reboot ? env.reboot() : -1 );
                if( rc != 0 )
                {
                    say( "power: the platform command failed" );
                    if( env.report )
                        env.report( std::string( "The device could not " ) + actionName( action ) + "." );
                    done( Result::Failed );
                    return;
                }
                done( Result::Started );
            };

            auto afterStop = [env, done, say, afterQuestions]() {
                if( env.stopTransport )
                    env.stopTransport();
                if( !env.leaveProject )
                {
                    afterQuestions();
                    return;
                }
                const bool loaded = env.status ? env.status().projectLoaded : false;
                if( !loaded )
                {
                    afterQuestions();
                    return;
                }
                env.leaveProject( afterQuestions, [done, say]() {
                    say( "power: cancelled at the unsaved-changes question" );
                    done( Result::Cancelled );
                } );
            };

            if( st.recording && env.confirmRecording )
            {
                env.confirmRecording( action, [afterStop, done, say]( bool ok ) {
                    if( !ok )
                    {
                        say( "power: cancelled at the recording question" );
                        done( Result::Cancelled );
                        return;
                    }
                    afterStop();
                } );
                return;
            }
            afterStop();
        }

    private:
        struct State { bool busy{ false }; };
        std::shared_ptr<State> state_{ std::make_shared<State>() };
    };
}
