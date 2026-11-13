// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free, I/O-free) WebDAV helpers: PROPFIND multistatus parser, URL building/escaping, Range-resume logic,
// cache invalidation, listing diff, auth headers. Host-tested by Kick/tests/platform/WebDavHelperTests.cpp.
#pragma once

#include "kick_fileSource.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <map>
#include <string>
#include <vector>

namespace ui
{
    namespace fileSource
    {
        namespace webdav
        {
            // ---------------------------------------------------------------- percent encoding / URLs
            inline std::string percentEncodePath( const std::string& s )
            {
                static const char* hex = "0123456789ABCDEF";
                std::string        out;
                out.reserve( s.size() );
                for( unsigned char c : s )
                {
                    if( std::isalnum( c ) || c == '-' || c == '.' || c == '_' || c == '~' || c == '/' )
                        out += (char)c;
                    else
                    {
                        out += '%';
                        out += hex[c >> 4];
                        out += hex[c & 15];
                    }
                }
                return out;
            }

            inline std::string percentDecode( const std::string& s )
            {
                auto hv = []( char c ) -> int {
                    if( c >= '0' && c <= '9' ) return c - '0';
                    if( c >= 'a' && c <= 'f' ) return c - 'a' + 10;
                    if( c >= 'A' && c <= 'F' ) return c - 'A' + 10;
                    return -1;
                };
                std::string out;
                out.reserve( s.size() );
                for( size_t i = 0; i < s.size(); ++i )
                {
                    if( s[i] == '%' && i + 2 < s.size() + 0 && hv( s[i + 1] ) >= 0 && hv( s[i + 2] ) >= 0 )
                    {
                        out += (char)( hv( s[i + 1] ) * 16 + hv( s[i + 2] ) );
                        i += 2;
                    }
                    else
                        out += s[i];
                }
                return out;
            }

            struct ParsedUrl
            {
                bool        ok{ false };
                std::string scheme; // lower case: "http" / "https"
                std::string host;   // lower case
                int         port{ 0 }; // 0 = scheme default
                std::string path;   // still percent-encoded, starts with '/'
                bool        hasUserInfo{ false };
            };

            inline ParsedUrl parseUrl( const std::string& url )
            {
                ParsedUrl p;
                auto      sep = url.find( "://" );
                if( sep == std::string::npos ) return p;
                p.scheme = url.substr( 0, sep );
                std::transform( p.scheme.begin(), p.scheme.end(), p.scheme.begin(), []( unsigned char c ) { return (char)std::tolower( c ); } );
                if( p.scheme != "http" && p.scheme != "https" ) return p;
                auto rest      = url.substr( sep + 3 );
                auto pathStart = rest.find_first_of( "/?#" );
                auto authority = rest.substr( 0, pathStart );
                p.path         = pathStart == std::string::npos ? "/" : rest.substr( pathStart );
                auto q         = p.path.find_first_of( "?#" );
                if( q != std::string::npos ) p.path.erase( q ); // WebDAV roots do not use query strings
                if( p.path.empty() || p.path[0] != '/' ) p.path = "/" + p.path;
                auto at = authority.rfind( '@' );
                if( at != std::string::npos )
                {
                    p.hasUserInfo = true;
                    authority     = authority.substr( at + 1 );
                }
                auto colon = authority.rfind( ':' );
                if( colon != std::string::npos && authority.find( ']', colon ) == std::string::npos )
                {
                    auto portStr = authority.substr( colon + 1 );
                    if( portStr.empty() || portStr.size() > 5 || !std::all_of( portStr.begin(), portStr.end(), []( unsigned char c ) { return std::isdigit( c ) != 0; } ) ) return p;
                    p.port    = std::atoi( portStr.c_str() );
                    authority = authority.substr( 0, colon );
                    if( p.port < 1 || p.port > 65535 ) return p;
                }
                if( authority.empty() ) return p;
                p.host = authority;
                std::transform( p.host.begin(), p.host.end(), p.host.begin(), []( unsigned char c ) { return (char)std::tolower( c ); } );
                p.ok = true;
                return p;
            }

            inline int effectivePort( const ParsedUrl& u ) { return u.port ? u.port : ( u.scheme == "https" ? 443 : 80 ); }

            inline bool sameOrigin( const ParsedUrl& a, const ParsedUrl& b )
            {
                return a.ok && b.ok && a.scheme == b.scheme && a.host == b.host && effectivePort( a ) == effectivePort( b );
            }

