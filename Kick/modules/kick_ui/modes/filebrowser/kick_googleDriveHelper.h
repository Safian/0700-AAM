// SPDX-License-Identifier: AGPL-3.0-or-later
// Pure (JUCE-free, I/O-free) Google Drive REST API v3 & OAuth 2.0 PKCE helpers:
// folder link/ID parsing, query/URL encoding, RFC 3339 timestamp parsing, OAuth PKCE S256 challenge,
// OAuth callback query extraction and credential/token serialization. Host-tested by GoogleDriveHelperTests.cpp.
#pragma once

#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace ui
{
    namespace fileSource
    {
        namespace gdrive
        {
            // ---------------------------------------------------------------- encoding helpers
            inline std::string encodeQueryComponent( const std::string& s )
            {
                static const char* hex = "0123456789ABCDEF";
                std::string        out;
                out.reserve( s.size() * 2 );
                for( char ch : s )
                {
                    const auto c = static_cast<unsigned char>( ch );
                    if( std::isalnum( c ) || c == '-' || c == '.' || c == '_' || c == '~' )
                        out += ch;
                    else
                    {
                        out += '%';
                        out += hex[c >> 4];
                        out += hex[c & 15];
                    }
                }
                return out;
            }

            inline std::string decodeQueryComponent( const std::string& s )
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
                    if( s[i] == '+' )
                        out += ' ';
                    else if( s[i] == '%' && i + 2 < s.size() && hv( s[i + 1] ) >= 0 && hv( s[i + 2] ) >= 0 )
                    {
                        out += static_cast<char>( hv( s[i + 1] ) * 16 + hv( s[i + 2] ) );
                        i += 2;
                    }
                    else
                        out += s[i];
                }
                return out;
            }

            inline std::string base64UrlEncode( const uint8_t* data, size_t len )
            {
                static const char* tbl = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_";
                std::string        out;
                out.reserve( ( len * 4 + 2 ) / 3 );
                size_t i = 0;
                while( i + 2 < len )
                {
                    uint32_t v = ( uint32_t( data[i] ) << 16 ) | ( uint32_t( data[i + 1] ) << 8 ) | uint32_t( data[i + 2] );
                    out += tbl[( v >> 18 ) & 63];
                    out += tbl[( v >> 12 ) & 63];
                    out += tbl[( v >> 6 ) & 63];
                    out += tbl[v & 63];
                    i += 3;
                }
                if( i + 1 == len )
                {
                    uint32_t v = uint32_t( data[i] ) << 16;
                    out += tbl[( v >> 18 ) & 63];
                    out += tbl[( v >> 12 ) & 63];
                }
                else if( i + 2 == len )
                {
                    uint32_t v = ( uint32_t( data[i] ) << 16 ) | ( uint32_t( data[i + 1] ) << 8 );
                    out += tbl[( v >> 18 ) & 63];
                    out += tbl[( v >> 12 ) & 63];
                    out += tbl[( v >> 6 ) & 63];
                }
                return out;
            }

            // Minimal FIPS 180-4 SHA-256 (self-contained so PKCE works cleanly across iOS, macOS, Linux, Windows and host tests).
            inline std::array<uint8_t, 32> sha256( const void* data, size_t len )
            {
                static const uint32_t k[64] = {
                    0x428a2f98u, 0x71374491u, 0xb5c0fbcfu, 0xe9b5dba5u, 0x3956c25bu, 0x59f111f1u, 0x923f82a4u, 0xab1c5ed5u,
                    0xd807aa98u, 0x12835b01u, 0x243185beu, 0x550c7dc3u, 0x72be5d74u, 0x80deb1feu, 0x9bdc06a7u, 0xc19bf174u,
                    0xe49b69c1u, 0xefbe4786u, 0x0fc19dc6u, 0x240ca1ccu, 0x2de92c6fu, 0x4a7484aau, 0x5cb0a9dcu, 0x76f988dau,
                    0x983e5152u, 0xa831c66du, 0xb00327c8u, 0xbf597fc7u, 0xc6e00bf3u, 0xd5a79147u, 0x06ca6351u, 0x14292967u,
                    0x27b70a85u, 0x2e1b2138u, 0x4d2c6dfcu, 0x53380d13u, 0x650a7354u, 0x766a0abbu, 0x81c2c92eu, 0x92722c85u,
                    0xa2bfe8a1u, 0xa81a664bu, 0xc24b8b70u, 0xc76c51a3u, 0xd192e819u, 0xd6990624u, 0xf40e3585u, 0x106aa070u,
                    0x19a4c116u, 0x1e376c08u, 0x2748774cu, 0x34b0bcb5u, 0x391c0cb3u, 0x4ed8aa4au, 0x5b9cca4fu, 0x682e6ff3u,
                    0x748f82eeu, 0x78a5636fu, 0x84c87814u, 0x8cc70208u, 0x90befffau, 0xa4506cebu, 0xbef9a3f7u, 0xc67178f2u
                };
                uint32_t h[8] = {
                    0x6a09e667u, 0xbb67ae85u, 0x3c6ef372u, 0xa54ff53au,
                    0x510e527fu, 0x9b05688cu, 0x1f83d9abu, 0x5be0cd19u
                };
                auto rotr = []( uint32_t x, uint32_t n ) { return ( x >> n ) | ( x << ( 32 - n ) ); };

                const auto* bytes = static_cast<const uint8_t*>( data );
                std::vector<uint8_t> msg( bytes, bytes + len );
                const uint64_t       bitLen = uint64_t( len ) * 8ull;
                msg.push_back( 0x80 );
                while( ( msg.size() % 64 ) != 56 )
                    msg.push_back( 0 );
                for( int i = 7; i >= 0; --i )
                    msg.push_back( static_cast<uint8_t>( ( bitLen >> ( i * 8 ) ) & 0xff ) );

                for( size_t off = 0; off < msg.size(); off += 64 )
                {
                    uint32_t w[64];
                    for( int i = 0; i < 16; ++i )
                    {
                        const size_t idx = off + size_t( i ) * 4;
                        w[i]             = ( uint32_t( msg[idx] ) << 24 ) | ( uint32_t( msg[idx + 1] ) << 16 )
                                         | ( uint32_t( msg[idx + 2] ) << 8 ) | uint32_t( msg[idx + 3] );
                    }
                    for( int i = 16; i < 64; ++i )
                    {
                        const uint32_t s0 = rotr( w[i - 15], 7 ) ^ rotr( w[i - 15], 18 ) ^ ( w[i - 15] >> 3 );
                        const uint32_t s1 = rotr( w[i - 2], 17 ) ^ rotr( w[i - 2], 19 ) ^ ( w[i - 2] >> 10 );
                        w[i]              = w[i - 16] + s0 + w[i - 7] + s1;
                    }
                    uint32_t a = h[0], b = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
                    for( int i = 0; i < 64; ++i )
                    {
                        const uint32_t S1   = rotr( e, 6 ) ^ rotr( e, 11 ) ^ rotr( e, 25 );
                        const uint32_t ch   = ( e & f ) ^ ( ( ~e ) & g );
                        const uint32_t temp1 = hh + S1 + ch + k[i] + w[i];
                        const uint32_t S0   = rotr( a, 2 ) ^ rotr( a, 13 ) ^ rotr( a, 22 );
                        const uint32_t maj  = ( a & b ) ^ ( a & c ) ^ ( b & c );
                        const uint32_t temp2 = S0 + maj;
                        hh = g; g = f; f = e; e = d + temp1; d = c; c = b; b = a; a = temp1 + temp2;
                    }
                    h[0] += a; h[1] += b; h[2] += c; h[3] += d;
                    h[4] += e; h[5] += f; h[6] += g; h[7] += hh;
                }

                std::array<uint8_t, 32> out{};
                for( int i = 0; i < 8; ++i )
                {
                    out[size_t( i ) * 4 + 0] = static_cast<uint8_t>( ( h[i] >> 24 ) & 0xff );
                    out[size_t( i ) * 4 + 1] = static_cast<uint8_t>( ( h[i] >> 16 ) & 0xff );
                    out[size_t( i ) * 4 + 2] = static_cast<uint8_t>( ( h[i] >> 8 ) & 0xff );
                    out[size_t( i ) * 4 + 3] = static_cast<uint8_t>( h[i] & 0xff );
                }
                return out;
            }

            inline std::string pkceS256Challenge( const std::string& verifier )
            {
                const auto d = sha256( verifier.data(), verifier.size() );
                return base64UrlEncode( d.data(), d.size() );
            }

            // ---------------------------------------------------------------- folder ID & link parsing
            struct FolderTarget
            {
                bool        ok{ false };
                std::string folderId;    // "root" or Drive ID
                std::string resourceKey; // optional resourceKey for link-shared folders
            };

            inline std::string trimWhitespace( const std::string& s )
            {
                size_t a = 0;
                while( a < s.size() && ( std::isspace( static_cast<unsigned char>( s[a] ) ) != 0 ) ) ++a;
                size_t b = s.size();
                while( b > a && ( std::isspace( static_cast<unsigned char>( s[b - 1] ) ) != 0 ) ) --b;
                return s.substr( a, b - a );
            }

            inline bool isValidDriveId( const std::string& id )
            {
                if( id == "root" )
                    return true;
                if( id.size() < 8 || id.size() > 128 )
                    return false;
                return std::all_of( id.begin(), id.end(), []( unsigned char c ) {
                    return std::isalnum( c ) || c == '-' || c == '_';
                } );
            }

            inline std::map<std::string, std::string> parseQueryPairs( const std::string& query )
            {
                std::map<std::string, std::string> out;
                size_t                             pos = 0;
                if( !query.empty() && query[0] == '?' )
                    pos = 1;
                while( pos < query.size() )
                {
                    const size_t amp = query.find( '&', pos );
                    const auto   kv  = query.substr( pos, amp == std::string::npos ? std::string::npos : amp - pos );
                    const size_t eq  = kv.find( '=' );
                    if( eq == std::string::npos )
                        out[decodeQueryComponent( kv )] = {};
                    else
                        out[decodeQueryComponent( kv.substr( 0, eq ) )] = decodeQueryComponent( kv.substr( eq + 1 ) );
                    if( amp == std::string::npos )
                        break;
                    pos = amp + 1;
                }
                return out;
            }

            // Accepts "root", "gdrive://root", "gdrive://folder/<ID>", "https://drive.google.com/drive/folders/<ID>?resourcekey=...",
            // "https://drive.google.com/open?id=<ID>", or a raw Drive folder ID.
            inline FolderTarget parseFolderTarget( const std::string& rawInput )
            {
                FolderTarget t;
                const auto   s = trimWhitespace( rawInput );
                if( s.empty() || s == "root" || s == "gdrive://root" || s == "gdrive://" )
                {
                    t.ok       = true;
                    t.folderId = "root";
                    return t;
                }
                if( s.rfind( "gdrive://folder/", 0 ) == 0 )
                {
                    auto rest = s.substr( 16 );
                    auto q    = rest.find_first_of( "?#/" );
                    if( q != std::string::npos )
                    {
                        if( rest[q] == '?' )
                        {
                            auto qp       = parseQueryPairs( rest.substr( q + 1 ) );
                            t.resourceKey = qp["resourcekey"];
                        }
                        rest.erase( q );
                    }
                    if( isValidDriveId( rest ) )
                    {
                        t.ok       = true;
                        t.folderId = rest;
                    }
                    return t;
                }
                if( s.find( "drive.google.com" ) != std::string::npos )
                {
                    const auto qpos   = s.find( '?' );
                    const auto path   = qpos == std::string::npos ? s : s.substr( 0, qpos );
                    auto       qpairs = qpos == std::string::npos ? std::map<std::string, std::string>() : parseQueryPairs( s.substr( qpos + 1 ) );
                    if( qpairs.count( "resourcekey" ) )
                        t.resourceKey = qpairs["resourcekey"];

                    const std::string foldersMarker = "/folders/";
                    auto              fpos          = path.find( foldersMarker );
                    if( fpos != std::string::npos )
                    {
                        auto id = path.substr( fpos + foldersMarker.size() );
                        auto sl = id.find( '/' );
                        if( sl != std::string::npos ) id.erase( sl );
                        if( isValidDriveId( id ) )
                        {
                            t.ok       = true;
                            t.folderId = id;
                        }
                        return t;
                    }
                    if( qpairs.count( "id" ) && isValidDriveId( qpairs["id"] ) )
                    {
                        t.ok       = true;
                        t.folderId = qpairs["id"];
                        return t;
                    }
                    return t;
                }
                if( isValidDriveId( s ) )
                {
                    t.ok       = true;
                    t.folderId = s;
                }
                return t;
            }

            // ---------------------------------------------------------------- Drive REST API URLs
            inline std::string escapeDriveQueryLiteral( const std::string& s )
            {
                std::string out;
                out.reserve( s.size() + 4 );
                for( char c : s )
                {
                    if( c == '\\' || c == '\'' )
                        out += '\\';
                    out += c;
                }
                return out;
            }

            inline std::string buildFilesListUrl( const std::string& parentId,
                                                  bool               sharedWithMe,
                                                  const std::string& exactName,
                                                  const std::string& pageToken,
                                                  const std::string& apiKey )
            {
                std::string q = sharedWithMe
                                    ? std::string( "sharedWithMe = true and trashed = false" )
                                    : ( "'" + escapeDriveQueryLiteral( parentId ) + "' in parents and trashed = false" );
                if( !exactName.empty() )
                    q += " and name = '" + escapeDriveQueryLiteral( exactName ) + "'";

                std::string url = "https://www.googleapis.com/drive/v3/files?q=" + encodeQueryComponent( q )
                                + "&fields=" + encodeQueryComponent( "nextPageToken,files(id,name,mimeType,size,modifiedTime,md5Checksum,shortcutDetails(targetId,targetMimeType))" )
                                + "&pageSize=200&supportsAllDrives=true&includeItemsFromAllDrives=true";
                if( !pageToken.empty() )
                    url += "&pageToken=" + encodeQueryComponent( pageToken );
                if( !apiKey.empty() )
                    url += "&key=" + encodeQueryComponent( apiKey );
                return url;
            }

            inline std::string buildFileGetUrl( const std::string& fileId, bool altMedia, const std::string& apiKey )
            {
                std::string url = "https://www.googleapis.com/drive/v3/files/" + encodeQueryComponent( fileId )
                                + ( altMedia
                                        ? std::string( "?alt=media&supportsAllDrives=true" )
                                        : std::string( "?fields=" ) + encodeQueryComponent( "id,name,mimeType,size,modifiedTime,md5Checksum,shortcutDetails(targetId,targetMimeType)" ) + "&supportsAllDrives=true" );
                if( !apiKey.empty() )
                    url += "&key=" + encodeQueryComponent( apiKey );
                return url;
            }

            inline std::string buildAboutUserUrl()
            {
                return "https://www.googleapis.com/drive/v3/about?fields=" + encodeQueryComponent( "user(displayName,emailAddress)" );
            }

            // Reversed OAuth Client ID scheme for Google iOS OAuth clients:
            // "<prefix>.apps.googleusercontent.com" -> "com.googleusercontent.apps.<prefix>".
            inline std::string buildReversedClientScheme( const std::string& clientId )
            {
                const auto               s      = trimWhitespace( clientId );
                static const std::string suffix = ".apps.googleusercontent.com";
                if( s.size() <= suffix.size() )
                    return {};
                if( s.compare( s.size() - suffix.size(), suffix.size(), suffix ) != 0 )
                    return {};
                const auto prefix = s.substr( 0, s.size() - suffix.size() );
                if( prefix.empty() || prefix.find_first_of( "/:?#@" ) != std::string::npos )
                    return {};
                return "com.googleusercontent.apps." + prefix;
            }

            inline std::string buildAuthUrl( const std::string& clientId,
                                             const std::string& redirectUri,
                                             const std::string& codeChallenge,
                                             const std::string& state,
                                             const std::string& loginHint,
                                             const std::string& scope = "https://www.googleapis.com/auth/drive.readonly https://www.googleapis.com/auth/userinfo.email" )
            {
                std::string url = "https://accounts.google.com/o/oauth2/v2/auth?client_id=" + encodeQueryComponent( clientId )
                                + "&redirect_uri=" + encodeQueryComponent( redirectUri )
                                + "&response_type=code"
                                + "&scope=" + encodeQueryComponent( scope )
                                + "&code_challenge=" + encodeQueryComponent( codeChallenge )
                                + "&code_challenge_method=S256"
                                + "&access_type=offline"
                                + "&prompt=consent"
                                + "&state=" + encodeQueryComponent( state );
                if( !loginHint.empty() )
                    url += "&login_hint=" + encodeQueryComponent( loginHint );
                return url;
            }

            struct OAuthCallback
            {
                std::string code;
                std::string state;
                std::string error;
                std::string errorDescription;
                bool        ok() const { return !code.empty() && error.empty(); }
            };

            inline OAuthCallback parseOAuthCallbackUrl( const std::string& callbackUrl )
            {
                OAuthCallback out;
                const auto    qpos = callbackUrl.find( '?' );
                if( qpos == std::string::npos )
                    return out;
                auto       end = callbackUrl.find( '#', qpos );
                const auto q   = end == std::string::npos ? callbackUrl.substr( qpos + 1 ) : callbackUrl.substr( qpos + 1, end - qpos - 1 );
                auto       kv  = parseQueryPairs( q );
                out.code             = kv["code"];
                out.state            = kv["state"];
                out.error            = kv["error"];
                out.errorDescription = kv["error_description"];
                return out;
            }

            // ---------------------------------------------------------------- RFC 3339 date parser
            // Parses "YYYY-MM-DDTHH:MM:SS(.sss)?(Z|+HH:MM|-HH:MM)" to epoch ms; 0 on invalid input.
            inline int64_t parseRfc3339Ms( const std::string& iso )
            {
                int year = 0, mon = 0, day = 0, hour = 0, min = 0, sec = 0;
                if( iso.size() < 19 || std::sscanf( iso.c_str(), "%4d-%2d-%2dT%2d:%2d:%2d", &year, &mon, &day, &hour, &min, &sec ) != 6 )
                    return 0;
                if( mon < 1 || mon > 12 || day < 1 || day > 31 || hour < 0 || hour > 23 || min < 0 || min > 59 || sec < 0 || sec > 60 )
                    return 0;

                // Howard Hinnant days_from_civil
                int64_t y   = year - ( mon <= 2 ? 1 : 0 );
                int64_t era = ( y >= 0 ? y : y - 399 ) / 400;
                int64_t yoe = y - era * 400;
                int64_t doy = ( 153 * ( mon + ( mon > 2 ? -3 : 9 ) ) + 2 ) / 5 + day - 1;
                int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
                int64_t days = era * 146097 + doe - 719468;

                int64_t ms = ( ( days * 86400 + int64_t( hour ) * 3600 + int64_t( min ) * 60 + sec ) * 1000 );

                size_t p = 19;
                if( p < iso.size() && iso[p] == '.' )
                {
                    ++p;
                    int fracMs = 0, digits = 0;
                    while( p < iso.size() && std::isdigit( static_cast<unsigned char>( iso[p] ) ) != 0 )
                    {
                        if( digits < 3 )
                            fracMs = fracMs * 10 + ( iso[p] - '0' );
                        ++digits;
                        ++p;
                    }
                    while( digits < 3 && digits > 0 )
                    {
                        fracMs *= 10;
                        ++digits;
                    }
                    ms += fracMs;
                }

                if( p < iso.size() && ( iso[p] == '+' || iso[p] == '-' ) )
                {
                    const int sign = iso[p] == '-' ? -1 : 1;
                    int       th = 0, tm = 0;
                    if( std::sscanf( iso.c_str() + p + 1, "%2d:%2d", &th, &tm ) >= 1 )
                        ms -= sign * ( int64_t( th ) * 3600 + int64_t( tm ) * 60 ) * 1000;
                }
                return ms;
            }

            // Parses secret credential lines (KEY=VALUE format, safe against JSON nesting issues in 0600 secret files).
            // Values must not contain CR/LF.
            struct Credentials
            {
                std::string mode{ "oauth" }; // "oauth" | "apikey" | "bearer"
                std::string clientId;
                std::string clientSecret;
                std::string redirectUri;
                std::string accessToken;
                std::string refreshToken;
                int64_t     expiresAtMs{ 0 };
                std::string apiKey;
                std::string resourceKey;
                std::string email;
                std::string displayName;
            };

            inline std::string stripLineBreaks( std::string s )
            {
                s.erase( std::remove_if( s.begin(), s.end(), []( char c ) { return c == '\r' || c == '\n'; } ), s.end() );
                return s;
            }

            inline std::string serializeCredentials( const Credentials& c )
            {
                std::string out;
                auto        add = [&]( const char* k, const std::string& v ) {
                    if( !v.empty() )
                        out += std::string( k ) + "=" + stripLineBreaks( v ) + "\n";
                };
                add( "mode", c.mode );
                add( "client_id", c.clientId );
                add( "client_secret", c.clientSecret );
                add( "redirect_uri", c.redirectUri );
                add( "access_token", c.accessToken );
                add( "refresh_token", c.refreshToken );
                if( c.expiresAtMs > 0 )
                    add( "expires_at_ms", std::to_string( c.expiresAtMs ) );
                add( "api_key", c.apiKey );
                add( "resource_key", c.resourceKey );
                add( "email", c.email );
                add( "display_name", c.displayName );
                return out;
            }

            inline Credentials parseCredentials( const std::string& text )
            {
                Credentials c;
                size_t      pos = 0;
                bool        anyKv = false;
                while( pos < text.size() )
                {
                    const auto nl   = text.find( '\n', pos );
                    auto       line = text.substr( pos, nl == std::string::npos ? std::string::npos : nl - pos );
                    if( !line.empty() && line.back() == '\r' )
                        line.pop_back();
                    const auto eq = line.find( '=' );
                    if( eq != std::string::npos )
                    {
                        anyKv       = true;
                        const auto k = line.substr( 0, eq );
                        const auto v = line.substr( eq + 1 );
                        if( k == "mode" ) c.mode = v;
                        else if( k == "client_id" ) c.clientId = v;
                        else if( k == "client_secret" ) c.clientSecret = v;
                        else if( k == "redirect_uri" ) c.redirectUri = v;
                        else if( k == "access_token" ) c.accessToken = v;
                        else if( k == "refresh_token" ) c.refreshToken = v;
                        else if( k == "expires_at_ms" ) c.expiresAtMs = std::atoll( v.c_str() );
                        else if( k == "api_key" ) c.apiKey = v;
                        else if( k == "resource_key" ) c.resourceKey = v;
                        else if( k == "email" ) c.email = v;
                        else if( k == "display_name" ) c.displayName = v;
                    }
                    if( nl == std::string::npos )
                        break;
                    pos = nl + 1;
                }
                // Legacy/raw single-line secret convenience: a raw "1//..." refresh token or "ya29..." access token.
                if( !anyKv )
                {
                    const auto single = trimWhitespace( text );
                    if( single.rfind( "1//", 0 ) == 0 )
                    {
                        c.mode         = "oauth";
                        c.refreshToken = single;
                    }
                    else if( !single.empty() )
                    {
                        c.mode        = "bearer";
                        c.accessToken = single;
                    }
                }
                return c;
            }
        } // namespace gdrive
    }     // namespace fileSource
} // namespace ui
