// SPDX-License-Identifier: AGPL-3.0-or-later
// Download queue: one low-priority background thread, cancellable, resumable (.part + Range). JUCE-free.
// Callbacks run on the worker thread; the UI must marshal them to the message thread itself. Loading a sample must only
// happen from onDone(Ok) with the finished cache file.
#pragma once

#include "kick_fileSource.h"
#include "kick_sourceCache.h"

#include <atomic>
#include <condition_variable>
#include <deque>
#include <memory>
#include <mutex>
#include <thread>
#if defined( __APPLE__ )
    #include <pthread.h>
    #include <pthread/qos.h>
#elif defined( __linux__ )
    #include <sys/resource.h>
    #include <sys/syscall.h>
#endif

namespace ui
{
    namespace fileSource
    {
        // Background network/disk threads must never compete with audio or the UI.
        inline void lowerCurrentThreadPriority()
        {
#if defined( __APPLE__ )
            pthread_set_qos_class_self_np( QOS_CLASS_UTILITY, 0 );
#elif defined( __linux__ )
            setpriority( PRIO_PROCESS, (id_t)syscall( SYS_gettid ), 10 );
#endif
        }

        struct DownloadJob
        {
            std::shared_ptr<FileBrowserSource>                              source;
            Entry                                                           remote; // fresh listing entry (validators)
            std::function<void( uint64_t id, int64_t done, int64_t total )> onProgress;
            std::function<void( uint64_t id, const Result&, const std::string& cacheFile )> onDone;
        };

        class DownloadQueue
        {
        public:
            explicit DownloadQueue( SourceCache& c ) : cache( c ) { worker = std::thread( [this] { run(); } ); }
            ~DownloadQueue() { shutdown(); }

            DownloadQueue( const DownloadQueue& ) = delete;
            DownloadQueue& operator=( const DownloadQueue& ) = delete;

            uint64_t enqueue( DownloadJob job )
            {
                std::lock_guard<std::mutex> l( m );
                auto                        j = std::make_shared<Item>();
                j->id                         = ++nextId;
                j->job                        = std::move( job );
                pending.push_back( j );
                cv.notify_one();
                return j->id;
            }

            // Cancels a queued or running job; a running download stops at the next chunk and keeps its .part for resume.
            void cancel( uint64_t id )
            {
                std::shared_ptr<Item> dropped;
                {
                    std::lock_guard<std::mutex> l( m );
                    for( auto it = pending.begin(); it != pending.end(); ++it )
                        if( ( *it )->id == id ) { dropped = *it; pending.erase( it ); break; }
                    if( !dropped && current && current->id == id ) current->cancelled = true;
                }
                if( dropped && dropped->job.onDone ) dropped->job.onDone( id, Result::make( Status::Cancelled ), {} );
            }

            void cancelAll()
            {
                std::deque<std::shared_ptr<Item>> dropped;
                {
                    std::lock_guard<std::mutex> l( m );
                    dropped.swap( pending );
                    if( current ) current->cancelled = true;
                }
                for( auto& d : dropped )
                    if( d->job.onDone ) d->job.onDone( d->id, Result::make( Status::Cancelled ), {} );
            }

            bool idle() const { std::lock_guard<std::mutex> l( m ); return pending.empty() && !current; }

            void shutdown()
            {
                {
                    std::lock_guard<std::mutex> l( m );
                    if( stopping ) return;
                    stopping = true;
                    if( current ) current->cancelled = true;
                }
                cv.notify_all();
                if( worker.joinable() ) worker.join();
            }

        private:
            struct Item
            {
                uint64_t          id{ 0 };
                DownloadJob       job;
                std::atomic<bool> cancelled{ false };
            };

            void run()
            {
                lowerCurrentThreadPriority();
                for( ;; )
                {
                    std::shared_ptr<Item> it;
                    {
                        std::unique_lock<std::mutex> l( m );
                        cv.wait( l, [this] { return stopping || !pending.empty(); } );
                        if( stopping ) return;
                        it      = pending.front();
                        current = it;
                        pending.pop_front();
                    }
                    process( *it );
                    std::lock_guard<std::mutex> l( m );
                    current.reset();
                }
            }

            void process( Item& it )
            {
                auto& job = it.job;
                auto  done = [&]( const Result& r, const std::string& file ) { if( job.onDone ) job.onDone( it.id, r, file ); };
                auto  cancelFn = [&it, this]() { return it.cancelled.load() || stoppingFlag(); };
                if( !job.source ) return done( Result::make( Status::Unsupported, "no source" ), {} );

                std::string direct;
                if( job.source->directFile( job.remote.path, direct ) ) return done( Result{}, direct );

                const auto key     = SourceCache::makeKey( job.source->id(), job.remote.path );
                const auto version = webdav::partMetaFor( job.remote );
                std::string cached;
                if( cache.lookup( key, version, cached ) ) return done( Result{}, cached );

                // resume decision for an existing .part
                int64_t offset   = 0;
                bool    complete = false;
                {
                    int64_t partSize = SourceCache::fileSize( cache.partPath( key ) );
                    webdav::PartMeta saved;
                    if( partSize > 0 && cache.loadPartMeta( key, saved ) )
                    {
                        auto d = webdav::decideResume( partSize, saved, job.remote );
                        if( d.kind == webdav::ResumeDecision::Kind::Resume ) offset = d.offset;
                        else if( d.kind == webdav::ResumeDecision::Kind::Complete ) complete = true;
                        else cache.discardPart( key );
                    }
                    else if( partSize >= 0 )
                        cache.discardPart( key );
                }
                if( !complete )
                {
                    cache.savePartMeta( key, version );
                    FetchResult fr = job.source->fetch( job.remote.path, cache.partPath( key ), offset, cancelFn, [&]( int64_t d, int64_t t ) {
                        if( job.onProgress ) job.onProgress( it.id, d, t );
                    } );
                    if( !fr.result.ok() && fr.result.httpStatus == 416 && offset > 0 ) // part longer than the remote file: refetch once from 0
                    {
                        cache.discardPart( key );
                        cache.savePartMeta( key, version );
                        offset = 0;
                        fr     = job.source->fetch( job.remote.path, cache.partPath( key ), 0, cancelFn, [&]( int64_t d, int64_t t ) {
                            if( job.onProgress ) job.onProgress( it.id, d, t );
                        } );
                    }
                    if( !fr.result.ok() ) // keep the .part: Cancelled/NetworkError resume later; BadResponse restarts via the meta check
                    {
                        if( fr.result.status == Status::IoError || fr.result.status == Status::BadResponse || fr.result.status == Status::NotFound ) cache.discardPart( key );
                        return done( fr.result, {} );
                    }
                    if( !fr.complete ) return done( Result::make( Status::NetworkError, "incomplete download" ), {} );
                }
                if( !cache.commit( key, version ) )
                {
                    cache.discardPart( key );
                    return done( Result::make( Status::IoError, "cache commit failed" ), {} );
                }
                std::string out;
                if( !cache.lookup( key, version, out ) ) return done( Result::make( Status::IoError, "cache entry vanished" ), {} );
                done( Result{}, out );
            }

            bool stoppingFlag() const { std::lock_guard<std::mutex> l( m ); return stopping; }

            SourceCache&                      cache;
            mutable std::mutex                m;
            std::condition_variable           cv;
            std::deque<std::shared_ptr<Item>> pending;
            std::shared_ptr<Item>             current;
            uint64_t                          nextId{ 0 };
            bool                              stopping{ false };
            std::thread                       worker;
        };
    }
}