            // scheme://host[:port] (no user info, never any credentials)
            inline std::string originOf( const ParsedUrl& u )
            {
                return u.scheme + "://" + u.host + ( u.port ? ":" + std::to_string( u.port ) : "" );
            }

            // Safe-to-log form of a URL: user info and query removed.
            inline std::string redactedUrl( const std::string& url )
            {
                auto p = parseUrl( url );
                return p.ok ? originOf( p ) + p.path : "<invalid url>";
            }

            // Collapses "//", resolves "." / ".."; a source path always starts with '/' and never leaves the root.
            inline std::string normaliseSourcePath( const std::string& path )
            {
                std::vector<std::string> parts;
                size_t                   i = 0;
                while( i <= path.size() )
                {
                    auto j = path.find( '/', i );
                    if( j == std::string::npos ) j = path.size();
                    auto seg = path.substr( i, j - i );
                    if( seg == ".." )
                    {
                        if( !parts.empty() ) parts.pop_back();
                    }
                    else if( !seg.empty() && seg != "." )
                        parts.push_back( seg );
                    i = j + 1;
                }
                std::string out;
                for( auto& s : parts ) out += "/" + s;
                return out.empty() ? "/" : out;
            }

            inline std::string parentPath( const std::string& path )
            {
                auto n = normaliseSourcePath( path );
                if( n == "/" ) return n;
                auto s = n.rfind( '/' );
                return s == 0 ? "/" : n.substr( 0, s );
            }

            inline std::string joinPath( const std::string& dir, const std::string& name )
            {
                return normaliseSourcePath( dir + "/" + name );
            }

            // Base URL of the server root (as configured by the user) + source path -> full request URL; each segment escaped once.
            // Collections get a trailing '/' as WebDAV servers expect.
            inline std::string buildUrl( const std::string& baseUrl, const std::string& sourcePath, bool isCollection )
            {
                auto b = parseUrl( baseUrl );
                if( !b.ok ) return {};
                auto basePath = normaliseSourcePath( percentDecode( b.path ) );
                auto full     = normaliseSourcePath( basePath + "/" + normaliseSourcePath( sourcePath ) );
                auto enc      = percentEncodePath( full );
                if( isCollection && enc.back() != '/' ) enc += '/';
                return originOf( b ) + enc;
            }

            // href of a multistatus entry (absolute URL or absolute path, percent-encoded) -> source path relative to baseUrl's path.
            // Returns false when the href is outside the base (a hostile/buggy server must not make us leave the root).
            inline bool hrefToSourcePath( const std::string& baseUrl, const std::string& href, std::string& out )
            {
                auto b = parseUrl( baseUrl );
                if( !b.ok ) return false;
                std::string hrefPath = href;
                if( href.find( "://" ) != std::string::npos )
                {
                    auto h = parseUrl( href );
                    if( !h.ok || !sameOrigin( h, b ) ) return false;
                    hrefPath = h.path;
                }
                auto q = hrefPath.find_first_of( "?#" );
                if( q != std::string::npos ) hrefPath.erase( q );
                if( hrefPath.empty() || hrefPath[0] != '/' ) return false;
                auto decoded = percentDecode( hrefPath );
                // reject a href that still contains ".." segments after decoding instead of silently normalising it
                if( decoded.find( "/../" ) != std::string::npos || ( decoded.size() >= 3 && decoded.compare( decoded.size() - 3, 3, "/.." ) == 0 ) ) return false;
                auto base = normaliseSourcePath( percentDecode( b.path ) );
                auto full = normaliseSourcePath( decoded );
                if( base == "/" )
                {
                    out = full;
                    return true;
                }
                if( full == base )
                {
                    out = "/";
                    return true;
                }
                if( full.size() > base.size() && full.compare( 0, base.size(), base ) == 0 && full[base.size()] == '/' )
                {
                    out = full.substr( base.size() );
                    return true;
                }
                return false;
            }

