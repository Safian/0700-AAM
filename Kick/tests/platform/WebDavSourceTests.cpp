// SPDX-License-Identifier: AGPL-3.0-or-later
// Integration test of WebDavSource (JUCE URL/WebInputStream) against data/webdav_test_server.py on 127.0.0.1.
// Args: <path to data/>. Needs python3. Covers PROPFIND list/stat, Basic auth, GET, Range resume, server ignoring Range,
// same-origin redirect, cross-origin redirect refusal, cancel, https-less builds, secret handling.
#include <juce_core/juce_core.h>
#include "../../modules/kick_ui/modes/filebrowser/kick_webdavSource.h"

#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <csignal>
#include <unistd.h>

using namespace ui::fileSource;

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } else if( std::getenv( "KICK_TEST_VERBOSE" ) ) std::printf( "ok %d\n", __LINE__ ); } while( 0 )

struct Server
{
    FILE* pipe{ nullptr };
    int   port{ 0 };
    int   pid{ 0 };
    bool  start( const std::string& script, const std::string& root, bool ignoreRange )
    {
        std::string cmd = std::string( ignoreRange ? "IGNORE_RANGE=1 " : "" ) + "exec python3 -I '" + script + "' '" + root + "'";
        pipe            = popen( cmd.c_str(), "r" );
        if( !pipe ) return false;
        char line[64] = {};
        if( !fgets( line, sizeof line, pipe ) ) return false;
        return std::sscanf( line, "PORT %d PID %d", &port, &pid ) == 2;
    }
    ~Server()
    {
        if( pid > 0 ) kill( pid, SIGTERM ); // the server never exits on its own
        if( pipe ) pclose( pipe );
    }
};

static std::string readAll( const std::string& p ) { std::ifstream f( p, std::ios::binary ); return std::string( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() ); }

