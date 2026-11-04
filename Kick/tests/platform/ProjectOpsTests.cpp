// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test of the project folder operations (Duplicate / Rename / Delete / Info) with temp folders.
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t ProjectOpsTests.cpp
#include "../../modules/kick_ui/recent_project/kick_project_ops.h"
#include <cstdio>
#include <cstdlib>
#include <unistd.h>

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

namespace fs = std::filesystem;
using namespace kick_project;

static void write( const fs::path& p, const std::string& text )
{
    fs::create_directories( p.parent_path() );
    std::ofstream o( p, std::ios::binary );
    o << text;
}
static std::string read( const fs::path& p )
{
    std::ifstream i( p, std::ios::binary );
    return std::string( std::istreambuf_iterator<char>( i ), {} );
}

int main()
{
    const fs::path root = fs::temp_directory_path() / ( "kick_prjops_" + std::to_string( std::rand() ) + "_" + std::to_string( (long)::getpid() ) );
    fs::remove_all( root );
    const auto projects = root / "projects";
    const std::string xml = "<ROOT><Project><PROGRAMS><PROGRAM id=\"1\"/><PROGRAM id=\"2\"/></PROGRAMS><SEQUENCES><SEQUENCE id=\"a\"/></SEQUENCES></Project></ROOT>";
    write( projects / "Beat/Beat.kickPrj", xml );
    write( projects / "Beat/Beat.kickPrj.bak", "old" );
    write( projects / "Beat/Beat_corrupt_20240101.kickPrj", "bad" );
    write( projects / "Beat/samples/kick.wav", std::string( 1000, 'k' ) );
    write( projects / "Beat/notes.txt", "n" );
    fs::create_directories( projects / "NoProject" );
    write( projects / "NoProject/readme.txt", "r" );

    std::atomic<bool> no{ false };

    // Info
    auto in = info( ( projects / "Beat" ).string() );
    CHECK( in.valid && in.name == "Beat" && in.programs == 2 && in.sequences == 1 && in.samples == 1 && in.bytes > 1000 && in.modifiedSec > 0 );
    CHECK( !info( ( projects / "NoProject" ).string() ).valid );
    CHECK( !info( ( projects / "ghost" ).string() ).valid );
    CHECK( formatBytes( 1536 ) == "1.5 KB" && formatBytes( 12 ) == "12 B" );

    // Duplicate
    std::string out;
    CHECK( duplicate( ( projects / "Beat" ).string(), projects.string(), "Beat 2", no, {}, &out ) == OpResult::Ok );
    CHECK( out == ( projects / "Beat 2" ).string() );
    CHECK( fs::exists( projects / "Beat 2/Beat 2.kickPrj" ) && !fs::exists( projects / "Beat 2/Beat.kickPrj" ) );
    CHECK( fs::exists( projects / "Beat 2/samples/kick.wav" ) && fs::exists( projects / "Beat 2/notes.txt" ) );
    CHECK( !fs::exists( projects / "Beat 2/Beat.kickPrj.bak" ) && !fs::exists( projects / "Beat 2/Beat_corrupt_20240101.kickPrj" ) );
    CHECK( read( projects / "Beat 2/Beat 2.kickPrj" ) == xml );
    CHECK( fs::exists( projects / "Beat/Beat.kickPrj" ) ); // original untouched
    CHECK( duplicate( ( projects / "Beat" ).string(), projects.string(), "Beat 2", no ) == OpResult::AlreadyExists );
    CHECK( duplicate( ( projects / "Beat" ).string(), projects.string(), "a/b", no ) == OpResult::BadName );
    CHECK( duplicate( ( projects / "Beat" ).string(), projects.string(), "  ", no ) == OpResult::BadName );
    CHECK( duplicate( ( projects / "NoProject" ).string(), projects.string(), "X", no ) == OpResult::NotAProject );
    CHECK( duplicate( ( projects / "ghost" ).string(), projects.string(), "X", no ) == OpResult::SourceMissing );
    std::atomic<bool> yes{ true };
    CHECK( duplicate( ( projects / "Beat" ).string(), projects.string(), "Cancelled", yes ) == OpResult::Cancelled );
    CHECK( !fs::exists( projects / "Cancelled" ) ); // no half project
    CHECK( suggestCopyName( "Beat", projects.string() ) == "Beat copy" );
    fs::create_directories( projects / "Beat copy" );
    CHECK( suggestCopyName( "Beat", projects.string() ) == "Beat copy 2" );

    // Rename
    auto prot = []( const std::string& p ) { return p.find( "Open" ) != std::string::npos; };
    CHECK( rename( ( projects / "Beat 2" ).string(), "Groove", prot, &out ) == OpResult::Ok );
    CHECK( out == ( projects / "Groove" ).string() && fs::exists( projects / "Groove/Groove.kickPrj" ) && !fs::exists( projects / "Beat 2" ) );
    CHECK( fs::exists( projects / "Groove/samples/kick.wav" ) );
    CHECK( rename( ( projects / "Groove" ).string(), "Beat", prot ) == OpResult::AlreadyExists ); // nothing is replaced
    CHECK( fs::exists( projects / "Groove/Groove.kickPrj" ) );
    CHECK( rename( ( projects / "Groove" ).string(), "a:b", prot ) == OpResult::BadName );
    CHECK( rename( ( projects / "NoProject" ).string(), "Y", prot ) == OpResult::NotAProject );
    write( projects / "Open1/Open1.kickPrj", xml );
    CHECK( rename( ( projects / "Open1" ).string(), "Closed", prot ) == OpResult::Protected && fs::exists( projects / "Open1/Open1.kickPrj" ) );
    CHECK( rename( ( projects / "Groove" ).string(), "Groove", prot ) == OpResult::Ok ); // same name: nothing to do

    // Delete
    CHECK( remove( ( projects / "NoProject" ).string(), prot, no ) == OpResult::NotAProject && fs::exists( projects / "NoProject/readme.txt" ) ); // never an ordinary folder
    CHECK( remove( ( projects / "Open1" ).string(), prot, no ) == OpResult::Protected && fs::exists( projects / "Open1" ) );
    CHECK( remove( ( projects / "Groove" ).string(), prot, no ) == OpResult::Ok && !fs::exists( projects / "Groove" ) );
    CHECK( remove( ( projects / "Groove" ).string(), prot, no ) == OpResult::SourceMissing );

    fs::remove_all( root );
    std::printf( g_fail ? "FAILED %d\n" : "PASS project ops\n", g_fail );
    return g_fail ? 1 : 0;
}