            // ---------------------------------------------------------------- authentication headers
            inline std::string base64Encode( const std::string& in )
            {
                static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
                std::string        out;
                size_t             i = 0;
                for( ; i + 2 < in.size(); i += 3 )
                {
                    uint32_t v = ( (uint8_t)in[i] << 16 ) | ( (uint8_t)in[i + 1] << 8 ) | (uint8_t)in[i + 2];
                    out += tbl[( v >> 18 ) & 63];
                    out += tbl[( v >> 12 ) & 63];
                    out += tbl[( v >> 6 ) & 63];
                    out += tbl[v & 63];
                }
                if( i + 1 == in.size() )
                {
                    uint32_t v = (uint8_t)in[i] << 16;
                    out += tbl[( v >> 18 ) & 63];
                    out += tbl[( v >> 12 ) & 63];
                    out += "==";
                }
                else if( i + 2 == in.size() )
                {
                    uint32_t v = ( (uint8_t)in[i] << 16 ) | ( (uint8_t)in[i + 1] << 8 );
                    out += tbl[( v >> 18 ) & 63];
                    out += tbl[( v >> 12 ) & 63];
                    out += tbl[( v >> 6 ) & 63];
                    out += '=';
                }
                return out;
            }

            // "Authorization: Basic ..." header line value (without CRLF). Header values must not contain CR/LF (header injection).
            inline bool hasLineBreak( const std::string& s ) { return s.find_first_of( "\r\n" ) != std::string::npos; }

            inline std::string basicAuthValue( const std::string& user, const std::string& password )
            {
                if( user.empty() && password.empty() ) return {};
                if( hasLineBreak( user ) || hasLineBreak( password ) ) return {};
                return "Basic " + base64Encode( user + ":" + password );
            }

            inline std::string bearerAuthValue( const std::string& token )
            {
                if( token.empty() || hasLineBreak( token ) ) return {};
                return "Bearer " + token;
            }

            // ---------------------------------------------------------------- dates
            // Days since 1970-01-01 for a proleptic Gregorian date (Howard Hinnant's algorithm).
            inline int64_t daysFromCivil( int64_t y, unsigned m, unsigned d )
            {
                y -= m <= 2;
                const int64_t  era = ( y >= 0 ? y : y - 399 ) / 400;
                const unsigned yoe = (unsigned)( y - era * 400 );
                const unsigned doy = ( 153 * ( m + ( m > 2 ? -3 : 9 ) ) + 2 ) / 5 + d - 1;
                const unsigned doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
                return era * 146097 + (int64_t)doe - 719468;
            }

            // RFC 1123 ("Sun, 06 Nov 1994 08:49:37 GMT") or ISO 8601 ("2024-03-01T10:20:30Z") -> ms since epoch; 0 when unparsable.
            inline int64_t parseHttpDateMs( const std::string& s )
            {
                static const char* months[] = { "jan", "feb", "mar", "apr", "may", "jun", "jul", "aug", "sep", "oct", "nov", "dec" };
                auto               num      = []( const std::string& t, size_t pos, size_t len ) -> int {
                    if( pos + len > t.size() ) return -1;
                    int v = 0;
                    for( size_t i = pos; i < pos + len; ++i )
                    {
                        if( !std::isdigit( (unsigned char)t[i] ) ) return -1;
                        v = v * 10 + ( t[i] - '0' );
                    }
                    return v;
                };
                if( s.size() >= 19 && s[4] == '-' && s[7] == '-' && ( s[10] == 'T' || s[10] == 't' ) )
                {
                    int Y = num( s, 0, 4 ), M = num( s, 5, 2 ), D = num( s, 8, 2 ), h = num( s, 11, 2 ), mi = num( s, 14, 2 ), se = num( s, 17, 2 );
                    if( Y < 0 || M < 1 || M > 12 || D < 1 || D > 31 || h < 0 || h > 23 || mi < 0 || mi > 59 || se < 0 || se > 60 ) return 0;
                    return ( daysFromCivil( Y, (unsigned)M, (unsigned)D ) * 86400 + h * 3600 + mi * 60 + se ) * 1000;
                }
                auto comma = s.find( ',' );
                auto t     = comma == std::string::npos ? s : s.substr( comma + 1 );
                size_t p   = 0;
                while( p < t.size() && t[p] == ' ' ) ++p;
                int D = num( t, p, 2 );
                if( D < 1 || D > 31 || p + 3 > t.size() ) return 0;
                p += 2;
                while( p < t.size() && t[p] == ' ' ) ++p;
                if( p + 3 > t.size() ) return 0;
                std::string mon = t.substr( p, 3 );
                std::transform( mon.begin(), mon.end(), mon.begin(), []( unsigned char c ) { return (char)std::tolower( c ); } );
                int M = 0;
                for( int i = 0; i < 12; ++i )
                    if( mon == months[i] ) M = i + 1;
                if( !M ) return 0;
                p += 3;
                while( p < t.size() && t[p] == ' ' ) ++p;
                int Y = num( t, p, 4 );
                if( Y < 0 ) return 0;
                p += 4;
                while( p < t.size() && t[p] == ' ' ) ++p;
                int h = num( t, p, 2 ), mi = num( t, p + 3, 2 ), se = num( t, p + 6, 2 );
                if( h < 0 || mi < 0 || se < 0 || h > 23 || mi > 59 || se > 60 ) return 0;
                return ( daysFromCivil( Y, (unsigned)M, (unsigned)D ) * 86400 + h * 3600 + mi * 60 + se ) * 1000;
            }

