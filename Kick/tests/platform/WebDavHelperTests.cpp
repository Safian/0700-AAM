// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for the pure WebDAV helpers. Argument: path of data/ (fixtures).
// Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined -o /tmp/t WebDavHelperTests.cpp && /tmp/t data
#include "../../modules/kick_ui/modes/filebrowser/kick_webdavHelper.h"
#include <cstdio>
#include <fstream>
#include <sstream>

using namespace ui::fileSource;
using namespace ui::fileSource::webdav;

static int g_fail = 0;
#define CHECK( c ) do { if( !( c ) ) { std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c ); ++g_fail; } } while( 0 )
#define CHECK_EQ_S( a, b ) do { std::string va = ( a ), vb = ( b ); if( va != vb ) { std::printf( "FAIL %s:%d: %s = '%s', expected '%s'\n", __FILE__, __LINE__, #a, va.c_str(), vb.c_str() ); ++g_fail; } } while( 0 )
#define CHECK_EQ( a, b ) do { long long va = (long long)( a ), vb = (long long)( b ); if( va != vb ) { std::printf( "FAIL %s:%d: %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb ); ++g_fail; } } while( 0 )

static std::string readFile( const std::string& p )
{
    std::ifstream f( p );
    std::stringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

int main( int argc, char** argv )
{
    const std::string dir = argc > 1 ? argv[1] : "data";

    // --- percent encoding / URLs
    CHECK_EQ_S( percentEncodePath( "/a b/ü&.wav" ), "/a%20b/%C3%BC%26.wav" );
    CHECK_EQ_S( percentDecode( "/a%20b/%C3%BC%26.wav" ), "/a b/ü&.wav" );
    CHECK_EQ_S( percentDecode( "100%" ), "100%" );
    CHECK_EQ_S( percentDecode( "%zz%4" ), "%zz%4" );
    CHECK_EQ_S( percentDecode( percentEncodePath( "/x y/%41" ) ), "/x y/%41" );
    CHECK_EQ_S( normaliseSourcePath( "a//b/./c/../d" ), "/a/b/d" );
    CHECK_EQ_S( normaliseSourcePath( "/../../x" ), "/x" );
    CHECK_EQ_S( normaliseSourcePath( "" ), "/" );
    CHECK_EQ_S( parentPath( "/a/b" ), "/a" );
    CHECK_EQ_S( parentPath( "/a" ), "/" );
    CHECK_EQ_S( parentPath( "/" ), "/" );
    CHECK_EQ_S( joinPath( "/", "x y" ), "/x y" );

    auto u = parseUrl( "HTTPS://User:pw@Example.COM:8443/dav/x?q=1" );
    CHECK( u.ok && u.scheme == "https" && u.host == "example.com" && u.port == 8443 && u.path == "/dav/x" && u.hasUserInfo );
    CHECK( !parseUrl( "ftp://h/" ).ok );
    CHECK( !parseUrl( "http://" ).ok );
    CHECK( !parseUrl( "http://h:99999/" ).ok );
    CHECK( !parseUrl( "http://h:abc/" ).ok );
    CHECK( !parseUrl( "nothing" ).ok );
    CHECK( parseUrl( "http://h" ).path == "/" );
    CHECK( sameOrigin( parseUrl( "http://h/a" ), parseUrl( "http://H:80/b" ) ) );
    CHECK( !sameOrigin( parseUrl( "http://h/a" ), parseUrl( "https://h/a" ) ) );
    CHECK( !sameOrigin( parseUrl( "http://h/a" ), parseUrl( "http://h:81/a" ) ) );
    CHECK_EQ_S( redactedUrl( "https://user:secret@host:1/p?token=abc" ), "https://host:1/p" );
    CHECK_EQ_S( buildUrl( "https://h/dav", "/Kick Loops/a&b.wav", false ), "https://h/dav/Kick%20Loops/a%26b.wav" );
    CHECK_EQ_S( buildUrl( "https://h/dav/", "/", true ), "https://h/dav/" );
    CHECK_EQ_S( buildUrl( "https://h", "/x", true ), "https://h/x/" );
    CHECK_EQ_S( buildUrl( "https://h/dav", "/../../etc", false ), "https://h/dav/etc" );
    CHECK_EQ_S( buildUrl( "bad", "/x", false ), "" );

    std::string p;
    CHECK( hrefToSourcePath( "https://h/dav", "/dav/a%20b/", p ) && p == "/a b" );
    CHECK( hrefToSourcePath( "https://h/dav", "/dav/", p ) && p == "/" );
    CHECK( hrefToSourcePath( "https://h/dav", "https://h/dav/x", p ) && p == "/x" );
    CHECK( !hrefToSourcePath( "https://h/dav", "https://evil/dav/x", p ) );
    CHECK( !hrefToSourcePath( "https://h/dav", "/davx/y", p ) );
    CHECK( !hrefToSourcePath( "https://h/dav", "/dav/%2e%2e/etc", p ) );
    CHECK( !hrefToSourcePath( "https://h/dav", "/dav/a/..", p ) );
    CHECK( !hrefToSourcePath( "https://h/dav", "relative", p ) );
    CHECK( hrefToSourcePath( "https://h", "/x", p ) && p == "/x" );

    // --- auth headers
    CHECK_EQ_S( base64Encode( "" ), "" );
    CHECK_EQ_S( base64Encode( "f" ), "Zg==" );
    CHECK_EQ_S( base64Encode( "fo" ), "Zm8=" );
    CHECK_EQ_S( base64Encode( "foo" ), "Zm9v" );
    CHECK_EQ_S( base64Encode( "foobar" ), "Zm9vYmFy" );
    CHECK_EQ_S( basicAuthValue( "Aladdin", "open sesame" ), "Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==" );
    CHECK_EQ_S( basicAuthValue( "u", "p\r\nX: y" ), "" );
    CHECK_EQ_S( bearerAuthValue( "tok" ), "Bearer tok" );
    CHECK_EQ_S( bearerAuthValue( "t\nx" ), "" );
    CHECK_EQ_S( bearerAuthValue( "" ), "" );

    // --- dates
    CHECK_EQ( parseHttpDateMs( "Sun, 06 Nov 1994 08:49:37 GMT" ), 784111777000LL );
    CHECK_EQ( parseHttpDateMs( "Thu, 01 Jan 1970 00:00:00 GMT" ), 0 );
    CHECK_EQ( parseHttpDateMs( "2024-03-01T10:20:30Z" ), 1709288430000LL );
    CHECK_EQ( parseHttpDateMs( "garbage" ), 0 );
    CHECK_EQ( parseHttpDateMs( "Sun, 06 Xxx 1994 08:49:37 GMT" ), 0 );
    CHECK_EQ( parseHttpDateMs( "" ), 0 );

    // --- PROPFIND fixtures
    {
        std::vector<Entry> es;
        auto xml = readFile( dir + "/propfind_multistatus.xml" );
        CHECK( !xml.empty() );
        CHECK( parsePropfind( xml, "http://example.test:8080/dav", "/Samples", es ) );
        // self skipped, /other outside base dropped, traversal href dropped => 3 entries
        CHECK_EQ( es.size(), 3 );
        if( es.size() == 3 )
        {
            CHECK( es[0].isDir && es[0].name == "Kick Loops" && es[0].path == "/Samples/Kick Loops" && es[0].size == -1 );
            CHECK_EQ( es[0].mtimeMs, 784111777000LL );
            CHECK( !es[1].isDir );
            CHECK_EQ_S( es[1].name, "Snare & Hat \xC3\xA1.wav" );
            CHECK_EQ( es[1].size, 123456 );
            CHECK_EQ_S( es[1].etag, "\"abc123\"" );
            CHECK_EQ( es[1].mtimeMs, parseHttpDateMs( "Tue, 03 Nov 2026 12:30:45 GMT" ) );
            CHECK_EQ_S( es[2].etag, "W/\"x<y\"" );
            CHECK_EQ( es[2].size, 42 );
        }
    }
    {
        std::vector<Entry> es;
        auto xml = readFile( dir + "/propfind_nextcloud_prefixless.xml" );
        CHECK( parsePropfind( xml, "https://c.test/remote.php/dav/files/u", "/", es ) );
        CHECK_EQ( es.size(), 1 );
        if( es.size() == 1 ) { CHECK_EQ_S( es[0].path, "/a.wav" ); CHECK_EQ( es[0].mtimeMs, 1709288430000LL ); CHECK( !es[0].isDir ); }
    }
    {
        std::vector<Entry> es;
        CHECK( !parsePropfind( "", "http://h/", "/", es ) );
        CHECK( !parsePropfind( "<html>no</html>", "http://h/", "/", es ) );
        CHECK( !parsePropfind( "<d:multistatus><d:response></d:multistatus>", "http://h/", "/", es ) );
        CHECK( !parsePropfind( "<d:multistatus", "http://h/", "/", es ) );
        CHECK( parsePropfind( "<d:multistatus xmlns:d='DAV:'/>", "http://h/", "/", es ) && es.empty() );
        // billion-laughs style DOCTYPE is skipped, entities are not expanded beyond the 5 + numeric ones
        CHECK( parsePropfind( "<?xml version='1.0'?><!DOCTYPE x [<!ENTITY a 'aaaa'>]><d:multistatus xmlns:d='DAV:'><d:response><d:href>/&a;.wav</d:href><d:propstat><d:prop><d:resourcetype/></d:prop><d:status>HTTP/1.1 200 OK</d:status></d:propstat></d:response></d:multistatus>", "http://h/", "/", es ) );
        CHECK( es.size() == 1 && es[0].name == "&a;.wav" );
        std::string deep = "<a>";
        for( int i = 0; i < 100; ++i ) deep += "<b>";
        XmlNode dn;
        CHECK( !parseXml( deep, dn ) ); // depth limit
        std::string big = "<d:multistatus>";
        for( int i = 0; i < 21000; ++i ) big += "<x/>";
        big += "</d:multistatus>";
        XmlNode n;
        CHECK( !parseXml( big, n ) );
        // truncated-at-every-position never crashes
        auto full = readFile( dir + "/propfind_multistatus.xml" );
        for( size_t cut = 0; cut < full.size(); cut += 7 ) parsePropfind( full.substr( 0, cut ), "http://example.test:8080/dav", "/Samples", es );
        // random garbage bytes
        unsigned seed = 12345;
        for( int k = 0; k < 200; ++k )
        {
            std::string g = full;
            for( int j = 0; j < 5; ++j ) { seed = seed * 1103515245u + 12345u; g[( seed >> 8 ) % g.size()] = (char)( seed >> 16 ); }
            parsePropfind( g, "http://example.test:8080/dav", "/Samples", es );
        }
    }

    // --- Range
    CHECK_EQ_S( rangeHeaderValue( 1048576 ), "bytes=1048576-" );
    auto cr = parseContentRange( "bytes 100-199/200" );
    CHECK( cr.ok && cr.start == 100 && cr.end == 199 && cr.total == 200 );
    cr = parseContentRange( "bytes 100-199/*" );
    CHECK( cr.ok && cr.total == -1 );
    cr = parseContentRange( "bytes */200" );
    CHECK( cr.ok && cr.unsatisfied && cr.total == 200 );
    CHECK( !parseContentRange( "bytes 200-100/300" ).ok );
    CHECK( !parseContentRange( "items 1-2/3" ).ok );
    CHECK( !parseContentRange( "bytes a-b/c" ).ok );
    CHECK( !parseContentRange( "" ).ok );

    auto d = classifyGetResponse( 100, 206, "bytes 100-199/200", 100 );
    CHECK( d.action == RangeAction::Append && d.total == 200 );
    d = classifyGetResponse( 100, 206, "bytes 0-199/200", 200 );
    CHECK( d.action == RangeAction::Fail ); // wrong start: never append
    d = classifyGetResponse( 100, 206, "", 100 );
    CHECK( d.action == RangeAction::Fail );
    d = classifyGetResponse( 0, 206, "bytes 0-9/10", 10 );
    CHECK( d.action == RangeAction::Fail ); // 206 without a Range request
    d = classifyGetResponse( 100, 200, "", 5000 );
    CHECK( d.action == RangeAction::RestartFromZero && d.total == 5000 );
    d = classifyGetResponse( 0, 200, "", -1 );
    CHECK( d.action == RangeAction::RestartFromZero && d.total == -1 );
    d = classifyGetResponse( 200, 416, "bytes */200", 0 );
    CHECK( d.action == RangeAction::AlreadyComplete && d.total == 200 );
    d = classifyGetResponse( 300, 416, "bytes */200", 0 );
    CHECK( d.action == RangeAction::RestartFromZero );
    d = classifyGetResponse( 100, 500, "", 0 );
    CHECK( d.action == RangeAction::Fail );

    // --- resume decisions / cache invalidation
    Entry e;
    e.size = 1000; e.mtimeMs = 5; e.etag = "\"v1\"";
    PartMeta saved = partMetaFor( e );
    auto r = decideResume( 400, saved, e );
    CHECK( r.kind == ResumeDecision::Kind::Resume && r.offset == 400 );
    CHECK( decideResume( 0, saved, e ).kind == ResumeDecision::Kind::Restart );
    CHECK( decideResume( 1000, saved, e ).kind == ResumeDecision::Kind::Complete );
    CHECK( decideResume( 1001, saved, e ).kind == ResumeDecision::Kind::Restart );
    Entry changed = e; changed.etag = "\"v2\"";
    CHECK( decideResume( 400, saved, changed ).kind == ResumeDecision::Kind::Restart );
    Entry noEtag = e; noEtag.etag.clear();
    PartMeta savedNoEtag = partMetaFor( noEtag );
    CHECK( decideResume( 400, savedNoEtag, noEtag ).kind == ResumeDecision::Kind::Resume );
    Entry mt = noEtag; mt.mtimeMs = 6;
    CHECK( decideResume( 400, savedNoEtag, mt ).kind == ResumeDecision::Kind::Restart );
    Entry unknown; // no validator at all
    CHECK( decideResume( 400, partMetaFor( unknown ), unknown ).kind == ResumeDecision::Kind::Restart );
    CHECK( !isCacheStale( saved, e ) );
    CHECK( isCacheStale( saved, changed ) );
    Entry sz = e; sz.size = 999;
    CHECK( isCacheStale( saved, sz ) );

    // --- listing diff
    {
        Entry a; a.path = "/a"; a.name = "a"; a.size = 1; a.mtimeMs = 1;
        Entry b; b.path = "/b"; b.name = "b"; b.size = 2; b.mtimeMs = 1;
        Entry c; c.path = "/c"; c.name = "c"; c.size = 3; c.mtimeMs = 1;
        Entry b2 = b; b2.size = 20;
        auto ch = diffListings( { a, b }, { b2, c } );
        CHECK_EQ( ch.size(), 3 );
        int added = 0, removed = 0, modified = 0;
        for( auto& x : ch ) { added += x.kind == Change::Kind::Added; removed += x.kind == Change::Kind::Removed; modified += x.kind == Change::Kind::Modified; }
        CHECK( added == 1 && removed == 1 && modified == 1 );
        CHECK( diffListings( { a, b }, { a, b } ).empty() );
        CHECK_EQ( diffListings( {}, { a, b } ).size(), 2 );
    }

    std::printf( g_fail ? "WebDavHelperTests: %d FAILED\n" : "WebDavHelperTests: all passed\n", g_fail );
    return g_fail ? 1 : 0;
}
