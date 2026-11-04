// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

namespace ui
{
    namespace fileUtils
    {
        /**
         * Thread-safe cache for calculated folder sizes.
         * Stores path -> (size, mtime). Invalidation happens when mtime changes.
         */
        class FolderSizeCache
        {
        public:
            struct Entry
            {
                int64_t size{ 0 };
                int64_t mtime{ 0 };
            };

            bool get( const std::string& path, int64_t currentMtime, int64_t& outSize ) const
            {
                std::lock_guard<std::mutex> lock( mutex );
                auto it = entries.find( path );
                if( it != entries.end() && it->second.mtime == currentMtime )
                {
                    outSize = it->second.size;
                    return true;
                }
                return false;
            }

            void put( const std::string& path, int64_t size, int64_t mtime )
            {
                std::lock_guard<std::mutex> lock( mutex );
                entries[path] = { size, mtime };
            }

            void invalidate( const std::string& path )
            {
                std::lock_guard<std::mutex> lock( mutex );
                entries.erase( path );
            }

            void clear()
            {
                std::lock_guard<std::mutex> lock( mutex );
                entries.clear();
            }

            size_t size() const
            {
                std::lock_guard<std::mutex> lock( mutex );
                return entries.size();
            }

        private:
            mutable std::mutex mutex;
            std::unordered_map<std::string, Entry> entries;
        };

        /// One entry of a folder listing for scanFolderSize.
        struct DirEntry
        {
            std::string path;
            bool        isDirectory{ false };
            bool        isSymlink{ false }; // a link counts with its own size and is never followed (no loops, no double counting)
            int64_t     size{ 0 };
            int64_t     mtime{ 0 };
        };

        /**
         * Folder size by one scan. `list(dir)` returns the entries of one folder (the caller's file system: JUCE on the device,
         * std::filesystem in the host test). Each direct sub-folder is one unit of work and is cached (path, mtime) when done, so a
         * cancelled scan keeps what it finished. Symbolic links are not followed; the depth is bounded (a mount loop cannot run
         * away). Returns -1 when `shouldExit` asked to stop.
         */
        template<class Lister>
        int64_t scanFolderSize( const std::string& dir, Lister&& list, FolderSizeCache& cache, const std::function<bool()>& shouldExit, int depth = 0 )
        {
            constexpr int kMaxDepth = 64;
            if( depth > kMaxDepth )
                return 0;
            int64_t total = 0;
            for( const DirEntry& e : list( dir ) )
            {
                if( shouldExit && shouldExit() )
                    return -1;
                if( e.isDirectory && !e.isSymlink )
                {
                    int64_t sub = 0;
                    if( !cache.get( e.path, e.mtime, sub ) )
                    {
                        sub = scanFolderSize( e.path, list, cache, shouldExit, depth + 1 );
                        if( sub < 0 )
                            return -1;
                        cache.put( e.path, sub, e.mtime );
                    }
                    total += sub;
                }
                else
                    total += e.size;
            }
            return total;
        }

        /**
         * Generation tracker for cancelling ongoing asynchronous tasks.
         * Each navigation / refresh increments the generation counter.
         * Background tasks check isCancelled( taskGeneration ) and exit early.
         */
        class GenerationTracker
        {
        public:
            uint64_t nextGeneration() noexcept
            {
                return ++currentGeneration;
            }

            uint64_t getGeneration() const noexcept
            {
                return currentGeneration.load();
            }

            bool isCancelled( uint64_t generation ) const noexcept
            {
                return generation != currentGeneration.load();
            }

            void reset() noexcept
            {
                currentGeneration.store( 0 );
            }

        private:
            std::atomic<uint64_t> currentGeneration{ 0 };
        };
    }
}