            // ---------------------------------------------------------------- tiny XML tree + PROPFIND parser
            struct XmlNode
            {
                std::string          name; // local name, lower case, namespace prefix removed
                std::string          text;
                std::vector<XmlNode> children;
            };

            namespace detail
            {
                inline std::string decodeEntities( const std::string& s )
                {
                    std::string out;
                    for( size_t i = 0; i < s.size(); ++i )
                    {
                        if( s[i] != '&' )
                        {
                            out += s[i];
                            continue;
                        }
                        auto semi = s.find( ';', i );
                        if( semi == std::string::npos || semi - i > 10 )
                        {
                            out += s[i];
                            continue;
                        }
                        auto ent = s.substr( i + 1, semi - i - 1 );
                        if( ent == "amp" ) out += '&';
                        else if( ent == "lt" ) out += '<';
                        else if( ent == "gt" ) out += '>';
                        else if( ent == "quot" ) out += '"';
                        else if( ent == "apos" ) out += '\'';
                        else if( ent.size() > 1 && ent[0] == '#' )
                        {
                            long cp = ent[1] == 'x' || ent[1] == 'X' ? std::strtol( ent.c_str() + 2, nullptr, 16 ) : std::strtol( ent.c_str() + 1, nullptr, 10 );
                            if( cp <= 0 || cp > 0x10FFFF || ( cp >= 0xD800 && cp < 0xE000 ) ) cp = 0xFFFD;
                            if( cp < 0x80 ) out += (char)cp;
                            else if( cp < 0x800 ) { out += (char)( 0xC0 | ( cp >> 6 ) ); out += (char)( 0x80 | ( cp & 63 ) ); }
                            else if( cp < 0x10000 ) { out += (char)( 0xE0 | ( cp >> 12 ) ); out += (char)( 0x80 | ( ( cp >> 6 ) & 63 ) ); out += (char)( 0x80 | ( cp & 63 ) ); }
                            else { out += (char)( 0xF0 | ( cp >> 18 ) ); out += (char)( 0x80 | ( ( cp >> 12 ) & 63 ) ); out += (char)( 0x80 | ( ( cp >> 6 ) & 63 ) ); out += (char)( 0x80 | ( cp & 63 ) ); }
                        }
                        else
                        {
                            out += s[i]; // unknown entity: keep literally (no DTD support, so no entity expansion attacks)
                            continue;
                        }
                        i = semi;
                    }
                    return out;
                }

                inline std::string localName( std::string tag )
                {
                    auto colon = tag.find( ':' );
                    if( colon != std::string::npos ) tag = tag.substr( colon + 1 );
                    std::transform( tag.begin(), tag.end(), tag.begin(), []( unsigned char c ) { return (char)std::tolower( c ); } );
                    return tag;
                }
            }

