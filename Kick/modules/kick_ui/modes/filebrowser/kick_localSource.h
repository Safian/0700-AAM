// SPDX-License-Identifier: AGPL-3.0-or-later
// M0: local-folder source behind FileBrowserSource (POSIX, JUCE-free). Paths are relative to the configured root and can never
// leave it (".." is normalised away). directFile() hands out the real file, so nothing is copied into the cache.
#pragma once

#include "kick_fileSource.h"
#include "kick_webdavHelper.h"

#include <algorithm>
#include <cstdio>
#include <dirent.h>
#include <fstream>
#include <map>
#include <mutex>
#include <sys/stat.h>

namespace ui
{
    namespace fileSource
    {
        class LocalFolderSource : public FileBrowserSource
        {
        public:
            LocalFolderSource( std::string sourceId, std::string name, std::string rootDir ) : sid( std::move( sourceId ) ), nm( std::move( name ) ), root( std::move( rootDir ) )
            {
                while( root.size() > 1 && root.back() == '/' ) root.pop_back();
            }

            std::string id() const override { return sid; }
            std::string displayName() const override { return nm; }
            State       state() const override
            {
                struct stat st;
                return ::stat( root.c_str(), &st ) == 0 && S_ISDIR( st.st_mode ) ? State::Ready : State::Error;
            }
            Result beginAuth( AuthChallenge& c, const CancelFn& ) override
            {
                c = AuthChallenge{};
                return state() == State::Ready ? Result{} : Result::make( Status::NotFound, "folder missing" );
            }

            ListResult list( const std::string& path, const CancelFn& cancel ) override
            {
                ListResult r;
                auto       sp = webdav::normaliseSourcePath( path );
                DIR*       d  = ::opendir( real( sp ).c_str() );
                if( !d ) { r.result = Result::make( errno == ENOENT ? Status::NotFound : Status::IoError, "cannot open folder" ); return r; }
                while( auto* e = ::readdir( d ) )
                {
                    if( cancel && cancel() ) { ::closedir( d ); r.entries.clear(); r.result = Result::make( Status::Cancelled ); return r; }
                    std::string n = e->d_name;
                    if( n == "." || n == ".." ) continue;
                    Entry en;
                    if( fill( webdav::joinPath( sp, n ), en ) ) r.entries.push_back( std::move( en ) );
                }
                ::closedir( d );
                std::sort( r.entries.begin(), r.entries.end(), []( const Entry& a, const Entry& b ) { return a.isDir != b.isDir ? a.isDir : a.name < b.name; } );
                return r;
            }

            Result stat( const std::string& path, Entry& out, const CancelFn& ) override
            {
                return fill( webdav::normaliseSourcePath( path ), out ) ? Result{} : Result::make( Status::NotFound );
            }

            // Plain copy (used only when a caller insists on a cache copy); resumes by appending from `offset`.
            FetchResult fetch( const std::string& path, const std::string& partFile, int64_t offset, const CancelFn& cancel, const ProgressFn& progress ) override
            {
                FetchResult fr;
                std::ifstream in( real( webdav::normaliseSourcePath( path ) ), std::ios::binary );
                if( !in ) { fr.result = Result::make( Status::NotFound ); return fr; }
                in.seekg( 0, std::ios::end );
                int64_t total = (int64_t)in.tellg();
                if( offset < 0 || offset > total ) offset = 0, fr.restarted = true;
                in.seekg( offset );
                std::ofstream out( partFile, offset > 0 ? std::ios::binary | std::ios::app : std::ios::binary | std::ios::trunc );
                if( !out ) { fr.result = Result::make( Status::IoError ); return fr; }
                char    buf[65536];
                int64_t done = offset;
                while( in )
                {
                    if( cancel && cancel() ) { fr.result = Result::make( Status::Cancelled ); fr.bytesOnDisk = done; return fr; }
                    in.read( buf, sizeof buf );
                    auto n = in.gcount();
                    if( n <= 0 ) break;
                    out.write( buf, n );
                    done += n;
                    if( progress ) progress( done, total );
                }
                out.flush();
                fr.complete    = out.good() && done == total;
                fr.totalBytes  = total;
                fr.bytesOnDisk = done;
                if( !fr.complete ) fr.result = Result::make( Status::IoError );
                return fr;
            }

            ChangeResult changes( const std::string& path, const CancelFn& cancel ) override
            {
                ChangeResult cr;
                auto         lr = list( path, cancel );
                cr.result       = lr.result;
                if( !lr.result.ok() ) return cr;
                std::lock_guard<std::mutex> l( m );
                auto&                       prev = snapshots[webdav::normaliseSourcePath( path )];
                cr.changes                       = webdav::diffListings( prev, lr.entries );
                prev                             = lr.entries;
                return cr;
            }

            void signOut() override {}

            bool directFile( const std::string& path, std::string& out ) const override
            {
                out = real( webdav::normaliseSourcePath( path ) );
                struct stat st;
                return ::stat( out.c_str(), &st ) == 0 && S_ISREG( st.st_mode );
            }

        private:
            std::string real( const std::string& sp ) const { return sp == "/" ? root : root + sp; }
            bool        fill( const std::string& sp, Entry& e ) const
            {
                struct stat st;
                if( ::stat( real( sp ).c_str(), &st ) != 0 ) return false;
                e.path  = sp;
                e.name  = sp == "/" ? "" : sp.substr( sp.rfind( '/' ) + 1 );
                e.isDir = S_ISDIR( st.st_mode );
                e.size  = e.isDir ? -1 : (int64_t)st.st_size;
#if defined( __APPLE__ )
                e.mtimeMs = (int64_t)st.st_mtimespec.tv_sec * 1000 + st.st_mtimespec.tv_nsec / 1000000;
#else
                e.mtimeMs = (int64_t)st.st_mtim.tv_sec * 1000 + st.st_mtim.tv_nsec / 1000000;
#endif
                return true;
            }

            std::string                          sid, nm, root;
            std::mutex                           m;
            std::map<std::string, std::vector<Entry>> snapshots;
        };
    }
}