int main( int argc, char** argv )
{
    std::setvbuf( stdout, nullptr, _IONBF, 0 );
    const std::string data = argc > 1 ? argv[1] : "data";
    auto              tmp  = juce::File::createTempFile( "kickwd" );
    tmp.createDirectory();
    auto root = tmp.getChildFile( "root" );
    root.getChildFile( "Drums/Kick Loops" ).createDirectory();
    std::string big( 300000, 'x' );
    for( size_t i = 0; i < big.size(); ++i ) big[i] = (char)( i * 7 + ( i >> 8 ) );
    root.getChildFile( "Drums/Snare & \xC3\xA1.wav" ).replaceWithData( big.data(), big.size() );
    root.getChildFile( "small.wav" ).replaceWithText( "hello" );

    Server srv;
    if( !srv.start( data + "/webdav_test_server.py", root.getFullPathName().toStdString(), false ) ) { std::printf( "FAIL cannot start python server\n" ); return 1; }
    const std::string base = "http://127.0.0.1:" + std::to_string( srv.port ) + "/dav";
    auto              secretFile = tmp.getChildFile( "s.secret" ).getFullPathName().toStdString();

    auto neverCancel = []() { return false; };
    {
        // no credentials stored but a user configured: refused before any request
        WebDavSource noSecret( SourceConfig{ "webdav", "t1", "T", base, "kick", "basic" }, secretFile );
        CHECK( noSecret.state() == State::SignedOut );
        auto lr = noSecret.list( "/", neverCancel );
        CHECK( lr.result.status == Status::AuthFailed );

        // wrong password
        CHECK( writeSecretFile( secretFile, "wrong" ) );
        WebDavSource bad( SourceConfig{ "webdav", "t1", "T", base, "kick", "basic" }, secretFile );
        lr = bad.list( "/", neverCancel );
        CHECK( lr.result.status == Status::AuthFailed && lr.result.httpStatus == 401 );
        CHECK( bad.state() == State::NeedsAuth );
        AuthChallenge ch;
        CHECK( bad.beginAuth( ch, neverCancel ).status == Status::AuthFailed && ch.kind == AuthChallenge::Kind::Credentials );
    }
    CHECK( writeSecretFile( secretFile, "s3cret\n" ) );
    WebDavSource src( SourceConfig{ "webdav", "t1", "T", base, "kick", "basic" }, secretFile );
    CHECK( src.state() == State::Ready );
    AuthChallenge ch;
    CHECK( src.beginAuth( ch, neverCancel ).ok() && ch.kind == AuthChallenge::Kind::None );

    auto lr = src.list( "/", neverCancel );
    CHECK( lr.result.ok() && lr.entries.size() == 2 );
    if( lr.entries.size() == 2 ) CHECK( lr.entries[0].isDir && lr.entries[0].name == "Drums" && lr.entries[1].name == "small.wav" && lr.entries[1].size == 5 && lr.entries[1].mtimeMs > 0 && !lr.entries[1].etag.empty() );
    lr = src.list( "/Drums", neverCancel );
    CHECK( lr.result.ok() && lr.entries.size() == 2 );
    if( lr.entries.size() == 2 ) CHECK( lr.entries[0].name == "Kick Loops" && lr.entries[1].name == "Snare & \xC3\xA1.wav" && lr.entries[1].size == (int64_t)big.size() );
    CHECK( src.list( "/nope", neverCancel ).result.status == Status::NotFound );

    Entry e;
    CHECK( src.stat( "/Drums/Snare & \xC3\xA1.wav", e, neverCancel ).ok() && e.size == (int64_t)big.size() && !e.isDir && e.path == "/Drums/Snare & \xC3\xA1.wav" );
    CHECK( src.stat( "/Drums", e, neverCancel ).ok() && e.isDir );

    auto part = tmp.getChildFile( "a.part" ).getFullPathName().toStdString();
    auto fr   = src.fetch( "/Drums/Snare & \xC3\xA1.wav", part, 0, neverCancel, nullptr );
    CHECK( fr.result.ok() && fr.complete && fr.totalBytes == (int64_t)big.size() && readAll( part ) == big );

    // resume: first 100000 bytes on disk, Range continues
    { std::ofstream f( part, std::ios::binary | std::ios::trunc ); f.write( big.data(), 100000 ); }
    fr = src.fetch( "/Drums/Snare & \xC3\xA1.wav", part, 100000, neverCancel, nullptr );
    CHECK( fr.result.ok() && fr.complete && !fr.restarted && readAll( part ) == big );

    // offset == size: server answers 416 "bytes */N" => already complete, file untouched
    fr = src.fetch( "/Drums/Snare & \xC3\xA1.wav", part, (int64_t)big.size(), neverCancel, nullptr );
    CHECK( fr.result.ok() && fr.complete && readAll( part ) == big );

    // offset beyond the end: 416 mismatch => reported so the queue refetches from 0
    fr = src.fetch( "/Drums/Snare & \xC3\xA1.wav", part, (int64_t)big.size() + 10, neverCancel, nullptr );
    CHECK( !fr.result.ok() && fr.result.httpStatus == 416 );

    // cancel after the first progress callback: Cancelled, part file keeps what arrived
    int calls = 0;
    fr = src.fetch( "/Drums/Snare & \xC3\xA1.wav", part, 0, [&]() { return calls > 0; }, [&]( int64_t, int64_t ) { ++calls; } );
    CHECK( fr.result.status == Status::Cancelled && fr.bytesOnDisk > 0 && !fr.complete );

    CHECK( src.fetch( "/missing.wav", part, 0, neverCancel, nullptr ).result.status == Status::NotFound );

    // changes(): first call everything Added, then a new file appears
    auto c1 = src.changes( "/", neverCancel );
    CHECK( c1.result.ok() && c1.changes.size() == 2 );
    root.getChildFile( "new.wav" ).replaceWithText( "n" );
    auto c2 = src.changes( "/", neverCancel );
    CHECK( c2.result.ok() && c2.changes.size() == 1 && c2.changes[0].kind == Change::Kind::Added && c2.changes[0].entry.name == "new.wav" );

    // redirects: same origin followed, other origin refused (Authorization must not leak)
    {
        WebDavSource redir( SourceConfig{ "webdav", "t2", "R", "http://127.0.0.1:" + std::to_string( srv.port ) + "/redir", "kick", "basic" }, secretFile );
        auto r = redir.fetch( "/small.wav", part, 0, neverCancel, nullptr );
        CHECK( r.result.ok() && readAll( part ) == "hello" );
        WebDavSource evil( SourceConfig{ "webdav", "t3", "E", "http://127.0.0.1:" + std::to_string( srv.port ) + "/evil", "kick", "basic" }, secretFile );
        r = evil.fetch( "/small.wav", part, 0, neverCancel, nullptr );
        CHECK( r.result.status == Status::BadResponse );
    }

    // unreachable server
    {
        WebDavSource gone( SourceConfig{ "webdav", "t4", "G", "http://127.0.0.1:1/dav", "kick", "basic" }, secretFile );
        auto r = gone.list( "/", neverCancel );
        CHECK( r.result.status == Status::NetworkError );
        CHECK( gone.state() == State::Offline );
    }

    // invalid URL / https handling
    {
        WebDavSource inv( SourceConfig{ "webdav", "t5", "I", "not a url", "", "basic" }, secretFile );
        CHECK( inv.state() == State::Error && inv.list( "/", neverCancel ).result.status == Status::BadResponse );
    }

    // signOut deletes the secret file
    src.signOut();
    CHECK( !juce::File( secretFile ).existsAsFile() && src.state() == State::SignedOut );

    // server that ignores Range: full body comes back with 200, file is rewritten from 0 and flagged
    {
        Server srv2;
        if( !srv2.start( data + "/webdav_test_server.py", root.getFullPathName().toStdString(), true ) ) { std::printf( "FAIL cannot start second server\n" ); return 1; }
        writeSecretFile( secretFile, "s3cret" );
        WebDavSource s2( SourceConfig{ "webdav", "t6", "S", "http://127.0.0.1:" + std::to_string( srv2.port ) + "/dav", "kick", "basic" }, secretFile );
        { std::ofstream f( part, std::ios::binary | std::ios::trunc ); f.write( big.data(), 5000 ); }
        auto r = s2.fetch( "/Drums/Snare & \xC3\xA1.wav", part, 5000, neverCancel, nullptr );
        CHECK( r.result.ok() && r.complete && r.restarted && readAll( part ) == big );
    }

    tmp.deleteRecursively();
    std::printf( g_fail ? "WebDavSourceTests: %d FAILED\n" : "WebDavSourceTests: all passed\n", g_fail );
    return g_fail ? 1 : 0;
}
