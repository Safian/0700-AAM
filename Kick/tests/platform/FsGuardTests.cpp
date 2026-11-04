// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the file browser safety rules.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t FsGuardTests.cpp
#include "../../modules/kick_ui/modes/filebrowser/kick_fsGuard.h"
#include <cstdio>
#include <set>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

using namespace kick_fsguard;

int main()
{
    // normalize
    CHECK( normalize( "/data//projects/" ) == "/data/projects" );
    CHECK( normalize( "/data/./projects/../sounds" ) == "/data/sounds" );
    CHECK( normalize( "/" ) == "/" );
    CHECK( normalize( "/.." ) == "/" );
    CHECK( normalize( "/a/b/../../.." ) == "/" );
    CHECK( normalize( "" ) == "." );
    CHECK( normalize( "a/../b" ) == "b" );
    CHECK( normalize( "../x" ) == "../x" );

    // isInside is component-wise.
    CHECK( isInside( "/data/projects/a", "/data/projects" ) );
    CHECK( isInside( "/data/projects", "/data/projects/" ) );
    CHECK( !isInside( "/data/projects2", "/data/projects" ) );
    CHECK( !isInside( "/data", "/data/projects" ) );
    CHECK( isInside( "/data/projects/../projects/x", "/data/projects" ) );
    CHECK( !isInside( "/data/projects/../x", "/data/projects" ) );
    CHECK( isInside( "/anything", "/" ) );

    // Protected paths.
    for( const char* p : { "/", "/data", "/data/", "/data/projects", "/data/sounds", "/data/demos", "/sdcard", "/media", "/usr", "/x", "/data/../data" } )
        CHECK( isProtected( p, "" ) );
    CHECK( !isProtected( "/data/projects/Beat", "" ) );
    CHECK( !isProtected( "/data/sounds/kick.wav", "" ) );
    CHECK( !isProtected( "/sdcard/my samples", "" ) );
    CHECK( !isProtected( "/media/Pen Drive/old", "" ) );
    CHECK( isProtected( "/media/Pen Drive", "", { "/media/Pen Drive" } ) ); // a Place root
    CHECK( isProtected( "/home/user/Splice/", "", { "/home/user/Splice" } ) );
    CHECK( isProtected( "", "" ) );
    // The open project: itself, its folder, ancestors, everything inside, by file or folder path.
    const std::string cur = "/data/projects/Beat/Beat.kickPrj";
    CHECK( isProtected( "/data/projects/Beat", cur ) );
    CHECK( isProtected( "/data/projects/Beat/Beat.kickPrj", cur ) );
    CHECK( isProtected( "/data/projects/Beat/samples", cur ) );
    CHECK( isProtected( "/data/projects/Beat/samples/kick.wav", cur ) );
    CHECK( isProtected( "/data/projects", cur ) );
    CHECK( isProtected( "/data", cur ) );
    CHECK( !isProtected( "/data/projects/Other", cur ) );
    CHECK( !isProtected( "/data/projects/Beat2", cur ) );
    CHECK( isProtected( "/data/projects/Beat", "/data/projects/Beat" ) ); // project given as a folder
    CHECK( isProtected( "/data/projects/Beat/x", "/data/projects/Beat" ) );
    CHECK( !isProtected( "/data/projects/Other", "/data/projects/Beat" ) );
    // The project lives on a drive.
    CHECK( isProtected( "/sdcard/songs/A", "/sdcard/songs/A/A.kickPrj" ) );
    CHECK( !isProtected( "/sdcard/songs/B", "/sdcard/songs/A/A.kickPrj" ) );

    // Paste into itself.
    CHECK( pasteIntoItself( "/sdcard/a", "/sdcard/a" ) );
    CHECK( pasteIntoItself( "/sdcard/a", "/sdcard/a/b/c" ) );
    CHECK( !pasteIntoItself( "/sdcard/a", "/sdcard/ab" ) );
    CHECK( !pasteIntoItself( "/sdcard/a", "/sdcard" ) );
    CHECK( !pasteIntoItself( "/sdcard/a/b", "/sdcard/a" ) );

    // Unique names.
    {
        std::set<std::string> have = { "a.wav", "a (2).wav", "b", "c.tar.gz", ".hidden" };
        auto ex = [&]( const std::string& n ) { return have.count( n ) > 0; };
        CHECK( uniqueName( "free.wav", ex ) == "free.wav" );
        CHECK( uniqueName( "a.wav", ex ) == "a (3).wav" );
        CHECK( uniqueName( "b", ex ) == "b (2)" );
        CHECK( uniqueName( "c.tar.gz", ex ) == "c.tar (2).gz" );
        CHECK( uniqueName( ".hidden", ex ) == ".hidden (2)" );
        auto always = []( const std::string& ) { return true; };
        CHECK( uniqueName( "x", always ).empty() ); // nothing free: refused
    }

    if( g_fail == 0 ) std::printf( "FsGuardTests: all passed\n" );
    return g_fail == 0 ? 0 : 1;
}
