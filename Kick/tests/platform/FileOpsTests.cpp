// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the file operations (temp folders only).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t FileOpsTests.cpp
#include "../../modules/kick_ui/modes/filebrowser/kick_fileOps.h"
#include <cstdio>
#include <cstdlib>
#include <thread>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

namespace fs = std::filesystem;
using namespace kick_fileops;

static void write( const fs::path& p, size_t bytes, char fill = 'x' )
{
    fs::create_directories( p.parent_path() );
    std::ofstream o( p, std::ios::binary );
    std::string   s( bytes, fill );
    o.write( s.data(), static_cast<std::streamsize>( s.size() ) );
}
static std::string read( const fs::path& p )
{
    std::ifstream i( p, std::ios::binary );
    return std::string( std::istreambuf_iterator<char>( i ), {} );
}

int main()
{
    const fs::path root = fs::temp_directory_path() / ( "kick_fileops_" + std::to_string( std::rand() ) + "_" + std::to_string( (long)::getpid() ) );
    fs::remove_all( root );
    fs::create_directories( root );
    std::atomic<bool> noCancel{ false };

    // Copy a file: new name when it exists, original untouched, progress ends at 100.
    write( root / "a/kick.wav", 3 * ( 1 << 20 ) + 17, 'k' );
    fs::create_directories( root / "b" );
    write( root / "b/kick.wav", 5, 'o' );
    int  lastPct = -1;
    bool monotone = true;
    Job  j;
    j.kind    = Kind::Copy;
    j.sources = { ( root / "a/kick.wav" ).string() };
    j.destDir = ( root / "b" ).string();
    auto r    = run( j, noCancel, [&]( const Progress& p ) { if( p.percent() < lastPct ) monotone = false; lastPct = p.percent(); } );
    CHECK( r.ok && !r.cancelled && r.done.size() == 1 );
    CHECK( fs::path( r.done[0] ).filename() == "kick (2).wav" );
    CHECK( read( root / "b/kick.wav" ) == "ooooo" ); // nothing replaced
    CHECK( fs::file_size( root / "b/kick (2).wav" ) == 3 * ( 1 << 20 ) + 17 );
    CHECK( fs::exists( root / "a/kick.wav" ) );
    CHECK( monotone && lastPct == 100 );

    // Copy a folder tree.
    write( root / "src/p/one.txt", 10 );
    write( root / "src/p/sub/two.txt", 20 );
    fs::create_directories( root / "src/p/empty" );
    fs::create_directories( root / "dst" );
    j.sources = { ( root / "src/p" ).string() };
    j.destDir = ( root / "dst" ).string();
    r         = run( j, noCancel );
    CHECK( r.ok && fs::exists( root / "dst/p/sub/two.txt" ) && fs::is_directory( root / "dst/p/empty" ) );

    // A folder into itself is refused, nothing created.
    j.sources = { ( root / "src/p" ).string() };
    j.destDir = ( root / "src/p/sub" ).string();
    r         = run( j, noCancel );
    CHECK( !r.ok && r.failed.size() == 1 && !fs::exists( root / "src/p/sub/p" ) );

    // Cancel during a copy: the partial target is gone, the source stays, cancelled flag set.
    write( root / "big/huge.bin", 20 * ( 1 << 20 ) );
    fs::create_directories( root / "out" );
    std::atomic<bool> cancel{ false };
    j.sources = { ( root / "big/huge.bin" ).string() };
    j.destDir = ( root / "out" ).string();
    r         = run( j, cancel, [&]( const Progress& p ) { if( p.bytesDone >= ( 2u << 20 ) ) cancel.store( true ); } );
    CHECK( r.cancelled && !r.ok );
    CHECK( !fs::exists( root / "out/huge.bin" ) );
    CHECK( fs::exists( root / "big/huge.bin" ) );
    cancel.store( false );

    // Move: same file system = rename; protected sources are refused.
    write( root / "m/x.txt", 4 );
    fs::create_directories( root / "m2" );
    j.kind    = Kind::Move;
    j.sources = { ( root / "m/x.txt" ).string() };
    j.destDir = ( root / "m2" ).string();
    r         = run( j, noCancel );
    CHECK( r.ok && !fs::exists( root / "m/x.txt" ) && fs::exists( root / "m2/x.txt" ) );
    write( root / "m/keep.txt", 4 );
    j.sources = { ( root / "m/keep.txt" ).string() };
    r         = run( j, noCancel, {}, []( const std::string& p ) { return p.find( "keep" ) != std::string::npos; } );
    CHECK( !r.ok && r.failed.size() == 1 && fs::exists( root / "m/keep.txt" ) && !fs::exists( root / "m2/keep.txt" ) );
    // Move into the folder it is already in: nothing happens, no error.
    j.sources = { ( root / "m2/x.txt" ).string() };
    r         = run( j, noCancel );
    CHECK( r.ok && fs::exists( root / "m2/x.txt" ) && !fs::exists( root / "m2/x (2).txt" ) );

    // Delete: a tree, with progress; a missing source is a failure, others still run.
    write( root / "d/t/a.txt", 1000 );
    write( root / "d/t/s/b.txt", 1000 );
    j.kind    = Kind::Delete;
    j.sources = { ( root / "d/t" ).string(), ( root / "d/ghost" ).string() };
    j.destDir.clear();
    r = run( j, noCancel );
    CHECK( !r.ok && r.failed.size() == 1 && !fs::exists( root / "d/t" ) );
    // Delete protected.
    write( root / "d/p/a.txt", 1 );
    j.sources = { ( root / "d/p" ).string() };
    r         = run( j, noCancel, {}, []( const std::string& ) { return true; } );
    CHECK( !r.ok && fs::exists( root / "d/p/a.txt" ) );
    // Delete cancelled half way: what is left is still a consistent tree.
    for( int i = 0; i < 20; ++i )
        write( root / "c" / ( "f" + std::to_string( i ) ), 10 );
    cancel.store( false );
    int seen = 0;
    j.sources = { ( root / "c" ).string() };
    r         = run( j, cancel, [&]( const Progress& ) { if( ++seen == 6 ) cancel.store( true ); } );
    CHECK( r.cancelled && fs::exists( root / "c" ) );

    // copyFolderTo: refuses an existing target, copies, cleans up on cancel.
    write( root / "f/src/a", 8 );
    CHECK( !copyFolderTo( ( root / "f/src" ).string(), ( root / "f" ).string(), noCancel ).ok );
    CHECK( copyFolderTo( ( root / "f/src" ).string(), ( root / "f/copy" ).string(), noCancel ).ok && fs::exists( root / "f/copy/a" ) );
    CHECK( !copyFolderTo( ( root / "f/src" ).string(), ( root / "f/src/in" ).string(), noCancel ).ok );
    write( root / "g/src/big", 8 * ( 1 << 20 ) );
    cancel.store( false );
    auto rc = copyFolderTo( ( root / "g/src" ).string(), ( root / "g/dst" ).string(), cancel, [&]( const Progress& p ) { if( p.bytesDone > ( 1u << 20 ) ) cancel.store( true ); } );
    CHECK( rc.cancelled && !fs::exists( root / "g/dst" ) );

    // Symlinks are not followed.
    std::error_code ec;
    fs::create_directories( root / "l/real" );
    write( root / "l/real/f", 3 );
    fs::create_directory_symlink( root / "l/real", root / "l/real/loop", ec );
    fs::create_directories( root / "l/out" );
    j.kind    = Kind::Copy;
    j.sources = { ( root / "l/real" ).string() };
    j.destDir = ( root / "l/out" ).string();
    r         = run( j, noCancel );
    CHECK( r.ok && fs::exists( root / "l/out/real/f" ) && !fs::exists( root / "l/out/real/loop" ) );

    fs::remove_all( root );
    std::printf( g_fail ? "FAILED %d\n" : "PASS file ops\n", g_fail );
    return g_fail ? 1 : 0;
}
