// SPDX-License-Identifier: AGPL-3.0-or-later
// Persistent list of configured browser sources (no secrets). Format: first line "kicksources1", then one source per line,
// TAB separated, fields percent-encoded: type, id, name, url-or-path, user, auth. Secrets live in <dir>/<id>.secret (0600).
#pragma once

#include "kick_secretStore.h"
#include "kick_webdavHelper.h"

#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace ui
{
    namespace fileSource
    {
        struct SourceConfig
        {
            std::string type; // "webdav" | "local"
            std::string id;   // [a-z0-9_-]+
            std::string name;
            std::string location; // base URL (webdav) or directory (local)
            std::string user;
            std::string auth{ "basic" }; // "basic" | "bearer"
        };

        inline bool isValidSourceId( const std::string& id )
        {
            return !id.empty() && id.size() <= 40 && std::all_of( id.begin(), id.end(), []( unsigned char c ) { return std::islower( c ) || std::isdigit( c ) || c == '-' || c == '_'; } );
        }

        inline std::string serializeSources( const std::vector<SourceConfig>& v )
        {
            std::ostringstream o;
            o << "kicksources1\n";
            for( auto& s : v )
            {
                auto e = []( const std::string& x ) { return webdav::percentEncodePath( x ); };
                o << e( s.type ) << '\t' << e( s.id ) << '\t' << e( s.name ) << '\t' << e( s.location ) << '\t' << e( s.user ) << '\t' << e( s.auth ) << '\n';
            }
            return o.str();
        }

        inline std::vector<SourceConfig> parseSources( const std::string& text )
        {
            std::vector<SourceConfig> out;
            std::istringstream        in( text );
            std::string               line;
            if( !std::getline( in, line ) || line != "kicksources1" ) return out;
            while( std::getline( in, line ) )
            {
                std::vector<std::string> f;
                size_t                   p = 0;
                while( true )
                {
                    auto t = line.find( '\t', p );
                    f.push_back( webdav::percentDecode( line.substr( p, t == std::string::npos ? std::string::npos : t - p ) ) );
                    if( t == std::string::npos ) break;
                    p = t + 1;
                }
                if( f.size() != 6 || ( f[0] != "webdav" && f[0] != "local" ) || !isValidSourceId( f[1] ) || f[3].empty() ) continue; // corrupt line: skipped
                if( f[0] == "webdav" && !webdav::parseUrl( f[3] ).ok ) continue;
                out.push_back( SourceConfig{ f[0], f[1], f[2].empty() ? f[1] : f[2], f[3], f[4], f[5] == "bearer" ? "bearer" : "basic" } );
            }
            return out;
        }

        inline std::vector<SourceConfig> loadSources( const std::string& file )
        {
            std::ifstream f( file );
            std::string   text( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
            return parseSources( text );
        }

        inline bool saveSources( const std::string& file, const std::vector<SourceConfig>& v )
        {
            return writeSecretFile( file, serializeSources( v ) ); // same atomic 0600 write; the file itself holds no secret
        }
    }
}
