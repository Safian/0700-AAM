// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the undo watcher classification (HDR-12): undo, redo, a new edit, several steps, no change, the messages.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t UndoWatchTests.cpp
#include "../../modules/kick_ui/dialogs/kick_undo_watch.h"
#include <cstdio>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_ui::undo_watch;

int main()
{
    const Snapshot s0{ 2, 0, "Create track", "" };     // two steps, nothing to redo
    const Snapshot s1{ 1, 1, "Create sequence", "Create track" }; // one undo
    const Snapshot s2{ 0, 2, "", "Create sequence" };  // two undos

    CHECK( classify( s0, s0 ).kind == Kind::None );
    {
        const auto e = classify( s0, s1 );
        CHECK( e.kind == Kind::Undid && e.name == "Create track" && message( e ) == "Undid: Create track" );
    }
    {
        const auto e = classify( s1, s0 );
        CHECK( e.kind == Kind::Redid && e.name == "Create track" && message( e ) == "Redid: Create track" );
    }
    CHECK( classify( s0, s2 ).kind == Kind::Changed ); // two at once (coalesced notifications)
    CHECK( classify( s2, s0 ).kind == Kind::Changed );
    CHECK( message( classify( s0, s2 ) ).empty() && message( classify( s0, s0 ) ).empty() );

    // A new edit while a redo step exists: the redo stack is gone, the undo stack grows - a redo only when the name is the pending one.
    {
        const Snapshot before{ 1, 1, "A", "B" };
        CHECK( classify( before, Snapshot{ 2, 0, "C", "" } ).kind == Kind::Changed );
        // (a new edit that happens to carry the very name of the pending redo step cannot be told from the redo: counts and names are equal)
        CHECK( classify( before, Snapshot{ 2, 0, "B", "" } ).kind == Kind::Redid );
    }
    // A new edit on an empty redo stack.
    CHECK( classify( s0, Snapshot{ 3, 0, "Rename", "" } ).kind == Kind::Changed );
    // Unnamed steps: the message is just the verb.
    {
        const Snapshot a{ 1, 0, "", "" }, b{ 0, 1, "", "" };
        CHECK( message( classify( a, b ) ) == "Undid" && message( classify( b, a ) ) == "Redid" );
    }
    // The same name twice: counts still tell undo from redo.
    {
        const Snapshot a{ 2, 0, "Record", "" }, b{ 1, 1, "Record", "Record" };
        CHECK( classify( a, b ).kind == Kind::Undid && classify( b, a ).kind == Kind::Redid );
    }

    if( g_fail == 0 ) std::printf( "UndoWatchTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