            // Non-validating, DTD-less parser; enough for DAV multistatus. Limits: depth 64, 20000 nodes.
            inline bool parseXml( const std::string& xml, XmlNode& root )
            {
                root = XmlNode{};
                std::vector<XmlNode*> stack;
                XmlNode               doc;
                stack.push_back( &doc );
                size_t nodes = 0;
                size_t i     = 0;
                while( i < xml.size() )
                {
                    if( xml[i] != '<' )
                    {
                        auto next = xml.find( '<', i );
                        if( next == std::string::npos ) next = xml.size();
                        stack.back()->text += detail::decodeEntities( xml.substr( i, next - i ) );
                        i = next;
                        continue;
                    }
                    if( xml.compare( i, 4, "<!--" ) == 0 )
                    {
                        auto e = xml.find( "-->", i + 4 );
                        if( e == std::string::npos ) return false;
                        i = e + 3;
                    }
                    else if( xml.compare( i, 9, "<![CDATA[" ) == 0 )
                    {
                        auto e = xml.find( "]]>", i + 9 );
                        if( e == std::string::npos ) return false;
                        stack.back()->text += xml.substr( i + 9, e - i - 9 );
                        i = e + 3;
                    }
                    else if( xml.compare( i, 2, "<?" ) == 0 )
                    {
                        auto e = xml.find( "?>", i + 2 );
                        if( e == std::string::npos ) return false;
                        i = e + 2;
                    }
                    else if( xml.compare( i, 2, "<!" ) == 0 )
                    {
                        auto e = xml.find( '>', i + 2 ); // DOCTYPE: skipped, never interpreted
                        if( e == std::string::npos ) return false;
                        i = e + 1;
                    }
                    else if( xml.compare( i, 2, "</" ) == 0 )
                    {
                        auto e = xml.find( '>', i + 2 );
                        if( e == std::string::npos || stack.size() < 2 ) return false;
                        auto name = detail::localName( xml.substr( i + 2, e - i - 2 ) );
                        while( !name.empty() && std::isspace( (unsigned char)name.back() ) ) name.pop_back();
                        if( stack.back()->name != name ) return false;
                        stack.pop_back();
                        i = e + 1;
                    }
                    else
                    {
                        // start tag (attributes skipped, quotes honoured)
                        size_t j = i + 1;
                        char   quote = 0;
                        while( j < xml.size() && ( quote || xml[j] != '>' ) )
                        {
                            if( quote ) { if( xml[j] == quote ) quote = 0; }
                            else if( xml[j] == '"' || xml[j] == '\'' ) quote = xml[j];
                            ++j;
                        }
                        if( j >= xml.size() ) return false;
                        bool selfClose = xml[j - 1] == '/';
                        auto tag       = xml.substr( i + 1, ( selfClose ? j - 1 : j ) - i - 1 );
                        auto sp        = tag.find_first_of( " \t\r\n" );
                        if( sp != std::string::npos ) tag.erase( sp );
                        if( tag.empty() || ++nodes > 20000 ) return false;
                        stack.back()->children.emplace_back();
                        auto* n = &stack.back()->children.back();
                        n->name = detail::localName( tag );
                        if( !selfClose )
                        {
                            if( stack.size() >= 64 ) return false;
                            stack.push_back( n );
                        }
                        i = j + 1;
                    }
                }
                if( stack.size() != 1 || doc.children.size() != 1 ) return false;
                root = std::move( doc.children[0] );
                return true;
            }

            inline const XmlNode* findChild( const XmlNode& n, const char* name )
            {
                for( auto& c : n.children )
                    if( c.name == name ) return &c;
                return nullptr;
            }

            inline std::string trimmed( std::string s )
            {
                size_t a = 0, b = s.size();
                while( a < b && std::isspace( (unsigned char)s[a] ) ) ++a;
                while( b > a && std::isspace( (unsigned char)s[b - 1] ) ) --b;
                return s.substr( a, b - a );
            }

            inline bool statusIs2xx( const std::string& statusLine )
            {
                // "HTTP/1.1 200 OK"
                auto sp = statusLine.find( ' ' );
                if( sp == std::string::npos ) return false;
                int code = std::atoi( statusLine.c_str() + sp + 1 );
                return code >= 200 && code < 300;
            }

