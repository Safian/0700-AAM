// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the unsaved-changes gate (decision table + order of the steps).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t UnsavedGateTests.cpp
#include "../../modules/kick_ui/recent_project/kick_unsaved.h"
#include <cstdio>
#include <string>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_unsaved;

struct Fake
{
    State       st;
    Choice      answer{ Choice::Cancel };
    bool        saveOk{ true }, saveAsOk{ true };
    std::string log;
    Env         env()
    {
        Env e;
        e.state  = [this] { return st; };
        e.ask    = [this]( std::function<void( Choice )> cb ) { log += "ask,"; cb( answer ); };
        e.save   = [this] { log += "save,"; return saveOk; };
        e.saveAs = [this]( std::function<void( bool )> cb ) { log += "saveAs,"; cb( saveAsOk ); };
        e.discard = [this] { log += "discard,"; };
        return e;
    }
    std::string run()
    {
        log.clear();
        leaveProject( env(), [this] { log += "proceed"; }, [this] { log += "cancelled"; } );
        return log;
    }
};

int main()
{
    // decision table
    CHECK( !needsPrompt( { false, true, false, false } ) );  // no project
    CHECK( !needsPrompt( { true, false, false, false } ) );  // nothing changed
    CHECK( needsPrompt( { true, true, false, false } ) );    // changed, auto save off
    CHECK( !needsPrompt( { true, true, true, false } ) );    // changed, auto save on: saved silently
    CHECK( needsPrompt( { true, true, true, true } ) );      // a demo can not be auto saved
    CHECK( needsPrompt( { true, true, false, true } ) );

    Fake f;
    f.st = { true, false, false, false };
    CHECK( f.run() == "proceed" );
    f.st = { true, true, true, false };
    CHECK( f.run() == "proceed" );

    f.st = { true, true, false, false };
    f.answer = Choice::Cancel;
    CHECK( f.run() == "ask,cancelled" ); // nothing happens, the project stays open
    f.answer = Choice::DontSave;
    CHECK( f.run() == "ask,discard,proceed" );
    f.answer = Choice::Save;
    CHECK( f.run() == "ask,save,proceed" );
    f.saveOk = false;
    CHECK( f.run() == "ask,save,cancelled" ); // a failed save keeps the project open
    f.saveOk = true;

    // demo / template: Save means Save As
    f.st = { true, true, true, true };
    f.answer = Choice::Save;
    CHECK( f.run() == "ask,saveAs,proceed" );
    f.saveAsOk = false;
    CHECK( f.run() == "ask,saveAs,cancelled" );
    f.answer = Choice::DontSave;
    CHECK( f.run() == "ask,discard,proceed" );

    std::printf( g_fail ? "FAILED %d\n" : "PASS unsaved gate\n", g_fail );
    return g_fail ? 1 : 0;
}
