// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the folder listing (hidden files, sorting, filter, search).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t FileListTests.cpp
#include "../../modules/kick_ui/modes/filebrowser/kick_fileList.h"
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <unistd.h>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

namespace fs = std::filesystem;
using namespace kick_filelist;

static void write( const fs::path& p, size_t bytes )
{
    fs::create_directories( p.parent_path() );
    std::ofstream o( p, std::ios::binary );
    o << std::string( bytes, 'x' );
}

int main()
{
    CHECK( naturalCompare( "Kick 2", "Kick 10" ) < 0 );
    CHECK( naturalCompare( "kick", "KICK" ) == 0 );
    CHECK( naturalCompare( "a01", "a1" ) == 0 );
    CHECK( naturalCompare( "a", "ab" ) < 0 );
    CHECK( naturalCompare( "b", "a" ) > 0 );
    CHECK( isHiddenOrSystem( ".DS_Store" ) && isHiddenOrSystem( "._x.wav" ) && isHiddenOrSystem( "System Volume Information" ) && isHiddenOrSystem( "$RECYCLE.BIN" ) );
    CHECK( !isHiddenOrSystem( "kick.wav" ) );
    CHECK( hasExtension( "a.WAV", ".wav;bwf" ) && hasExtension( "a.bwf", "wav;bwf" ) && !hasExtension( "a.mid", ".wav;bwf" ) && !hasExtension( "wav", ".wav" ) );

    const fs::path root = fs::temp_directory_path() / ( "kick_filelist_" + std::to_string( std::rand() ) + "_" + std::to_string( (long)::getpid() ) );
    fs::remove_all( root );
    write( root / "zeta.wav", 10 );
    write( root / "Alpha 10.wav", 300 );
    write( root / "Alpha 2.wav", 200 );
    write( root / "notes.txt", 5 );
    write( root / ".hidden.wav", 1 );
    write( root / "._junk.wav", 1 );
    write( root / "Beta/in.wav", 1 );
    write( root / "alpha dir/x", 1 );
    fs::create_directories( root / "System Volume Information" );

    auto l = list( root.string() );
    CHECK( l.ok && l.entries.size() == 6 );
    CHECK( l.entries[0].isDir && l.entries[0].name == "alpha dir" && l.entries[1].name == "Beta" ); // folders first, no case
    CHECK( l.entries[2].name == "Alpha 2.wav" && l.entries[3].name == "Alpha 10.wav" ); // natural order
    ListOptions all;
    all.hideSystem = false;
    CHECK( list( root.string(), all ).entries.size() == 9 );

    ListOptions o;
    o.extensions = ".wav";
    l            = list( root.string(), o );
    CHECK( l.entries.size() == 5 ); // 2 folders + 3 wav, notes.txt gone, folders stay
    o.key = SortKey::Size;
    o.ascending = false;
    l = list( root.string(), o );
    CHECK( l.entries[2].name == "Alpha 10.wav" && l.entries[4].name == "zeta.wav" );
    o.key = SortKey::Date;
    o.ascending = true;
    CHECK( list( root.string(), o ).entries.size() == 5 );

    CHECK( !list( ( root / "nope" ).string() ).ok );

    std::atomic<bool> no{ false };
    auto s = search( root.string(), "alpha", false, 100, no );
    CHECK( s.entries.size() == 3 && !s.truncated );
    s = search( root.string(), "IN", true, 100, no );
    CHECK( s.entries.size() == 1 && s.entries[0].name == "in.wav" ); // below Beta; the hidden System Volume Information is skipped
    s = search( root.string(), "wav", true, 2, no );
    CHECK( s.entries.size() == 2 && s.truncated );
    std::atomic<bool> yes{ true };
    CHECK( search( root.string(), "a", true, 100, yes ).cancelled );
    s = search( root.string(), "x", true, 100, no );
    CHECK( s.entries.size() == 2 ); // notes.txt and alpha dir/x
    // never into a hidden folder
    write( root / ".git/secret.wav", 1 );
    CHECK( search( root.string(), "secret", true, 100, no ).entries.empty() );

    fs::remove_all( root );
    std::printf( g_fail ? "FAILED %d\n" : "PASS file list\n", g_fail );
    return g_fail ? 1 : 0;
}