            // Parses a 207 multistatus. `requestPath` (source path of the collection that was asked) is skipped from the output.
            // Entries whose href lies outside the base URL are dropped. Properties are only taken from propstat blocks with a 2xx status.
            inline bool parsePropfind( const std::string& xml, const std::string& baseUrl, const std::string& requestPath, std::vector<Entry>& out )
            {
                out.clear();
                if( xml.size() > 16u * 1024 * 1024 ) return false;
                XmlNode root;
                if( !parseXml( xml, root ) || root.name != "multistatus" ) return false;
                const auto self = normaliseSourcePath( requestPath );
                for( auto& resp : root.children )
                {
                    if( resp.name != "response" ) continue;
                    auto* href = findChild( resp, "href" );
                    if( !href ) continue;
                    std::string path;
                    if( !hrefToSourcePath( baseUrl, trimmed( href->text ), path ) ) continue;
                    if( path == self ) continue;
                    Entry e;
                    e.path = path;
                    e.name = path == "/" ? "" : path.substr( path.rfind( '/' ) + 1 );
                    if( e.name.empty() ) continue;
                    bool haveType = false;
                    for( auto& ps : resp.children )
                    {
                        if( ps.name != "propstat" ) continue;
                        auto* st = findChild( ps, "status" );
                        if( st && !statusIs2xx( trimmed( st->text ) ) ) continue;
                        auto* prop = findChild( ps, "prop" );
                        if( !prop ) continue;
                        for( auto& p : prop->children )
                        {
                            if( p.name == "resourcetype" )
                            {
                                haveType = true;
                                e.isDir  = findChild( p, "collection" ) != nullptr;
                            }
                            else if( p.name == "getcontentlength" )
                            {
                                auto t = trimmed( p.text );
                                if( !t.empty() && std::all_of( t.begin(), t.end(), []( unsigned char c ) { return std::isdigit( c ) != 0; } ) && t.size() < 19 )
                                    e.size = std::atoll( t.c_str() );
                            }
                            else if( p.name == "getlastmodified" )
                                e.mtimeMs = parseHttpDateMs( trimmed( p.text ) );
                            else if( p.name == "creationdate" && e.mtimeMs == 0 )
                                e.mtimeMs = parseHttpDateMs( trimmed( p.text ) );
                            else if( p.name == "getetag" )
                                e.etag = trimmed( p.text );
                        }
                    }
                    if( !haveType && !e.isDir && e.size < 0 ) e.isDir = href->text.size() && trimmed( href->text ).back() == '/';
                    if( e.isDir ) e.size = -1;
                    out.push_back( std::move( e ) );
                }
                return true;
            }

            // PROPFIND request body (only what the browser shows).
            inline const char* propfindBody()
            {
                return "<?xml version=\"1.0\" encoding=\"utf-8\"?><d:propfind xmlns:d=\"DAV:\"><d:prop><d:resourcetype/><d:getcontentlength/>"
                       "<d:getlastmodified/><d:getetag/></d:prop></d:propfind>";
            }

            // ---------------------------------------------------------------- Range / resume
            inline std::string rangeHeaderValue( int64_t offset ) { return "bytes=" + std::to_string( offset ) + "-"; }

            struct ContentRange
            {
                bool    ok{ false };
                bool    unsatisfied{ false }; // "bytes */total"
                int64_t start{ 0 };
                int64_t end{ 0 };
                int64_t total{ -1 }; // -1 = "*"
            };

            inline ContentRange parseContentRange( const std::string& v )
            {
                ContentRange cr;
                auto         s = trimmed( v );
                if( s.compare( 0, 6, "bytes " ) != 0 ) return cr;
                s = s.substr( 6 );
                auto slash = s.find( '/' );
                if( slash == std::string::npos ) return cr;
                auto range = s.substr( 0, slash ), tot = s.substr( slash + 1 );
                if( tot == "*" ) cr.total = -1;
                else if( !tot.empty() && std::all_of( tot.begin(), tot.end(), []( unsigned char c ) { return std::isdigit( c ) != 0; } ) && tot.size() < 19 ) cr.total = std::atoll( tot.c_str() );
                else return cr;
                if( range == "*" )
                {
                    cr.unsatisfied = true;
                    cr.ok          = cr.total >= 0;
                    return cr;
                }
                auto dash = range.find( '-' );
                if( dash == std::string::npos ) return cr;
                auto a = range.substr( 0, dash ), b = range.substr( dash + 1 );
                if( a.empty() || b.empty() || a.size() > 18 || b.size() > 18 ) return cr;
                if( !std::all_of( a.begin(), a.end(), []( unsigned char c ) { return std::isdigit( c ) != 0; } ) || !std::all_of( b.begin(), b.end(), []( unsigned char c ) { return std::isdigit( c ) != 0; } ) ) return cr;
                cr.start = std::atoll( a.c_str() );
                cr.end   = std::atoll( b.c_str() );
                cr.ok    = cr.end >= cr.start;
                return cr;
            }

            enum class RangeAction
            {
                Append,           // write the body after the existing bytes
                RestartFromZero,  // truncate the .part file and write the body from byte 0
                AlreadyComplete,  // nothing to download (416 with offset == total)
                Fail
            };

            struct RangeDecision
            {
                RangeAction action{ RangeAction::Fail };
                int64_t     total{ -1 }; // full file size if the response tells
            };

