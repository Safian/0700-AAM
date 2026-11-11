// SPDX-License-Identifier: AGPL-3.0-or-later
// Owns the configured sources, the shared cache, the download queue and the (single, low-priority) listing thread of the
// file browser. All network work happens on these worker threads; callbacks run on them too and must be marshalled to the
// message thread by the caller (juce::MessageManager::callAsync).
#pragma once

#include "kick_downloadQueue.h"
#include "kick_googleDriveSource.h"
#include "kick_localSource.h"
#include "kick_sourceConfig.h"
#include "kick_webdavSource.h"

#include <condition_variable>
#include <thread>

namespace ui
{
    namespace fileSource
    {
        class SourceHub
        {
        public:
            using ListCallback = std::function<void( uint64_t gen, const ListResult& )>;

            SourceHub( const std::string& prefsDir, const std::string& cacheDir, int64_t cacheLimitBytes )
                : configFile( prefsDir + "/file_sources.txt" ), secretDir( prefsDir + "/file_sources" ), cache( cacheDir, cacheLimitBytes ), queue( cache )
            {
                ::mkdir( prefsDir.c_str(), 0700 );
                ::mkdir( secretDir.c_str(), 0700 );
                reload();
                lister = std::thread( [this] { listLoop(); } );
            }
            ~SourceHub()
            {
                {
                    std::lock_guard<std::mutex> l( m );
                    stopping = true;
                    ++gen;
                }
                cv.notify_all();
                if( lister.joinable() ) lister.join();
                queue.shutdown();
            }

            void reload()
            {
                std::lock_guard<std::mutex> l( m );
                configs = loadSources( configFile );
                sources.clear();
                for( auto& c : configs ) sources.push_back( make( c ) );
            }

            std::vector<std::shared_ptr<FileBrowserSource>> all() const { std::lock_guard<std::mutex> l( m ); return sources; }
            std::vector<SourceConfig> allConfigs() const { std::lock_guard<std::mutex> l( m ); return configs; }

            std::shared_ptr<FileBrowserSource> find( const std::string& id ) const
            {
                std::lock_guard<std::mutex> l( m );
                for( auto& s : sources )
                    if( s->id() == id ) return s;
                return nullptr;
            }

            std::string secretPathFor( const std::string& id ) const { return secretDir + "/" + id + ".secret"; }

            // Adds or replaces a WebDAV source. `secret` empty = keep the stored one. Returns false on invalid input / I/O error.
            bool upsertWebDav( SourceConfig c, const std::string& secret )
            {
                c.type = "webdav";
                if( !isValidSourceId( c.id ) || !webdav::parseUrl( c.location ).ok || webdav::hasLineBreak( c.user ) || webdav::hasLineBreak( secret ) ) return false;
                if( !secret.empty() && !writeSecretFile( secretPathFor( c.id ), secret ) ) return false;
                std::vector<SourceConfig> next = allConfigs();
                bool                      found = false;
                for( auto& x : next )
                    if( x.id == c.id ) { x = c; found = true; }
                if( !found ) next.push_back( c );
                if( !saveSources( configFile, next ) ) return false;
                reload();
                return true;
            }

            // Adds or replaces a local-folder source (no secret).
            bool upsertLocal( SourceConfig c )
            {
                c.type = "local";
                if( !isValidSourceId( c.id ) || c.location.empty() || webdav::hasLineBreak( c.location ) || webdav::hasLineBreak( c.name ) ) return false;
                std::vector<SourceConfig> next  = allConfigs();
                bool                      found = false;
                for( auto& x : next )
                    if( x.id == c.id ) { x = c; found = true; }
                if( !found ) next.push_back( c );
                if( !saveSources( configFile, next ) ) return false;
                reload();
                return true;
            }

            // Adds or replaces a Google Drive REST API v3 source (`secret` = key=value credentials). Empty `secret` keeps existing.
            bool upsertGoogleDrive( SourceConfig c, const std::string& secret )
            {
                c.type = "gdrive";
                if( !isValidSourceId( c.id ) || c.location.empty() || webdav::hasLineBreak( c.location ) || webdav::hasLineBreak( c.user ) || webdav::hasLineBreak( c.name ) ) return false;
                if( !secret.empty() && !writeSecretFile( secretPathFor( c.id ), secret ) ) return false;
                std::vector<SourceConfig> next  = allConfigs();
                bool                      found = false;
                for( auto& x : next )
                    if( x.id == c.id ) { x = c; found = true; }
                if( !found ) next.push_back( c );
                if( !saveSources( configFile, next ) ) return false;
                reload();
                return true;
            }

            void removeSource( const std::string& id )
            {
                if( auto s = find( id ) ) s->signOut();
                auto next = allConfigs();
                next.erase( std::remove_if( next.begin(), next.end(), [&]( const SourceConfig& x ) { return x.id == id; } ), next.end() );
                saveSources( configFile, next );
                reload();
            }

            // Last request wins; an older one still running is cancelled via its generation.
            uint64_t listAsync( std::shared_ptr<FileBrowserSource> src, const std::string& path, ListCallback cb )
            {
                std::lock_guard<std::mutex> l( m );
                ++gen;
                req.src  = std::move( src );
                req.path = path;
                req.cb   = std::move( cb );
                req.gen  = gen;
                req.pending = true;
                cv.notify_all();
                return gen;
            }
            void cancelListing()
            {
                std::lock_guard<std::mutex> l( m );
                ++gen;
                req.pending = false;
            }

            SourceCache&   getCache() { return cache; }
            DownloadQueue& getQueue() { return queue; }

        private:
            std::shared_ptr<FileBrowserSource> make( const SourceConfig& c ) const
            {
                if( c.type == "gdrive" && ( c.auth == "oauth" || c.auth == "apikey" || c.auth == "bearer" || c.location.rfind( "gdrive://", 0 ) == 0 ) )
                    return std::make_shared<GoogleDriveSource>( c, secretPathFor( c.id ) );
                if( c.type == "local" || c.type == "splice" || c.type == "gdrive" ) return std::make_shared<LocalFolderSource>( c.id, c.name, c.location );
                return std::make_shared<WebDavSource>( c, secretPathFor( c.id ) );
            }

            struct Request
            {
                std::shared_ptr<FileBrowserSource> src;
                std::string                        path;
                ListCallback                       cb;
                uint64_t                           gen{ 0 };
                bool                               pending{ false };
            };

            void listLoop()
            {
                lowerCurrentThreadPriority();
                for( ;; )
                {
                    Request r;
                    {
                        std::unique_lock<std::mutex> l( m );
                        cv.wait( l, [this] { return stopping || req.pending; } );
                        if( stopping ) return;
                        r           = req;
                        req.pending = false;
                    }
                    auto cancelled = [this, g = r.gen]() { std::lock_guard<std::mutex> l( m ); return stopping || gen != g; };
                    auto res       = r.src->list( r.path, cancelled );
                    if( !cancelled() && r.cb ) r.cb( r.gen, res );
                }
            }

            std::string                                     configFile, secretDir;
            mutable std::mutex                              m;
            std::condition_variable                         cv;
            std::vector<SourceConfig>                       configs;
            std::vector<std::shared_ptr<FileBrowserSource>> sources;
            Request                                         req;
            uint64_t                                        gen{ 0 };
            bool                                            stopping{ false };
            SourceCache                                     cache;
            DownloadQueue                                   queue;
            std::thread                                     lister;
        };
    }
}
