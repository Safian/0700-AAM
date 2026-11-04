// SPDX-License-Identifier: AGPL-3.0-or-later
#pragma once

#include <atomic>
#include <cstdint>
#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

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
