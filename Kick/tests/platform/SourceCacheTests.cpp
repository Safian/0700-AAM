// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test: CacheIndex (LRU/limit/corrupt index), SourceCache (files, missing/corrupt handling), secret files (0600),
// source config round trip, LocalFolderSource, DownloadQueue (mock source: resume, cancel, restart, direct file).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t SourceCacheTests.cpp -lpthread && /tmp/t
#include "../../modules/kick_ui/modes/filebrowser/kick_downloadQueue.h"
#include "../../modules/kick_ui/modes/filebrowser/kick_localSource.h"
#include "../../modules/kick_ui/modes/filebrowser/kick_sourceConfig.h"

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <future>
#include <thread>

using namespace ui::fileSource;

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )
#define CHECK_EQ( a, b ) do { long long va = (long long)( a ), vb = (long long)( b ); if( va != vb ) { std::printf( "FAIL %s:%d: %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } } while( 0 )

static std::string mkTmp()
{
    char buf[] = "/tmp/kicksrcXXXXXX";
    return mkdtemp( buf );
}
static void writeFile( const std::string& p, const std::string& s ) { std::ofstream f( p, std::ios::binary | std::ios::trunc ); f << s; }
static std::string readAll( const std::string& p ) { std::ifstream f( p, std::ios::binary ); return std::string( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() ); }

// Mock remote with an in-memory file, honouring Range like a good server; can be told to stop after N bytes (network drop).
class MockSource : public FileBrowserSource
{
public:
    std::string content;
    std::string etag{ "\"e1\"" };
    bool        honourRange{ true };
    int64_t     dropAfter{ -1 };
    std::atomic<int> fetches{ 0 };
    std::atomic<int64_t> lastOffset{ -1 };
    std::atomic<bool> slow{ false };

    std::string id() const override { return "mock"; }
    std::string displayName() const override { return "Mock"; }
    State       state() const override { return State::Ready; }
    Result      beginAuth( AuthChallenge&, const CancelFn& ) override { return {}; }
    ListResult  list( const std::string&, const CancelFn& ) override { return {}; }
    Result      stat( const std::string&, Entry&, const CancelFn& ) override { return {}; }
    ChangeResult changes( const std::string&, const CancelFn& ) override { return {}; }
    void        signOut() override {}
    FetchResult fetch( const std::string&, const std::string& part, int64_t offset, const CancelFn& cancel, const ProgressFn& progress ) override
    {
        ++fetches;
        lastOffset = offset;
        FetchResult fr;
        fr.totalBytes = (int64_t)content.size();
        int64_t pos = offset;
        if( !honourRange && offset > 0 ) { pos = 0; fr.restarted = true; }
        std::ofstream out( part, pos > 0 ? std::ios::binary | std::ios::app : std::ios::binary | std::ios::trunc );
        while( pos < (int64_t)content.size() )
        {
            if( cancel() ) { out.flush(); fr.result = Result::make( Status::Cancelled ); fr.bytesOnDisk = pos; return fr; }
            if( dropAfter >= 0 && pos >= dropAfter ) { out.flush(); fr.result = Result::make( Status::NetworkError, "drop" ); fr.bytesOnDisk = pos; return fr; }
            int64_t n = std::min<int64_t>( 100, (int64_t)content.size() - pos );
            if( dropAfter >= 0 ) n = std::min<int64_t>( n, dropAfter - pos );
            out.write( content.data() + pos, n );
            pos += n;
            if( progress ) progress( pos, fr.totalBytes );
            if( slow ) std::this_thread::sleep_for( std::chrono::milliseconds( 2 ) );
        }
        out.flush();
        fr.complete = true;
        fr.bytesOnDisk = pos;
        return fr;
    }
    Entry entry() const { Entry e; e.path = "/f.wav"; e.name = "f.wav"; e.size = (int64_t)content.size(); e.etag = etag; e.mtimeMs = 7; return e; }
};

struct Waiter
{
    std::promise<std::pair<Result, std::string>> p;
    DownloadJob make( std::shared_ptr<FileBrowserSource> s, const Entry& e )
    {
        DownloadJob j;
        j.source = std::move( s );
        j.remote = e;
        j.onDone = [this]( uint64_t, const Result& r, const std::string& f ) { p.set_value( { r, f } ); };
        return j;
    }
};