            // Interprets the response to a GET that carried "Range: bytes=<offset>-" (offset 0 = no Range header was sent).
            // contentLength: the Content-Length of the response body (-1 unknown).
            inline RangeDecision classifyGetResponse( int64_t offset, int httpStatus, const std::string& contentRangeHeader, int64_t contentLength )
            {
                RangeDecision d;
                if( httpStatus == 206 )
                {
                    auto cr = parseContentRange( contentRangeHeader );
                    if( offset <= 0 || !cr.ok || cr.unsatisfied || cr.start != offset ) // a 206 we did not ask for / wrong start: never append
                        return d;
                    d.action = RangeAction::Append;
                    d.total  = cr.total;
                    if( d.total < 0 && contentLength >= 0 ) d.total = offset + contentLength;
                    return d;
                }
                if( httpStatus == 200 )
                {
                    d.action = offset > 0 ? RangeAction::RestartFromZero : RangeAction::RestartFromZero; // the whole body follows
                    d.total  = contentLength;
                    return d;
                }
                if( httpStatus == 416 )
                {
                    auto cr = parseContentRange( contentRangeHeader );
                    if( cr.ok && cr.unsatisfied && cr.total == offset )
                    {
                        d.action = RangeAction::AlreadyComplete;
                        d.total  = cr.total;
                    }
                    else if( offset > 0 )
                        d.action = RangeAction::RestartFromZero; // part file longer than the remote file or unknown total: caller refetches from 0
                    return d;
                }
                return d;
            }

            // Validators saved next to a .part file when the download started.
            struct PartMeta
            {
                std::string etag;
                int64_t     size{ -1 };
                int64_t     mtimeMs{ 0 };
            };

            inline PartMeta partMetaFor( const Entry& e ) { return PartMeta{ e.etag, e.size, e.mtimeMs }; }

            inline bool sameRemoteVersion( const PartMeta& a, const PartMeta& b )
            {
                if( !a.etag.empty() && !b.etag.empty() ) return a.etag == b.etag && ( a.size < 0 || b.size < 0 || a.size == b.size );
                if( a.size < 0 || b.size < 0 ) return false; // no validator at all: cannot prove it is the same file
                return a.size == b.size && a.mtimeMs == b.mtimeMs;
            }

            struct ResumeDecision
            {
                enum class Kind { Restart, Resume, Complete } kind{ Kind::Restart };
                int64_t offset{ 0 };
            };

            // Decides what to do with an existing .part file (partSize bytes, saved validators `saved`) given the current remote entry.
            inline ResumeDecision decideResume( int64_t partSize, const PartMeta& saved, const Entry& remote )
            {
                ResumeDecision d;
                if( partSize <= 0 ) return d;
                if( !sameRemoteVersion( saved, partMetaFor( remote ) ) ) return d;
                if( remote.size >= 0 )
                {
                    if( partSize > remote.size ) return d;
                    if( partSize == remote.size )
                    {
                        d.kind   = ResumeDecision::Kind::Complete;
                        d.offset = partSize;
                        return d;
                    }
                }
                d.kind   = ResumeDecision::Kind::Resume;
                d.offset = partSize;
                return d;
            }

            // ---------------------------------------------------------------- cache invalidation / diff
            // True when a cached copy made from `cached` must not be used for the current remote entry.
            inline bool isCacheStale( const PartMeta& cached, const Entry& remote ) { return !sameRemoteVersion( cached, partMetaFor( remote ) ); }

            inline bool isCacheStaleMeta( const PartMeta& cached, const PartMeta& remote ) { return !sameRemoteVersion( cached, remote ); }

            // Differences between two listings of the same directory (by path).
            inline std::vector<Change> diffListings( const std::vector<Entry>& before, const std::vector<Entry>& after )
            {
                std::vector<Change>           out;
                std::map<std::string, Entry>  old;
                for( auto& e : before ) old[e.path] = e;
                std::map<std::string, bool> seen;
                for( auto& e : after )
                {
                    seen[e.path] = true;
                    auto it      = old.find( e.path );
                    if( it == old.end() ) out.push_back( { Change::Kind::Added, e } );
                    else if( it->second.isDir != e.isDir || ( !e.isDir && !sameRemoteVersion( partMetaFor( it->second ), partMetaFor( e ) ) ) )
                        out.push_back( { Change::Kind::Modified, e } );
                }
                for( auto& e : before )
                    if( !seen.count( e.path ) ) out.push_back( { Change::Kind::Removed, e } );
                return out;
            }
        }
    }
}
