// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for incoming shared/Open-In files (iOS Share Sheet / Inbox / Files) and Splice folder discovery.
#include "../../modules/kick_ui/modes/filebrowser/kick_incomingFiles.h"

#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <unistd.h>

namespace fs = std::filesystem;
using namespace kick_incoming;

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )

static std::string mkTmp()
{
    static int  seq = 0;
    std::string p   = "/tmp/kick_inc_test_" + std::to_string( ::getpid() ) + "_" + std::to_string( ++seq );
    fs::remove_all( p );
    fs::create_directories( p );
    return p;
}

static void writeFile( const std::string& p, const std::string& data )
{
    fs::create_directories( fs::path( p ).parent_path() );
    std::ofstream f( p, std::ios::binary | std::ios::trunc );
    f << data;
}

static std::string readFile( const std::string& p )
{
    std::ifstream f( p, std::ios::binary );
    return std::string( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
}

int main()
{
    // Classification & supported extensions
    CHECK( classifyExtension( "kick_loop.wav" ) == IncomingKind::Sample );
    CHECK( classifyExtension( "/var/Inbox/SNARE.WAV" ) == IncomingKind::Sample );
    CHECK( classifyExtension( "vox.m4a" ) == IncomingKind::Sample );
    CHECK( classifyExtension( "bass.flac" ) == IncomingKind::Sample );
    CHECK( classifyExtension( "pad.aiff" ) == IncomingKind::Sample );
    CHECK( classifyExtension( "arp.mid" ) == IncomingKind::Midi );
    CHECK( classifyExtension( "beat.kickPrj" ) == IncomingKind::Project );
    CHECK( classifyExtension( "readme.txt" ) == IncomingKind::Unsupported );
    CHECK( isSupportedFile( "hihat.ogg" ) );
    CHECK( !isSupportedFile( "notes.pdf" ) );

    // Sanitisation & collision-safe naming
    CHECK( sanitiseFileName( "../../etc/bad:name.wav" ) == "bad_name.wav" );
    {
        const auto root = mkTmp();
        const auto d    = root + "/samples";
        fs::create_directories( d );
        CHECK( uniqueDestinationPath( d, "loop.wav" ) == d + "/loop.wav" );
        writeFile( d + "/loop.wav", "v1" );
        CHECK( uniqueDestinationPath( d, "loop.wav" ) == d + "/loop (2).wav" );
        writeFile( d + "/loop (2).wav", "v2" );
        CHECK( uniqueDestinationPath( d, "loop.wav" ) == d + "/loop (3).wav" );
        fs::remove_all( root );
    }

    // Import copies sample to samplesDir & keeps source when removeSource=false
    {
        const auto   root    = mkTmp();
        const auto   src     = root + "/splice_export/124bpm_kick.wav";
        const auto   samples = root + "/Documents/Kick/sounds";
        const auto   proj    = root + "/Documents/Kick/projects";
        writeFile( src, "RIFF_WAV_DATA" );

        std::string  dest, err;
        IncomingKind kind = IncomingKind::Unsupported;
        CHECK( importIncomingFile( src, samples, proj, false, dest, kind, err ) );
        CHECK( kind == IncomingKind::Sample );
        CHECK( dest == samples + "/124bpm_kick.wav" );
        CHECK( readFile( dest ) == "RIFF_WAV_DATA" );
        CHECK( fs::exists( src ) );

        // Second import creates unique suffix instead of overwriting
        std::string dest2;
        CHECK( importIncomingFile( src, samples, proj, true, dest2, kind, err ) );
        CHECK( dest2 == samples + "/124bpm_kick (2).wav" );
        CHECK( !fs::exists( src ) ); // removed on success when move=true
        fs::remove_all( root );
    }

    // Project bundle / single .kickPrj import into projectsDir
    {
        const auto   root    = mkTmp();
        const auto   srcPrj  = root + "/inbox/MyBeat.kickPrj";
        const auto   samples = root + "/samples";
        const auto   proj    = root + "/projects";
        writeFile( srcPrj, "<kick/>" );
        std::string  dest, err;
        IncomingKind kind = IncomingKind::Unsupported;
        CHECK( importIncomingFile( srcPrj, samples, proj, true, dest, kind, err ) );
        CHECK( kind == IncomingKind::Project );
        CHECK( dest == proj + "/MyBeat/MyBeat.kickPrj" );
        CHECK( readFile( dest ) == "<kick/>" );
        CHECK( !fs::exists( srcPrj ) );
        fs::remove_all( root );
    }

    // Inbox directory sweep handles samples + MIDI and skips unsupported
    {
        const auto root    = mkTmp();
        const auto inbox   = root + "/Documents/Inbox";
        const auto samples = root + "/Documents/Kick/sounds";
        const auto proj    = root + "/Documents/Kick/projects";
        writeFile( inbox + "/Splice_Loop.wav", "wavdata" );
        writeFile( inbox + "/Chords.mid", "MThd" );
        writeFile( inbox + "/ignore.txt", "nope" );

        const auto sum = sweepInboxDirectory( inbox, samples, proj );
        CHECK( sum.importedPaths.size() == 2 );
        CHECK( sum.skippedUnsupported == 1 );
        CHECK( fs::exists( samples + "/Splice_Loop.wav" ) );
        CHECK( fs::exists( samples + "/Chords.mid" ) );
        CHECK( !fs::exists( inbox + "/Splice_Loop.wav" ) );
        fs::remove_all( root );
    }

    // Splice folder resolution priority (Documents/Splice, Documents/Kick/Splice, sounds/Splice, fallback)
    {
        const auto root       = mkTmp();
        const auto docs       = root + "/Documents";
        const auto kickDocs   = docs + "/Kick";
        const auto sounds     = kickDocs + "/sounds";
        const auto home       = root + "/Home";
        fs::create_directories( sounds + "/Splice" );
        fs::create_directories( home + "/Splice" );

        std::vector<std::string> cands = { docs, kickDocs, sounds, home };
        CHECK( resolveSpliceDir( "", cands, docs + "/Splice" ) == sounds + "/Splice" );

        fs::create_directories( docs + "/Splice" );
        CHECK( resolveSpliceDir( "", cands, "" ) == docs + "/Splice" );

        const auto custom = root + "/CustomSplice";
        fs::create_directories( custom );
        CHECK( resolveSpliceDir( custom, cands, "" ) == custom );
        fs::remove_all( root );
    }

    std::printf( g_fail == 0 ? "PASS IncomingFilesTests\n" : "FAIL (%d)\n", g_fail );
    return g_fail == 0 ? 0 : 1;
}
