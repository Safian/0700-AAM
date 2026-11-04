// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the shutdown / restart sequence (order of the questions, recording / unsaved / transport conditions, Cancel, failures).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t PowerLogicTests.cpp
// The platform commands are mocks: nothing here powers anything off.
#include "../../modules/kick_ui/dialogs/kick_power_logic.h"
#include "../../modules/kick_ui/recent_project/kick_unsaved.h"
#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_power;

struct Fake
{
    Status                  st;
    kick_unsaved::State     us;
    bool                    recordingOk{ true };
    kick_unsaved::Choice    answer{ kick_unsaved::Choice::Save };
    bool                    saveOk{ true }, closeOk{ true };
    int                     powerRc{ 0 }, rebootRc{ 0 };
    std::string             trace;
    std::string             reported;
    bool                    deferred{ false }; // hold the unsaved answer until release()
    std::function<void()>   pending;
    Runner                  runner;

    Env env()
    {
        Env e;
        e.status           = [this] { return st; };
        e.confirmRecording = [this]( Action, std::function<void( bool )> cb ) { trace += "askRec,"; cb( recordingOk ); };
        e.stopTransport    = [this] { trace += "stop,"; };
        e.leaveProject     = [this]( std::function<void()> proceed, std::function<void()> cancelled ) {
            kick_unsaved::Env u;
            u.state   = [this] { return us; };
            u.ask     = [this]( std::function<void( kick_unsaved::Choice )> cb ) {
                trace += "askUnsaved,";
                if( deferred ) { pending = [this, cb] { cb( answer ); }; return; }
                cb( answer );
            };
            u.save    = [this] { trace += "save,"; return saveOk; };
            u.saveAs  = []( std::function<void( bool )> cb ) { cb( true ); };
            u.discard = [this] { trace += "discard,"; };
            kick_unsaved::leaveProject( u, proceed, cancelled );
        };
        e.closeProject = [this] { trace += "close,"; return closeOk; };
        e.report       = [this]( const std::string& m ) { reported = m; };
        e.flushLogs    = [this] { trace += "flush,"; };
        e.syncDisk     = [this] { trace += "sync,"; };
        e.powerOff     = [this] { trace += "poweroff,"; return powerRc; };
        e.reboot       = [this] { trace += "reboot,"; return rebootRc; };
        e.log          = []( const char* ) {};
        return e;
    }
    bool   called{ false }; // the result callback of the last run() (it may come later when a question is held)
    Result last{ Result::Busy };
    Result run( Action a )
    {
        trace.clear();
        reported.clear();
        called = false;
        last   = Result::Busy;
        runner.run( env(), a, [this]( Result x ) { last = x; called = true; } );
        return last; // Busy while a question is still held (called == false)
    }
};