int main()
{
    // --- index: LRU, pins, limit
    {
        CacheIndex ix;
        webdav::PartMeta v;
        ix.put( "a", 100, v ); ix.put( "b", 100, v ); ix.put( "c", 100, v );
        ix.touch( "a" );
        auto ev = ix.planEviction( 250 );
        CHECK_EQ( ev.size(), 1 );
        CHECK( ev.size() == 1 && ev[0] == "b" );
        ix.pin( "b" );
        ev = ix.planEviction( 250 );
        CHECK( ev.size() == 1 && ev[0] == "c" );
        ev = ix.planEviction( 50 );
        CHECK( ev.size() == 2 ); // b is pinned, a and c go
        ix.unpin( "b" );
        CHECK( ix.planEviction( 300 ).empty() );
        ev = ix.planEviction( 0, "a" );
        CHECK( std::find( ev.begin(), ev.end(), "a" ) == ev.end() );
        // serialize/parse round trip keeps LRU order
        CacheIndex iy;
        CHECK_EQ( iy.parse( ix.serialize() ), 0 );
        CHECK_EQ( iy.count(), 3 );
        CHECK_EQ( iy.totalSize(), 300 );
        auto e1 = ix.planEviction( 250 ), e2 = iy.planEviction( 250 );
        CHECK( e1 == e2 );
        // corrupt lines are skipped, wrong header gives empty index
        CacheIndex iz;
        CHECK_EQ( iz.parse( "kickcache1\nzz\t1\t\t1\t1\n0123456789abcdef\tNaN\t\t1\t1\n0123456789abcdef\t5\t\t-1\t0\nshort\n" ), 3 );
        CHECK_EQ( iz.count(), 1 );
        CHECK_EQ( iz.parse( "garbage\n" ), 0 );
        CHECK_EQ( iz.count(), 0 );
    }

    // --- cache files
    {
        auto dir = mkTmp() + "/cache";
        webdav::PartMeta v{ "\"x\"", 5, 1 };
        auto key = SourceCache::makeKey( "s", "/a.wav" );
        CHECK( key.size() == 16 && key != SourceCache::makeKey( "s", "/b.wav" ) && key != SourceCache::makeKey( "t", "/a.wav" ) );
        {
            SourceCache c( dir, 1000 );
            writeFile( c.partPath( key ), "12345" );
            CHECK( c.commit( key, v ) );
            std::string f;
            CHECK( c.lookup( key, v, f ) && readAll( f ) == "12345" );
            // stale remote version invalidates and deletes the file
            webdav::PartMeta v2 = v; v2.etag = "\"y\"";
            CHECK( !c.lookup( key, v2, f ) );
            CHECK_EQ( SourceCache::fileSize( c.dataPath( key ) ), -1 );
            // wrong-length part is never published
            writeFile( c.partPath( key ), "123" );
            CHECK( !c.commit( key, v ) );
            c.discardPart( key );
            // corrupt (truncated) cached file is detected and dropped
            writeFile( c.partPath( key ), "12345" );
            CHECK( c.commit( key, v ) );
            writeFile( c.dataPath( key ), "12" );
            CHECK( !c.lookup( key, v, f ) );
            CHECK_EQ( c.count(), 0 );
            // missing file is detected
            writeFile( c.partPath( key ), "12345" );
            CHECK( c.commit( key, v ) );
            ::unlink( c.dataPath( key ).c_str() );
            CHECK( !c.lookup( key, v, f ) );
            CHECK_EQ( c.count(), 0 );
            // size limit + LRU
            c.setLimit( 12 );
            for( int i = 0; i < 4; ++i )
            {
                auto k = SourceCache::makeKey( "s", "/f" + std::to_string( i ) );
                writeFile( c.partPath( k ), "12345" );
                CHECK( c.commit( k, v ) );
            }
            CHECK( c.totalSize() <= 12 );
            CHECK_EQ( c.count(), 2 );
            std::string f0;
            CHECK( !c.lookup( SourceCache::makeKey( "s", "/f0" ), v, f0 ) );
            CHECK( c.lookup( SourceCache::makeKey( "s", "/f3" ), v, f0 ) );
        }
        // index survives a restart
        SourceCache c2( dir, 12 );
        CHECK_EQ( c2.count(), 2 );
        // corrupt index file => starts empty, no crash
        writeFile( dir + "/index.txt", "\x01\x02garbage" );
        SourceCache c3( dir, 12 );
        CHECK_EQ( c3.count(), 0 );
    }

    // --- secret files / config
    {
        auto d = mkTmp();
        auto f = d + "/s.secret";
        CHECK( writeSecretFile( f, "pw" ) );
        struct stat st;
        CHECK( ::stat( f.c_str(), &st ) == 0 && ( st.st_mode & 0777 ) == 0600 );
        std::string out;
        CHECK( readSecretFile( f, out ) && out == "pw" );
        CHECK( writeSecretFile( f, "pw2" ) && readSecretFile( f, out ) && out == "pw2" );
        ::stat( f.c_str(), &st );
        CHECK( ( st.st_mode & 0777 ) == 0600 );
        ::chmod( f.c_str(), 0644 );
        CHECK( !readSecretFile( f, out ) ); // loosened permissions: refused
        deleteSecretFile( f );
        CHECK( !readSecretFile( f, out ) );

        std::vector<SourceConfig> v = { { "webdav", "nas", "My NAS\ttab", "https://nas.local/dav", "me@x", "basic" }, { "local", "splice", "Splice", "/Users/x/Splice", "", "basic" } };
        auto cf = d + "/file_sources.txt";
        CHECK( saveSources( cf, v ) );
        ::stat( cf.c_str(), &st );
        CHECK( ( st.st_mode & 0777 ) == 0600 );
        auto r = loadSources( cf );
        CHECK_EQ( r.size(), 2 );
        if( r.size() == 2 ) CHECK( r[0].name == "My NAS\ttab" && r[0].location == "https://nas.local/dav" && r[1].type == "local" );
        CHECK( parseSources( "kicksources1\nwebdav\tBAD ID\tn\thttp://h/\t\tbasic\nwebdav\tok\tn\tnot-a-url\t\tbasic\nx\n" ).empty() );
        CHECK( parseSources( "nope" ).empty() );
        CHECK( isValidSourceId( "nas-1_a" ) && !isValidSourceId( "../x" ) && !isValidSourceId( "" ) && !isValidSourceId( "A" ) );
    }

    // --- local source
    {
        auto d = mkTmp();
        ::mkdir( ( d + "/sub" ).c_str(), 0700 );
        writeFile( d + "/a.wav", "abcdef" );
        writeFile( d + "/sub/b.wav", "x" );
        LocalFolderSource src( "loc", "Local", d );
        CHECK( src.state() == State::Ready );
        auto lr = src.list( "/", nullptr );
        CHECK( lr.result.ok() && lr.entries.size() == 2 && lr.entries[0].isDir && lr.entries[1].name == "a.wav" && lr.entries[1].size == 6 );
        auto esc = src.list( "/../../../etc", nullptr ); // cannot leave the root: resolves to /etc under root => not found
        CHECK( esc.result.status == Status::NotFound );
        std::string direct;
        CHECK( src.directFile( "/a.wav", direct ) && direct == d + "/a.wav" );
        CHECK( !src.directFile( "/sub", direct ) );
        Entry en;
        CHECK( src.stat( "/sub/b.wav", en, nullptr ).ok() && en.size == 1 );
        CHECK( src.changes( "/", nullptr ).changes.size() == 2 );
        writeFile( d + "/c.wav", "z" );
        auto ch = src.changes( "/", nullptr );
        CHECK( ch.changes.size() == 1 && ch.changes[0].kind == Change::Kind::Added );
        auto part = d + "/p.part";
        auto fr = src.fetch( "/a.wav", part, 0, []() { return false; }, nullptr );
        CHECK( fr.complete && readAll( part ) == "abcdef" );
        writeFile( part, "abc" );
        fr = src.fetch( "/a.wav", part, 3, []() { return false; }, nullptr );
        CHECK( fr.complete && readAll( part ) == "abcdef" );
    }

    // --- download queue
    {
        auto dir = mkTmp() + "/cache";
        SourceCache cache( dir, 1 << 20 );
        DownloadQueue q( cache );
        auto src = std::make_shared<MockSource>();
        src->content = std::string( 1000, 'k' );
        for( size_t i = 0; i < src->content.size(); ++i ) src->content[i] = (char)( 'a' + i % 26 );

        // 1) interrupted download keeps .part, the next job resumes with Range from the part size
        src->dropAfter = 400;
        {
            Waiter w; auto fut = w.p.get_future();
            q.enqueue( w.make( src, src->entry() ) );
            auto r = fut.get();
            CHECK( r.first.status == Status::NetworkError );
        }
        auto key = SourceCache::makeKey( "mock", "/f.wav" );
        CHECK_EQ( SourceCache::fileSize( cache.partPath( key ) ), 400 );
        src->dropAfter = -1;
        {
            Waiter w; auto fut = w.p.get_future();
            q.enqueue( w.make( src, src->entry() ) );
            auto r = fut.get();
            CHECK( r.first.ok() );
            CHECK( src->lastOffset == 400 );
            CHECK( readAll( r.second ) == src->content );
        }
        // 2) second request is a cache hit: no fetch
        {
            int before = src->fetches;
            Waiter w; auto fut = w.p.get_future();
            q.enqueue( w.make( src, src->entry() ) );
            CHECK( fut.get().first.ok() );
            CHECK_EQ( src->fetches, before );
        }
        // 3) remote changed (new etag): refetch from 0, old data not reused
        {
            src->etag = "\"e2\""; src->content[0] = 'Z';
            Waiter w; auto fut = w.p.get_future();
            q.enqueue( w.make( src, src->entry() ) );
            auto r = fut.get();
            CHECK( r.first.ok() && src->lastOffset == 0 && readAll( r.second ) == src->content );
        }
        // 4) part file from an older version + changed remote => restart (not append to wrong data)
        {
            auto k2 = SourceCache::makeKey( "mock", "/g.wav" );
            writeFile( cache.partPath( k2 ), std::string( 300, '?' ) );
            Entry old = src->entry(); old.path = "/g.wav"; old.etag = "\"old\"";
            cache.savePartMeta( k2, webdav::partMetaFor( old ) );
            Entry now = src->entry(); now.path = "/g.wav";
            Waiter w; auto fut = w.p.get_future();
            q.enqueue( w.make( src, now ) );
            auto r = fut.get();
            CHECK( r.first.ok() && src->lastOffset == 0 && readAll( r.second ) == src->content );
        }
        // 5) server ignores Range: file is rewritten from 0 and is still correct
        {
            src->honourRange = false;
            auto k3 = SourceCache::makeKey( "mock", "/h.wav" );
            Entry e3 = src->entry(); e3.path = "/h.wav";
            writeFile( cache.partPath( k3 ), src->content.substr( 0, 250 ) );
            cache.savePartMeta( k3, webdav::partMetaFor( e3 ) );
            Waiter w; auto fut = w.p.get_future();
            q.enqueue( w.make( src, e3 ) );
            auto r = fut.get();
            CHECK( r.first.ok() && src->lastOffset == 250 && readAll( r.second ) == src->content );
            src->honourRange = true;
        }
        // 6) cancel a running job: stops, keeps the .part, reports Cancelled
        {
            src->slow = true;
            Entry e4 = src->entry(); e4.path = "/i.wav";
            Waiter w; auto fut = w.p.get_future();
            std::atomic<int64_t> prog{ 0 };
            auto j = w.make( src, e4 );
            j.onProgress = [&]( uint64_t, int64_t d, int64_t ) { prog = d; };
            auto id = q.enqueue( std::move( j ) );
            while( prog.load() < 200 ) std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
            q.cancel( id );
            auto r = fut.get();
            CHECK( r.first.status == Status::Cancelled );
            auto k4 = SourceCache::makeKey( "mock", "/i.wav" );
            CHECK( SourceCache::fileSize( cache.partPath( k4 ) ) > 0 && SourceCache::fileSize( cache.partPath( k4 ) ) < 1000 );
            src->slow = false;
        }
        // 7) cancel a queued job and cancelAll
        {
            src->slow = true;
            Entry ea = src->entry(); ea.path = "/q1.wav";
            Entry eb = src->entry(); eb.path = "/q2.wav";
            Waiter wa, wb;
            auto fa = wa.p.get_future(), fb = wb.p.get_future();
            q.enqueue( wa.make( src, ea ) );
            q.enqueue( wb.make( src, eb ) );
            q.cancelAll();
            CHECK( fa.get().first.status == Status::Cancelled );
            CHECK( fb.get().first.status == Status::Cancelled );
            src->slow = false;
            while( !q.idle() ) std::this_thread::sleep_for( std::chrono::milliseconds( 1 ) );
        }
        // 8) direct-file sources bypass the cache
        {
            auto d = mkTmp();
            writeFile( d + "/x.wav", "data" );
            auto loc = std::make_shared<LocalFolderSource>( "loc", "L", d );
            Entry el; el.path = "/x.wav";
            Waiter w; auto fut = w.p.get_future();
            q.enqueue( w.make( loc, el ) );
            auto r = fut.get();
            CHECK( r.first.ok() && r.second == d + "/x.wav" );
        }
        // 9) shutdown with a running job returns promptly
        {
            src->slow = true;
            Entry e = src->entry(); e.path = "/z.wav";
            static Waiter w; // outlives the queue: shutdown() below completes the job (Cancelled) on the worker
            auto fut = w.p.get_future();
            q.enqueue( w.make( src, e ) );
            std::this_thread::sleep_for( std::chrono::milliseconds( 20 ) );
            q.shutdown();
            CHECK( fut.get().first.status == Status::Cancelled );
        }
        q.shutdown();
    }

    std::printf( g_fail ? "SourceCacheTests: %d FAILED\n" : "SourceCacheTests: all passed\n", g_fail );
    return g_fail ? 1 : 0;
}
