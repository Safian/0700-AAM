// SPDX-License-Identifier: AGPL-3.0-or-later
// Host test for FolderSizeCache and GenerationTracker. Build: g++ -std=c++17 -Wall -Wextra -fsanitize=address,undefined
#include "../../modules/kick_ui/modes/filebrowser/kick_folderSizeHelper.h"
#include <cassert>
#include <chrono>
#include <cstdio>
#include <thread>
#include <vector>

static int g_fail = 0;
#define CHECK( cond )                                                                                               \
    do {                                                                                                           \
        if( !( cond ) ) {                                                                                          \
            std::printf( "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond );                                         \
            ++g_fail;                                                                                              \
        }                                                                                                          \
    } while( 0 )

#define CHECK_EQ( a, b )                                                                                           \
    do {                                                                                                           \
        long long va = (long long)( a ), vb = (long long)( b );                                                    \
        if( va != vb ) {                                                                                           \
            std::printf( "FAIL %s:%d  %s = %lld, expected %lld\n", __FILE__, __LINE__, #a, va, vb );              \
            ++g_fail;                                                                                              \
        }                                                                                                          \
    } while( 0 )

using ui::fileUtils::FolderSizeCache;
using ui::fileUtils::GenerationTracker;

static void test_cache_basic()
{
    FolderSizeCache cache;
    CHECK_EQ( cache.size(), 0 );

    int64_t outSize = 0;
    CHECK( !cache.get( "/path/to/folder", 1000, outSize ) );

    cache.put( "/path/to/folder", 1234567, 1000 );
    CHECK_EQ( cache.size(), 1 );

    // Matching mtime -> hit
    CHECK( cache.get( "/path/to/folder", 1000, outSize ) );
    CHECK_EQ( outSize, 1234567 );

    // Changed mtime -> miss (invalidation)
    CHECK( !cache.get( "/path/to/folder", 1001, outSize ) );

    // Update with new mtime
    cache.put( "/path/to/folder", 7654321, 1001 );
    CHECK( cache.get( "/path/to/folder", 1001, outSize ) );
    CHECK_EQ( outSize, 7654321 );
    CHECK( !cache.get( "/path/to/folder", 1000, outSize ) );

    // Invalidate
    cache.invalidate( "/path/to/folder" );
    CHECK_EQ( cache.size(), 0 );
    CHECK( !cache.get( "/path/to/folder", 1001, outSize ) );

    // Clear
    cache.put( "/a", 10, 100 );
    cache.put( "/b", 20, 100 );
    CHECK_EQ( cache.size(), 2 );
    cache.clear();
    CHECK_EQ( cache.size(), 0 );
}

static void test_generation_tracker()
{
    GenerationTracker tracker;
    CHECK_EQ( tracker.getGeneration(), 0 );
    CHECK( !tracker.isCancelled( 0 ) );
    CHECK( tracker.isCancelled( 1 ) );

    uint64_t g1 = tracker.nextGeneration();
    CHECK_EQ( g1, 1 );
    CHECK_EQ( tracker.getGeneration(), 1 );
    CHECK( tracker.isCancelled( 0 ) );
    CHECK( !tracker.isCancelled( 1 ) );

    uint64_t g2 = tracker.nextGeneration();
    CHECK_EQ( g2, 2 );
    CHECK( tracker.isCancelled( 0 ) );
    CHECK( tracker.isCancelled( 1 ) );
    CHECK( !tracker.isCancelled( 2 ) );
}

static void test_cache_concurrency()
{
    FolderSizeCache cache;
    constexpr int numThreads = 8;
    constexpr int opsPerThread = 2000;
    std::vector<std::thread> threads;
    threads.reserve( numThreads );

    for( int t = 0; t < numThreads; ++t )
    {
        threads.emplace_back( [&cache, t]() {
            for( int i = 0; i < opsPerThread; ++i )
            {
                std::string path = "/folder_" + std::to_string( ( t * 10 + i ) % 50 );
                int64_t mtime = 1000 + ( i % 5 );
                int64_t size = 100 + i;

                cache.put( path, size, mtime );

                int64_t outSize = 0;
                cache.get( path, mtime, outSize );

                if( i % 20 == 0 )
                    cache.invalidate( path );
            }
        } );
    }

    for( auto& th : threads )
        th.join();

    CHECK( cache.size() <= 50 );
}

static void test_cancellation_simulation()
{
    GenerationTracker tracker;
    FolderSizeCache cache;

    uint64_t taskGen = tracker.getGeneration();
    std::atomic<bool> taskStarted{ false };
    std::atomic<int> processedCount{ 0 };
    std::atomic<bool> taskAborted{ false };

    // Simulate background folder scan that checks cancellation
    std::thread worker( [&]() {
        taskStarted = true;
        for( int i = 0; i < 1000; ++i )
        {
            if( tracker.isCancelled( taskGen ) )
            {
                taskAborted = true;
                return;
            }
            ++processedCount;
            std::this_thread::sleep_for( std::chrono::microseconds( 200 ) );
        }
        cache.put( "/slow/folder", 999999, 500 );
    } );

    while( !taskStarted )
        std::this_thread::yield();

    // Give it a tiny bit of time to process a few items
    std::this_thread::sleep_for( std::chrono::milliseconds( 5 ) );

    // User navigates away -> bump generation
    tracker.nextGeneration();

    worker.join();

    CHECK( taskAborted.load() );
    CHECK( processedCount.load() < 1000 );

    int64_t outSize = 0;
    CHECK( !cache.get( "/slow/folder", 500, outSize ) );
}

int main()
{
    test_cache_basic();
    test_generation_tracker();
    test_cache_concurrency();
    test_cancellation_simulation();

    if( g_fail == 0 )
        std::printf( "folder size helper tests OK\n" );
    else
        std::printf( "folder size helper tests FAILED: %d errors\n", g_fail );

    return g_fail ? 1 : 0;
}