int main()
{
    // 1. Clean project, nothing running: stop -> close -> flush -> sync -> poweroff (no question).
    {
        Fake f;
        f.st = { true, false, false };
        f.us = { true, false, false, false };
        CHECK( f.run( Action::ShutDown ) == Result::Started );
        CHECK( f.trace == "stop,close,flush,sync,poweroff," );
    }
    // 2. Restart goes the same way ending in reboot.
    {
        Fake f;
        f.st = { true, false, true };
        f.us = { true, false, false, false };
        CHECK( f.run( Action::Restart ) == Result::Started );
        CHECK( f.trace == "stop,close,flush,sync,reboot," );
    }
    // 3. No project: no gate, no close, still flush / sync / poweroff.
    {
        Fake f;
        f.st = { false, false, false };
        CHECK( f.run( Action::ShutDown ) == Result::Started );
        CHECK( f.trace == "stop,flush,sync,poweroff," );
    }
    // 4. Unsaved changes, Save: the question comes after the transport stop and before the close.
    {
        Fake f;
        f.st = { true, false, true };
        f.us = { true, true, false, false };
        f.answer = kick_unsaved::Choice::Save;
        CHECK( f.run( Action::ShutDown ) == Result::Started );
        CHECK( f.trace == "stop,askUnsaved,save,close,flush,sync,poweroff," );
    }
    // 5. Don't save: the discard is armed before the close.
    {
        Fake f;
        f.st = { true, false, false };
        f.us = { true, true, false, false };
        f.answer = kick_unsaved::Choice::DontSave;
        CHECK( f.run( Action::ShutDown ) == Result::Started );
        CHECK( f.trace == "stop,askUnsaved,discard,close,flush,sync,poweroff," );
    }
    // 6. Cancel at the unsaved question: nothing is closed / synced / powered off, the runner is free again.
    {
        Fake f;
        f.st = { true, false, true };
        f.us = { true, true, false, false };
        f.answer = kick_unsaved::Choice::Cancel;
        CHECK( f.run( Action::ShutDown ) == Result::Cancelled );
        CHECK( f.trace == "stop,askUnsaved," );
        CHECK( !f.runner.busy() );
        f.answer = kick_unsaved::Choice::DontSave;
        CHECK( f.run( Action::ShutDown ) == Result::Started ); // a new request works
    }
    // 7. Auto save on: no question, the close saves.
    {
        Fake f;
        f.st = { true, false, false };
        f.us = { true, true, true, false };
        CHECK( f.run( Action::ShutDown ) == Result::Started );
        CHECK( f.trace == "stop,close,flush,sync,poweroff," );
    }
    // 8. A demo with changes always asks; Save means Save As (here it succeeds).
    {
        Fake f;
        f.st = { true, false, false };
        f.us = { true, true, true, true };
        CHECK( f.run( Action::ShutDown ) == Result::Started );
        CHECK( f.trace.find( "askUnsaved," ) != std::string::npos );
    }
    // 9. Recording: asked first, before anything is stopped; "no" leaves everything running.
    {
        Fake f;
        f.st = { true, true, true };
        f.us = { true, true, false, false };
        f.recordingOk = false;
        CHECK( f.run( Action::ShutDown ) == Result::Cancelled );
        CHECK( f.trace == "askRec," );
        CHECK( !f.runner.busy() );
    }
    // 10. Recording confirmed: recording question -> stop -> unsaved question -> save -> close -> poweroff.
    {
        Fake f;
        f.st = { true, true, true };
        f.us = { true, true, false, false };
        CHECK( f.run( Action::ShutDown ) == Result::Started );
        CHECK( f.trace == "askRec,stop,askUnsaved,save,close,flush,sync,poweroff," );
    }
    // 11. Save fails: the gate cancels, the device stays up.
    {
        Fake f;
        f.st = { true, false, false };
        f.us = { true, true, false, false };
        f.saveOk = false;
        CHECK( f.run( Action::ShutDown ) == Result::Cancelled );
        CHECK( f.trace == "stop,askUnsaved,save," );
    }
    // 12. closeProject fails (auto save could not write): reported, no sync / poweroff.
    {
        Fake f;
        f.st = { true, false, false };
        f.us = { true, true, true, false };
        f.closeOk = false;
        CHECK( f.run( Action::ShutDown ) == Result::Failed );
        CHECK( f.trace == "stop,close," );
        CHECK( !f.reported.empty() );
        CHECK( !f.runner.busy() );
    }
    // 13. The platform command fails: reported, runner free (the project is closed already).
    {
        Fake f;
        f.st = { true, false, false };
        f.us = { true, false, false, false };
        f.powerRc = -1;
        CHECK( f.run( Action::ShutDown ) == Result::Failed );
        CHECK( f.trace == "stop,close,flush,sync,poweroff," && !f.reported.empty() && !f.runner.busy() );
        f.rebootRc = 3;
        CHECK( f.run( Action::Restart ) == Result::Failed );
    }
    // 14. A second request while the question is open (and after Started) is refused.
    {
        Fake f;
        f.st = { true, false, false };
        f.us = { true, true, false, false };
        f.deferred = true;
        f.run( Action::ShutDown );
        CHECK( !f.called && f.runner.busy() ); // the question is on screen: no result yet
        Result second = Result::Started;
        f.runner.run( f.env(), Action::Restart, [&]( Result r ) { second = r; } );
        CHECK( second == Result::Busy );
        Result first = Result::Busy;
        // Answer the pending question: a Started request stays busy for ever (the device is going down).
        f.deferred = false;
        f.pending();
        CHECK( f.called && f.last == Result::Started );
        CHECK( f.runner.busy() );
        f.runner.run( f.env(), Action::ShutDown, [&]( Result r ) { first = r; } );
        CHECK( first == Result::Busy );
    }
    if( g_fail == 0 ) std::printf( "PowerLogicTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
