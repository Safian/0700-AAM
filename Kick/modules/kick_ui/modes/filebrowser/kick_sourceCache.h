// SPDX-License-Identifier: AGPL-3.0-or-later
// Shared download cache for file-browser sources: size limit, LRU eviction, missing/corrupt file handling, .part + validator
// files for resumable downloads. JUCE-free (POSIX + std). Thread-safe. Host-tested by SourceCacheTests.cpp.
#pragma once

#include "kick_webdavHelper.h"

#include <cstdio>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace ui
{
    namespace fileSource
    {
        // ------------------------------------------------------------ pure index (no file access)
        class CacheIndex
        {
        public:
            struct Item
            {
                std::string        key;
                int64_t            size{ 0 };
                int64_t            lastUse{ 0 }; // monotonically increasing logical clock
                webdav::PartMeta   version;
                int                pins{ 0 };
            };

            void put( const std::string& key, int64_t size, const webdav::PartMeta& v )
            {
                auto& it   = items[key];
                it.key     = key;
                it.size    = size;
                it.version = v;
                it.lastUse = ++clock;
            }
            bool get( const std::string& key, Item& out ) const
            {
                auto it = items.find( key );
                if( it == items.end() ) return false;
                out = it->second;
                return true;
            }
            void touch( const std::string& key )
            {
                auto it = items.find( key );
                if( it != items.end() ) it->second.lastUse = ++clock;
            }
            void pin( const std::string& key )
            {
                auto it = items.find( key );
                if( it != items.end() ) ++it->second.pins;
            }
            void unpin( const std::string& key )
            {
                auto it = items.find( key );
                if( it != items.end() && it->second.pins > 0 ) --it->second.pins;
            }
            void remove( const std::string& key ) { items.erase( key ); }
            int64_t totalSize() const
            {
                int64_t t = 0;
                for( auto& kv : items ) t += kv.second.size;
                return t;
            }
            size_t count() const { return items.size(); }

            // Keys to delete (least recently used first, never pinned ones) so that total <= limit. `keep` is never chosen.
            std::vector<std::string> planEviction( int64_t limitBytes, const std::string& keep = {} ) const
            {
                std::vector<std::string> out;
                int64_t                  total = totalSize();
                if( total <= limitBytes ) return out;
                std::vector<const Item*> order;
                for( auto& kv : items )
                    if( kv.second.pins == 0 && kv.first != keep ) order.push_back( &kv.second );
                std::sort( order.begin(), order.end(), []( const Item* a, const Item* b ) { return a->lastUse < b->lastUse; } );
                for( auto* it : order )
                {
                    if( total <= limitBytes ) break;
                    out.push_back( it->key );
                    total -= it->size;
                }
                return out;
            }

            // Text form: one item per line "key<TAB>size<TAB>etag(pct)<TAB>remoteSize<TAB>mtime". Order == LRU order.
            std::string serialize() const
            {
                std::vector<const Item*> order;
                for( auto& kv : items ) order.push_back( &kv.second );
                std::sort( order.begin(), order.end(), []( const Item* a, const Item* b ) { return a->lastUse < b->lastUse; } );
                std::ostringstream o;
                o << "kickcache1\n";
                for( auto* it : order )
                    o << it->key << '\t' << it->size << '\t' << webdav::percentEncodePath( it->version.etag ) << '\t' << it->version.size << '\t' << it->version.mtimeMs << '\n';
                return o.str();
            }

            // Tolerant parse: a corrupt line is skipped, a wrong header yields an empty index. Returns number of skipped lines.
            int parse( const std::string& text )
            {
                items.clear();
                clock = 0;
                std::istringstream in( text );
                std::string        line;
                if( !std::getline( in, line ) || line != "kickcache1" ) return 0;
                int bad = 0;
                while( std::getline( in, line ) )
                {
                    std::vector<std::string> f;
                    size_t                   p = 0;
                    while( true )
                    {
                        auto t = line.find( '\t', p );
                        f.push_back( line.substr( p, t == std::string::npos ? std::string::npos : t - p ) );
                        if( t == std::string::npos ) break;
                        p = t + 1;
                    }
                    if( f.size() != 5 || f[0].empty() || f[0].find_first_not_of( "0123456789abcdef" ) != std::string::npos ) { ++bad; continue; }
                    char *e1 = nullptr, *e2 = nullptr, *e3 = nullptr;
                    long long sz = std::strtoll( f[1].c_str(), &e1, 10 ), rs = std::strtoll( f[3].c_str(), &e2, 10 ), mt = std::strtoll( f[4].c_str(), &e3, 10 );
                    if( f[1].empty() || *e1 || f[3].empty() || *e2 || f[4].empty() || *e3 || sz < 0 ) { ++bad; continue; }
                    put( f[0], sz, webdav::PartMeta{ webdav::percentDecode( f[2] ), rs, mt } );
                }
                return bad;
            }

        private:
            std::map<std::string, Item> items;
            int64_t                     clock{ 0 };
        };

        // ------------------------------------------------------------ cache with files
        class SourceCache
        {
        public:
            SourceCache( std::string dir, int64_t limitBytes ) : root( std::move( dir ) ), limit( limitBytes )
            {
                ::mkdir( root.c_str(), 0700 );
                loadIndex();
            }

            // 64-bit FNV-1a of "sourceId\npath" as 16 hex digits.
            static std::string makeKey( const std::string& sourceId, const std::string& path )
            {
                uint64_t h = 1469598103934665603ull;
                for( unsigned char c : sourceId + "\n" + path )
                {
                    h ^= c;
                    h *= 1099511628211ull;
                }
                char buf[17];
                std::snprintf( buf, sizeof buf, "%016llx", (unsigned long long)h );
                return buf;
            }

            std::string dataPath( const std::string& key ) const { return root + "/" + key + ".bin"; }
            std::string partPath( const std::string& key ) const { return root + "/" + key + ".part"; }
            std::string metaPath( const std::string& key ) const { return root + "/" + key + ".part.meta"; }

            // Returns the cached file for `key` if it exists, has the recorded size and still matches the remote version.
            // A stale, missing or size-mismatching (corrupt/truncated) entry is dropped together with its file.
            bool lookup( const std::string& key, const webdav::PartMeta& remote, std::string& outFile )
            {
                std::lock_guard<std::mutex> l( m );
                CacheIndex::Item            it;
                if( !index.get( key, it ) ) return false;
                int64_t actual = fileSize( dataPath( key ) );
                if( actual < 0 || actual != it.size || webdav::isCacheStaleMeta( it.version, remote ) )
                {
                    index.remove( key );
                    ::unlink( dataPath( key ).c_str() );
                    saveIndex();
                    return false;
                }
                index.touch( key );
                outFile = dataPath( key );
                return true;
            }

            void pin( const std::string& key ) { std::lock_guard<std::mutex> l( m ); index.pin( key ); }
            void unpin( const std::string& key ) { std::lock_guard<std::mutex> l( m ); index.unpin( key ); }

            // Moves the finished .part into place, records it, evicts LRU entries down to the limit. Returns false on I/O failure.
            bool commit( const std::string& key, const webdav::PartMeta& version )
            {
                std::lock_guard<std::mutex> l( m );
                int64_t                     sz = fileSize( partPath( key ) );
                if( sz < 0 ) return false;
                if( version.size >= 0 && sz != version.size ) // incomplete or wrong length: never publish
                    return false;
                if( ::rename( partPath( key ).c_str(), dataPath( key ).c_str() ) != 0 ) return false;
                ::unlink( metaPath( key ).c_str() );
                index.put( key, sz, version );
                evictLocked( key );
                saveIndex();
                return true;
            }

            void savePartMeta( const std::string& key, const webdav::PartMeta& v ) const
            {
                std::ofstream f( metaPath( key ), std::ios::trunc );
                f << webdav::percentEncodePath( v.etag ) << '\t' << v.size << '\t' << v.mtimeMs << '\n';
            }
            bool loadPartMeta( const std::string& key, webdav::PartMeta& v ) const
            {
                std::ifstream f( metaPath( key ) );
                std::string   line;
                if( !std::getline( f, line ) ) return false;
                auto a = line.find( '\t' );
                auto b = a == std::string::npos ? a : line.find( '\t', a + 1 );
                if( b == std::string::npos ) return false;
                v.etag    = webdav::percentDecode( line.substr( 0, a ) );
                v.size    = std::atoll( line.substr( a + 1, b - a - 1 ).c_str() );
                v.mtimeMs = std::atoll( line.substr( b + 1 ).c_str() );
                return true;
            }
            void discardPart( const std::string& key ) const
            {
                ::unlink( partPath( key ).c_str() );
                ::unlink( metaPath( key ).c_str() );
            }
            static int64_t fileSize( const std::string& p )
            {
                struct stat st;
                return ::stat( p.c_str(), &st ) == 0 && S_ISREG( st.st_mode ) ? (int64_t)st.st_size : -1;
            }

            void setLimit( int64_t bytes )
            {
                std::lock_guard<std::mutex> l( m );
                limit = bytes;
                evictLocked( {} );
                saveIndex();
            }
            int64_t totalSize() const { std::lock_guard<std::mutex> l( m ); return index.totalSize(); }
            size_t  count() const { std::lock_guard<std::mutex> l( m ); return index.count(); }

        private:
            void evictLocked( const std::string& keep )
            {
                for( auto& k : index.planEviction( limit, keep ) )
                {
                    ::unlink( dataPath( k ).c_str() );
                    index.remove( k );
                }
            }
            void loadIndex()
            {
                std::ifstream f( root + "/index.txt" );
                std::string   text( ( std::istreambuf_iterator<char>( f ) ), std::istreambuf_iterator<char>() );
                index.parse( text );
            }
            void saveIndex() const
            {
                auto tmp = root + "/index.txt.tmp";
                {
                    std::ofstream f( tmp, std::ios::trunc );
                    f << index.serialize();
                }
                ::rename( tmp.c_str(), ( root + "/index.txt" ).c_str() );
            }

            std::string        root;
            int64_t            limit;
            mutable std::mutex m;
            CacheIndex         index;
        };
    }
}
